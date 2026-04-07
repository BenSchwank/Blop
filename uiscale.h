#ifndef UISCALE_H
#define UISCALE_H

class QWidget;

namespace UiScale {

int dp(int px);
int sp(int px);
int androidTopInsetPx(QWidget *reference = nullptr);
int androidBottomInsetPx(QWidget *reference = nullptr);

} // namespace UiScale

#endif // UISCALE_H
