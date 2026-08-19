#include "blopassistantvoice.h"
#include "blopassistantprefs.h"

#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>
#include <QtGlobal>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
namespace {

bool haveBin(const QString &name) {
  return !QStandardPaths::findExecutable(name).isEmpty();
}

QString espeakVoice() {
  const QString id = BlopAssistantPrefs::voiceId();
  if (id == QLatin1String("de-male"))
    return QStringLiteral("de");
  if (id == QLatin1String("en"))
    return QStringLiteral("en+f3");
  return QStringLiteral("de+f3");
}

int espeakSpeed() {
  // Map SAPI-ish -8..4 onto a speaking rate that is not frantic.
  return qBound(105, 138 + BlopAssistantPrefs::speechRate() * 6, 165);
}

} // namespace
#endif

void BlopAssistantVoice::speak(const QString &text) {
  if (!BlopAssistantPrefs::speakReplies())
    return;
  const QString t = text.trimmed();
  if (t.isEmpty())
    return;

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
  const int rate = BlopAssistantPrefs::speechRate();
  if (haveBin(QStringLiteral("pico2wave")) &&
      (haveBin(QStringLiteral("aplay")) || haveBin(QStringLiteral("paplay")))) {
    QTemporaryFile wav(QStringLiteral("/tmp/blop-voice-XXXXXX.wav"));
    wav.setAutoRemove(false);
    if (wav.open()) {
      const QString path = wav.fileName();
      wav.close();
      if (QProcess::execute(QStringLiteral("pico2wave"),
                            {QStringLiteral("-l"), QStringLiteral("de-DE"),
                             QStringLiteral("-w"), path, t}) == 0) {
        if (haveBin(QStringLiteral("paplay")))
          QProcess::startDetached(QStringLiteral("paplay"), {path});
        else
          QProcess::startDetached(QStringLiteral("aplay"),
                                  {QStringLiteral("-q"), path});
        return;
      }
    }
  }
  if (haveBin(QStringLiteral("spd-say"))) {
    // Negative rate = slower, less "announcer".
    const QString spdRate = QString::number(-25 + rate * 4);
    if (QProcess::startDetached(
            QStringLiteral("spd-say"),
            {QStringLiteral("-l"), QStringLiteral("de"), QStringLiteral("-r"),
             spdRate, QStringLiteral("-i"), QStringLiteral("-15"),
             QStringLiteral("-t"), QStringLiteral("female1"), t}))
      return;
  }
  const QString voice = espeakVoice();
  const QString speed = QString::number(espeakSpeed());
  if (haveBin(QStringLiteral("espeak-ng"))) {
    QProcess::startDetached(QStringLiteral("espeak-ng"),
                            {QStringLiteral("-v"), voice, QStringLiteral("-s"),
                             speed, QStringLiteral("-p"), QStringLiteral("38"),
                             QStringLiteral("-g"), QStringLiteral("5"),
                             QStringLiteral("-a"), QStringLiteral("70"), t});
    return;
  }
  QProcess::startDetached(QStringLiteral("espeak"),
                          {QStringLiteral("-v"), voice, QStringLiteral("-s"),
                           speed, QStringLiteral("-p"), QStringLiteral("38"),
                           QStringLiteral("-g"), QStringLiteral("5"), t});
#elif defined(Q_OS_WIN)
  const QByteArray b64 = t.toUtf8().toBase64();
  const int rate = BlopAssistantPrefs::speechRate();
  const QString voiceId = BlopAssistantPrefs::voiceId();
  QString prefer;
  if (voiceId == QLatin1String("de-male"))
    prefer = QStringLiteral("male-de");
  else if (voiceId == QLatin1String("en"))
    prefer = QStringLiteral("en");
  else
    prefer = QStringLiteral("female-de");

  const QString cmd = QStringLiteral(
      "$bytes = [Convert]::FromBase64String('%1'); "
      "$t = [Text.Encoding]::UTF8.GetString($bytes); "
      "Add-Type -AssemblyName System.Speech; "
      "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
      "$s.Rate = %2; $s.Volume = 82; "
      "$prefer = '%3'; "
      "$picked = $false; "
      "foreach ($v in $s.GetInstalledVoices()) { "
      "  $i = $v.VoiceInfo; $n = $i.Name; $c = $i.Culture.Name; "
      "  $de = $c -like 'de*'; $en = $c -like 'en*'; "
      "  $f = [string]$i.Gender -eq 'Female'; "
      "  if ($prefer -eq 'female-de' -and $de -and $f) { $s.SelectVoice($n); $picked = $true; break } "
      "  if ($prefer -eq 'male-de' -and $de -and -not $f) { $s.SelectVoice($n); $picked = $true; break } "
      "  if ($prefer -eq 'en' -and $en) { $s.SelectVoice($n); $picked = $true; break } "
      "} "
      "if (-not $picked) { "
      "  foreach ($v in $s.GetInstalledVoices()) { "
      "    if ($v.VoiceInfo.Culture.Name -like 'de*') { $s.SelectVoice($v.VoiceInfo.Name); break } "
      "  } "
      "} "
      "$s.Speak($t);");
  QProcess::startDetached(
      QStringLiteral("powershell"),
      {QStringLiteral("-NoProfile"), QStringLiteral("-WindowStyle"),
       QStringLiteral("Hidden"), QStringLiteral("-Command"),
       cmd.arg(QString::fromLatin1(b64), QString::number(rate), prefer)});
#else
  Q_UNUSED(t);
#endif
}
