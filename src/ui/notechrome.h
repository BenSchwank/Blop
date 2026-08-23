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
  // Studio K/J mix: editor chrome defaults to light (white pill, white
  // tool sheet, light page surround). Dark Drawboard chrome remains
  // available via ui/editor_chrome_mode=dark.
  static Mode m = Mode::Light;
  static bool loaded = false;
  if (!loaded) {
    QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    const QString v =
        s.value(QStringLiteral("ui/editor_chrome_mode"), QStringLiteral("light"))
            .toString();
    m = (v == QLatin1String("dark")) ? Mode::Dark : Mode::Light;
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
  // True black around the page — avoid bluish library surfaces (#0B0B1A).
  return isDark() ? QColor(0, 0, 0) : QColor(245, 245, 245);
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
  return isDark() ? QColor(36, 36, 36) : QColor(255, 255, 255);
}
inline QColor toolbarFillEnd() {
  return isDark() ? QColor(28, 28, 28) : QColor(248, 248, 248);
}
/// Floating bottom notch: stronger edge so it never blends into the canvas.
inline QColor notchBorder() {
  return isDark() ? QColor(70, 70, 70) : QColor(160, 160, 160);
}

inline QString rgbaCss(const QColor &c, int alpha = -1) {
  const int a = (alpha >= 0) ? alpha : c.alpha();
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(a);
}

inline QString overlayCardStyle(const QString &objectName) {
  return QStringLiteral(
             "#%1 {"
             "  background-color: %2;"
             "  border: 1px solid %3;"
             "  border-radius: 18px;"
             "}")
      .arg(objectName, panelElevated().name(QColor::HexRgb),
           border().name(QColor::HexRgb));
}

} // namespace NoteChrome
