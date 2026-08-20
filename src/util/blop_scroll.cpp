#include "blop_scroll.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QByteArray>
#include <QComboBox>
#include <QCoreApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsView>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QTextEdit>
#include <QTimer>
#include <QTouchEvent>
#include <QVariant>
#include <QWidget>

namespace {

constexpr const char *kInstalled = "blopFingerScroll";
constexpr const char *kFitContents = "blopFitContents";
constexpr int kClickDelayMs = 180;
constexpr int kDirectDragPx = 10;

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

bool canScroll(const QAbstractScrollArea *area, Qt::Orientation o) {
  if (!area)
    return false;
  const QScrollBar *sb =
      (o == Qt::Vertical) ? area->verticalScrollBar() : area->horizontalScrollBar();
  return sb && sb->maximum() > sb->minimum();
}

bool canScrollEither(const QAbstractScrollArea *area) {
  return canScroll(area, Qt::Vertical) || canScroll(area, Qt::Horizontal);
}

bool isPassthroughWidget(const QWidget *w) {
  if (!w)
    return true;
  if (qobject_cast<const QLineEdit *>(w) || qobject_cast<const QTextEdit *>(w) ||
      qobject_cast<const QPlainTextEdit *>(w) ||
      qobject_cast<const QAbstractSlider *>(w) ||
      qobject_cast<const QAbstractSpinBox *>(w) ||
      qobject_cast<const QComboBox *>(w) || qobject_cast<const QMenu *>(w))
    return true;
  return false;
}

bool isUnderDrawingCanvas(QWidget *w) {
  for (QWidget *p = w; p; p = p->parentWidget()) {
    if (auto *area = qobject_cast<QAbstractScrollArea *>(p)) {
      if (isDrawingCanvas(area) || isTextEditor(area))
        return true;
    }
  }
  return false;
}

QAbstractScrollArea *enclosingScrollable(QWidget *w) {
  for (QWidget *p = w; p; p = p->parentWidget()) {
    auto *area = qobject_cast<QAbstractScrollArea *>(p);
    if (!area)
      continue;
    if (isDrawingCanvas(area) || isTextEditor(area))
      return nullptr;
    if (canScrollEither(area))
      return area;
  }
  return nullptr;
}

void applyScrollerMetrics(QWidget *vp) {
  QScroller *scroller = QScroller::scroller(vp);
  if (!scroller)
    return;

  QScrollerProperties sp = scroller->scrollerProperties();
  sp.setScrollMetric(QScrollerProperties::DragStartDistance, QVariant(0.0025));
  sp.setScrollMetric(QScrollerProperties::MousePressEventDelay, QVariant(0.0));
  sp.setScrollMetric(QScrollerProperties::DecelerationFactor, QVariant(0.15));
  sp.setScrollMetric(QScrollerProperties::DragVelocitySmoothingFactor,
                     QVariant(0.8));
  sp.setScrollMetric(QScrollerProperties::ScrollingCurve,
                     QVariant::fromValue(QEasingCurve(QEasingCurve::OutQuad)));
  sp.setScrollMetric(QScrollerProperties::AxisLockThreshold, QVariant(0.55));
  const auto overshootOn =
      QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable);
  sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, overshootOn);
  sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, overshootOn);
  scroller->setScrollerProperties(sp);
}

int listContentHeight(QAbstractItemView *view) {
  if (!view)
    return 0;
  int h = view->frameWidth() * 2;
  if (auto *list = qobject_cast<QListWidget *>(view)) {
    const int n = list->count();
    const int spacing = list->spacing();
    for (int i = 0; i < n; ++i) {
      int rh = list->sizeHintForRow(i);
      if (rh <= 0 && list->item(i))
        rh = list->item(i)->sizeHint().height();
      if (rh <= 0)
        rh = 32;
      h += rh;
    }
    if (n > 1 && spacing > 0)
      h += spacing * (n - 1);
    return h;
  }
  if (!view->model())
    return h;
  const int rows = view->model()->rowCount(view->rootIndex());
  for (int i = 0; i < rows; ++i) {
    const int rh = view->sizeHintForRow(i);
    h += rh > 0 ? rh : 32;
  }
  return h;
}

void fitListNow(QAbstractItemView *view) {
  if (!view)
    return;
  view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  view->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  view->setFixedHeight(qMax(0, listContentHeight(view)));
}

