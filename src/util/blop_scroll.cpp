#include "blop_scroll.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QByteArray>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
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
#include <QTouchEvent>
#include <QVariant>
#include <QWidget>

namespace {

constexpr const char *kInstalled = "blopFingerScroll";
constexpr const char *kFitContents = "blopFitContents";
constexpr const char *kPreferClick = "blopPreferClick";
constexpr const char *kNoFingerScroll = "blopNoFingerScroll";
constexpr const char *kVerticalOnly = "blopVerticalOnly";
constexpr int kDirectDragPx = 8;
constexpr int kTouchDragPx = 24;
constexpr int kPreferClickDragPx = 32;

bool preferClickWidget(const QWidget *w) {
  for (const QWidget *p = w; p; p = p->parentWidget()) {
    if (p->property(kPreferClick).toBool())
      return true;
  }
  return false;
}

bool fingerScrollBlocked(const QWidget *w) {
  for (const QWidget *p = w; p; p = p->parentWidget()) {
    if (p->property(kNoFingerScroll).toBool())
      return true;
  }
  return false;
}

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
    // Inner lists that size to their rows must not steal the flick from an
    // outer QScrollArea (a leftover 1–8 px range looks like "can't scroll").
    if (area->property(kFitContents).toBool())
      continue;
    QScrollBar *vs = area->verticalScrollBar();
    QScrollBar *hs = area->horizontalScrollBar();
    const int vr = vs ? vs->maximum() - vs->minimum() : 0;
    const int hr = hs ? hs->maximum() - hs->minimum() : 0;
    if (vr >= 12 || hr >= 12)
      return area;
  }
  return nullptr;
}

