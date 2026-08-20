#include "blop_scroll.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QByteArray>
#include <QCoreApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsView>
#include <QPlainTextEdit>
#include <QScroller>
#include <QScrollerProperties>
#include <QTextEdit>
#include <QVariant>
#include <QWidget>

namespace {

constexpr const char *kInstalled = "blopFingerScroll";

bool isDrawingCanvas(const QAbstractScrollArea *area) {
  if (qobject_cast<const QGraphicsView *>(area))
    return true;
  const QByteArray cls(area->metaObject()->className());
  return cls.contains("WebEngine") || cls.contains("QQuick");
}

bool isTextEditor(const QAbstractScrollArea *area) {
  return qobject_cast<const QPlainTextEdit *>(area) ||
         qobject_cast<const QTextEdit *>(area);
}

void applyScrollerMetrics(QAbstractScrollArea *area, QWidget *vp) {
  QScroller *scroller = QScroller::scroller(vp);
  if (!scroller)
    return;

  QScrollerProperties sp = scroller->scrollerProperties();
  // ~2.5 mm: a stationary tap is a click; a short flick is a scroll.
  sp.setScrollMetric(QScrollerProperties::DragStartDistance, QVariant(0.0025));
  // Delay delivering mouse-press so a drag can cancel the click.
  sp.setScrollMetric(QScrollerProperties::MousePressEventDelay, QVariant(0.18));
  sp.setScrollMetric(QScrollerProperties::DecelerationFactor, QVariant(0.15));
  sp.setScrollMetric(QScrollerProperties::DragVelocitySmoothingFactor,
                     QVariant(0.8));
  sp.setScrollMetric(QScrollerProperties::ScrollingCurve,
                     QVariant::fromValue(QEasingCurve(QEasingCurve::OutQuad)));
  sp.setScrollMetric(QScrollerProperties::AxisLockThreshold, QVariant(0.55));

  const bool vOff = area->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
  const bool hOff = area->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
  const bool shortStrip = area->height() > 0 && area->height() <= 72 && vOff;
  const bool horizontalOnly = (vOff && !hOff) || shortStrip;
  const bool verticalOnly = hOff && !vOff;

  const auto overshootOff =
      QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff);
  const auto overshootOn =
      QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable);

  if (horizontalOnly) {
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, overshootOff);
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, overshootOn);
  } else if (verticalOnly) {
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, overshootOff);
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, overshootOn);
  } else {
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, overshootOn);
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, overshootOn);
  }
  scroller->setScrollerProperties(sp);
}

class FingerScrollFilter final : public QObject {
public:
  explicit FingerScrollFilter(QObject *parent) : QObject(parent) {}

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() != QEvent::Show)
      return false;
    if (auto *area = qobject_cast<QAbstractScrollArea *>(watched))
      BlopScroll::enableFingerScroll(area);
    return false;
  }
};

} // namespace

void BlopScroll::enableFingerScroll(QWidget *target) {
  if (!target)
    return;

  auto *area = qobject_cast<QAbstractScrollArea *>(target);
  if (!area && target->parentWidget())
    area = qobject_cast<QAbstractScrollArea *>(target->parentWidget());
  if (!area)
    return;
  if (isDrawingCanvas(area))
    return;

  QWidget *vp = area->viewport() ? area->viewport() : static_cast<QWidget *>(area);
  if (area->property(kInstalled).toBool()) {
    applyScrollerMetrics(area, vp);
    return;
  }

  area->setProperty(kInstalled, true);
  vp->setProperty(kInstalled, true);

  area->setAttribute(Qt::WA_AcceptTouchEvents, true);
  vp->setAttribute(Qt::WA_AcceptTouchEvents, true);

  if (auto *view = qobject_cast<QAbstractItemView *>(area)) {
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  }

  // Detach any older grab on the area itself so we only scroll the viewport.
  QScroller::ungrabGesture(area);
  QScroller::ungrabGesture(vp);

  QScroller::grabGesture(vp, QScroller::TouchGesture);
#ifndef Q_OS_ANDROID
  // Mouse-drag scroll for trackpads / "finger as mouse". Skip text editors
  // so click-drag still selects. Android uses TouchGesture only — mixing
  // LeftMouseButtonGesture there used to stick taps.
  if (!isTextEditor(area))
    QScroller::grabGesture(vp, QScroller::LeftMouseButtonGesture);
#endif

  applyScrollerMetrics(area, vp);
}

void BlopScroll::installApplicationWide(QCoreApplication *app) {
  if (!app)
    return;
  static bool installed = false;
  if (installed)
    return;
  installed = true;
  app->installEventFilter(new FingerScrollFilter(app));
}
