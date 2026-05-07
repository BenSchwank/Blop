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

// Usable horizontal width = screen width minus 2 * safeHorizontalPaddingPx.
// Use this for any widget that should fit within the device viewport.
int androidContentWidthPx(QWidget *reference = nullptr);

} // namespace UiScale

#endif // UISCALE_H
