#pragma once

// Drawboard-inspired note-chrome palette. Light/Dark always mirrors the app
// theme (BlopTheme) so an open note never disagrees with the library.

#include "blop_theme.h"

#include <QColor>
#include <QPoint>
#include <QString>
#include <QWidget>

#include <algorithm>

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
  // Studio Canvas dark well; Light slate keeps A4 contrast.
  return isDark() ? QColor(0x26, 0x29, 0x30) : QColor(0xE2, 0xE8, 0xF0);
}
/// Hairline between the L-frame and the note well.
inline QColor frameEdge() { return QColor(0x4A, 0x4E, 0x58); }
inline QColor panelBg() {
  // Light slate family — aligned with canvasBg / rail / Zoom HUD.
  return isDark() ? QColor(0x23, 0x25, 0x2A) : QColor(0xF8, 0xFA, 0xFC);
}
inline QColor panelElevated() {
  return isDark() ? QColor(0x35, 0x38, 0x40) : QColor(255, 255, 255);
}
inline QColor border() {
  return isDark() ? QColor(0x52, 0x56, 0x5E) : QColor(0xCB, 0xD5, 0xE1);
}
inline QColor borderSoft() {
  return isDark() ? QColor(0x3C, 0x3F, 0x46) : QColor(0xE2, 0xE8, 0xF0);
}
inline QColor textPrimary() {
  return isDark() ? QColor(0xF4, 0xF5, 0xF7) : QColor(0x0F, 0x17, 0x2A);
}
inline QColor textSecondary() {
  return isDark() ? QColor(0xA7, 0xAD, 0xB6) : QColor(0x47, 0x55, 0x69);
}
inline QColor accent() { return QColor(91, 157, 255); }
inline QColor accentSoft() { return QColor(91, 157, 255, 40); }
inline QColor toolbarFill() {
  // Title bar / tool chrome sits on the content step, above the sidebar.
  return isDark() ? QColor(0x23, 0x25, 0x2A) : QColor(255, 255, 255);
}
inline QColor toolbarFillEnd() {
  return isDark() ? QColor(0x1A, 0x19, 0x16) : QColor(0xF8, 0xFA, 0xFC);
}
/// Floating bottom notch: stronger edge so it never blends into the canvas.
inline QColor notchBorder() {
  return isDark() ? QColor(0x4A, 0x4E, 0x58) : QColor(0xCB, 0xD5, 0xE1);
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

/// Keep a child widget fully inside its parent (local coords). Does not
/// resize — callers that need content-sized popups should adjustSize first.
inline void clampWidgetToParent(QWidget *w, QWidget *parent, int margin = 12) {
  if (!w || !parent)
    return;
  const int pw = parent->width();
  const int ph = parent->height();
  const int ww = w->width();
  const int wh = w->height();
  const int nx =
      std::clamp(w->x(), margin, std::max(margin, pw - ww - margin));
  const int ny =
      std::clamp(w->y(), margin, std::max(margin, ph - wh - margin));
  if (nx != w->x() || ny != w->y())
    w->move(nx, ny);
}

/// Place a Qt::Popup under a global anchor, then clamp into host client area.
inline void clampPopupToHost(QWidget *popup, const QPoint &globalPos,
                             QWidget *host, int margin = 12) {
  if (!popup)
    return;
  popup->adjustSize();
  QWidget *clampHost = host ? host : popup->parentWidget();
  if (!clampHost)
    clampHost = popup->window();
  if (!clampHost) {
    popup->move(globalPos);
    return;
  }
  const QPoint topLeft = clampHost->mapToGlobal(QPoint(0, 0));
  const int pw = clampHost->width();
  const int ph = clampHost->height();
  const int ww = popup->width();
  const int wh = popup->height();
  int nx = globalPos.x();
  int ny = globalPos.y();
  nx = std::clamp(nx, topLeft.x() + margin,
                  topLeft.x() + std::max(margin, pw - ww - margin));
  ny = std::clamp(ny, topLeft.y() + margin,
                  topLeft.y() + std::max(margin, ph - wh - margin));
  popup->move(nx, ny);
}

/// Shared Studio Dock second-tap popover chrome (Drawboard-dark).
inline QString studioPopoverStyle(const QString &objectName) {
  return QStringLiteral(
             "QFrame#%1, QWidget#%1 {"
             "  background: #181B22;"
             "  border: 1px solid rgba(255,255,255,0.12);"
             "  border-radius: 12px;"
             "}"
             "QLabel, QRadioButton, QCheckBox { color: #E2E8F0; font-size: 12px; }"
             "QRadioButton::indicator {"
             "  width: 14px; height: 14px; border-radius: 7px;"
             "  border: 2px solid rgba(255,255,255,0.35);"
             "  background: transparent;"
             "}"
             "QRadioButton::indicator:checked {"
             "  border: 2px solid #5B9DFF;"
             "  background: #5B9DFF;"
             "}"
             "QTabWidget::pane { border: none; background: transparent; }"
             "QTabBar::tab {"
             "  background: transparent; color: #94A3B8; padding: 6px 10px;"
             "  border: none; font-size: 11px; font-weight: 600;"
             "}"
             "QTabBar::tab:selected { color: #F8FAFC;"
             "  border-bottom: 2px solid #5B9DFF; }"
             "QPushButton {"
             "  background: rgba(255,255,255,0.06); color: #E2E8F0;"
             "  border: 1px solid rgba(255,255,255,0.10); border-radius: 8px;"
             "  padding: 6px 10px; font-size: 12px; font-weight: 600;"
             "}"
             "QPushButton:hover { background: rgba(91,157,255,0.16);"
             "  border-color: #5B9DFF; }"
             "QPushButton:checked { background: rgba(91,157,255,0.22);"
             "  border-color: #5B9DFF; color: #F8FAFC; }"
             "QPushButton#ShapesCatTab {"
             "  background: transparent; border: none; border-radius: 0;"
             "  border-bottom: 2px solid transparent; color: #94A3B8;"
             "  padding: 6px 10px; font-size: 11px; font-weight: 600;"
             "}"
             "QPushButton#ShapesCatTab:hover {"
             "  background: transparent; border: none;"
             "  border-bottom: 2px solid transparent; color: #E2E8F0;"
             "}"
             "QPushButton#ShapesCatTab:checked,"
             "QPushButton#ShapesCatTab:checked:hover {"
             "  background: transparent; border: none;"
             "  border-bottom: 2px solid #5B9DFF; color: #F8FAFC;"
             "}"
             "QWidget#ShapesCatalogPage { background: #181B22; }"
             "QSlider::groove:horizontal {"
             "  height: 4px; background: rgba(255,255,255,0.12); border-radius: 2px;"
             "}"
             "QSlider::handle:horizontal {"
             "  width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;"
             "  background: #5B9DFF;"
             "}")
      .arg(objectName);
}

} // namespace NoteChrome