void applyScrollerMetrics(QWidget *vp, bool verticalOnly) {
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
  // Vertical-only: lock to Y early so sideways flicks never pan the sheet.
  sp.setScrollMetric(QScrollerProperties::AxisLockThreshold,
                     QVariant(verticalOnly ? 0.15 : 0.55));
  const auto overshootOn =
      QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable);
  const auto overshootOff =
      QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff);
  sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                     verticalOnly ? overshootOff : overshootOn);
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
  // A couple of extra pixels so delegate rounding never leaves a tiny inner
  // scroll range that steals flicks from the outer scroller.
  view->setFixedHeight(qMax(0, listContentHeight(view) + 6));
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
  explicit FingerScrollFilter(QObject *parent) : QObject(parent) {}

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::Show) {
      if (auto *area = qobject_cast<QAbstractScrollArea *>(watched))
        BlopScroll::enableFingerScroll(area);
      return false;
    }

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
    bool fromTouch{false};
    int touchId{-1};
  } m;

  static bool isSynthesizedMouse(const QMouseEvent *me) {
    return me && me->source() != Qt::MouseEventNotSynthesized;
  }

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
    if (m.area->property(kVerticalOnly).toBool())
      return;
    if (auto *h = m.area->horizontalScrollBar()) {
      if (h->maximum() > h->minimum())
        h->setValue(h->value() - delta.x());
    }
  }

  void cancelPressedControl() {
    if (auto *btn = qobject_cast<QAbstractButton *>(m.pressWidget))
      btn->setDown(false);
  }

  void clearSession() { m = Session{}; }

  /// Deadline until which the synthesized mouse stream that trails a finger
  /// flick is swallowed (otherwise the flick also clicks the row under it).
  qint64 m_eatSyntheticUntilMs{0};

  bool eatingSynthetic() const {
    return m_eatSyntheticUntilMs > 0 &&
           QDateTime::currentMSecsSinceEpoch() < m_eatSyntheticUntilMs;
  }

  int dragSlop() const {
    if (preferClickWidget(m.pressWidget) || preferClickWidget(m.area))
      return kPreferClickDragPx;
    return m.fromTouch ? kTouchDragPx : kDirectDragPx;
  }

  bool beginSession(QWidget *w, const QPointF &global, qint64 ts, bool fromTouch,
                    int touchId) {
    if (!w || isPassthroughWidget(w) || isUnderDrawingCanvas(w) ||
        fingerScrollBlocked(w))
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
    return true;
  }

  bool onDrag(const QPointF &global, qint64 ts) {
    if (!m.active)
      return false;

    const QPoint delta = (global - m.lastGlobal).toPoint();
    m.lastGlobal = global;

    if (!m.scrolling) {
      const QPoint total = (global - m.pressGlobal).toPoint();
      if (total.manhattanLength() < dragSlop())
        return false;
    }

    feed(QScroller::InputMove, global, ts);
    QScroller *sc = scrollerForTarget();
    if (!(sc && sc->state() == QScroller::Dragging)) {
      if (sc)
        sc->stop();
      applyDirect(delta);
    }
    if (!m.scrolling) {
      m.scrolling = true;
      cancelPressedControl();
    }
    return true;
  }

  bool finish(const QPointF &global, qint64 ts) {
    if (!m.active)
      return false;
    const bool wasScrolling = m.scrolling;
    const bool wasTouch = m.fromTouch;
    if (wasScrolling) {
      feed(QScroller::InputRelease, global, ts);
      cancelPressedControl();
    } else if (QScroller *sc = scrollerForTarget()) {
      sc->stop();
    }
    clearSession();
    // A finger flick ends with TouchEnd; the synthesized mouse release that
    // trails it must be swallowed so the row under the finger is not clicked.
    if (wasScrolling && wasTouch)
      m_eatSyntheticUntilMs = QDateTime::currentMSecsSinceEpoch() + 400;
    return wasScrolling; // eat release only if we scrolled (no click)
  }

  bool onMousePress(QObject *watched, QMouseEvent *me) {
    if (!me || me->button() != Qt::LeftButton)
      return false;
    if (isSynthesizedMouse(me)) {
      // Touch drives the scroller through QTouchEvents; the synthesized mouse
      // stream is what makes buttons, list rows and check boxes clickable, so
      // only swallow it while a flick is actually running.
      if (eatingSynthetic())
        return true;
      return m.active && m.fromTouch && m.scrolling;
    }
    beginSession(qobject_cast<QWidget *>(watched), me->globalPosition(),
                 eventTs(me), false, -1);
    // Deliver the press so Qt keeps sending moves and a stationary tap clicks.
    return false;
  }

  bool onMouseMove(QMouseEvent *me) {
    if (!me)
      return false;
    if (isSynthesizedMouse(me)) {
      if (eatingSynthetic())
        return true;
      return m.active && m.fromTouch && m.scrolling;
    }
    if (!m.active || m.fromTouch)
      return false;
    return onDrag(me->globalPosition(), eventTs(me));
  }

  bool onMouseRelease(QMouseEvent *me) {
    if (!me)
      return false;
    if (isSynthesizedMouse(me)) {
      if (eatingSynthetic()) {
        m_eatSyntheticUntilMs = 0;
        return true;
      }
      return m.active && m.fromTouch && m.scrolling;
    }
    if (me->button() != Qt::LeftButton)
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
      m_eatSyntheticUntilMs = 0;
      auto *w = qobject_cast<QWidget *>(watched);
      const auto &pt = points.first();
      if (!beginSession(w, pt.globalPosition(), eventTs(te), true, int(pt.id())))
        return false;
      // Same as mouse: let the widget see the press so a tap still clicks.
      return false;
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
    if (type == QEvent::TouchUpdate)
      return onDrag(global, ts);
    return finish(global, ts);
  }
};

} // namespace

void BlopScroll::enableFingerScroll(QWidget *target, Axes axes) {
  if (!target)
    return;

  auto *area = qobject_cast<QAbstractScrollArea *>(target);
  if (!area && target->parentWidget())
    area = qobject_cast<QAbstractScrollArea *>(target->parentWidget());
  if (!area)
    return;
  if (isDrawingCanvas(area))
    return;

  const bool verticalOnly = axes == Axes::VerticalOnly;
  if (verticalOnly)
    area->setProperty(kVerticalOnly, true);

  QWidget *vp = area->viewport() ? area->viewport() : static_cast<QWidget *>(area);
  if (area->property(kInstalled).toBool()) {
    if (verticalOnly) {
      area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      if (auto *hs = area->horizontalScrollBar())
        hs->setEnabled(false);
    }
    applyScrollerMetrics(vp, area->property(kVerticalOnly).toBool());
    return;
  }

  area->setProperty(kInstalled, true);
  vp->setProperty(kInstalled, true);

  area->setAttribute(Qt::WA_AcceptTouchEvents, true);
  vp->setAttribute(Qt::WA_AcceptTouchEvents, true);

  if (verticalOnly) {
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (auto *hs = area->horizontalScrollBar())
      hs->setEnabled(false);
  }

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
  applyScrollerMetrics(vp, verticalOnly);
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
