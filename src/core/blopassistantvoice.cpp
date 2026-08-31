#include "blopassistantvoice.h"
#include "blopassistantprefs.h"

#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>
#include <QtGlobal>

#ifdef BLOP_HAS_TEXTTOSPEECH
#include <QLocale>
#include <QTextToSpeech>
#include <QVoice>
#endif

namespace {

#ifdef BLOP_HAS_TEXTTOSPEECH
class VoiceHost : public QObject {
public:
  static VoiceHost &instance() {
    static VoiceHost host;
    return host;
  }

  void speak(const QString &text) {
    ensure();
    if (!m_tts)
      return;
    pickVoice();
    const int rate = BlopAssistantPrefs::speechRate();
    m_tts->setRate(qBound(-1.0, rate / 8.0, 0.35));
    m_tts->setPitch(-0.12);
    m_tts->setVolume(0.82);
    m_tts->stop();
    m_tts->say(text);
  }

  bool ready() {
    ensure();
    return m_tts && m_tts->state() != QTextToSpeech::Error;
  }

private:
  VoiceHost() : QObject(nullptr) {}

  void ensure() {
    if (m_tts)
      return;
    QString engine;
#ifdef Q_OS_WIN
    const QStringList engines = QTextToSpeech::availableEngines();
    if (engines.contains(QLatin1String("winrt")))
      engine = QStringLiteral("winrt");
    else if (engines.contains(QLatin1String("sapi")))
      engine = QStringLiteral("sapi");
#endif
    if (engine.isEmpty())
      m_tts = new QTextToSpeech(this);
    else
      m_tts = new QTextToSpeech(engine, this);
  }

  static int scoreVoice(const QVoice &v, const QString &prefer) {
    const QString name = v.name();
    const QString loc = v.locale().name();
    const bool de = loc.startsWith(QLatin1String("de"), Qt::CaseInsensitive);
    const bool en = loc.startsWith(QLatin1String("en"), Qt::CaseInsensitive);
    const bool female = v.gender() == QVoice::Female;
    int s = 0;
    if (name.contains(QLatin1String("Natural"), Qt::CaseInsensitive))
      s += 80;
    if (name.contains(QLatin1String("Neural"), Qt::CaseInsensitive))
      s += 70;
    if (name.contains(QLatin1String("Online"), Qt::CaseInsensitive))
      s += 40;
    if (name.contains(QLatin1String("OneCore"), Qt::CaseInsensitive))
      s += 20;
    if (prefer == QLatin1String("en")) {
      if (en)
        s += 50;
    } else {
      if (de)
        s += 50;
      if (prefer == QLatin1String("de-male"))
        s += female ? 0 : 25;
      else
        s += female ? 25 : 0;
      if (name.contains(QLatin1String("Hedda"), Qt::CaseInsensitive) ||
          name.contains(QLatin1String("Katja"), Qt::CaseInsensitive) ||
          name.contains(QLatin1String("Ingrid"), Qt::CaseInsensitive))
        s += 30;
      if (name.contains(QLatin1String("Stefan"), Qt::CaseInsensitive) ||
          name.contains(QLatin1String("Conrad"), Qt::CaseInsensitive))
        s += (prefer == QLatin1String("de-male")) ? 30 : 0;
    }
    return s;
  }

  void pickVoice() {
    if (!m_tts)
      return;
    const QString id = BlopAssistantPrefs::voiceId();
    QString prefer = QStringLiteral("de-female");
    if (id == QLatin1String("de-male"))
      prefer = QStringLiteral("de-male");
    else if (id == QLatin1String("en"))
      prefer = QStringLiteral("en");
    int best = -1;
    QVoice chosen;
    const QList<QVoice> voices = m_tts->availableVoices();
    for (const QVoice &v : voices) {
      const int s = scoreVoice(v, prefer);
      if (s > best) {
        best = s;
        chosen = v;
      }
    }
    if (best >= 0)
      m_tts->setVoice(chosen);
    if (prefer == QLatin1String("en"))
      m_tts->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    else
      m_tts->setLocale(QLocale(QLocale::German, QLocale::Germany));
  }

  QTextToSpeech *m_tts{nullptr};
};
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
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
  return qBound(88, 118 + BlopAssistantPrefs::speechRate() * 5, 150);
}
#endif

