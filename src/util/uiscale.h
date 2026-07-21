#ifndef UISCALE_H
#define UISCALE_H

class QWidget;

namespace UiScale {

int dp(int px);
int sp(int px);
int androidTopInsetPx(QWidget *reference = nullptr);
int androidBottomInsetPx(QWidget *reference = nullptr);
int safeTopPx(QWidget *reference = nullptr);
int safeBottomPx(QWidget *reference = nullptr);
int safeHorizontalPaddingPx(QWidget *reference = nullptr);

// Real device screen dimensions, usable even before the window has been shown.
// Falls back to the QWidget's own size on desktop / unknown screen.
int androidScreenWidthPx(QWidget *reference = nullptr);
int androidScreenHeightPx(QWidget *reference = nullptr);

// Like androidScreenWidthPx but reduced by a small safety margin (dp(12)) to
// account for system cutouts / curved-edge insets that Qt's availableGeometry
// does not always report on Android. Use this whenever you would otherwise
// place widgets all the way to the right edge.
int androidUsableViewportWidthPx(QWidget *reference = nullptr);

// Usable horizontal width = usable viewport minus 2 * safeHorizontalPaddingPx.
// Use this for any widget that should fit within the device viewport.
int androidContentWidthPx(QWidget *reference = nullptr);

// True if the current Android screen is at least 600 dp wide (material
// "tablet" threshold). Implementation uses the screen's logicalDotsPerInch
// to convert raw available pixels to DP, which is robust against Qt's
// HighDPI auto-scaling being on or off (raw screen->availableGeometry()
// values can otherwise leak through unscaled).
bool isAndroidTablet(QWidget *reference = nullptr);

/// Phone editor chrome (AndroidPhoneToolbar, no desktop bottom notch).
/// On device: true for non-tablet Android. On desktop: set
/// BLOP_SIMULATE_ANDROID_PHONE=1 to exercise the same layout paths.
bool isAndroidPhoneUi(QWidget *reference = nullptr);

} // namespace UiScale

#endif // UISCALE_H
