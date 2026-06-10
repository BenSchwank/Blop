#include "overlayscrollindicator.h"

#include "uiscale.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVariant>

#include <algorithm>

static const char *kIndicatorInstalledTag = "blop_overlay_scroll_installed";

void OverlayScrollIndicator::install(QAbstractScrollArea *area) {
  if (!area)
    return;
  if (area->property(kIndicatorInstalledTag).toBool())
    return;
  area->setProperty(kIndicatorInstalledTag, true);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  new OverlayScrollIndicator(area, Vertical);
  new OverlayScrollIndicator(area, Horizontal);
}

OverlayScrollIndicator::OverlayScrollIndicator(QAbstractScrollArea *area,
                                               Orientation o)
    : QWidget(area ? area->viewport() : nullptr), m_area(area),
      m_orientation(o) {
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFocusPolicy(Qt::NoFocus);

  // v3.17.4: animate a Q_PROPERTY instead of installing a QGraphicsOpacityEffect.
  // The effect forced an offscreen pixmap rasterisation per frame which on the
  // Android software path showed up as visible scroll-stutter.
  m_fadeAnim = new QPropertyAnimation(this, "indicatorOpacity", this);
  m_hideTimer = new QTimer(this);
  m_hideTimer->setSingleShot(true);
  m_hideTimer->setInterval(600);
  connect(m_hideTimer, &QTimer::timeout, this,
          &OverlayScrollIndicator::scheduleHide);

  // v3.17.5: coalesce per-pixel positionSelf() / setGeometry() into a
  // 16 ms cadence. valueChanged() fires for every scroll pixel; without
  // coalescing the indicator's setGeometry() invalidated the parent
  // viewport on every fire, multiplying the layout cost.
  m_repositionCoalescer = new QTimer(this);
  m_repositionCoalescer->setSingleShot(true);
  m_repositionCoalescer->setInterval(16);
  connect(m_repositionCoalescer, &QTimer::timeout, this,
          &OverlayScrollIndicator::positionSelf);

  if (m_area) {
    QScrollBar *sb = (m_orientation == Vertical) ? m_area->verticalScrollBar()
                                                 : m_area->horizontalScrollBar();
    if (sb) {
      connect(sb, &QScrollBar::valueChanged, this,
              &OverlayScrollIndicator::onScrolled);
      connect(sb, &QScrollBar::rangeChanged, this,
              &OverlayScrollIndicator::onRangeChanged);
    }
    if (m_area->viewport())
      m_area->viewport()->installEventFilter(this);
  }

  positionSelf();
  hide();
}

void OverlayScrollIndicator::setIndicatorOpacity(qreal o) {
  o = std::clamp(o, 0.0, 1.0);
  if (qFuzzyCompare(o + 1.0, m_opacity + 1.0))
    return;
  m_opacity = o;
  update();
}

bool OverlayScrollIndicator::eventFilter(QObject *obj, QEvent *event) {
  if (m_area && obj == m_area->viewport()) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
      positionSelf();
      raise();
    }
  }
  return QWidget::eventFilter(obj, event);
}

void OverlayScrollIndicator::onScrolled() {
  // Defer the actual setGeometry() to the next coalescer tick so a burst of
  // valueChanged() emissions becomes one geometry update.
  if (m_repositionCoalescer && !m_repositionCoalescer->isActive())
    m_repositionCoalescer->start();
  showIndicator();
}

void OverlayScrollIndicator::onRangeChanged() {
  if (m_repositionCoalescer && !m_repositionCoalescer->isActive())
    m_repositionCoalescer->start();
}

void OverlayScrollIndicator::positionSelf() {
  if (!m_area || !m_area->viewport())
    return;
  QWidget *vp = m_area->viewport();
  const int margin = UiScale::dp(2);
  const int thickness = UiScale::dp(4);
  const int minHandle = UiScale::dp(24);

  if (m_orientation == Vertical) {
    QScrollBar *sb = m_area->verticalScrollBar();
    if (!sb) {
      hide();
      return;
    }
    const int range = sb->maximum() - sb->minimum();
    const int page = sb->pageStep();
    const int total = range + page;
    if (range <= 0 || total <= 0 || vp->height() <= margin * 2) {
      hide();
      return;
    }
    const int trackH = std::max(0, vp->height() - 2 * margin);
    int handleH = std::max(minHandle, trackH * page / total);
    handleH = std::min(handleH, trackH);
    const int handleY =
        margin + (trackH - handleH) * (sb->value() - sb->minimum()) / range;
    setGeometry(vp->width() - thickness - margin, handleY, thickness, handleH);
  } else {
    QScrollBar *sb = m_area->horizontalScrollBar();
    if (!sb) {
      hide();
      return;
    }
    const int range = sb->maximum() - sb->minimum();
    const int page = sb->pageStep();
    const int total = range + page;
    if (range <= 0 || total <= 0 || vp->width() <= margin * 2) {
      hide();
      return;
    }
    const int trackW = std::max(0, vp->width() - 2 * margin);
    int handleW = std::max(minHandle, trackW * page / total);
    handleW = std::min(handleW, trackW);
    const int handleX =
        margin + (trackW - handleW) * (sb->value() - sb->minimum()) / range;
    setGeometry(handleX, vp->height() - thickness - margin, handleW, thickness);
  }
  raise();
}

void OverlayScrollIndicator::showIndicator() {
  if (!m_area || !m_area->viewport())
    return;
  show();
  raise();
  m_fadeAnim->stop();
  m_fadeAnim->setDuration(80);
  m_fadeAnim->setStartValue(m_opacity);
  m_fadeAnim->setEndValue(0.78);
  m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_fadeAnim->start();
  m_hideTimer->start();
}

void OverlayScrollIndicator::scheduleHide() {
  m_fadeAnim->stop();
  m_fadeAnim->setDuration(200);
  m_fadeAnim->setStartValue(m_opacity);
  m_fadeAnim->setEndValue(0.0);
  m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_fadeAnim->start();
}

void OverlayScrollIndicator::paintEvent(QPaintEvent *) {
  if (m_opacity <= 0.0)
    return;
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setOpacity(m_opacity);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(160, 168, 195, 200));
  const qreal r = std::min(width(), height()) / 2.0;
  p.drawRoundedRect(rect(), r, r);
}
