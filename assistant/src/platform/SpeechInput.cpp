#include "SpeechInput.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <QPointer>
#include <QtCore/private/qandroidextras_p.h>
#endif

namespace {

QString tempDir() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QStringLiteral("/BlopAssistent");
    QDir().mkpath(dir);
    return dir;
}

QString readUtf8(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

const char kListenScript[] = R"ps1(
param(
    [Parameter(Mandatory = $true)][string]$StopFile
)
$ErrorActionPreference = 'Stop'
$outFile = "$StopFile.out"
$errFile = "$StopFile.err"
try {
    Add-Type -AssemblyName System.Speech
    $engine = $null
    foreach ($name in @('de-DE', 'de', 'en-US')) {
        try {
            $culture = [System.Globalization.CultureInfo]::GetCultureInfo($name)
            $engine = New-Object System.Speech.Recognition.SpeechRecognitionEngine($culture)
            break
        } catch {
            $engine = $null
        }
    }
    if (-not $engine) {
        $engine = New-Object System.Speech.Recognition.SpeechRecognitionEngine
    }
    $engine.SetInputToDefaultAudioDevice()
    $engine.LoadGrammar((New-Object System.Speech.Recognition.DictationGrammar))
    try {
        $engine.EndSilenceTimeout = [TimeSpan]::FromMilliseconds(450)
        $engine.EndSilenceTimeoutAmbiguous = [TimeSpan]::FromMilliseconds(700)
    } catch {}
    if (Test-Path -LiteralPath $outFile) { Remove-Item -LiteralPath $outFile -Force }
    $utf8 = New-Object System.Text.UTF8Encoding $false
    $engine.add_SpeechRecognized({
        param($sender, $e)
        $text = $e.Result.Text
        if ($text) {
            [System.IO.File]::AppendAllText($outFile, $text + [Environment]::NewLine, $utf8)
        }
    }.GetNewClosure())
    Add-Type -AssemblyName System.Windows.Forms
    $engine.RecognizeAsync([System.Speech.Recognition.RecognizeMode]::Multiple)
    $deadline = (Get-Date).AddSeconds(20)
    while (-not (Test-Path -LiteralPath $StopFile)) {
        if ((Get-Date) -gt $deadline) { break }
        [System.Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 50
    }
    try { $engine.RecognizeAsyncStop() } catch {}
    Start-Sleep -Milliseconds 350
    try { $engine.RecognizeAsyncCancel() } catch {}
    exit 0
} catch {
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($errFile, $_.Exception.Message, $utf8)
    exit 1
}
)ps1";

QString friendlySpeechError(const QString &raw) {
    QString text = raw.trimmed();
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    while (text.contains(QStringLiteral("  ")))
        text.replace(QStringLiteral("  "), QStringLiteral(" "));
    if (text.isEmpty())
        return QStringLiteral("Spracherkennung ist nicht verfügbar. Bitte tippen.");
    if (text.size() > 180)
        text = text.left(177) + QStringLiteral("...");
    return text;
}

} // namespace

SpeechInput::SpeechInput(QObject *parent) : QObject(parent) {
    m_poll = new QTimer(this);
    m_poll->setInterval(100);
    connect(m_poll, &QTimer::timeout, this, &SpeechInput::pollHeard);
}

SpeechInput::~SpeechInput() {
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        QFile stop(m_stopFile);
        if (stop.open(QIODevice::WriteOnly))
            stop.close();
        m_proc->waitForFinished(800);
        if (m_proc->state() != QProcess::NotRunning)
            m_proc->kill();
    }
}

