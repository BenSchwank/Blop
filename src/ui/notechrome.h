#pragma once

// Drawboard-inspired note-chrome palette.
// Separate from BlopTheme (library / app chrome) so the editor can use a
// Drawboard-like charcoal look, with an optional light editor variant.

#include <QColor>
#include <QSettings>
#include <QString>

namespace NoteChrome {

enum class Mode { Dark, Light };

inline Mode &modeRef() {
  static Mode m = Mode::Dark;
  static bool loaded = false;
  if (!loaded) {
    QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    const QString v =
        s.value(QStringLiteral("ui/editor_chrome_mode"), QStringLiteral("dark"))
            .toString();
    m = (v == QLatin1String("light")) ? Mode::Light : Mode::Dark;
    loaded = true;
  }
  return m;
}

inline Mode mode() { return modeRef(); }

inline void setMode(Mode m) {
  modeRef() = m;
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/editor_chrome_mode"),
             m == Mode::Light ? QStringLiteral("light")
                              : QStringLiteral("dark"));
}

inline void toggleMode() {
  setMode(mode() == Mode::Dark ? Mode::Light : Mode::Dark);
}

inline bool isDark() { return mode() == Mode::Dark; }

inline QColor canvasBg() {
  return isDark() ? QColor(42, 42, 42) : QColor(232, 232, 232);
}
inline QColor panelBg() {
  return isDark() ? QColor(30, 30, 30) : QColor(245, 245, 245);
}
inline QColor panelElevated() {
  return isDark() ? QColor(38, 38, 38) : QColor(255, 255, 255);
}
inline QColor border() {
  return isDark() ? QColor(64, 64, 64) : QColor(200, 200, 200);
}
inline QColor borderSoft() {
  return isDark() ? QColor(55, 55, 55) : QColor(210, 210, 210);
}
inline QColor textPrimary() {
  return isDark() ? QColor(232, 232, 232) : QColor(32, 32, 32);
}
inline QColor textSecondary() {
  return isDark() ? QColor(180, 180, 180) : QColor(96, 96, 96);
}
inline QColor accent() { return QColor(91, 157, 255); }
inline QColor accentSoft() { return QColor(91, 157, 255, 40); }
inline QColor toolbarFill() {
  return isDark() ? QColor(36, 36, 36) : QColor(250, 250, 250);
}
inline QColor toolbarFillEnd() {
  return isDark() ? QColor(28, 28, 28) : QColor(240, 240, 240);
}

} // namespace NoteChrome
