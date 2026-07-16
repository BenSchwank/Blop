#include "blopstyle.h"

#include "blop_theme.h"
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

// v3.17.1: BlopStyle is now a theme-aware proxy. Every overlay/card in
// the app calls `surfaceStyle()`, so making this follow BlopTheme
// propagates Dark/Light to dozens of surfaces with zero call-site
// edits. The frosted alpha (~94%) is preserved in both modes.
QColor surfaceBg() {
  QColor c = BlopTheme::surfaceBase();
  c.setAlpha(240);
  return c;
}
QColor surfaceBorder() {
  // Quiet hairline — professional document chrome instead of a loud accent glow.
  QColor a = BlopTheme::accentPrimary();
  a.setAlpha(BlopTheme::instance().isDark() ? 40 : 56);
  return a;
}
QColor surfaceShadow() {
  // Soft lift; keep sheets calm rather than dramatic.
  return BlopTheme::instance().isDark() ? QColor(0, 0, 0, 96)
                                        : QColor(10, 12, 28, 32);
}
QColor backdrop(bool forAndroid) {
  // Scrim opacity: Android historically darker (200) to fully veil the
  // canvas; desktop lighter (128). Light-mode scrim uses a soft black
  // veil to keep modal hierarchy.
  if (BlopTheme::instance().isDark())
    return forAndroid ? QColor(6, 8, 16, 200) : QColor(6, 8, 16, 128);
  return forAndroid ? QColor(0, 0, 0, 140) : QColor(0, 0, 0, 96);
}
QColor accent() { return BlopTheme::accentPrimary(); }
QColor textPrimary() {
  QColor c = BlopTheme::textPrimary();
  c.setAlpha(250);
  return c;
}
QColor textSecondary() {
  QColor c = BlopTheme::textSecondary();
  c.setAlpha(220);
  return c;
}

// v3.18.1: aligned to the BlopTheme r16 radius token (was 18) so the sheet
// surfaces sit on the same r8/r12/r16 raster as the rest of the app.
int surfaceRadiusDp() { return BlopTheme::r16; }

QString toolButtonAccentQss(const QColor &accent, int radiusPx) {
  return QStringLiteral(
             "QToolButton { background: transparent; border: none; "
             "padding: 0; border-radius: %1px; }"
             "QToolButton:hover { background: %2; }"
             "QToolButton:pressed { background: %3; }")
      .arg(radiusPx)
      .arg(accent.lighter(110).name(QColor::HexArgb),
           accent.darker(110).name(QColor::HexArgb));
}

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
    posAnim->setDuration(BlopMotion::kEmphasis);
    posAnim->setStartValue(start);
    posAnim->setEndValue(target);
    posAnim->setEasingCurve(BlopMotion::kEaseStandard);
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // Opacity anim (top-level dialog -> windowOpacity; embedded card -> skip
    // to avoid QGraphicsOpacityEffect cost which is the same Windows
    // off-screen-pixmap perf pitfall fixed in Phase A for MorphTray).
    if (m_card->isWindow()) {
      m_card->setWindowOpacity(0.0);
      auto *opAnim = new QPropertyAnimation(m_card, "windowOpacity", m_card);
      opAnim->setDuration(BlopMotion::kStandard);
      opAnim->setStartValue(0.0);
      opAnim->setEndValue(1.0);
      opAnim->setEasingCurve(BlopMotion::kEaseStandard);
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