class FitContentsHelper final : public QObject {
public:
  explicit FitContentsHelper(QAbstractItemView *view) : QObject(view), m_view(view) {
    if (view && view->model()) {
      QObject::connect(view->model(), &QAbstractItemModel::rowsInserted, this,
                       [this]() { fitListNow(m_view); });
      QObject::connect(view->model(), &QAbstractItemModel::rowsRemoved, this,
                       [this]() { fitListNow(m_view); });
      QObject::connect(view->model(), &QAbstractItemModel::modelReset, this,
                       [this]() { fitListNow(m_view); });
      QObject::connect(view->model(), &QAbstractItemModel::layoutChanged, this,
                       [this]() { fitListNow(m_view); });
    }
    if (view)
      view->installEventFilter(this);
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == m_view && (event->type() == QEvent::Show ||
                              event->type() == QEvent::FontChange ||
                              event->type() == QEvent::StyleChange)) {
      fitListNow(m_view);
    }
    return false;
  }

private:
  QAbstractItemView *m_view{nullptr};
};

class FingerScrollFilter final : public QObject {
public:
  explicit FingerScrollFilter(QObject *parent) : QObject(parent) {
    m_clickDelay = new QTimer(this);
    m_clickDelay->setSingleShot(true);
    m_clickDelay->setInterval(kClickDelayMs);
    connect(m_clickDelay, &QTimer::timeout, this, [this]() { replayHeldPress(); });
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::Show) {
      if (auto *area = qobject_cast<QAbstractScrollArea *>(watched))
        BlopScroll::enableFingerScroll(area);
      return false;
    }
    if (m_replaying)
      return false;

    switch (event->type()) {
    case QEvent::MouseButtonPress:
      return onMousePress(watched, static_cast<QMouseEvent *>(event));
    case QEvent::MouseMove:
      return onMouseMove(static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonRelease:
      return onMouseRelease(static_cast<QMouseEvent *>(event));
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
      return onTouch(watched, static_cast<QTouchEvent *>(event));
    default:
      return false;
    }
  }

private:
  struct Session {
    QPointer<QWidget> pressWidget;
    QPointer<QAbstractScrollArea> area;
    QPointer<QWidget> target;
    QPointF pressGlobal;
    QPointF lastGlobal;
    bool active{false};
    bool scrolling{false};
    bool direct{false};
    bool replayedPress{false};
    bool fromTouch{false};
    int touchId{-1};
  } m;

  QTimer *m_clickDelay{nullptr};
  bool m_replaying{false};

  static qint64 eventTs(const QInputEvent *e) {
    if (!e)
      return 0;
    const ulong ts = e->timestamp();
    return ts ? static_cast<qint64>(ts) : 0;
  }

  QScroller *scrollerForTarget() const {
    return m.target ? QScroller::scroller(m.target.data()) : nullptr;
  }

  void feed(QScroller::Input input, const QPointF &global, qint64 ts) {
    if (!m.target)
      return;
    if (QScroller *sc = scrollerForTarget())
      sc->handleInput(input, m.target->mapFromGlobal(global), ts);
  }

  void applyDirect(const QPoint &delta) {
    if (!m.area)
      return;
    if (auto *v = m.area->verticalScrollBar()) {
      if (v->maximum() > v->minimum())
        v->setValue(v->value() - delta.y());
    }
    if (auto *h = m.area->horizontalScrollBar()) {
      if (h->maximum() > h->minimum())
        h->setValue(h->value() - delta.x());
    }
  }

  void releaseGrab() {
    if (QWidget *g = QWidget::mouseGrabber()) {
      if (g == m.pressWidget || g == m.target)
        g->releaseMouse();
    }
  }

  void clearSession() {
    releaseGrab();
    m_clickDelay->stop();
    m = Session{};
  }

  void replayMouse(QEvent::Type type, const QPointF &global) {
    QWidget *w = m.pressWidget;
    if (!w)
      return;
    const QPointF local = w->mapFromGlobal(global);
    const Qt::MouseButtons buttons = (type == QEvent::MouseButtonRelease)
                                         ? Qt::MouseButtons(Qt::NoButton)
                                         : Qt::MouseButtons(Qt::LeftButton);
    QMouseEvent ev(type, local, global, Qt::LeftButton, buttons, Qt::NoModifier);
    m_replaying = true;
    QCoreApplication::sendEvent(w, &ev);
    m_replaying = false;
  }

  void replayHeldPress() {
    if (!m.active || m.scrolling || m.replayedPress)
      return;
    if (QScroller *sc = scrollerForTarget())
      sc->stop();
    m.replayedPress = true;
    releaseGrab();
    replayMouse(QEvent::MouseButtonPress, m.pressGlobal);
    m.active = false;
  }