#ifdef Q_OS_WIN
bool speakWindows(const QString &text) {
  const QByteArray b64 = text.toUtf8().toBase64();
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
      "$prefer = '%3'; "
      "$rate = [int]%2; "
      "$spoken = $false; "
      "function Score([string]$n, [string]$c) { "
      "  $s = 0; $nl = $n.ToLower(); $cl = $c.ToLower(); "
      "  if ($nl -match 'natural') { $s += 80 }; "
      "  if ($nl -match 'neural') { $s += 70 }; "
      "  if ($nl -match 'online') { $s += 40 }; "
      "  if ($prefer -eq 'en') { if ($cl -like 'en*') { $s += 50 } } "
      "  else { if ($cl -like 'de*') { $s += 50 } }; "
      "  if ($prefer -eq 'female-de' -and ($nl -match 'hedda|katja|ingrid|zira')) { $s += 35 }; "
      "  if ($prefer -eq 'male-de' -and ($nl -match 'stefan|conrad|david')) { $s += 35 }; "
      "  return $s "
      "} "
      "try { "
      "  $voice = New-Object -ComObject SAPI.SpVoice; "
      "  $cat = New-Object -ComObject SAPI.SpObjectTokenCategory; "
      "  $cat.SetId('HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices', $false); "
      "  $best = $null; $bestS = -1; "
      "  foreach ($tok in $cat.EnumerateTokens()) { "
      "    $n = $tok.GetDescription(); "
      "    $sc = Score $n $n; "
      "    if ($sc -gt $bestS) { $bestS = $sc; $best = $tok } "
      "  } "
      "  if ($best -ne $null) { $voice.Voice = $best }; "
      "  $voice.Rate = $rate; $voice.Volume = 82; "
      "  $voice.Speak($t) | Out-Null; "
      "  $spoken = $true; "
      "} catch {} "
      "if (-not $spoken) { "
      "  Add-Type -AssemblyName System.Speech; "
      "  $s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
      "  $s.Rate = $rate; $s.Volume = 82; "
      "  $bestN = $null; $bestS = -1; "
      "  foreach ($v in $s.GetInstalledVoices()) { "
      "    $i = $v.VoiceInfo; $sc = Score $i.Name $i.Culture.Name; "
      "    if ($prefer -eq 'female-de' -and $i.Culture.Name -like 'de*' -and "
      "        [string]$i.Gender -eq 'Female') { $sc += 20 }; "
      "    if ($prefer -eq 'male-de' -and $i.Culture.Name -like 'de*' -and "
      "        [string]$i.Gender -ne 'Female') { $sc += 20 }; "
      "    if ($sc -gt $bestS) { $bestS = $sc; $bestN = $i.Name } "
      "  } "
      "  if ($bestN) { $s.SelectVoice($bestN) }; "
      "  $esc = [System.Security.SecurityElement]::Escape($t); "
      "  $lang = 'de-DE'; if ($prefer -eq 'en') { $lang = 'en-US' }; "
      "  $pct = [int]((-12) + $rate * 2); "
      "  $ssml = \"<speak version='1.0' xml:lang='$lang'><prosody rate='${pct}%' pitch='-1st'>$esc</prosody></speak>\"; "
      "  try { $s.SpeakSsml($ssml) } catch { $s.Speak($t) } "
      "}");
  return QProcess::startDetached(
      QStringLiteral("powershell"),
      {QStringLiteral("-NoProfile"), QStringLiteral("-WindowStyle"),
       QStringLiteral("Hidden"), QStringLiteral("-Command"),
       cmd.arg(QString::fromLatin1(b64), QString::number(rate), prefer)});
}
#endif

} // namespace

void BlopAssistantVoice::speak(const QString &text) {
  if (!BlopAssistantPrefs::speakReplies())
    return;
  const QString t = text.trimmed();
  if (t.isEmpty())
    return;

#ifdef BLOP_HAS_TEXTTOSPEECH
  VoiceHost::instance().speak(t);
  return;
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
  const int rate = BlopAssistantPrefs::speechRate();
  if (haveBin(QStringLiteral("pico2wave")) &&
      (haveBin(QStringLiteral("sox")) || haveBin(QStringLiteral("paplay")) ||
       haveBin(QStringLiteral("aplay")))) {
    QTemporaryFile wav(QStringLiteral("/tmp/blop-voice-XXXXXX.wav"));
    wav.setAutoRemove(false);
    if (wav.open()) {
      const QString path = wav.fileName();
      wav.close();
      if (QProcess::execute(QStringLiteral("pico2wave"),
                            {QStringLiteral("-l"), QStringLiteral("de-DE"),
                             QStringLiteral("-w"), path, t}) == 0) {
        if (haveBin(QStringLiteral("sox"))) {
          QProcess::startDetached(QStringLiteral("sox"),
                                  {path, QStringLiteral("-d"),
                                   QStringLiteral("tempo"),
                                   QStringLiteral("0.86")});
          return;
        }
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
    const QString spdRate = QString::number(-40 + rate * 4);
    if (QProcess::startDetached(
            QStringLiteral("spd-say"),
            {QStringLiteral("-l"), QStringLiteral("de"), QStringLiteral("-r"),
             spdRate, QStringLiteral("-p"), QStringLiteral("-20"),
             QStringLiteral("-i"), QStringLiteral("-20"),
             QStringLiteral("-t"), QStringLiteral("female1"), t}))
      return;
  }
  const QString voice = espeakVoice();
  const QString speed = QString::number(espeakSpeed());
  if (haveBin(QStringLiteral("espeak-ng"))) {
    QProcess::startDetached(QStringLiteral("espeak-ng"),
                            {QStringLiteral("-v"), voice, QStringLiteral("-s"),
                             speed, QStringLiteral("-p"), QStringLiteral("32"),
                             QStringLiteral("-g"), QStringLiteral("8"),
                             QStringLiteral("-a"), QStringLiteral("62"), t});
    return;
  }
  QProcess::startDetached(QStringLiteral("espeak"),
                          {QStringLiteral("-v"), voice, QStringLiteral("-s"),
                           speed, QStringLiteral("-p"), QStringLiteral("32"),
                           QStringLiteral("-g"), QStringLiteral("8"), t});
#elif defined(Q_OS_WIN)
  speakWindows(t);
#else
  Q_UNUSED(t);
#endif
}
