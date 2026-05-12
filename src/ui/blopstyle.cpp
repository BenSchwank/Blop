#include "blopstyle.h"

#include "uiscale.h"

#include <QColor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QObject>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QString>
#include <QVariantAnimation>
#include <QWidget>

namespace BlopStyle {

QColor surfaceBg() { return QColor(28, 30, 46, 240); }
QColor surfaceBorder() { return QColor(124, 92, 252, 56); }
QColor surfaceShadow() { return QColor(0, 0, 0, 115); }
QColor backdrop(bool forAndroid) {
  return forAndroid ? QColor(6, 8, 16, 200) : QColor(6, 8, 16, 128);
}
QColor accent() { return QColor(124, 92, 252); }
QColor textPrimary() { return QColor(236, 240, 252, 250); }
QColor textSecondary() { return QColor(180, 188, 215, 220); }

int surfaceRadiusDp() { return 18; }

QString surfaceStyle(const QString &cardName) {
  const QColor bg = surfaceBg();
  const QColor bd = surfaceBorder();
  const int radius = UiScale::dp(surfaceRadiusDp());
  return QStringLiteral(
             "#%1 {"
             "  background-color: rgba(%2, %3, %4, %5);"
             "  border: 1px solid rgba(%6, %7, %8, %9);"
             "  border-radius: %10px;"
             "}")
      .arg(cardName)
      .arg(bg.red())
      .arg(bg.green())
      .arg(bg.blue())
      .arg(QString::number(bg.alphaF(), 'f', 3))
      .arg(bd.red())
      .arg(bd.green())
      .arg(bd.blue())
      .arg(QString::number(bd.alphaF(), 'f', 3))
      .arg(radius);
}

void applySurface(QWidget *card, const QString &objectName) {
  if (!card) return;
  card->setObjectName(objectName);
  card->setStyleSheet(card->styleSheet() + surfaceStyle(objectName));
  if (!qobject_cast<QGraphicsDropShadowEffect *>(card->graphicsEffect())) {
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(UiScale::dp(32));
    shadow->setOffset(0, UiScale::dp(12));
    shadow->setColor(surfaceShadow());
    card->setGraphicsEffect(shadow);
  }
}

void applyBackdrop(QWidget *host, bool forAndroid) {
  if (!host) return;
  const QColor bd = backdrop(forAndroid);
  host->setStyleSheet(QStringLiteral(
                          "#%1 { background-color: rgba(%2, %3, %4, %5); }")
                          .arg(host->objectName().isEmpty()
                                   ? QStringLiteral("BlopBackdrop")
                                   : host->objectName())
                          .arg(bd.red())
                          .arg(bd.green())
                          .arg(bd.blue())
                          .arg(QString::number(bd.alphaF(), 'f', 3)));
  if (host->objectName().isEmpty())
    host->setObjectName(QStringLiteral("BlopBackdrop"));
}

namespace {

class PresentAnimFilter : public QObject {
public:
  explicit PresentAnimFilter(QWidget *card, int slideDp, bool fromBottom)
      : QObject(card),
        m_card(card),
        m_slideDp(slideDp),
        m_fromBottom(fromBottom) {}

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched != m_card || event->type() != QEvent::Show || m_fired)
      return QObject::eventFilter(watched, event);
    m_fired = true;
    const int offset = UiScale::dp(m_slideDp);
    const QPoint target = m_card->pos();
    const QPoint start =
        m_fromBottom ? QPoint(target.x(), target.y() + offset)
                     : QPoint(target.x(), target.y() + offset / 2);
    m_card->move(start);

    // Position anim (translate to target).
    auto *posAnim = new QPropertyAnimation(m_card, "pos", m_card);
    posAnim->setDuration(280);
    posAnim->setStartValue(start);
    posAnim->setEndValue(target);
    posAnim->setEasingCurve(QEasingCurve::OutCubic);
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // Opacity anim (top-level dialog -> windowOpacity; embedded card -> skip
    // to avoid QGraphicsOpacityEffect cost which is the same Windows
    // off-screen-pixmap perf pitfall fixed in Phase A for MorphTray).
    if (m_card->isWindow()) {
      m_card->setWindowOpacity(0.0);
      auto *opAnim = new QPropertyAnimation(m_card, "windowOpacity", m_card);
      opAnim->setDuration(220);
      opAnim->setStartValue(0.0);
      opAnim->setEndValue(1.0);
      opAnim->setEasingCurve(QEasingCurve::OutCubic);
      opAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    m_card->removeEventFilter(this);
    deleteLater();
    return QObject::eventFilter(watched, event);
  }

private:
  QWidget *m_card{nullptr};
  int m_slideDp{24};
  bool m_fromBottom{false};
  bool m_fired{false};
};

} // namespace

void installPresentAnim(QWidget *card) {
  if (!card) return;
  auto *f = new PresentAnimFilter(card, 24, /*fromBottom*/ false);
  card->installEventFilter(f);
}

void installPresentAnimFromBottom(QWidget *card, int slideDistanceDp) {
  if (!card) return;
  auto *f = new PresentAnimFilter(card, slideDistanceDp, /*fromBottom*/ true);
  card->installEventFilter(f);
}

} // namespace BlopStyle
