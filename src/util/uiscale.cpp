#include "uiscale.h"

#include <QGuiApplication>
#include <QPoint>
#include <QScreen>
#include <QWindow>
#include <QWidget>
#include <QtGlobal>
#include <QtMath>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

namespace {

#if defined(Q_OS_ANDROID)
// Reads the platform status-bar height straight from Android's dimen resource.
// Under edge-to-edge (setDecorFitsSystemWindows(false)) the window fills the
// whole screen, so QScreen::availableGeometry can report a zero top inset even
// though the transparent status bar is still drawn over our content. Using this
// as a floor keeps the header from sliding under the system status bar.
int androidStatusBarHeightResourcePx() {
  QJniObject activity = QJniObject::callStaticObjectMethod(
      "org/qtproject/qt/android/QtNative", "activity",
      "()Landroid/app/Activity;");
  if (!activity.isValid())
    return 0;
  QJniObject resources = activity.callObjectMethod(
      "getResources", "()Landroid/content/res/Resources;");
  if (!resources.isValid())
    return 0;
  QJniObject dimenName =
      QJniObject::fromString(QStringLiteral("status_bar_height"));
  QJniObject defType = QJniObject::fromString(QStringLiteral("dimen"));
  QJniObject defPackage = QJniObject::fromString(QStringLiteral("android"));
  const jint resId = resources.callMethod<jint>(
      "getIdentifier",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
      dimenName.object<jstring>(), defType.object<jstring>(),
      defPackage.object<jstring>());
  if (resId <= 0)
    return 0;
  return qMax(0, resources.callMethod<jint>("getDimensionPixelSize", "(I)I",
                                            resId));
}

// Screen-space y of our top-level window's top edge. This is what lets us tell
// whether Qt is drawing edge-to-edge (window at y=0, status bar overlaps our
// content) or whether the window is already positioned below the status bar
// (window at y=statusBarHeight, no overlap).
int androidWindowTopOnScreenPx(QWidget *reference) {
  QWindow *win = nullptr;
  if (reference) {
    if (QWidget *top = reference->window())
      win = top->windowHandle();
  }
  if (!win) {
    const QList<QWindow *> tops = QGuiApplication::topLevelWindows();
    for (QWindow *w : tops) {
      if (w && w->isVisible()) {
        win = w;
        break;
      }
    }
  }
  if (!win)
    return 0;
  return qMax(0, win->mapToGlobal(QPoint(0, 0)).y());
}
#endif

QScreen *bestScreen(QWidget *reference) {
  if (reference && reference->windowHandle() && reference->windowHandle()->screen()) {
    return reference->windowHandle()->screen();
  }
  return QGuiApplication::primaryScreen();
}

// v3.17.5: cache the primary-screen density. Previously every call to
// dp() did a QScreen::logicalDotsPerInch() lookup -- the cpp file alone
// has 300+ call sites, many in paint/resize hot paths. On Android the
// screen accessor can route through JNI on some Qt versions; even
// without that the divide and bound were measurable in tight loops.
//
// Invalidation: hooked into QGuiApplication::primaryScreenChanged and
// QScreen::logicalDotsPerInchChanged the first time we resolve the
// density (lazy, so we don't need to be careful about init order vs
// QGuiApplication).
qreal s_cachedPrimaryDensity = -1.0;

qreal computeDensity(QScreen *screen) {
  if (!screen)
    return 1.0;
  const qreal raw = screen->logicalDotsPerInch() / 160.0;
  return qBound<qreal>(1.0, raw, 2.2);
}

qreal primaryDensityCached() {
  if (s_cachedPrimaryDensity > 0.0)
    return s_cachedPrimaryDensity;
  QScreen *primary = QGuiApplication::primaryScreen();
  s_cachedPrimaryDensity = computeDensity(primary);
  if (auto *app = QGuiApplication::instance()) {
    QObject::connect(qApp, &QGuiApplication::primaryScreenChanged, app,
                     [](QScreen *) { s_cachedPrimaryDensity = -1.0; });
    if (primary) {
      QObject::connect(primary, &QScreen::logicalDotsPerInchChanged, app,
                       [](qreal) { s_cachedPrimaryDensity = -1.0; });
    }
  }
  return s_cachedPrimaryDensity;
}

qreal densityFor(QWidget *reference) {
  if (!reference || !reference->windowHandle() ||
      !reference->windowHandle()->screen()) {
    // Hot path: ~95% of dp() calls pass nullptr. Cached single-screen
    // density covers it without any QScreen lookup at all.
    return primaryDensityCached();
  }
  return computeDensity(reference->windowHandle()->screen());
}

} // namespace

namespace UiScale {

int dp(int px) {
  return qMax(1, qRound(px * primaryDensityCached()));
}

int sp(int px) {
  return qMax(1, qRound(px * primaryDensityCached()));
}

int androidTopInsetPx(QWidget *reference) {
  QScreen *screen = bestScreen(reference);
  int availTop = 0;
  int fullTop = 0;
  int availInset = 0;
  if (screen) {
    const QRect full = screen->geometry();
    const QRect avail = screen->availableGeometry();
    fullTop = full.top();
    availTop = avail.top();
    availInset = qMax(0, avail.top() - full.top());
  }
  int inset = availInset;
#if defined(Q_OS_ANDROID)
  // The header is laid out *inside* our top-level window, so the only padding it
  // needs is however much the status bar actually overlaps that window's top:
  //
  //   inset = statusBarHeight - windowTopOnScreen
  //
  // - True edge-to-edge: the window starts at screen y=0, so we reserve the full
  //   status-bar height and the header sits flush beneath the visible bar.
  // - Window already positioned below the status bar (windowTop == statusBar):
  //   the needed inset collapses to 0, so we don't add a second status-bar-sized
  //   gap on top of the one the window manager already applied (the "doppelt so
  //   dick" / doubled bar regression).
  const int statusBar = androidStatusBarHeightResourcePx();
  const int windowTop = androidWindowTopOnScreenPx(reference);
  inset = qBound(0, statusBar - windowTop, dp(72));
  qWarning("BlopInset: availTop=%d fullTop=%d availInset=%d statusBar=%d "
           "windowTop=%d -> inset=%d",
           availTop, fullTop, availInset, statusBar, windowTop, inset);
#else
  Q_UNUSED(availTop);
  Q_UNUSED(fullTop);
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
  // Material design defines the tablet breakpoint as >=600 dp wide.
  // Translated into physical units that's roughly 95 mm
  // (600 dp / 160 dp/in * 25.4 mm/in). screen->physicalSize() comes
  // straight from the Android display metrics and is unaffected by
  // Qt's HighDPI scaling mode or by OEM-specific logicalDpi quirks
  // (some Samsung / cheap-OEM devices report logicalDpi=96 even on
  // 480 dpi panels), so it's the most reliable phone-vs-tablet check.
  const QSizeF mm = screen->physicalSize();
  if (mm.isEmpty())
    return false;
  const qreal widthMm = qMin(mm.width(), mm.height());
  return widthMm >= 90.0;
#else
  Q_UNUSED(reference);
  return false;
#endif
}

} // namespace UiScale
