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

inline bool unusableShortcut(const QKeySequence &seq) {
  if (seq.isEmpty() || seq.count() < 1)
    return true;
  const QString t = seq.toString(QKeySequence::PortableText);
  if (t.compare(QLatin1String("Esc"), Qt::CaseInsensitive) == 0 ||
      t.compare(QLatin1String("Escape"), Qt::CaseInsensitive) == 0)
    return true;
  if (seq[0].key() == Qt::Key_Escape)
    return true;
  return false;
}

inline QKeySequence openChatSequence() {
  QSettings s = store();
  const QString v = s.value(QStringLiteral("hotkeys/openChat"),
                            QStringLiteral("Ctrl+Shift+Space"))
                        .toString();
  const QKeySequence seq(v);
  if (unusableShortcut(seq))
    return QKeySequence(QStringLiteral("Ctrl+Shift+Space"));
  return seq;
}

inline void setOpenChatSequence(const QKeySequence &seq) {
  QSettings s = store();
  s.setValue(QStringLiteral("hotkeys/openChat"), seq.toString());
}

inline QKeySequence pushToTalkSequence() {
  QSettings s = store();
  const QString v = s.value(QStringLiteral("hotkeys/pushToTalk"),
                            QStringLiteral("Ctrl+Shift+T"))
                        .toString();
  const QKeySequence seq(v);
  if (unusableShortcut(seq))
    return QKeySequence(QStringLiteral("Ctrl+Shift+T"));
  return seq;
}

inline void setPushToTalkSequence(const QKeySequence &seq) {
  QSettings s = store();
  s.setValue(QStringLiteral("hotkeys/pushToTalk"), seq.toString());
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
  return qBound(-8, s.value(QStringLiteral("voice/rate"), -2).toInt(), 4);
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

} // namespace BlopAssistantPrefs