bool SpeechInput::available() const {
#if defined(Q_OS_WIN) || defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

bool SpeechInput::listening() const { return m_listening; }

void SpeechInput::report(const QString &text, bool ok) {
    if (m_reported)
        return;
    m_reported = true;
    if (m_poll)
        m_poll->stop();
    if (m_listening) {
        m_listening = false;
        emit listeningChanged(false);
    }
    if (ok)
        emit recognized(text);
    else
        emit failed(text);
}

void SpeechInput::start() {
    if (m_listening)
        return;
    m_reported = false;

#if defined(Q_OS_ANDROID)
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      QJniObject::fromString(QStringLiteral("android.speech.action.RECOGNIZE_SPEECH"))
                          .object<jstring>());
    if (!intent.isValid()) {
        report(QStringLiteral("Spracherkennung ist nicht verfügbar. Bitte tippen."), false);
        return;
    }
    const auto putString = [&intent](const char *key, const QString &value) {
        intent.callObjectMethod(
            "putExtra",
            "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
            QJniObject::fromString(QString::fromUtf8(key)).object<jstring>(),
            QJniObject::fromString(value).object<jstring>());
    };
    putString("android.speech.extra.LANGUAGE_MODEL", QStringLiteral("free_form"));
    putString("android.speech.extra.LANGUAGE", QStringLiteral("de-DE"));
    putString("android.speech.extra.PROMPT", QStringLiteral("Befehl an Blop"));

    m_listening = true;
    emit listeningChanged(true);

    QPointer<SpeechInput> self(this);
    QtAndroidPrivate::startActivity(
        intent, 4201,
        [self](int, int resultCode, const QJniObject &data) {
            if (!self)
                return;
            const QString heard = [&]() -> QString {
                if (resultCode != -1 || !data.isValid())
                    return {};
                const QJniObject list = data.callObjectMethod(
                    "getStringArrayListExtra",
                    "(Ljava/lang/String;)Ljava/util/ArrayList;",
                    QJniObject::fromString(QStringLiteral("android.speech.extra.RESULTS"))
                        .object<jstring>());
                if (!list.isValid() || list.callMethod<jint>("size") <= 0)
                    return {};
                return list.callObjectMethod("get", "(I)Ljava/lang/Object;", 0).toString();
            }();
            const bool canceled = resultCode != -1;
            QMetaObject::invokeMethod(
                self,
                [self, heard, canceled]() {
                    if (!self)
                        return;
                    if (!heard.trimmed().isEmpty())
                        self->report(heard.trimmed(), true);
                    else if (canceled)
                        self->report(QStringLiteral("Abgebrochen."), false);
                    else
                        self->report(QStringLiteral("Nichts erkannt."), false);
                },
                Qt::QueuedConnection);
        });
    return;
#elif defined(Q_OS_WIN)
    const QString dir = tempDir();
    const QString scriptPath = dir + QStringLiteral("/listen.ps1");
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        report(QStringLiteral("Spracherkennung ist nicht verfügbar. Bitte tippen."), false);
        return;
    }
    script.write("\xEF\xBB\xBF");
    script.write(kListenScript);
    script.close();

    m_stopFile = dir + QStringLiteral("/stop-") +
                 QUuid::createUuid().toString(QUuid::Id128);
    QFile::remove(m_stopFile);
    QFile::remove(m_stopFile + QStringLiteral(".out"));
    QFile::remove(m_stopFile + QStringLiteral(".err"));

    const QString powershell =
        QFileInfo::exists(QStringLiteral(
            "C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"))
            ? QStringLiteral("C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe")
            : QStringLiteral("powershell");

    if (m_proc)
        m_proc->deleteLater();
    m_proc = new QProcess(this);
    m_proc->setProgram(powershell);
    m_proc->setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-STA"),
                          QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                          QStringLiteral("-File"), scriptPath, QStringLiteral("-StopFile"),
                          m_stopFile});
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString heard = readUtf8(m_stopFile + QStringLiteral(".out")).simplified();
        const QString err = readUtf8(m_stopFile + QStringLiteral(".err"));
        QFile::remove(m_stopFile);
        QFile::remove(m_stopFile + QStringLiteral(".out"));
        QFile::remove(m_stopFile + QStringLiteral(".err"));
        m_stopFile.clear();
        if (m_reported)
            return;
        if (!heard.isEmpty())
            report(heard, true);
        else if (code != 0)
            report(friendlySpeechError(err), false);
        else
            report(QStringLiteral("Nichts erkannt."), false);
    });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            report(QStringLiteral("Spracherkennung ist nicht verfügbar. Bitte tippen."), false);
    });

    m_listening = true;
    emit listeningChanged(true);
    m_proc->start();
    m_poll->start();
#else
    report(QStringLiteral("Spracherkennung ist hier nicht verfügbar. Bitte tippen."), false);
#endif
}

void SpeechInput::pollHeard() {
#if defined(Q_OS_WIN)
    if (m_reported || m_stopFile.isEmpty())
        return;
    const QString err = readUtf8(m_stopFile + QStringLiteral(".err")).trimmed();
    if (!err.isEmpty()) {
        stop();
        report(friendlySpeechError(err), false);
        return;
    }
    const QString heard = readUtf8(m_stopFile + QStringLiteral(".out")).simplified();
    if (heard.isEmpty())
        return;
    stop();
    report(heard, true);
#else
    return;
#endif
}

void SpeechInput::stop() {
#if defined(Q_OS_ANDROID)
    return;
#else
    if (m_stopFile.isEmpty() || QFile::exists(m_stopFile))
        return;
    QFile stop(m_stopFile);
    if (stop.open(QIODevice::WriteOnly))
        stop.close();
#endif
}