  bool beginSession(QWidget *w, const QPointF &global, qint64 ts, bool fromTouch,
                    int touchId) {
    if (!w || isPassthroughWidget(w) || isUnderDrawingCanvas(w))
      return false;
    QAbstractScrollArea *area = enclosingScrollable(w);
    if (!area)
      return false;

    BlopScroll::enableFingerScroll(area);
    QWidget *vp = area->viewport() ? area->viewport() : static_cast<QWidget *>(area);

    m = Session{};
    m.pressWidget = w;
    m.area = area;
    m.target = vp;
    m.pressGlobal = global;
    m.lastGlobal = global;
    m.active = true;
    m.fromTouch = fromTouch;
    m.touchId = touchId;

    feed(QScroller::InputPress, global, ts);
    if (!fromTouch && w)
      w->grabMouse();
    m_clickDelay->start();
    return true;
  }

  bool onDrag(const QPointF &global, qint64 ts) {
    if (!m.active)
      return false;

    const QPoint delta = (global - m.lastGlobal).toPoint();
    m.lastGlobal = global;

    if (!m.scrolling) {
      const QPoint total = (global - m.pressGlobal).toPoint();
      if (total.manhattanLength() < kDirectDragPx)
        return true;
    }

    feed(QScroller::InputMove, global, ts);
    QScroller *sc = scrollerForTarget();
    if (sc && sc->state() == QScroller::Dragging) {
      m.scrolling = true;
      m.direct = false;
      m_clickDelay->stop();
      return true;
    }

    m.scrolling = true;
    m.direct = true;
    m_clickDelay->stop();
    if (sc)
      sc->stop();
    applyDirect(delta);
    return true;
  }

  bool finish(const QPointF &global, qint64 ts) {
    if (!m.active && !m.replayedPress)
      return false;

    const bool wasScrolling = m.scrolling;
    const bool replayed = m.replayedPress;

    if (m.active && !wasScrolling && !replayed) {
      if (QScroller *sc = scrollerForTarget())
        sc->stop();
      releaseGrab();
      replayMouse(QEvent::MouseButtonPress, m.pressGlobal);
      replayMouse(QEvent::MouseButtonRelease, global);
      clearSession();
      return true;
    }

    if (m.active && wasScrolling) {
      if (!m.direct)
        feed(QScroller::InputRelease, global, ts);
      else if (QScroller *sc = scrollerForTarget())
        sc->stop();
      clearSession();
      return true;
    }

    clearSession();
    return false;
  }

  bool onMousePress(QObject *watched, QMouseEvent *me) {
    if (!me || me->button() != Qt::LeftButton)
      return false;
    if (m.fromTouch && me->source() == Qt::MouseEventSynthesizedByQt)
      return true;
    auto *w = qobject_cast<QWidget *>(watched);
    if (!beginSession(w, me->globalPosition(), eventTs(me), false, -1))
      return false;
    return true;
  }

  bool onMouseMove(QMouseEvent *me) {
    if (!m.active)
      return false;
    if (m.fromTouch && me && me->source() == Qt::MouseEventSynthesizedByQt)
      return true;
    if (!me || !(me->buttons() & Qt::LeftButton))
      return false;
    return onDrag(me->globalPosition(), eventTs(me));
  }

  bool onMouseRelease(QMouseEvent *me) {
    if (m.fromTouch && me && me->source() == Qt::MouseEventSynthesizedByQt)
      return m.active || m.replayedPress;
    if (!me || me->button() != Qt::LeftButton)
      return false;
    return finish(me->globalPosition(), eventTs(me));
  }

  bool onTouch(QObject *watched, QTouchEvent *te) {
    if (!te)
      return false;
    const auto points = te->points();
    if (points.isEmpty())
      return false;

    const QEvent::Type type = te->type();
    if (type == QEvent::TouchBegin) {
      auto *w = qobject_cast<QWidget *>(watched);
      const auto &pt = points.first();
      if (!beginSession(w, pt.globalPosition(), eventTs(te), true, int(pt.id())))
        return false;
      te->setAccepted(true);
      return true;
    }

    if (!m.active || !m.fromTouch)
      return false;

    QPointF global = m.lastGlobal;
    for (const auto &pt : points) {
      if (int(pt.id()) == m.touchId) {
        global = pt.globalPosition();
        break;
      }
    }
    const qint64 ts = eventTs(te);
    te->setAccepted(true);
    if (type == QEvent::TouchUpdate)
      return onDrag(global, ts);
    return finish(global, ts);
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
    applyScrollerMetrics(vp);
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

  // Drive scrolling via handleInput from the app-wide filter so a press that
  // starts on a child button still flicks the parent. Do not grab mouse
  // gestures here — they would race the delayed-click path.
  QScroller::ungrabGesture(area);
  QScroller::ungrabGesture(vp);
  QScroller::scroller(vp);
  applyScrollerMetrics(vp);
}

void BlopScroll::makeListFitContents(QAbstractItemView *view) {
  if (!view)
    return;
  fitListNow(view);
  if (view->property(kFitContents).toBool())
    return;
  view->setProperty(kFitContents, true);
  new FitContentsHelper(view);
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
