#include "blopstyle.h"

#include "blop_theme.h"
#include "notechrome.h"
#include "uiscale.h"

#include <QColor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QObject>
#include <QPalette>
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
QColor surfaceShadow() {
  // J/K: deeper, cooler shadow so cards clearly float above the page.
  return BlopTheme::instance().isDark() ? QColor(0, 0, 0, 90)
                                        : QColor(15, 18, 38, 42);
}
QColor surfaceBorder() {
  // Neutral hairline — Notion quiet, not an accent glow.
  return BlopTheme::instance().isDark() ? QColor(255, 255, 255, 18)
                                        : QColor(20, 24, 44, 16);
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

int surfaceRadiusDp() { return BlopTheme::r12; }

int touchTargetMinDp() { return 40; }

QColor paperBg() { return QColor(0xF7, 0xF7, 0xF5); }
QColor paperBgLibrary() { return QColor(0xF5, 0xF5, 0xF5); }
QColor paperInk() { return QColor(0x1C, 0x1E, 0x24); }
QColor paperInkMuted() { return QColor(0x6B, 0x6F, 0x76); }
QColor paperChipBg() { return QColor(0xF0, 0xEF, 0xED); }
QColor paperRowBg() { return QColor(0xFF, 0xFF, 0xFF); }
QColor obsidianBg() { return QColor(0x1A, 0x1A, 0x1A); }
QColor obsidianDesk() { return QColor(0x12, 0x12, 0x12); }
QColor obsidianNav() { return QColor(0x16, 0x18, 0x1E); }
QColor obsidianText() { return QColor(0xF2, 0xF2, 0xF2); }
QColor obsidianSheet() { return QColor(0x1E, 0x1E, 0x1E); }

namespace {

QString buildSegmentQss(const QColor &acc, const QColor &textIdle,
                        const QColor &textActive, bool darkSurface) {
  const QString accHex = acc.name(QColor::HexRgb);
  const QString accSoft = QStringLiteral("rgba(%1,%2,%3,0.20)")
                              .arg(acc.red())
                              .arg(acc.green())
                              .arg(acc.blue());
  const QString hover = darkSurface ? QStringLiteral("rgba(255,255,255,0.06)")
                                    : QStringLiteral("rgba(0,0,0,0.04)");
  const QString minH = QString::number(UiScale::dp(touchTargetMinDp() - 8));
  QString qss = QStringLiteral(
      "QPushButton {"
      "  background: transparent;"
      "  color: %1;"
      "  border: 1px solid rgba(120,130,160,0.28);"
      "  border-radius: 10px;"
      "  padding: 8px 14px;"
      "  min-height: %5px;"
      "  font-size: 13px;"
      "  font-weight: 600;"
      "}"
      "QPushButton:checked {"
      "  background: %2;"
      "  color: %3;"
      "  border: 1px solid %4;"
      "}"
      "QPushButton:hover:!checked { background: %6; }"
      "QPushButton:pressed { background: %2; }");
  qss.replace(QStringLiteral("%1"), textIdle.name(QColor::HexRgb));
  qss.replace(QStringLiteral("%2"), accSoft);
  qss.replace(QStringLiteral("%3"), textActive.name(QColor::HexRgb));
  qss.replace(QStringLiteral("%4"), accHex);
  qss.replace(QStringLiteral("%5"), minH);
  qss.replace(QStringLiteral("%6"), hover);
  return qss;
}

} // namespace

QString segmentQss() {
  return buildSegmentQss(BlopTheme::accentPrimary(), BlopTheme::textSecondary(),
                         BlopTheme::textPrimary(),
                         BlopTheme::instance().isDark());
}

QString noteSegmentQss() {
  return buildSegmentQss(NoteChrome::accent(), NoteChrome::textSecondary(),
                         NoteChrome::textPrimary(), NoteChrome::isDark());
}

QString paperSegmentQss() {
  return buildSegmentQss(BlopTheme::accentPrimary(), paperInkMuted(), paperInk(),
                         /*darkSurface=*/false);
}

QString paperPrimaryButtonQss() {
  const QColor acc = BlopTheme::accentPrimary();
  return QStringLiteral(
             "QPushButton {"
             "  background: %1; color: #FFFFFF; border: none;"
             "  border-radius: 10px; padding: 10px 18px; font-weight: 700;"
             "  min-height: %2px;"
             "}"
             "QPushButton:hover { background: %3; }"
             "QPushButton:pressed { background: %4; }"
             "QPushButton:disabled { background: #E8E6E2; color: #9A9CA3; }")
      .arg(acc.name(QColor::HexRgb),
           QString::number(UiScale::dp(touchTargetMinDp() - 8)),
           acc.lighter(108).name(QColor::HexRgb),
           acc.darker(108).name(QColor::HexRgb));
}

QString paperSecondaryButtonQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: %1; color: %2;"
             "  border: 1px solid rgba(20,24,40,0.12); border-radius: 10px;"
             "  padding: 10px 16px; font-weight: 600; text-align: left;"
             "  min-height: %3px;"
             "}"
             "QPushButton:hover { border-color: rgba(20,24,40,0.22);"
             "  background: #EBEAE6; }"
             "QPushButton:pressed { background: #E4E3DF; }")
      .arg(paperChipBg().name(QColor::HexRgb), paperInk().name(QColor::HexRgb),
           QString::number(UiScale::dp(touchTargetMinDp() - 8)));
}

