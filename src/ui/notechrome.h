#pragma once

// Drawboard-inspired note-chrome palette. Light/Dark always mirrors the app
// theme (BlopTheme) so an open note never disagrees with the library.

#include "blop_theme.h"

#include <QColor>
#include <QString>

namespace NoteChrome {

enum class Mode { Dark, Light };

inline Mode mode() {
  return BlopTheme::instance().isDark() ? Mode::Dark : Mode::Light;
}

/// User-initiated switch (Seite & Notiz, rail toggle): changes the app theme.
inline void setMode(Mode m) {
  BlopTheme::instance().setMode(m == Mode::Dark ? BlopTheme::Mode::Dark
                                                : BlopTheme::Mode::Light);
}

inline void toggleMode() {
  setMode(mode() == Mode::Dark ? Mode::Light : Mode::Dark);
}

inline bool isDark() { return mode() == Mode::Dark; }

inline QColor canvasBg() {
  // Same black rail as the light Hauptmenü sidebar.
  return isDark() ? QColor(0x1A, 0x19, 0x16) : QColor(245, 245, 245);
}
inline QColor panelBg() {
  return isDark() ? QColor(0x23, 0x25, 0x2A) : QColor(245, 245, 245);
}
inline QColor panelElevated() {
  return isDark() ? QColor(0x35, 0x38, 0x40) : QColor(255, 255, 255);
}
inline QColor border() {
  return isDark() ? QColor(0x52, 0x56, 0x5E) : QColor(200, 200, 200);
}
inline QColor borderSoft() {
  return isDark() ? QColor(0x3C, 0x3F, 0x46) : QColor(210, 210, 210);
}
inline QColor textPrimary() {
  return isDark() ? QColor(0xF4, 0xF5, 0xF7) : QColor(32, 32, 32);
}
inline QColor textSecondary() {
  return isDark() ? QColor(0xA7, 0xAD, 0xB6) : QColor(96, 96, 96);
}
inline QColor accent() { return QColor(91, 157, 255); }
inline QColor accentSoft() { return QColor(91, 157, 255, 40); }
inline QColor toolbarFill() {
  // Title bar / tool chrome sits on the content step, above the sidebar.
  return isDark() ? QColor(0x23, 0x25, 0x2A) : QColor(255, 255, 255);
}
inline QColor toolbarFillEnd() {
  return isDark() ? QColor(0x1A, 0x19, 0x16) : QColor(248, 248, 248);
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
