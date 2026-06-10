#include "blopripple.h"

#include "uiscale.h"

#include <QEasingCurve>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSequentialAnimationGroup>
#include <QVariantAnimation>
#include <QWidget>

void BlopRipple::setRipScale(qreal s) {
  m_ripScale = s;
  update();
}

void BlopRipple::setRipOpacity(qreal o) {
  m_ripOpacity = o;
  update();
}

BlopRipple::BlopRipple(const QColor &accent, QWidget *parent)
    : QWidget(parent), m_accent(accent) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  setFocusPolicy(Qt::NoFocus);
  if (!parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool |
                   Qt::WindowTransparentForInput |
                   Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
  }
}

void BlopRipple::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const QPointF c(width() / 2.0, height() / 2.0);
  QColor fill = m_accent;
  fill.setAlpha(90);

  const qreal dp = UiScale::dp(1);
  const qreal baseR = UiScale::dp(36) * m_ripScale;
  const qreal offsets[3] = {-4.0 * dp, 0.0, 4.0 * dp};

  p.setOpacity(qBound(0.0, m_ripOpacity, 1.0));
  p.setPen(Qt::NoPen);
  p.setBrush(fill);
  for (qreal ox : offsets) {
    QPainterPath path;
    path.addEllipse(c + QPointF(ox, 0), baseR, baseR);
    p.drawPath(path);
  }
}

void BlopRipple::spawn(QWidget *host, const QPoint &globalPos,
                       const QColor &accent) {
#ifdef Q_OS_ANDROID
  // Top-level Qt::Tool widgets crash on Android's single-window surface
  // (Qt 6.10 inproc). The tile-press scale animation gives enough haptic
  // feedback; we skip the ripple entirely on phones/tablets.
  Q_UNUSED(host);
  Q_UNUSED(globalPos);
  Q_UNUSED(accent);
  return;
#else
  Q_UNUSED(host);

  QWidget *rip = new BlopRipple(accent, nullptr);
  const int w = UiScale::dp(96);
  const int h = UiScale::dp(48);
  rip->resize(w, h);
  rip->move(globalPos.x() - w / 2, globalPos.y() - h / 2);

  if (QScreen *scr = QGuiApplication::screenAt(globalPos))
    rip->setScreen(scr);

  rip->show();

  auto *grp = new QParallelAnimationGroup(rip);

  auto *scaleAnim = new QPropertyAnimation(rip, "ripScale", rip);
  scaleAnim->setDuration(320);
  scaleAnim->setStartValue(0.0);
  scaleAnim->setEndValue(1.4);
  scaleAnim->setEasingCurve(QEasingCurve::OutElastic);

  auto *opAnim = new QPropertyAnimation(rip, "ripOpacity", rip);
  opAnim->setDuration(280);
  opAnim->setStartValue(1.0);
  opAnim->setEndValue(0.0);
  opAnim->setEasingCurve(QEasingCurve::OutCubic);

  grp->addAnimation(scaleAnim);
  grp->addAnimation(opAnim);
  QObject::connect(grp, &QParallelAnimationGroup::finished, rip,
                   &QWidget::close);
  grp->start(QAbstractAnimation::DeleteWhenStopped);
#endif
}

void BlopRipple::animatePress(QWidget *target, qreal pressedScale) {
  if (!target)
    return;
#ifdef Q_OS_ANDROID
  // Geometry shrink on layout-managed header pills causes ghosting and
  // layout thrash on Android. Opacity-only press is enough tactile feedback.
  QPointer<QWidget> safe(target);
  const qreal peak = qBound(0.75, pressedScale, 1.0);
  auto *down = new QPropertyAnimation(target, "windowOpacity", target);
  down->setDuration(90);
  down->setStartValue(1.0);
  down->setEndValue(peak);
  auto *up = new QPropertyAnimation(target, "windowOpacity", target);
  up->setDuration(180);
  up->setStartValue(peak);
  up->setEndValue(1.0);
  auto *seq = new QSequentialAnimationGroup(target);
  seq->addAnimation(down);
  seq->addAnimation(up);
  QObject::connect(seq, &QSequentialAnimationGroup::finished, target,
                   [safe]() {
                     if (safe)
                       safe->setWindowOpacity(1.0);
                   });
  seq->start(QAbstractAnimation::DeleteWhenStopped);
#else
  // Anchored on the widget's current geometry. We animate the geometry
  // toward a shrunk-around-center rectangle and back, giving the user a
  // light bounce. Cheap (one QVariantAnimation), safe on any platform,
  // and doesn't fight QGraphicsEffect (which can hit Android GL paths).
  const QRect base = target->geometry();
  if (base.isEmpty())
    return;
  const qreal s = qBound(0.5, pressedScale, 1.0);
  const int shrinkW = int(base.width() * (1.0 - s) * 0.5);
  const int shrinkH = int(base.height() * (1.0 - s) * 0.5);
  const QRect pressed =
      base.adjusted(shrinkW, shrinkH, -shrinkW, -shrinkH);

  QPointer<QWidget> safe(target);

  auto *down = new QVariantAnimation(target);
  down->setDuration(90);
  down->setEasingCurve(QEasingCurve::OutCubic);
  down->setStartValue(base);
  down->setEndValue(pressed);
  QObject::connect(down, &QVariantAnimation::valueChanged, target,
                   [safe](const QVariant &v) {
                     if (safe)
                       safe->setGeometry(v.toRect());
                   });

  auto *up = new QVariantAnimation(target);
  up->setDuration(220);
  up->setEasingCurve(QEasingCurve::OutBack);
  up->setStartValue(pressed);
  up->setEndValue(base);
  QObject::connect(up, &QVariantAnimation::valueChanged, target,
                   [safe](const QVariant &v) {
                     if (safe)
                       safe->setGeometry(v.toRect());
                   });

  auto *seq = new QSequentialAnimationGroup(target);
  seq->addAnimation(down);
  seq->addAnimation(up);
  QObject::connect(seq, &QSequentialAnimationGroup::finished, target,
                   [safe, base]() {
                     if (safe)
                       safe->setGeometry(base);
                   });
  seq->start(QAbstractAnimation::DeleteWhenStopped);
#endif
}

void BlopRipple::attachPressFeedback(QAbstractButton *btn, qreal pressedScale) {
  if (!btn)
    return;
  if (btn->property("blopPressFeedback").toBool())
    return;
  btn->setProperty("blopPressFeedback", true);
  QObject::connect(btn, &QAbstractButton::pressed, btn, [btn, pressedScale]() {
    BlopRipple::animatePress(btn, pressedScale);
  });
}