QString paperDestructiveButtonQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: #FDF2F2; color: #C0392B;"
             "  border: 1px solid rgba(192,57,43,0.28); border-radius: 10px;"
             "  padding: 10px 16px; font-weight: 600; text-align: left;"
             "  min-height: %1px;"
             "}"
             "QPushButton:hover { background: #FAE5E5; border-color: rgba(192,57,43,0.45); }"
             "QPushButton:pressed { background: #F5D6D6; }")
      .arg(UiScale::dp(touchTargetMinDp() - 8));
}

QString paperInputQss() {
  const QColor acc = BlopTheme::accentPrimary();
  return QStringLiteral(
             "QLineEdit, QPlainTextEdit, QTextEdit {"
             "  background: #FFFFFF; color: %1;"
             "  border: 1px solid rgba(20,24,40,0.12); border-radius: 10px;"
             "  padding: 8px 12px; selection-background-color: %2;"
             "  selection-color: #FFFFFF;"
             "}"
             "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {"
             "  border: 1px solid %2;"
             "}")
      .arg(paperInk().name(QColor::HexRgb), acc.name(QColor::HexRgb));
}

void paintPaperSurface(QWidget *w, const QString &objectName) {
  if (!w)
    return;
  if (!objectName.isEmpty())
    w->setObjectName(objectName);
  w->setAttribute(Qt::WA_StyledBackground, true);
  w->setAutoFillBackground(true);
  QPalette pal = w->palette();
  const QColor bg = paperBg();
  const QColor ink = paperInk();
  pal.setColor(QPalette::Window, bg);
  pal.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
  pal.setColor(QPalette::Text, ink);
  pal.setColor(QPalette::WindowText, ink);
  pal.setColor(QPalette::Button, paperChipBg());
  pal.setColor(QPalette::ButtonText, ink);
  pal.setColor(QPalette::PlaceholderText, paperInkMuted());
  w->setPalette(pal);
  const QString name =
      w->objectName().isEmpty() ? QStringLiteral("BlopPaper") : w->objectName();
  w->setStyleSheet(
      QStringLiteral("QWidget#%1 { background-color: %2; color: %3; }")
          .arg(name, bg.name(QColor::HexRgb), ink.name(QColor::HexRgb)));
}

