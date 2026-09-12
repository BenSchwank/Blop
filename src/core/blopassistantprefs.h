#pragma once

#include <QKeySequence>
#include <QSettings>
#include <QString>
#include <QtGlobal>

// Persisted companion prefs. Independent of Blop's note-app settings.
namespace BlopAssistantPrefs {

inline QSettings store() {
  return QSettings(QStringLiteral("Blop"), QStringLiteral("BlopAssistantSandbox"));
}

inline bool isEscapeShortcut(const QKeySequence &seq) {
  if (seq.isEmpty() || seq.count() < 1)
    return false;
  const QString t = seq.toString(QKeySequence::PortableText);
  if (t.compare(QLatin1String("Esc"), Qt::CaseInsensitive) == 0 ||
      t.compare(QLatin1String("Escape"), Qt::CaseInsensitive) == 0)
    return true;
  return seq[0].key() == Qt::Key_Escape;
}

// Empty is allowed (hotkey off). Only Escape is rejected.
inline bool unusableShortcut(const QKeySequence &seq) {
  return isEscapeShortcut(seq);
}

inline QKeySequence readHotkey(const QString &key, const QString &factoryDefault) {
  QSettings s = store();
  if (!s.contains(key))
    return QKeySequence(factoryDefault);
  const QString v = s.value(key).toString().trimmed();
  if (v.isEmpty())
    return QKeySequence();
  const QKeySequence seq(v, QKeySequence::PortableText);
  if (seq.isEmpty() || isEscapeShortcut(seq))
    return QKeySequence();
  return seq;
}

inline QKeySequence openChatSequence() {
  return readHotkey(QStringLiteral("hotkeys/openChat"),
                    QStringLiteral("Ctrl+Shift+Space"));
}

inline void setOpenChatSequence(const QKeySequence &seq) {
  QSettings s = store();
  if (isEscapeShortcut(seq))
    s.setValue(QStringLiteral("hotkeys/openChat"), QString());
  else
    s.setValue(QStringLiteral("hotkeys/openChat"),
               seq.toString(QKeySequence::PortableText));
}

inline QKeySequence pushToTalkSequence() {
  return readHotkey(QStringLiteral("hotkeys/pushToTalk"),
                    QStringLiteral("Ctrl+Shift+T"));
}

inline void setPushToTalkSequence(const QKeySequence &seq) {
  QSettings s = store();
  if (isEscapeShortcut(seq))
    s.setValue(QStringLiteral("hotkeys/pushToTalk"), QString());
  else
    s.setValue(QStringLiteral("hotkeys/pushToTalk"),
               seq.toString(QKeySequence::PortableText));
}

inline bool speakReplies() {
  QSettings s = store();
  return s.value(QStringLiteral("voice/enabled"), true).toBool();
}

inline void setSpeakReplies(bool on) {
  QSettings s = store();
  s.setValue(QStringLiteral("voice/enabled"), on);
}

// SAPI-style rate: -10 .. 10. Default slower than stock so it sounds less rushed.
inline int speechRate() {
  QSettings s = store();
  return qBound(-8, s.value(QStringLiteral("voice/rate"), -4).toInt(), 4);
}

inline void setSpeechRate(int rate) {
  QSettings s = store();
  s.setValue(QStringLiteral("voice/rate"), qBound(-8, rate, 4));
}

// auto | de-female | de-male | en
inline QString voiceId() {
  QSettings s = store();
  return s.value(QStringLiteral("voice/id"), QStringLiteral("auto")).toString();
}

inline void setVoiceId(const QString &id) {
  QSettings s = store();
  s.setValue(QStringLiteral("voice/id"), id);
}

inline bool llmEnabled() {
  QSettings s = store();
  return s.value(QStringLiteral("llm/enabled"), true).toBool();
}

inline void setLlmEnabled(bool on) {
  QSettings s = store();
  s.setValue(QStringLiteral("llm/enabled"), on);
}

inline QString llmApiKey() {
  const QString env = qEnvironmentVariable("BLOP_ASSISTANT_API_KEY");
  if (!env.isEmpty())
    return env;
  const QString openai = qEnvironmentVariable("OPENAI_API_KEY");
  if (!openai.isEmpty())
    return openai;
  QSettings s = store();
  return s.value(QStringLiteral("llm/apiKey")).toString();
}

inline void setLlmApiKey(const QString &key) {
  QSettings s = store();
  s.setValue(QStringLiteral("llm/apiKey"), key);
}

inline QString llmBaseUrl() {
  QSettings s = store();
  return s.value(QStringLiteral("llm/baseUrl"),
                 QStringLiteral("https://api.openai.com/v1"))
      .toString()
      .trimmed();
}

inline void setLlmBaseUrl(const QString &url) {
  QSettings s = store();
  s.setValue(QStringLiteral("llm/baseUrl"), url.trimmed());
}

inline QString llmModel() {
  QSettings s = store();
  return s.value(QStringLiteral("llm/model"), QStringLiteral("gpt-4o-mini"))
      .toString()
      .trimmed();
}

inline void setLlmModel(const QString &model) {
  QSettings s = store();
  s.setValue(QStringLiteral("llm/model"), model.trimmed());
}

inline bool llmReady() { return llmEnabled() && !llmApiKey().isEmpty(); }

inline QString sttModel() {
  const QString base = llmBaseUrl().toLower();
  if (base.contains(QLatin1String("groq")))
    return QStringLiteral("whisper-large-v3");
  return QStringLiteral("whisper-1");
}

} // namespace BlopAssistantPrefs
