#pragma once

class QString;
class QWidget;

/// Single source for Android-phone shell metrics (bottom nav, sheets, CTAs).
/// Library/editor screens should read these instead of inventing padding.
namespace PhoneChrome {

int primaryCtaDp();
int iconButtonDp();
int rowMinDp();
int rightReserveDp();

/// Inner bar height (icons + labels), without system inset.
int barContentHeightPx();

/// Full overlay height: bar + safe-bottom. Content must clear this.
int overlayHeightPx(QWidget *reference);

/// Bottom padding for scrollable library/dashboard content.
int contentBottomInsetPx(QWidget *reference);

int sidePadPx(QWidget *reference);
int sheetMaxWidthPx(QWidget *reference);

QString bottomNavQss();
QString navItemQss(bool active);

} // namespace PhoneChrome
