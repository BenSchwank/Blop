#pragma once

// Drawboard-inspired note-chrome palette. Light/Dark always mirrors the app
// theme (BlopTheme) so an open note never disagrees with the library.

#include "blop_theme.h"

#include <QColor>
#include <QEvent>
#include <QPainter>
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

/// Tool-Popover-Tokens — abgeleitet vom Kuli-Menue (PenStudioPopover), das
/// die Referenz fuer Aufbau und Farben ist. Folgen dem Darstellungsmodus
/// (Hell / Dunkel / System -> BlopTheme::isDark()).
inline QColor studioFill() {
  return isDark() ? QColor(0x18, 0x1B, 0x22) : QColor(0xFF, 0xFF, 0xFF);
}
inline QColor studioInk() {
  return isDark() ? QColor(0xF4, 0xF6, 0xFA) : QColor(0x22, 0x22, 0x22);
}
inline QColor studioMuted() {
  return isDark() ? QColor(0xA7, 0xAD, 0xB6) : QColor(0x6B, 0x6B, 0x6B);
}
inline QColor studioEdge() {
  return isDark() ? QColor(255, 255, 255, 31) : QColor(0xE5, 0xE7, 0xEB);
}

/// Gemeinsames Stylesheet aller Werkzeug-Popover (Radierer, Formen, Messen):
/// Abschnitts-Labels, Karten-Buttons, Regler wie im Kuli-Menue.
inline QString studioPopoverStyle(const QString &objectName) {
  const bool dark = isDark();
  const QString bg = studioFill().name(QColor::HexRgb);
  const QString edge = dark ? QStringLiteral("rgba(255,255,255,0.12)")
                            : QStringLiteral("#E5E7EB");
  const QString ink = studioInk().name(QColor::HexRgb);
  const QString muted = studioMuted().name(QColor::HexRgb);
  const QString cardBg = dark ? QStringLiteral("rgba(255,255,255,0.06)")
                              : panelBg().name(QColor::HexRgb);
  const QString cardBd = dark ? QStringLiteral("rgba(255,255,255,0.12)")
                              : borderSoft().name(QColor::HexRgb);
  const QString indBd = dark ? QStringLiteral("rgba(255,255,255,0.35)")
                             : QStringLiteral("#94A3B8");
  QString qss = QStringLiteral(
      "QFrame#%1, QWidget#%1 {"
      "  background: %2; border: 1px solid %3; border-radius: 12px;"
      "}"
      "QLabel { color: %5; font-size: 11px; font-weight: 600;"
      "  background: transparent; }"
      "QLabel#StudioValue { color: %4; font-size: 12px; }"
      "QRadioButton, QCheckBox { color: %4; font-size: 12px; font-weight: 500;"
      "  background: transparent; spacing: 8px; }"
      "QRadioButton::indicator {"
      "  width: 14px; height: 14px; border-radius: 9px;"
      "  border: 2px solid %8; background: transparent;"
      "}"
      "QRadioButton::indicator:checked {"
      "  border: 2px solid #5B9DFF; background: #5B9DFF;"
      "}"
      "QCheckBox::indicator {"
      "  width: 14px; height: 14px; border-radius: 4px;"
      "  border: 2px solid %8; background: transparent;"
      "}"
      "QCheckBox::indicator:checked {"
      "  border: 2px solid #5B9DFF; background: #5B9DFF;"
      "}"
      "QPushButton {"
      "  background: %6; color: %4; border: 1px solid %7; border-radius: 8px;"
      "  padding: 6px 10px; font-size: 12px; font-weight: 600;"
      "}"
      "QPushButton:hover { background: rgba(91,157,255,0.14);"
      "  border-color: #5B9DFF; }"
      "QPushButton:checked { background: rgba(91,157,255,0.18);"
      "  border-color: #5B9DFF; color: %4; }"
      "QPushButton#ShapesCatTab {"
      "  background: transparent; border: none; border-radius: 0;"
      "  border-bottom: 2px solid transparent; color: %5;"
      "  padding: 6px 10px; font-size: 11px; font-weight: 600;"
      "}"
      "QPushButton#ShapesCatTab:hover {"
      "  background: transparent; border: none;"
      "  border-bottom: 2px solid transparent; color: %4;"
      "}"
      "QPushButton#ShapesCatTab:checked,"
      "QPushButton#ShapesCatTab:checked:hover {"
      "  background: transparent; border: none;"
      "  border-bottom: 2px solid #5B9DFF; color: %4;"
      "}"
      "QWidget#ShapesCatalogPage, QStackedWidget#ShapesCatalogStack {"
      "  background: transparent; }"
      "QSlider { background: transparent; min-height: 18px; }"
      "QSlider::groove:horizontal {"
      "  height: 4px; background: %7; border: none; border-radius: 2px;"
      "}"
      "QSlider::sub-page:horizontal { background: #5B9DFF; border-radius: 2px; }"
      "QSlider::add-page:horizontal { background: %7; border-radius: 2px; }"
      "QSlider::handle:horizontal {"
      "  width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;"
      "  background: #5B9DFF; border: none;"
      "}");
  return qss.arg(objectName, bg, edge, ink, muted, cardBg, cardBd, indBd);
}

/// Die Studio-Popover sind transluzente Top-Level-Fenster: der
/// Stylesheet-Hintergrund wird von QWidget::grab() nicht zuverlaessig
/// mitgerendert (Screenshot zeigt dann die Seite durch). Wie beim
/// PenStudioPopover malen wir Karte + Rand deshalb im Paint-Event
/// selbst; das Stylesheet bleibt fuer Texte/Regler zustaendig.
class StudioPopupPaintFilter : public QObject {
public:
  explicit StudioPopupPaintFilter(QObject *popup) : QObject(popup) {}

protected:
  bool eventFilter(QObject *obj, QEvent *e) override {
    if (e->type() == QEvent::Paint) {
      auto *w = qobject_cast<QWidget *>(obj);
      if (w) {
        QPainter p(w);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(w->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(Qt::NoPen);
        p.setBrush(studioFill());
        p.drawRoundedRect(r, 12, 12);
        p.setPen(QPen(studioEdge(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 12, 12);
      }
    }
    return QObject::eventFilter(obj, e);
  }
};

/// Popover-Fenster vorbereiten: transparente Ecken (saubere Rundung) und
/// Stylesheet-Hintergrund auch fuer reine QWidget-Popover.
inline void prepareStudioPopup(QWidget *pop, const QString &objectName) {
  if (!pop)
    return;
  pop->setObjectName(objectName);
  pop->setAttribute(Qt::WA_TranslucentBackground, true);
  pop->setAttribute(Qt::WA_StyledBackground, true);
  pop->setStyleSheet(studioPopoverStyle(objectName));
  pop->installEventFilter(new StudioPopupPaintFilter(pop));
}

/// „Gefaehrliche" Aktion im Popover (z. B. Seite leeren), theme-abhaengig.
inline QString studioDangerButtonStyle() {
  return isDark()
             ? QStringLiteral(
                   "QPushButton { background: rgba(248,113,113,0.16);"
                   " color: #FCA5A5; border: 1px solid rgba(248,113,113,0.35); }"
                   "QPushButton:hover { background: rgba(248,113,113,0.26);"
                   " border-color: #F87171; }")
             : QStringLiteral(
                   "QPushButton { background: rgba(220,38,38,0.07);"
                   " color: #DC2626; border: 1px solid rgba(220,38,38,0.35); }"
                   "QPushButton:hover { background: rgba(220,38,38,0.14);"
                   " border-color: #DC2626; }");
}

} // namespace NoteChrome
