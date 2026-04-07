#include "uiscale.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QWidget>
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
  if (!screen) {
    return 0;
  }
  const QRect full = screen->geometry();
  const QRect avail = screen->availableGeometry();
  return qMax(0, avail.top() - full.top());
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
