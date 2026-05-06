#include "uiscale.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
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
  // On some Android devices the top inset can exceed older hardcoded values.
  // Keep it bounded only against unrealistic transition spikes.
  inset = qBound(0, inset, dp(72));
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

int safeTopPx(QWidget *reference) {
#if defined(Q_OS_ANDROID)
  return androidTopInsetPx(reference);
#else
  Q_UNUSED(reference);
  return 0;
#endif
}

int safeBottomPx(QWidget *reference) {
#if defined(Q_OS_ANDROID)
  return androidBottomInsetPx(reference);
#else
  Q_UNUSED(reference);
  return 0;
#endif
}

int safeHorizontalPaddingPx(QWidget *reference) {
  QScreen *screen = bestScreen(reference);
  if (!screen)
    return dp(10);
  const int minSide = qMin(screen->availableGeometry().width(), screen->availableGeometry().height());
  if (minSide >= dp(900))
    return dp(24);
  if (minSide >= dp(600))
    return dp(18);
  return dp(10);
}

} // namespace UiScale
