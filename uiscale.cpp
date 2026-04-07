#include "uiscale.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>
#include <QtGlobal>
#include <QtMath>

namespace {

QScreen *bestScreen(QWidget *reference) {
  if (reference && reference->windowHandle() && reference->windowHandle()->screen()) {
    return reference->windowHandle()->screen();
  }
  return QGuiApplication::primaryScreen();
}

qreal densityFor(QWidget *reference) {
  QScreen *screen = bestScreen(reference);
  if (!screen) {
    return 1.0;
  }
  const qreal raw = screen->logicalDotsPerInch() / 160.0;
  return qBound<qreal>(1.0, raw, 2.2);
}

} // namespace

namespace UiScale {

int dp(int px) {
  return qMax(1, qRound(px * densityFor(nullptr)));
}

int sp(int px) {
  return qMax(1, qRound(px * densityFor(nullptr)));
}

int androidTopInsetPx(QWidget *reference) {
  QScreen *screen = bestScreen(reference);
  int inset = 0;
  if (screen) {
    const QRect full = screen->geometry();
    const QRect avail = screen->availableGeometry();
    inset = qMax(0, avail.top() - full.top());
  }
#if defined(Q_OS_ANDROID)
  // QWindow::safeAreaMargins() is not available on all Qt-for-Android targets (e.g. CI Qt 6.6);
  // rely on screen geometry plus a minimum inset below.
  // Edge-to-edge / fullscreen: geometry can still be 0 while status icons draw
  // over the first row — keep a minimum strip so tabs clear the status bar.
  if (inset < dp(24))
    inset = qMax(inset, dp(24));
#endif
  return inset;
}

int androidBottomInsetPx(QWidget *reference) {
  QScreen *screen = bestScreen(reference);
  if (!screen) {
    return 0;
  }
  const QRect full = screen->geometry();
  const QRect avail = screen->availableGeometry();
  return qMax(0, full.bottom() - avail.bottom());
}

} // namespace UiScale
