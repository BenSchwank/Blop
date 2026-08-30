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
  // Same charcoal as library sidebar — not pure black.
  return isDark() ? QColor(0x16, 0x18, 0x1E) : QColor(245, 245, 245);
}
inline QColor panelBg() {
  return isDark() ? QColor(0x1A, 0x1D, 0x24) : QColor(245, 245, 245);
}
inline QColor panelElevated() {
  return isDark() ? QColor(0x22, 0x26, 0x2F) : QColor(255, 255, 255);
}
inline QColor border() {
  return isDark() ? QColor(0x3A, 0x3E, 0x48) : QColor(200, 200, 200);
}
inline QColor borderSoft() {
  return isDark() ? QColor(0x2E, 0x32, 0x3A) : QColor(210, 210, 210);
}
inline QColor textPrimary() {
  return isDark() ? QColor(0xF2, 0xF2, 0xF2) : QColor(32, 32, 32);
}
inline QColor textSecondary() {
  return isDark() ? QColor(0xB8, 0xBC, 0xC4) : QColor(96, 96, 96);
}
inline QColor accent() { return QColor(91, 157, 255); }
inline QColor accentSoft() { return QColor(91, 157, 255, 40); }
inline QColor toolbarFill() {
  // Title bar / tool chrome = sidebar family (readable vs canvas).
  return isDark() ? QColor(0x1A, 0x1D, 0x24) : QColor(255, 255, 255);
}
inline QColor toolbarFillEnd() {
  return isDark() ? QColor(0x16, 0x18, 0x1E) : QColor(248, 248, 248);
}
/// Floating bottom notch: stronger edge so it never blends into the canvas.
inline QColor notchBorder() {
  return isDark() ? QColor(0x4A, 0x4E, 0x58) : QColor(160, 160, 160);
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
