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
    [Parameter(Mandatory = $true)][string]$Ear
)
$ErrorActionPreference = 'Stop'
$goFile = $Ear + '.go'
$stopFile = $Ear + '.stop'
$outFile = $Ear + '.out'
$errFile = $Ear + '.err'
$doneFile = $Ear + '.done'
$quitFile = $Ear + '.quit'
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
    $utf8 = New-Object System.Text.UTF8Encoding $false
    $engine.add_SpeechRecognized({
        param($sender, $e)
        $text = $e.Result.Text
        if ($text) {
            [System.IO.File]::AppendAllText($outFile, $text + [Environment]::NewLine, $utf8)
        }
    }.GetNewClosure())
    Add-Type -AssemblyName System.Windows.Forms
    while (-not (Test-Path -LiteralPath $quitFile)) {
        while (-not (Test-Path -LiteralPath $goFile) -and -not (Test-Path -LiteralPath $quitFile)) {
            [System.Windows.Forms.Application]::DoEvents()
            Start-Sleep -Milliseconds 40
        }
        if (Test-Path -LiteralPath $quitFile) { break }
        if (Test-Path -LiteralPath $stopFile) {
            if (Test-Path -LiteralPath $goFile) { Remove-Item -LiteralPath $goFile -Force }
            [System.IO.File]::WriteAllText($doneFile, '1', $utf8)
            Remove-Item -LiteralPath $stopFile -Force -ErrorAction SilentlyContinue
            continue
        }
        foreach ($path in @($outFile, $errFile, $doneFile)) {
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        }
        $engine.RecognizeAsync([System.Speech.Recognition.RecognizeMode]::Multiple)
        $deadline = (Get-Date).AddSeconds(60)
        while (-not (Test-Path -LiteralPath $stopFile) -and -not (Test-Path -LiteralPath $quitFile)) {
            if ((Get-Date) -gt $deadline) { break }
            [System.Windows.Forms.Application]::DoEvents()
            Start-Sleep -Milliseconds 30
        }
        try { $engine.RecognizeAsyncStop() } catch {}
        Start-Sleep -Milliseconds 350
        try { $engine.RecognizeAsyncCancel() } catch {}
        [System.IO.File]::WriteAllText($doneFile, '1', $utf8)
        Remove-Item -LiteralPath $goFile -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $stopFile -Force -ErrorAction SilentlyContinue
    }
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
    warm();
}

SpeechInput::~SpeechInput() {
    if (!m_ear.isEmpty()) {
        QFile quit(m_ear + QStringLiteral(".quit"));
        if (quit.open(QIODevice::WriteOnly))
            quit.close();
        QFile stop(m_ear + QStringLiteral(".stop"));
        if (stop.open(QIODevice::WriteOnly))
            stop.close();
    }
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
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
    warm();
    if (!m_proc || m_proc->state() == QProcess::NotRunning) {
        const QString err = m_bootError.isEmpty()
                                ? readUtf8(m_ear + QStringLiteral(".err"))
                                : m_bootError;
        m_bootError.clear();
        report(friendlySpeechError(err), false);
        return;
    }
    for (const QString &suffix :
         {QStringLiteral(".out"), QStringLiteral(".err"), QStringLiteral(".done"),
          QStringLiteral(".stop")})
        QFile::remove(m_ear + suffix);
    QFile go(m_ear + QStringLiteral(".go"));
    if (!go.open(QIODevice::WriteOnly)) {
        report(QStringLiteral("Spracherkennung ist nicht verfügbar. Bitte tippen."), false);
        return;
    }
    go.close();
    m_listening = true;
    emit listeningChanged(true);
    m_poll->start();
#else
    report(QStringLiteral("Spracherkennung ist hier nicht verfügbar. Bitte tippen."), false);
#endif
}

void SpeechInput::warm() {
#if defined(Q_OS_WIN)
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        return;
    const QString dir = tempDir();
    const QString scriptPath = dir + QStringLiteral("/listen.ps1");
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_bootError = QStringLiteral("Spracherkennung ist nicht verfügbar. Bitte tippen.");
        return;
    }
    script.write("\xEF\xBB\xBF");
    script.write(kListenScript);
    script.close();

    m_ear = dir + QStringLiteral("/ear-") + QUuid::createUuid().toString(QUuid::Id128);
    for (const QString &suffix :
         {QStringLiteral(".go"), QStringLiteral(".stop"), QStringLiteral(".out"),
          QStringLiteral(".err"), QStringLiteral(".done"), QStringLiteral(".quit")})
        QFile::remove(m_ear + suffix);

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
                          QStringLiteral("-File"), scriptPath, QStringLiteral("-Ear"), m_ear});
    connect(m_proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString heard = readUtf8(m_ear + QStringLiteral(".out")).simplified();
        const QString err = readUtf8(m_ear + QStringLiteral(".err"));
        if (m_reported)
            return;
        if (!m_listening) {
            if (code != 0)
                m_bootError = friendlySpeechError(err);
            return;
        }
        if (!heard.isEmpty())
            report(heard, true);
        else if (code != 0)
            report(friendlySpeechError(err), false);
        else
            report(QStringLiteral("Nichts erkannt."), false);
    });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_bootError = QStringLiteral("Spracherkennung ist nicht verfügbar. Bitte tippen.");
        if (m_listening && !m_reported)
            report(m_bootError, false);
    });
    m_proc->start();
#else
    return;
#endif
}

void SpeechInput::pollHeard() {
#if defined(Q_OS_WIN)
    if (m_reported || !m_listening || m_ear.isEmpty())
        return;
    const QString err = readUtf8(m_ear + QStringLiteral(".err")).trimmed();
    if (!err.isEmpty()) {
        stop();
        report(friendlySpeechError(err), false);
        return;
    }
    if (!QFile::exists(m_ear + QStringLiteral(".done")))
        return;
    const QString heard = readUtf8(m_ear + QStringLiteral(".out")).simplified();
    if (heard.isEmpty())
        report(QStringLiteral("Nichts erkannt."), false);
    else
        report(heard, true);
#else
    return;
#endif
}

void SpeechInput::stop() {
#if defined(Q_OS_ANDROID)
    return;
#else
    if (!m_listening || m_ear.isEmpty() || QFile::exists(m_ear + QStringLiteral(".stop")))
        return;
    QFile stop(m_ear + QStringLiteral(".stop"));
    if (stop.open(QIODevice::WriteOnly))
        stop.close();
#endif
}
