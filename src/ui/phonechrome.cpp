#include "phonechrome.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QtGlobal>
#include <QString>
#include <QWidget>

namespace PhoneChrome {

int primaryCtaDp() { return 48; }
int iconButtonDp() { return BlopStyle::touchTargetMinDp(); }
int rowMinDp() { return 48; }
int rightReserveDp() { return 12; }

int barContentHeightPx() { return UiScale::dp(56); }

int overlayHeightPx(QWidget *reference) {
  return barContentHeightPx() + UiScale::safeBottomPx(reference);
}

int contentBottomInsetPx(QWidget *reference) {
  return overlayHeightPx(reference) + UiScale::dp(8);
}

int sidePadPx(QWidget *reference) {
  return qMax(UiScale::safeHorizontalPaddingPx(reference), UiScale::dp(14));
}

int sheetMaxWidthPx(QWidget *reference) {
  const int side = sidePadPx(reference);
  const int reserve = UiScale::dp(rightReserveDp());
  QWidget *win = reference ? reference->window() : nullptr;
  const int winW = win ? win->width() : UiScale::dp(360);
  return qMin(UiScale::androidContentWidthPx(reference),
              qMax(UiScale::dp(280), winW - side - (side + reserve)));
}

QString bottomNavQss() {
  const bool dark = BlopTheme::instance().isDark();
  const QString bg = dark ? BlopStyle::obsidianNav().name(QColor::HexRgb)
                          : BlopStyle::paperBg().name(QColor::HexRgb);
  const QString hair = dark ? QStringLiteral("rgba(255,255,255,0.10)")
                            : QStringLiteral("rgba(20,24,40,0.10)");
  return QStringLiteral(
             "QWidget#PhoneShellNav {"
             "  background: %1;"
             "  border-top: 1px solid %2;"
             "}")
      .arg(bg, hair);
}

QString navItemQss(bool active) {
  const bool dark = BlopTheme::instance().isDark();
  const QString idle = dark ? QStringLiteral("#9AA3BB")
                            : BlopStyle::paperInkMuted().name(QColor::HexRgb);
  const QString on = QStringLiteral("#5B9DFF");
  const QString press = dark ? QStringLiteral("rgba(255,255,255,0.08)")
                             : QStringLiteral("rgba(20,24,40,0.06)");
  return QStringLiteral(
             "QToolButton {"
             "  background: transparent; border: none;"
             "  color: %1; font-size: 11px; font-weight: %2;"
             "  padding-top: 4px;"
             "}"
             "QToolButton:pressed { background: %3; border-radius: 10px; }")
      .arg(active ? on : idle)
      .arg(active ? 700 : 600)
      .arg(press);
}

} // namespace PhoneChrome
