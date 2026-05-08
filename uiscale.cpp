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

int androidScreenWidthPx(QWidget *reference) {
  QScreen *screen = bestScreen(reference);
  if (screen) {
    const int w = screen->availableGeometry().width();
    if (w > 0)
      return w;
  }
  // Fallback: widget geometry if available, otherwise a sensible default that
  // still produces non-collapsed layouts.
  if (reference && reference->width() > 0)
    return reference->width();
  return 360;
}

int androidScreenHeightPx(QWidget *reference) {
  QScreen *screen = bestScreen(reference);
  if (screen) {
    const int h = screen->availableGeometry().height();
    if (h > 0)
      return h;
  }
  if (reference && reference->height() > 0)
    return reference->height();
  return 640;
}

int androidUsableViewportWidthPx(QWidget *reference) {
  const int raw = androidScreenWidthPx(reference);
#if defined(Q_OS_ANDROID)
  // Reserve a small safety margin against system cutouts / gesture insets that
  // Qt's availableGeometry does not always reflect on Android.
  return qMax(dp(160), raw - dp(12));
#else
  Q_UNUSED(reference);
  return raw;
#endif
}

int androidContentWidthPx(QWidget *reference) {
  const int w = androidUsableViewportWidthPx(reference);
  const int pad = safeHorizontalPaddingPx(reference);
  return qMax(dp(160), w - 2 * pad);
}

bool isAndroidTablet(QWidget *reference) {
#if defined(Q_OS_ANDROID)
  QScreen *screen = bestScreen(reference);
  if (!screen)
    return false;
  // Convert raw available pixels to material design DP via the actual
  // logicalDotsPerInch (1 dp = 1 px at 160 dpi). This is robust regardless
  // of whether Qt's HighDPI auto-scaling is currently active, because we
  // never go through dp() (which clamps the density at 2.2).
  const qreal logicalDpi = screen->logicalDotsPerInch();
  const int rawW = screen->availableGeometry().width();
  if (rawW <= 0)
    return false;
  const int dpW = int(rawW * 160.0 / qMax<qreal>(96.0, logicalDpi));
  return dpW >= 600;
#else
  Q_UNUSED(reference);
  return false;
#endif
}

} // namespace UiScale
