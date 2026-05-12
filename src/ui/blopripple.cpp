#include "blopripple.h"

#include "uiscale.h"
#ifdef Q_OS_ANDROID
#include "uiprofile.h"
#endif

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScreen>

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
  if (!UiProfile().shouldAnimateRich())
    return;
#else
  Q_UNUSED(host);
#endif

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
}