QString quietIconButtonQss(int radiusPx) {
  const QString hover = BlopTheme::instance().isDark()
                            ? QStringLiteral("rgba(255,255,255,0.10)")
                            : QStringLiteral("rgba(0,0,0,0.08)");
  const QString press = BlopTheme::instance().isDark()
                            ? QStringLiteral("rgba(255,255,255,0.16)")
                            : QStringLiteral("rgba(0,0,0,0.12)");
  return QStringLiteral(
             "QToolButton, QPushButton {"
             "  background: transparent; border: none;"
             "  border-radius: %1px; padding: 0;"
             "  min-width: %2px; min-height: %2px;"
             "}"
             "QToolButton:hover, QPushButton:hover { background: %3; }"
             "QToolButton:pressed, QPushButton:pressed { background: %4; }")
      .arg(radiusPx)
      .arg(UiScale::dp(touchTargetMinDp()))
      .arg(hover, press);
}

QString menuItemQss(bool destructive) {
  auto rgba = [](const QColor &c) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
  };
  const QColor accentSoft = NoteChrome::accentSoft();
  const QString text =
      destructive ? QStringLiteral("#FF6B6B") : QStringLiteral("#F0F0F0");
  const QString press = rgba(accentSoft);
  QColor hoverCol = accentSoft;
  hoverCol.setAlpha(qMax(18, hoverCol.alpha() / 2));
  const QString hover = rgba(hoverCol);
  QString qss = QStringLiteral(
      "QPushButton {"
      "  background: transparent;"
      "  color: %1;"
      "  border: none;"
      "  text-align: left;"
      "  padding: %4px %5px;"
      "  font-size: 13px;"
      "  font-weight: 500;"
      "  border-radius: 8px;"
      "}"
      "QPushButton:hover { background: %3; }"
      "QPushButton:pressed {"
      "  background: %2;"
      "  color: #FFFFFF;"
      "}"
      "QPushButton:focus { outline: none; }");
  qss.replace(QStringLiteral("%1"), text);
  qss.replace(QStringLiteral("%2"), press);
  qss.replace(QStringLiteral("%3"), hover);
  qss.replace(QStringLiteral("%4"), QString::number(UiScale::dp(12)));
  qss.replace(QStringLiteral("%5"), QString::number(UiScale::dp(18)));
  return qss;
}

QString obsidianSeparatorQss() {
  return QStringLiteral(
      "QFrame {"
      "  background: rgba(255,255,255,0.10);"
      "  border: none;"
      "  max-height: 1px;"
      "  margin: 6px 10px;"
      "}");
}

QString paperScrollbarQss() {
  return QStringLiteral(
      "QScrollBar:vertical {"
      "  background: transparent; width: 8px; margin: 4px 2px;"
      "}"
      "QScrollBar::handle:vertical {"
      "  background: rgba(20,24,40,0.18); border-radius: 4px; min-height: 24px;"
      "}"
      "QScrollBar::handle:vertical:hover { background: rgba(20,24,40,0.28); }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
      "  height: 0; background: none; border: none;"
      "}"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
      "  background: none;"
      "}"
      "QScrollBar:horizontal { height: 0; }");
}

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
    shadow->setBlurRadius(UiScale::dp(16));
    shadow->setOffset(0, UiScale::dp(6));
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
    posAnim->setDuration(BlopMotion::kStandard);
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
      opAnim->setDuration(BlopMotion::kFast);
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
  int m_slideDp{12};
  bool m_fromBottom{false};
  bool m_fired{false};
};

} // namespace

void installPresentAnim(QWidget *card) {
  if (!card) return;
  auto *f = new PresentAnimFilter(card, 12, /*fromBottom*/ false);
  card->installEventFilter(f);
}

void installPresentAnimFromBottom(QWidget *card, int slideDistanceDp) {
  if (!card) return;
  auto *f = new PresentAnimFilter(card, slideDistanceDp, /*fromBottom*/ true);
  card->installEventFilter(f);
}

} // namespace BlopStyle
