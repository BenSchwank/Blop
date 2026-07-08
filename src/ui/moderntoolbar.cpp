#include "moderntoolbar.h"
#include "blop_theme.h"
#include "blopripple.h"
#include "UIStyles.h"
#include "tools/ToolManager.h"
#include "tools/ToolUIBridge.h"
#include "tools/ShapeTool.h"
#include "tools/WritingTools.h"
#include "uiscale.h"
#include <QApplication>
#include <QButtonGroup>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include "editoroverlays.h"
#include <QDialog>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QMainWindow>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPauseAnimation>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QPushButton>
#include <QRegion>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QResizeEvent>
#include <cmath>
#include <QtGlobal>
#include <QtMath>

#ifdef Q_OS_ANDROID
#define BLOP_TOOLBAR_LONGPRESS 1
#else
#define BLOP_TOOLBAR_LONGPRESS 0
#endif

// v3.16.1 UI roadmap (deferred): bottom-sheet style for Settings/Profile/NewNote,
// tab bar reskin, swipe page transitions in the editor, sidebar push animation.

namespace {

// 64×64 logical coords (same convention as MainWindow::createModernIcon) — identical on all platforms.
void drawToolbarGlyph64(QPainter *p, const QString &name, const QColor &color) {
  p->setRenderHint(QPainter::Antialiasing);
  const QColor primary(240, 244, 255, qBound(212, color.alpha(), 255));
  auto familyAccent = [&](const QString &iconName) -> QColor {
    // Writing tools: warmer creative family.
    if (iconName == QLatin1String("pen") || iconName == QLatin1String("pencil") ||
        iconName == QLatin1String("highlighter") || iconName == QLatin1String("eraser") ||
        iconName == QLatin1String("lasso") || iconName == QLatin1String("ruler") ||
        iconName == QLatin1String("shape") || iconName == QLatin1String("stickynote") ||
        iconName == QLatin1String("text") || iconName == QLatin1String("image")) {
      if (iconName == QLatin1String("pen"))
        return QColor(122, 136, 255, qBound(185, color.alpha(), 255));
      if (iconName == QLatin1String("pencil"))
        return QColor(102, 170, 255, qBound(185, color.alpha(), 255));
      if (iconName == QLatin1String("highlighter"))
        return QColor(132, 206, 255, qBound(185, color.alpha(), 255));
      if (iconName == QLatin1String("eraser"))
        return QColor(244, 126, 176, qBound(185, color.alpha(), 255));
      if (iconName == QLatin1String("ruler"))
        return QColor(92, 214, 190, qBound(185, color.alpha(), 255));
      return QColor(132, 140, 255, qBound(185, color.alpha(), 255));
    }
    // Navigation tools: cooler utility family.
    if (iconName == QLatin1String("hand") || iconName == QLatin1String("overview") ||
        iconName == QLatin1String("dock_float") || iconName == QLatin1String("dock_fixed")) {
      return QColor(86, 184, 255, qBound(185, color.alpha(), 255));
    }
    // Action/system tools.
    return QColor(110, 166, 255, qBound(185, color.alpha(), 255));
  };
  const QColor accent = familyAccent(name);
  QColor accentSoft = accent;
  accentSoft.setAlpha(132);

  const QPen primaryPen(primary, 3.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen primaryThin(primary, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen accentPen(accent, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen accentThick(accent, 5.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  auto drawAccentDot = [&](qreal x, qreal y, qreal r) {
#ifndef Q_OS_ANDROID
    p->setPen(Qt::NoPen);
    p->setBrush(accent);
    p->drawEllipse(QPointF(x, y), r, r);
#else
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(r);
#endif
  };

  p->setPen(primaryPen);
  p->setBrush(Qt::NoBrush);

  if (name == QLatin1String("settings")) {
    constexpr qreal kPi = 3.14159265358979323846;
    p->drawEllipse(QPointF(32, 32), 12.5, 12.5);
    p->setPen(accentPen);
    p->drawEllipse(QPointF(32, 32), 6.2, 6.2);
    for (int i = 0; i < 6; ++i) {
      const qreal a = (i * kPi) / 3.0;
      const qreal cs = qCos(a);
      const qreal sn = qSin(a);
      p->setPen(primaryThin);
      p->drawLine(QPointF(32 + cs * 15.5, 32 + sn * 15.5),
                  QPointF(32 + cs * 20.0, 32 + sn * 20.0));
    }
    return;
  }
  if (name == QLatin1String("save")) {
    p->setPen(primaryPen);
    p->drawRoundedRect(18, 14, 28, 35, 5, 5);
    p->fillRect(QRectF(22, 18, 20, 8), accentSoft);
    p->setPen(accentPen);
    p->drawLine(24, 33, 40, 33);
    p->drawLine(24, 39, 36, 39);
    return;
  }
  if (name == QLatin1String("palette")) {
    QPainterPath plate;
    plate.moveTo(18, 36);
    plate.cubicTo(15, 25, 21, 17, 31, 16);
    plate.cubicTo(42, 16, 49, 24, 47, 35);
    plate.cubicTo(45, 44, 37, 48, 28, 47);
    plate.cubicTo(22, 47, 18, 43, 18, 36);
    p->setPen(primaryPen);
    p->setBrush(accentSoft);
    p->drawPath(plate);
    drawAccentDot(25, 26, 2.7);
    drawAccentDot(33, 24, 2.7);
    drawAccentDot(37, 31, 2.7);
    return;
  }
  if (name == QLatin1String("brush_size")) {
    p->setPen(primaryPen);
    p->drawLine(19, 45, 45, 19);
    p->setPen(accentThick);
    p->drawLine(29, 35, 37, 27);
    p->setPen(accentPen);
    p->drawEllipse(14, 36, 12, 12);
    return;
  }
  if (name == QLatin1String("dock_float")) {
    p->setPen(primaryPen);
    p->drawRoundedRect(15, 24, 34, 24, 5, 5);
    p->fillRect(QRectF(20, 29, 11, 6), accentSoft);
    p->setPen(accentPen);
    p->drawLine(32, 24, 32, 13);
    p->drawLine(27, 18, 32, 13);
    p->drawLine(37, 18, 32, 13);
    return;
  }
  if (name == QLatin1String("dock_fixed")) {
    p->setPen(primaryThin);
    p->drawRoundedRect(13, 20, 17, 26, 4, 4);
    p->drawRoundedRect(34, 20, 17, 26, 4, 4);
    p->setPen(accentPen);
    p->drawLine(30, 28, 34, 28);
    p->drawLine(30, 35, 34, 35);
    p->setPen(Qt::NoPen);
    p->setBrush(accentSoft);
    p->drawRoundedRect(16, 23, 6, 8, 2, 2);
    p->drawRoundedRect(42, 23, 6, 8, 2, 2);
    return;
  }
  if (name == QLatin1String("overview")) {
    QPainterPath home;
    home.moveTo(12, 28);
    home.lineTo(32, 13);
    home.lineTo(52, 28);
    home.lineTo(52, 45);
    home.lineTo(12, 45);
    home.closeSubpath();
    p->setPen(primaryPen);
    p->drawPath(home);
    p->fillRect(QRectF(18, 34, 28, 8), accentSoft);
    p->setPen(accentPen);
    p->drawLine(23, 45, 23, 33);
    p->drawLine(41, 45, 41, 33);
    return;
  }
  if (name == QLatin1String("pen")) {
    QPainterPath body;
    body.moveTo(18, 44);
    body.lineTo(37, 25);
    body.lineTo(44, 32);
    body.lineTo(25, 51);
    body.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(accentSoft);
    p->drawPath(body);
    p->setPen(primaryPen);
    p->drawPath(body);
    p->setPen(accentPen);
    p->drawLine(29, 47, 40, 36);
    QPainterPath nib;
    nib.moveTo(44, 32);
    nib.lineTo(49, 27);
    nib.lineTo(46, 24);
    nib.lineTo(41, 29);
    nib.closeSubpath();
    p->setPen(primaryThin);
    p->setBrush(QColor(236, 239, 248, 230));
    p->drawPath(nib);
    return;
  }
  if (name == QLatin1String("pencil")) {
    QPainterPath shaft;
    shaft.moveTo(16, 45);
    shaft.lineTo(39, 22);
    shaft.lineTo(45, 28);
    shaft.lineTo(22, 51);
    shaft.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(accentSoft);
    p->drawPath(shaft);
    p->setPen(primaryPen);
    p->drawPath(shaft);
    p->setPen(accentPen);
    p->drawLine(24, 48, 42, 30);
    QPainterPath woodTip;
    woodTip.moveTo(45, 28);
    woodTip.lineTo(50, 23);
    woodTip.lineTo(46, 19);
    woodTip.lineTo(41, 24);
    woodTip.closeSubpath();
    p->setPen(primaryThin);
    p->setBrush(QColor(225, 200, 158, 230));
    p->drawPath(woodTip);
    p->setPen(QPen(QColor(110, 114, 130, 230), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawLine(47, 22, 50, 19);
    return;
  }
  if (name == QLatin1String("highlighter")) {
    QPainterPath marker;
    marker.moveTo(16, 44);
    marker.lineTo(33, 27);
    marker.lineTo(44, 38);
    marker.lineTo(27, 55);
    marker.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(accentSoft);
    p->drawPath(marker);
    p->setPen(primaryPen);
    p->drawPath(marker);
    p->setPen(accentPen);
    p->drawLine(20, 45, 39, 26);
    p->drawLine(25, 50, 44, 31);
    QPainterPath cap;
    cap.moveTo(33, 27);
    cap.lineTo(39, 21);
    cap.lineTo(50, 32);
    cap.lineTo(44, 38);
    cap.closeSubpath();
    p->setPen(primaryThin);
    p->setBrush(QColor(231, 236, 248, 232));
    p->drawPath(cap);
    p->setPen(accentThick);
    p->drawLine(46, 35, 51, 30);
    return;
  }
  if (name == QLatin1String("eraser")) {
    p->setPen(primaryPen);
    p->setBrush(accentSoft);
    p->drawRoundedRect(15, 24, 33, 21, 6, 6);
    p->setPen(accentPen);
    p->drawLine(19, 35, 45, 35);
    return;
  }
  if (name == QLatin1String("lasso")) {
    QPainterPath lasso;
    lasso.moveTo(17, 32);
    lasso.cubicTo(17, 20, 29, 14, 39, 21);
    lasso.cubicTo(48, 28, 46, 42, 32, 44);
    lasso.cubicTo(22, 46, 15, 41, 17, 32);
    p->setPen(primaryPen);
    p->drawPath(lasso);
    p->setPen(accentPen);
    p->drawArc(QRectF(34, 35, 11, 11), 20 * 16, 280 * 16);
    return;
  }
  if (name == QLatin1String("ruler")) {
    p->setPen(primaryPen);
    p->drawRoundedRect(13, 32, 38, 9, 3, 3);
    p->setBrush(accentSoft);
    p->setPen(Qt::NoPen);
    p->drawRect(15, 34, 34, 5);
    p->setPen(accentPen);
    for (int x = 18; x <= 46; x += 4)
      p->drawLine(x, 32, x, (x % 8 == 2) ? 26 : 29);
    return;
  }
  if (name == QLatin1String("shape")) {
    p->setPen(Qt::NoPen);
    p->setBrush(accentSoft);
    p->drawRoundedRect(19, 19, 10, 10, 4, 4);
    p->setPen(primaryPen);
    p->drawRoundedRect(15, 15, 34, 34, 7, 7);
    p->setPen(accentPen);
    p->drawEllipse(24, 24, 16, 16);
    return;
  }
  if (name == QLatin1String("stickynote")) {
    p->setPen(primaryPen);
    p->drawRoundedRect(18, 14, 28, 36, 4, 4);
    p->setPen(Qt::NoPen);
    p->setBrush(accentSoft);
    p->drawRect(22, 20, 16, 17);
    p->setPen(accentPen);
    p->drawLine(39, 14, 39, 22);
    p->drawLine(33, 22, 39, 22);
    return;
  }
  if (name == QLatin1String("text")) {
    QFont f = p->font();
    f.setPixelSize(30);
    f.setBold(true);
    p->setFont(f);
    p->setPen(primaryPen);
    p->drawText(QRect(0, 1, 64, 64), Qt::AlignCenter, QStringLiteral("T"));
    p->setPen(accentPen);
    p->drawLine(20, 46, 44, 46);
    return;
  }
  if (name == QLatin1String("image")) {
    p->setPen(primaryPen);
    p->drawRoundedRect(14, 20, 36, 27, 4, 4);
    p->setPen(accentPen);
    QPainterPath mountain;
    mountain.moveTo(16, 42);
    mountain.lineTo(27, 30);
    mountain.lineTo(35, 36);
    mountain.lineTo(47, 24);
    mountain.lineTo(48, 42);
    p->drawPath(mountain);
    drawAccentDot(41, 24, 3.0);
    return;
  }
  if (name == QLatin1String("hand")) {
    QPainterPath hand;
    hand.moveTo(29, 41);
    hand.cubicTo(23, 39, 20, 32, 22, 25);
    hand.cubicTo(23, 19, 29, 16, 33, 20);
    hand.lineTo(35, 14);
    hand.cubicTo(39, 12, 43, 16, 43, 22);
    hand.lineTo(43, 28);
    hand.cubicTo(46, 34, 40, 42, 32, 44);
    hand.cubicTo(27, 44, 24, 43, 29, 41);
    p->setPen(primaryPen);
    p->setBrush(accentSoft);
    p->drawPath(hand);
    p->setPen(accentPen);
    p->drawLine(29, 23, 29, 32);
    p->drawLine(34, 21, 34, 31);
    return;
  }
  if (name == QLatin1String("undo")) {
    QPainterPath arc;
    arc.moveTo(48, 27);
    arc.cubicTo(34, 13, 12, 20, 17, 38);
    p->setPen(primaryPen);
    p->drawPath(arc);
    p->setPen(accentPen);
    p->drawLine(17, 38, 11, 32);
    p->drawLine(11, 32, 22, 30);
    return;
  }
  if (name == QLatin1String("redo")) {
    QPainterPath arc;
    arc.moveTo(16, 27);
    arc.cubicTo(30, 13, 52, 20, 47, 38);
    p->setPen(primaryPen);
    p->drawPath(arc);
    p->setPen(accentPen);
    p->drawLine(47, 38, 53, 32);
    p->drawLine(53, 32, 42, 30);
    return;
  }
  if (name == QLatin1String("more")) {
    // Drei vertikale Punkte (Material "more vertical") - oberer + unterer in
    // primary, mittlerer in Accent, damit es zur Toolbar-Palette passt.
    p->setPen(Qt::NoPen);
    p->setBrush(primary);
    p->drawEllipse(QPointF(32, 18), 3.4, 3.4);
    p->drawEllipse(QPointF(32, 46), 3.4, 3.4);
    p->setBrush(accent);
    p->drawEllipse(QPointF(32, 32), 3.4, 3.4);
    return;
  }
  // Fallback: single-character / unknown (legacy)
  QFont f = p->font();
  f.setPixelSize(28);
  p->setFont(f);
  p->drawText(QRect(0, 0, 64, 64), Qt::AlignCenter, name);
}

} // namespace

// Shared entry point so other widgets (BloomMenu petals) can reuse the exact
// same glyph set the toolbar buttons draw.
void blopDrawToolbarGlyph64(QPainter *p, const QString &name,
                            const QColor &color) {
  drawToolbarGlyph64(p, name, color);
}

// =============================================================================
// v3.16.0: Active-Indicator Pill
// Small 28dp x 3dp accent-Lila pill that slides horizontally under the
// currently active tool button, Material-3-style. Pure paint, no logic.
// =============================================================================
class ActiveIndicatorPill : public QWidget {
public:
  explicit ActiveIndicatorPill(QWidget *parent, const QColor &accent)
      : QWidget(parent), m_accent(accent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
  }
  void setAccent(const QColor &c) {
    m_accent = c;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(m_accent);
    const qreal r = std::min(width(), height()) / 2.0;
    p.drawRoundedRect(rect(), r, r);
  }

private:
  QColor m_accent;
};

// =============================================================================
// v3.16.0: Morph-Tray container
// Custom-painted container that holds the tool settings widgets and visually
// continues the notch by having square top corners and rounded bottom
// corners. Background is the same translucent indigo as the toolbar pill.
// =============================================================================
class MorphTray : public QWidget {
public:
  explicit MorphTray(QWidget *parent = nullptr) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
  }

  // v3.16.1: alpha is applied directly in paintEvent instead of via
  // QGraphicsOpacityEffect, because QGraphicsOpacityEffect rasterises the
  // entire widget tree into an offscreen pixmap every frame on Windows -
  // combined with the simultaneous geometry animation this caused a very
  // visible stutter when the tray opened. Painting alpha into the fill
  // ourselves keeps the geometry animation on the cheap path.
  void setAlpha(double a) {
    a = qBound(0.0, a, 1.0);
    if (qFuzzyCompare(a + 1.0, m_alpha + 1.0))
      return;
    m_alpha = a;
    update();
  }
  double alpha() const { return m_alpha; }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    const qreal br = UiScale::dp(24);
    QRectF r = rect();
    path.moveTo(r.topLeft());
    path.lineTo(r.topRight());
    path.lineTo(r.right(), r.bottom() - br);
    path.quadTo(r.right(), r.bottom(), r.right() - br, r.bottom());
    path.lineTo(r.left() + br, r.bottom());
    path.quadTo(r.left(), r.bottom(), r.left(), r.bottom() - br);
    path.closeSubpath();
    // v3.16.1: match BlopStyle::surfaceBg / surfaceBorder so MorphTray looks
    // identical to all other overlays.
    QColor fill(28, 30, 46, qRound(240 * m_alpha));
    p.fillPath(path, fill);
    QColor borderC(124, 92, 252, qRound(56 * m_alpha));
    QPen border(borderC, 1.0);
    p.setPen(border);
    p.drawPath(path);
  }

private:
  double m_alpha{1.0};
};

// =============================================================================
// 1. ToolbarBtn Implementation
// =============================================================================
ToolbarBtn::ToolbarBtn(const QString &iconName, QWidget *parent)
    : QWidget(parent), m_iconName(iconName) {
  setBtnSize(UiScale::dp(40));
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_AcceptTouchEvents, true);
  // v3.16.0: let the parent toolbar's painted notch (and the sliding
  // active-indicator pill that sits below the active button) show through
  // the button. Without translucency Qt fills the rectangle with the palette
  // window color, which hides the indicator's bottom slice and the notch's
  // rounded corners on small button cells.
  setAttribute(Qt::WA_TranslucentBackground, true);
}

void ToolbarBtn::setHoldProgress(double v) {
  if (qFuzzyCompare(v + 1.0, m_holdProgress + 1.0))
    return;
  m_holdProgress = v;
  update();
  if (m_pressing && !m_longPressTriggered && v >= 1.0) {
    m_longPressTriggered = true;
    emit longPressed();
  }
}
void ToolbarBtn::setBtnSize(int s) {
  if (m_size != s) {
    m_size = s;
    setFixedSize(s, s);
    update();
  }
}
void ToolbarBtn::setIcon(const QString &name) {
  m_iconName = name;
  update();
}
void ToolbarBtn::setDrawFloatingBg(bool draw) {
  m_drawFloatingBg = draw;
  update();
}
void ToolbarBtn::setActive(bool active) {
  // v3.16.1: full early-out (no update()) so that ModernToolbar::setToolMode's
  // sweeping setActive(false) over all 11 buttons is essentially free for the
  // ones that were already inactive. Previously every miss still scheduled a
  // repaint, which added up to dozens of paints per tool change.
  if (m_active == active)
    return;
  m_active = active;

  // v3.16.0: animate the lift offset. Active -> lift up ~6dp with OutBack
  // overshoot. Inactive -> settle back down with OutCubic. The animation
  // pointer is parented to this widget; QPointer prevents dangling.
  if (!m_liftAnim) {
    m_liftAnim = new QPropertyAnimation(this, "liftOffset", this);
  } else {
    m_liftAnim->stop();
  }
  m_liftAnim->setStartValue(m_liftOffset);
  if (active) {
    m_liftAnim->setEndValue((double)UiScale::dp(6));
    m_liftAnim->setDuration(260);
    m_liftAnim->setEasingCurve(QEasingCurve::OutBack);
  } else {
    m_liftAnim->setEndValue(0.0);
    m_liftAnim->setDuration(200);
    m_liftAnim->setEasingCurve(QEasingCurve::OutCubic);
  }
  m_liftAnim->start();
  update();
}
void ToolbarBtn::mousePressEvent(QMouseEvent *e) {
#if BLOP_TOOLBAR_LONGPRESS
  if (e->button() == Qt::LeftButton || e->button() == Qt::NoButton) {
    m_pressing = true;
    m_longPressTriggered = false;
    m_holdProgress = 0.0;
    if (!m_holdAnim) {
      m_holdAnim = new QPropertyAnimation(this, "holdProgress", this);
      m_holdAnim->setEasingCurve(QEasingCurve::Linear);
    } else {
      m_holdAnim->stop();
    }
    m_holdAnim->setStartValue(0.0);
    m_holdAnim->setEndValue(1.0);
    m_holdAnim->setDuration(700);
    m_holdAnim->start();
    update();
    e->accept();
    return;
  }
  QWidget::mousePressEvent(e);
#else
  if (e->button() == Qt::LeftButton || e->button() == Qt::NoButton) {
    BlopRipple::spawn(this, mapToGlobal(rect().center()), m_accentColor);
    emit clicked();
  } else
    QWidget::mousePressEvent(e);
#endif
}
void ToolbarBtn::mouseReleaseEvent(QMouseEvent *e) {
#if BLOP_TOOLBAR_LONGPRESS
  const bool inside = rect().contains(e->pos());
  const bool shouldClick = m_pressing && !m_longPressTriggered && inside &&
                           (e->button() == Qt::LeftButton ||
                            e->button() == Qt::NoButton);
  if (m_holdAnim)
    m_holdAnim->stop();
  m_pressing = false;
  m_longPressTriggered = false;
  m_holdProgress = 0.0;
  update();
  if (shouldClick) {
    BlopRipple::spawn(this, mapToGlobal(rect().center()), m_accentColor);
    emit clicked();
    e->accept();
    return;
  }
  QWidget::mouseReleaseEvent(e);
#else
  QWidget::mouseReleaseEvent(e);
#endif
}
void ToolbarBtn::enterEvent(QEnterEvent *) {
  m_hover = true;
  update();
}
void ToolbarBtn::leaveEvent(QEvent *) {
  m_hover = false;
  update();
}
void ToolbarBtn::animateSelect() {
  QPropertyAnimation *anim = new QPropertyAnimation(this, "animScale");
  anim->setDuration(300);
  anim->setKeyValueAt(0, 1.0);
  anim->setKeyValueAt(0.5, 1.3);
  anim->setKeyValueAt(1, 1.0);
  anim->setEasingCurve(QEasingCurve::OutBack);
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToolbarBtn::triggerClick() {
  BlopRipple::spawn(this, mapToGlobal(rect().center()), m_accentColor);
  animateSelect();
  emit clicked();
}
void ToolbarBtn::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  if (m_drawFloatingBg) {
    p.setBrush(QColor(30, 28, 52, 245));
    QColor ring = m_accentColor;
    ring.setAlpha(60);
    p.setPen(QPen(ring, 1));
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));
  }

  const int w = width();
  const int h = height();
  const int r = m_size / 2 - 4;

  // v3.16.0: while the active button is lifted, paint a soft shadow at the
  // original (un-lifted) position to keep the visual link to the pill.
  if (m_active && m_liftOffset > 0.5) {
    const QPointF shadowCenter(w / 2.0, h / 2.0 + 1.0);
    const qreal shadowR = r + 4.0;
    QRadialGradient grad(shadowCenter, shadowR);
    grad.setColorAt(0.0, QColor(0, 0, 0, 80));
    grad.setColorAt(0.7, QColor(0, 0, 0, 28));
    grad.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(shadowCenter, shadowR, shadowR);
  }

  p.save();
  if (m_liftOffset > 0.0)
    p.translate(0.0, -m_liftOffset);

  if (m_active) {
    p.setBrush(m_accentColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect().center(), r, r);
  } else if (m_hover) {
    p.setBrush(QColor(140, 156, 255, 64));
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect().center(), r, r);
  }

  const QColor fg =
      m_active ? QColor(255, 255, 255, 255) : QColor(236, 240, 252, 250);
  p.save();
  p.translate(w / 2.0, h / 2.0);
  p.scale(m_animScale, m_animScale);
  const qreal g = qMin(w, h) / 64.0;
  p.scale(g, g);
  p.translate(-32, -32);
  drawToolbarGlyph64(&p, m_iconName, fg);
  p.restore();

  if (m_active && m_holdProgress > 0.0) {
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(216, 213, 255, 220), 3);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF arcRect = rect().adjusted(3, 3, -3, -3);
    p.drawArc(arcRect, 90 * 16, -m_holdProgress * 360.0 * 16.0);
  }
  p.restore();
}

// =============================================================================
// 2. Tool Preview Widget
// =============================================================================
class ToolPreviewWidget : public QWidget {
public:
  ToolPreviewWidget(QWidget *parent) : QWidget(parent) {
    setFixedHeight(UiScale::dp(50));
    setAttribute(Qt::WA_TranslucentBackground);
  }
  void updateConfig(ToolMode m, ToolConfig c) {
    m_mode = m;
    m_config = c;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect paperRect = rect().adjusted(5, 5, -5, -5);
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(paperRect, 6, 6);

    if (m_mode == ToolMode::Highlighter) {
      p.setPen(QPen(QColor(60, 60, 60)));
      p.setFont(QFont("Arial", 12, QFont::Bold));
      p.drawText(paperRect, Qt::AlignCenter, "Text Text");
      p.setCompositionMode(QPainter::CompositionMode_Multiply);
    }

    if (m_mode == ToolMode::Shape) {
      QColor sh = m_config.penColor;
      sh.setAlphaF(m_config.opacity);
      QPen pshape;
      pshape.setColor(sh);
      pshape.setWidth(qMax(2, m_config.penWidth));
      pshape.setCapStyle(Qt::FlatCap);
      pshape.setJoinStyle(Qt::MiterJoin);
      p.setPen(pshape);
      p.setBrush(Qt::NoBrush);
      QRectF rr = paperRect.adjusted(14, 12, -14, -12);
      p.drawPath(ShapeTool::buildShapePath(rr, m_config));
      return;
    }

    QColor c = m_config.penColor;
    double alpha = m_config.opacity;
    int penW = m_config.penWidth;
    QPen pen;
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    if (m_mode == ToolMode::Pencil) {
      double hardnessFactor = (m_config.hardness / 100.0) * 0.5;
      alpha = alpha * (1.0 - hardnessFactor);
      c.setAlphaF(std::max(0.05, alpha));
      int noiseDensity = 50;
      if (m_config.texture == "Fein")
        noiseDensity = 25;
      else if (m_config.texture == "Grob")
        noiseDensity = 65;
      else if (m_config.texture == "Kohle") {
        noiseDensity = 80;
        penW = penW * 1.2;
      }
      QPixmap noisePx = getNoiseTexture(c, noiseDensity);
      pen.setBrush(QBrush(noisePx));
      pen.setStyle(Qt::SolidLine);
      pen.setWidth(penW);
    } else if (m_mode == ToolMode::Highlighter) {
      alpha = 0.4 * alpha;
      c.setAlphaF(std::max(0.1, alpha));
      penW = std::max(10, penW * 3);
      if (m_config.tipType == HighlighterTip::Chisel) {
        pen.setCapStyle(Qt::FlatCap);
        pen.setJoinStyle(Qt::MiterJoin);
      }
      pen.setColor(c);
      pen.setStyle(Qt::SolidLine);
      pen.setWidth(penW);
    } else {
      c.setAlphaF(alpha);
      pen.setColor(c);
      pen.setStyle(Qt::SolidLine);
      pen.setWidth(penW);
    }
    p.setPen(pen);
    int w = width();
    int h = height();
    QPainterPath path;
    if (m_mode == ToolMode::Highlighter) {
      path.moveTo(20, h / 2);
      path.lineTo(w - 20, h / 2);
    } else {
      path.moveTo(15, h / 2);
      path.cubicTo(w / 3, 10, 2 * w / 3, h - 10, w - 15, h / 2);
    }
    p.drawPath(path);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  }

private:
  ToolMode m_mode{ToolMode::Pen};
  ToolConfig m_config;
};

#ifdef Q_OS_ANDROID
namespace {
class AndroidToolSettingsScrim : public QWidget {
public:
  explicit AndroidToolSettingsScrim(QWidget *layer) : QWidget(layer) {
    setGeometry(layer->rect());
    setStyleSheet(QStringLiteral("background:rgba(0,0,0,0.42);"));
  }
protected:
  void resizeEvent(QResizeEvent *) override {
    if (parentWidget())
      setGeometry(parentWidget()->rect());
  }
  void mousePressEvent(QMouseEvent *e) override {
    e->accept();
    if (parentWidget())
      parentWidget()->deleteLater();
  }
};
} // namespace
#endif

// Click-outside-to-dismiss scrim for the desktop morph tray. Transparent
// (no dimming) but catches the press and closes its parent layer.
namespace {
class DesktopTraySCrim : public QWidget {
public:
  explicit DesktopTraySCrim(QWidget *layer) : QWidget(layer) {
    setGeometry(layer->rect());
    setAttribute(Qt::WA_TranslucentBackground);
  }
protected:
  void resizeEvent(QResizeEvent *) override {
    if (parentWidget())
      setGeometry(parentWidget()->rect());
  }
  void mousePressEvent(QMouseEvent *e) override {
    e->accept();
    if (parentWidget())
      parentWidget()->deleteLater();
  }
};
} // namespace

// =============================================================================
// 3. Settings sheet content (desktop: wrapped in QDialog; Android: embedded in central widget)
// =============================================================================
class ToolSettingsContent : public QWidget {
public:
  ToolSettingsContent(ToolMode mode, ToolConfig config, QWidget *parent)
      : QWidget(parent), m_mode(mode), m_config(config) {
    // v3.16.0: content widget is always transparent now; the surrounding
    // MorphTray paints the notch-continuation background so the inner
    // content does not need its own card. This avoids the double-card
    // look that the Android stylesheet previously produced.
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(BlopTheme::themed(
        "QLabel { color: #DDD; font-weight: bold; font-size: 11px; background: "
        "transparent; }"
        "QSlider::groove:horizontal { height: 4px; background: #444; "
        "border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #6c5ce7; border-radius: "
        "2px; }"
        "QSlider::handle:horizontal { background: #FFFFFF; width: 14px; "
        "height: 14px; margin: -5px 0; border-radius: 7px; }"
        "QCheckBox { color: #DDD; background: transparent; }"
        "QPushButton { background-color: #444; color: #BBB; border: none; "
        "border-radius: 4px; padding: 5px; }"
        "QPushButton:checked { background-color: #6c5ce7; color: white; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton#DangerBtn { background-color: #A00; color: white; }"
        "QPushButton#DangerBtn:hover { background-color: #C00; }"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(12);

    if (m_mode != ToolMode::Eraser && m_mode != ToolMode::Lasso) {
      m_previewWidget = new ToolPreviewWidget(this);
      m_previewWidget->updateConfig(m_mode, m_config);
      layout->addWidget(m_previewWidget);
    } else {
      m_previewWidget = nullptr;
    }

    if (m_mode == ToolMode::Pen || m_mode == ToolMode::Pencil ||
        m_mode == ToolMode::Highlighter) {
      layout->addWidget(new QLabel("Dicke:"));
      QSlider *slSize = new QSlider(Qt::Horizontal);
      slSize->setRange(1, 50);
      slSize->setValue(m_config.penWidth);
      connect(slSize, &QSlider::valueChanged, [this](int v) {
        m_config.penWidth = v;
        updatePreview();
        apply();
      });
      layout->addWidget(slSize);

      layout->addWidget(new QLabel("Deckkraft:"));
      QSlider *slOpacity = new QSlider(Qt::Horizontal);
      slOpacity->setRange(10, 100);
      slOpacity->setValue(m_config.opacity * 100);
      connect(slOpacity, &QSlider::valueChanged, [this](int v) {
        m_config.opacity = v / 100.0;
        updatePreview();
        apply();
      });
      layout->addWidget(slOpacity);

      if (m_mode == ToolMode::Pen) {
        layout->addWidget(new QLabel("Glättung:"));
        QSlider *slSmooth = new QSlider(Qt::Horizontal);
        slSmooth->setRange(0, 100);
        slSmooth->setValue(m_config.smoothing);
        connect(slSmooth, &QSlider::valueChanged, [this](int v) {
          m_config.smoothing = v;
          apply();
        });
        layout->addWidget(slSmooth);
        QCheckBox *chkPressure = new QCheckBox("Drucksensitivität");
        chkPressure->setChecked(m_config.pressureSensitivity);
        connect(chkPressure, &QCheckBox::toggled, [this](bool c) {
          m_config.pressureSensitivity = c;
          apply();
        });
        layout->addWidget(chkPressure);

        layout->addWidget(new QLabel("Halten → Form"));
        layout->addWidget(new QLabel("Empfindlichkeit:"));
        QSlider *slHoldPen = new QSlider(Qt::Horizontal);
        slHoldPen->setRange(0, 100);
        slHoldPen->setValue(m_config.holdShapeSensitivity);
        connect(slHoldPen, &QSlider::valueChanged, [this](int v) {
          m_config.holdShapeSensitivity = v;
          apply();
        });
        layout->addWidget(slHoldPen);
        layout->addWidget(new QLabel("Haltezeit (ms):"));
        QSlider *slDelayPen = new QSlider(Qt::Horizontal);
        slDelayPen->setRange(200, 900);
        slDelayPen->setValue(m_config.holdStillDelayMs);
        connect(slDelayPen, &QSlider::valueChanged, [this](int v) {
          m_config.holdStillDelayMs = v;
          apply();
        });
        layout->addWidget(slDelayPen);
        QCheckBox *chkHC = new QCheckBox("Kreis-Erkennung");
        chkHC->setChecked(m_config.holdEnableCircle);
        connect(chkHC, &QCheckBox::toggled, [this](bool on) {
          m_config.holdEnableCircle = on;
          apply();
        });
        layout->addWidget(chkHC);
        QCheckBox *chkHT = new QCheckBox("Dreieck-Erkennung");
        chkHT->setChecked(m_config.holdEnableTriangle);
        connect(chkHT, &QCheckBox::toggled, [this](bool on) {
          m_config.holdEnableTriangle = on;
          apply();
        });
        layout->addWidget(chkHT);
      } else if (m_mode == ToolMode::Pencil) {
        layout->addWidget(new QLabel("Härte:"));
        QSlider *slHardness = new QSlider(Qt::Horizontal);
        slHardness->setRange(0, 100);
        slHardness->setValue(m_config.hardness);
        connect(slHardness, &QSlider::valueChanged, [this](int v) {
          m_config.hardness = v;
          updatePreview();
          apply();
        });
        layout->addWidget(slHardness);
        QCheckBox *chkTilt = new QCheckBox("Neigung");
        chkTilt->setChecked(m_config.tiltShading);
        connect(chkTilt, &QCheckBox::toggled, [this](bool c) {
          m_config.tiltShading = c;
          apply();
        });
        layout->addWidget(chkTilt);
        layout->addWidget(new QLabel("Textur:"));
        QHBoxLayout *texLayout = new QHBoxLayout;
        texLayout->setSpacing(5);
        QButtonGroup *texGroup = new QButtonGroup(this);
        QStringList textures = {"Fein", "Grob", "Kohle"};
        for (const QString &tex : textures) {
          QPushButton *btn = new QPushButton(tex);
          btn->setCheckable(true);
          btn->setFixedHeight(25);
          if (m_config.texture == tex)
            btn->setChecked(true);
          texGroup->addButton(btn);
          texLayout->addWidget(btn);
          connect(btn, &QPushButton::clicked, [this, tex]() {
            m_config.texture = tex;
            updatePreview();
            apply();
          });
        }
        layout->addLayout(texLayout);
      } else if (m_mode == ToolMode::Highlighter) {
        QCheckBox *chkSmart = new QCheckBox("Smart Line");
        chkSmart->setChecked(m_config.smartLine);
        connect(chkSmart, &QCheckBox::toggled, [this](bool c) {
          m_config.smartLine = c;
          apply();
        });
        layout->addWidget(chkSmart);
        QCheckBox *chkBehind = new QCheckBox("Hinter Text");
        chkBehind->setChecked(m_config.drawBehind);
        connect(chkBehind, &QCheckBox::toggled, [this](bool c) {
          m_config.drawBehind = c;
          apply();
        });
        layout->addWidget(chkBehind);
        layout->addWidget(new QLabel("Spitze:"));
        QHBoxLayout *tipLayout = new QHBoxLayout;
        tipLayout->setSpacing(5);
        QButtonGroup *tipGroup = new QButtonGroup(this);
        auto addTipBtn = [&](QString text, HighlighterTip type) {
          QPushButton *btn = new QPushButton(text);
          btn->setCheckable(true);
          btn->setFixedHeight(25);
          if (m_config.tipType == type)
            btn->setChecked(true);
          tipGroup->addButton(btn);
          tipLayout->addWidget(btn);
          connect(btn, &QPushButton::clicked, [this, type]() {
            m_config.tipType = type;
            updatePreview();
            apply();
          });
        };
        addTipBtn("Rund", HighlighterTip::Round);
        addTipBtn("Abgeschrägt", HighlighterTip::Chisel);
        layout->addLayout(tipLayout);
      }
      layout->addWidget(new QLabel("Farbe:"));
      QHBoxLayout *colors = new QHBoxLayout;
      colors->setSpacing(8);
      auto swatchStyle = [](const QColor &c, bool selected) {
        if (selected) {
          return QStringLiteral(
                     "QPushButton { background-color: %1; border-radius: 12px; "
                     "border: 2px solid #6BA3F5; }"
                     "QPushButton:hover { border-color: #8EB8F8; }")
              .arg(c.name());
        }
        return QStringLiteral(
                   "QPushButton { background-color: %1; border-radius: 12px; "
                   "border: 1px solid rgba(15,23,42,180); }"
                   "QPushButton:hover { border-color: rgba(55,65,81,220); }")
            .arg(c.name());
      };
      QList<QPushButton *> quickButtons;
      auto refreshQuickButtons = [this, &quickButtons, swatchStyle]() {
        for (auto *qb : quickButtons) {
          const int qIdx = qb->property("quickIndex").toInt();
          QColor qc = qb->property("quickColor").value<QColor>();
          if (!qc.isValid() && qIdx >= 0 &&
              qIdx < static_cast<int>(m_config.quickColors.size())) {
            qc = m_config.quickColors[qIdx];
          }
          if (!qc.isValid())
            qc = QColor(Qt::black);
          qb->setStyleSheet(swatchStyle(qc, qIdx == m_selectedQuickIndex));
        }
      };
      for (int i = 0; i < static_cast<int>(m_config.quickColors.size()); ++i) {
        const QColor c = m_config.quickColors[i].isValid()
                             ? m_config.quickColors[i]
                             : QColor(Qt::black);
        QPushButton *b = new QPushButton;
        b->setFixedSize(24, 24);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("quickIndex", i);
        b->setProperty("quickColor", c);
        quickButtons.append(b);
        colors->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, b, &refreshQuickButtons]() {
          const int idx = b->property("quickIndex").toInt();
          QColor c = b->property("quickColor").value<QColor>();
          if (!c.isValid() && idx >= 0 &&
              idx < static_cast<int>(m_config.quickColors.size())) {
            c = m_config.quickColors[idx];
          }
          if (!c.isValid())
            c = QColor(Qt::black);

          // 1. Tap/Klick: nur auswählen (mit Highlight), 2. Tap/Klick: bearbeiten.
          if (m_selectedQuickIndex != idx) {
            m_selectedQuickIndex = idx;
            m_config.penColor = c;
            refreshQuickButtons();
            updatePreview();
            apply();
            return;
          }

          QWidget *overlayHost = parentWidget();
          if (!overlayHost)
            overlayHost = this->window();
          hide();
          const bool ok = showColorPickerOverlay(
              overlayHost, &c, QStringLiteral("Schnellfarbe bearbeiten"));
          show();
          raise();
#ifndef Q_OS_ANDROID
          activateWindow();
#endif
          if (ok) {
            b->setProperty("quickColor", c);
            if (idx >= 0 && idx < static_cast<int>(m_config.quickColors.size()))
              m_config.quickColors[idx] = c;
            m_config.penColor = c;
            refreshQuickButtons();
            updatePreview();
            apply();
          }
        });
      }
      refreshQuickButtons();
      QPushButton *btnWheel = new QPushButton("...");
      btnWheel->setFixedSize(24, 24);
      btnWheel->setStyleSheet(BlopTheme::themed(
          "background-color: #2D2D30; color: #F4F2FF; "
          "border-radius: 12px; border: 1px solid #666;"));
      connect(btnWheel, &QPushButton::clicked, [this]() {
        QColor c = m_config.penColor;
        // Nicht this->window(): Das wäre das kleine Popup selbst; Hauptfenster
        // (Parent-Widget / Dialog) für Vollbild-Overlay.
        QWidget *overlayHost = parentWidget();
        if (!overlayHost)
          overlayHost = this->window();
        // Qt::Popup liegt immer über Kind-Widgets des Hauptfensters — Overlay
        // würde sonst hinter den Tool-Einstellungen hängen.
        hide();
        const bool ok =
            showColorPickerOverlay(overlayHost, &c, QStringLiteral("Stiftfarbe"));
        show();
        raise();
#ifndef Q_OS_ANDROID
        activateWindow();
#endif
        if (ok) {
          m_config.penColor = c;
          updatePreview();
          apply();
        }
      });
      colors->addWidget(btnWheel);
      colors->addStretch();
      layout->addLayout(colors);
    } else if (m_mode == ToolMode::Eraser) {
      layout->addWidget(new QLabel("Modus:"));
      QHBoxLayout *modeLayout = new QHBoxLayout;
      modeLayout->setSpacing(5);
      QButtonGroup *modeGroup = new QButtonGroup(this);
      QPushButton *btnPixel = new QPushButton("Pixel");
      btnPixel->setCheckable(true);
      btnPixel->setFixedHeight(30);
      QPushButton *btnObject = new QPushButton("Objekt");
      btnObject->setCheckable(true);
      btnObject->setFixedHeight(30);
      if (m_config.eraserMode == EraserMode::Pixel)
        btnPixel->setChecked(true);
      else
        btnObject->setChecked(true);
      modeGroup->addButton(btnPixel);
      modeGroup->addButton(btnObject);
      modeLayout->addWidget(btnPixel);
      modeLayout->addWidget(btnObject);
      layout->addLayout(modeLayout);

      QWidget *sizeContainer = new QWidget;
      QVBoxLayout *sizeLayout = new QVBoxLayout(sizeContainer);
      sizeLayout->setContentsMargins(0, 0, 0, 0);
      sizeLayout->addWidget(new QLabel("Größe:"));
      QSlider *slEraserSize = new QSlider(Qt::Horizontal);
      slEraserSize->setRange(5, 100);
      slEraserSize->setValue(m_config.penWidth);
      sizeLayout->addWidget(slEraserSize);
      layout->addWidget(sizeContainer);
      auto updateVisibility = [=]() {
        sizeContainer->setVisible(m_config.eraserMode == EraserMode::Pixel);
      };
      updateVisibility();
      connect(btnPixel, &QPushButton::clicked, [=]() {
        m_config.eraserMode = EraserMode::Pixel;
        updateVisibility();
        apply();
      });
      connect(btnObject, &QPushButton::clicked, [=]() {
        m_config.eraserMode = EraserMode::Object;
        updateVisibility();
        apply();
      });
      connect(slEraserSize, &QSlider::valueChanged, [this](int v) {
        m_config.penWidth = v;
        apply();
      });

      QCheckBox *chkKeepInk = new QCheckBox("Nur Marker löschen");
      chkKeepInk->setChecked(m_config.eraserKeepInk);
      connect(chkKeepInk, &QCheckBox::toggled, [this](bool c) {
        m_config.eraserKeepInk = c;
        apply();
      });
      layout->addWidget(chkKeepInk);

      layout->addSpacing(10);
      QPushButton *btnClear = new QPushButton("Seite leeren");
      btnClear->setObjectName("DangerBtn");
      btnClear->setFixedHeight(35);
      btnClear->setCursor(Qt::PointingHandCursor);
      connect(btnClear, &QPushButton::clicked, [this]() {
        if (QMessageBox::question(this, "Seite leeren", "Alles löschen?",
                                  QMessageBox::Yes | QMessageBox::No) ==
            QMessageBox::Yes) {
          QGraphicsView *view = nullptr;
          for (QWidget *w : QApplication::topLevelWidgets()) {
            view = w->findChild<QGraphicsView *>();
            if (view)
              break;
          }
          if (view && view->scene()) {
            QList<QGraphicsItem *> items = view->scene()->items();
            for (auto *item : items) {
              if (dynamic_cast<QGraphicsPathItem *>(item)) {
                view->scene()->removeItem(item);
                delete item;
              }
            }
            view->viewport()->update();
          }
        }
      });
      layout->addWidget(btnClear);
    } else if (m_mode == ToolMode::Lasso) {
      layout->addWidget(new QLabel("Auswahl-Modus:"));
      QHBoxLayout *modeLayout = new QHBoxLayout;
      modeLayout->setSpacing(5);
      QButtonGroup *modeGroup = new QButtonGroup(this);
      QPushButton *btnFree = new QPushButton("Freihand");
      btnFree->setCheckable(true);
      btnFree->setFixedHeight(30);
      QPushButton *btnRect = new QPushButton("Rechteck");
      btnRect->setCheckable(true);
      btnRect->setFixedHeight(30);
      if (m_config.lassoMode == LassoMode::Freehand)
        btnFree->setChecked(true);
      else
        btnRect->setChecked(true);
      modeGroup->addButton(btnFree);
      modeGroup->addButton(btnRect);
      modeLayout->addWidget(btnFree);
      modeLayout->addWidget(btnRect);
      layout->addLayout(modeLayout);
      connect(btnFree, &QPushButton::clicked, [this]() {
        m_config.lassoMode = LassoMode::Freehand;
        apply();
      });
      connect(btnRect, &QPushButton::clicked, [this]() {
        m_config.lassoMode = LassoMode::Rectangle;
        apply();
      });

      QCheckBox *chkAspect = new QCheckBox("Seitenverhältnis sperren");
      chkAspect->setChecked(m_config.aspectLock);
      connect(chkAspect, &QCheckBox::toggled, [this](bool c) {
        m_config.aspectLock = c;
        apply();
      });
      layout->addWidget(chkAspect);
    } else if (m_mode == ToolMode::Ruler) {
      // --- RULER SETTINGS ---
      layout->addWidget(new QLabel("Einheit:"));
      QHBoxLayout *unitLayout = new QHBoxLayout;
      unitLayout->setSpacing(5);
      QButtonGroup *unitGroup = new QButtonGroup(this);
      struct UnitEntry { QString label; RulerUnit unit; };
      QList<UnitEntry> units = {{"px", RulerUnit::Pixel}, {"cm", RulerUnit::Centimeter}, {"Zoll", RulerUnit::Inch}};
      for (const UnitEntry &u : units) {
        QPushButton *btn = new QPushButton(u.label);
        btn->setCheckable(true);
        btn->setFixedHeight(30);
        if (m_config.rulerUnit == u.unit) btn->setChecked(true);
        unitGroup->addButton(btn);
        unitLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, [this, u]() {
          m_config.rulerUnit = u.unit;
          apply();
        });
      }
      layout->addLayout(unitLayout);

      QCheckBox *chkSnap = new QCheckBox("Einrasten");
      chkSnap->setChecked(m_config.rulerSnap);
      connect(chkSnap, &QCheckBox::toggled, [this](bool v) {
        m_config.rulerSnap = v;
        apply();
      });
      layout->addWidget(chkSnap);

      QCheckBox *chkCompass = new QCheckBox("Rundes Lineal");
      chkCompass->setChecked(m_config.compassMode);
      connect(chkCompass, &QCheckBox::toggled, [this](bool v) {
        m_config.compassMode = v;
        apply();
      });
      layout->addWidget(chkCompass);

      QCheckBox *chkInfinite = new QCheckBox("Unendliches Lineal");
      chkInfinite->setChecked(m_config.infiniteRuler);
      connect(chkInfinite, &QCheckBox::toggled, [this](bool v) {
        m_config.infiniteRuler = v;
        apply();
      });
      layout->addWidget(chkInfinite);
    } else if (m_mode == ToolMode::Shape) {
      layout->addWidget(new QLabel("Dicke:"));
      QSlider *slSize = new QSlider(Qt::Horizontal);
      slSize->setRange(1, 50);
      slSize->setValue(m_config.penWidth);
      connect(slSize, &QSlider::valueChanged, [this](int v) {
        m_config.penWidth = v;
        updatePreview();
        apply();
      });
      layout->addWidget(slSize);

      layout->addWidget(new QLabel("Deckkraft:"));
      QSlider *slOpacity = new QSlider(Qt::Horizontal);
      slOpacity->setRange(10, 100);
      slOpacity->setValue(static_cast<int>(m_config.opacity * 100));
      connect(slOpacity, &QSlider::valueChanged, [this](int v) {
        m_config.opacity = v / 100.0;
        updatePreview();
        apply();
      });
      layout->addWidget(slOpacity);

      layout->addWidget(new QLabel("Farbe:"));
      QHBoxLayout *colors = new QHBoxLayout;
      colors->setSpacing(8);
      for (int i = 0; i < static_cast<int>(m_config.quickColors.size()); ++i) {
        const QColor col = m_config.quickColors[i].isValid() ? m_config.quickColors[i] : QColor(Qt::black);
        QPushButton *b = new QPushButton;
        b->setFixedSize(24, 24);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: %1; border-radius: 12px; border: 1px solid rgba(15,23,42,180); }")
                             .arg(col.name()));
        colors->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i, col]() {
          if (i >= 0 && i < static_cast<int>(m_config.quickColors.size()))
            m_config.quickColors[i] = col;
          m_config.penColor = col;
          updatePreview();
          apply();
        });
      }
      QPushButton *btnWheel = new QPushButton("...");
      btnWheel->setFixedSize(24, 24);
      btnWheel->setStyleSheet(BlopTheme::themed(
          "background-color: #2D2D30; color: #F4F2FF; "
          "border-radius: 12px; border: 1px solid #666;"));
      connect(btnWheel, &QPushButton::clicked, [this]() {
        QColor c = m_config.penColor;
        QWidget *overlayHost = parentWidget();
        if (!overlayHost)
          overlayHost = this->window();
        hide();
        const bool ok =
            showColorPickerOverlay(overlayHost, &c, QStringLiteral("Formfarbe"));
        show();
        raise();
#ifndef Q_OS_ANDROID
        activateWindow();
#endif
        if (ok) {
          m_config.penColor = c;
          updatePreview();
          apply();
        }
      });
      colors->addWidget(btnWheel);
      colors->addStretch();
      layout->addLayout(colors);

      layout->addWidget(new QLabel("Form:"));
      QHBoxLayout *kindRow = new QHBoxLayout;
      kindRow->setSpacing(4);
      QButtonGroup *kindGroup = new QButtonGroup(this);
      struct KindEntry {
        QString label;
        ShapeToolKind kind;
      };
      const QList<KindEntry> kindList = {
          {QStringLiteral("Rechteck"), ShapeToolKind::Rectangle},
          {QStringLiteral("Kreis"), ShapeToolKind::Circle},
          {QStringLiteral("Achsen"), ShapeToolKind::Axes2D},
          {QStringLiteral("Sinus"), ShapeToolKind::SineGraph},
          {QStringLiteral("Graph"), ShapeToolKind::CoordinateGraph}};
      for (const KindEntry &ke : kindList) {
        QPushButton *bk = new QPushButton(ke.label);
        bk->setCheckable(true);
        bk->setFixedHeight(26);
        if (m_config.shapeToolKind == ke.kind)
          bk->setChecked(true);
        kindGroup->addButton(bk);
        kindRow->addWidget(bk);
        connect(bk, &QPushButton::clicked, [this, ke]() {
          m_config.shapeToolKind = ke.kind;
          syncShapeOptionPanels();
          updatePreview();
          apply();
        });
      }
      layout->addLayout(kindRow);

      m_shapeTicksPanel = new QWidget(this);
      QVBoxLayout *ticksLay = new QVBoxLayout(m_shapeTicksPanel);
      ticksLay->setContentsMargins(0, 0, 0, 0);
      ticksLay->setSpacing(4);
      ticksLay->addWidget(new QLabel(QStringLiteral("Achsen: Teilstriche (je Richtung):")));
      QSlider *slTicks = new QSlider(Qt::Horizontal);
      slTicks->setRange(2, 12);
      slTicks->setValue(m_config.shapeAxisTicks);
      connect(slTicks, &QSlider::valueChanged, [this](int v) {
        m_config.shapeAxisTicks = v;
        updatePreview();
        apply();
      });
      ticksLay->addWidget(slTicks);
      layout->addWidget(m_shapeTicksPanel);

      m_shapeSinePanel = new QWidget(this);
      QVBoxLayout *sineLay = new QVBoxLayout(m_shapeSinePanel);
      sineLay->setContentsMargins(0, 0, 0, 0);
      sineLay->setSpacing(6);
      QCheckBox *chkSineFixed =
          new QCheckBox(QStringLiteral("Feste Parameter verwenden"));
      chkSineFixed->setChecked(m_config.shapeSineFixedParams);
      chkSineFixed->setStyleSheet(
          BlopTheme::themed(QStringLiteral("color:#DDD;font-weight:bold;")));
      sineLay->addWidget(chkSineFixed);

      auto *lblSineMode = new QLabel();
      lblSineMode->setWordWrap(true);
      lblSineMode->setStyleSheet(
          BlopTheme::themed(QStringLiteral("color:#888;font-weight:normal;")));
      sineLay->addWidget(lblSineMode);

      m_shapeSineParamsWidget = new QWidget(m_shapeSinePanel);
      QVBoxLayout *sineParamsLay = new QVBoxLayout(m_shapeSineParamsWidget);
      sineParamsLay->setContentsMargins(0, 0, 0, 0);
      sineParamsLay->setSpacing(6);
      auto *sineHelp = new QLabel(
          QStringLiteral(
              "y = Mitte \u2212 (a\u00b745\u00b7sin(b\u00b72\u03c0\u00b7(x\u2212x links)/100 + c) + d\u00b745); "
              "b = volle Perioden je 100 px Breite."));
      sineHelp->setWordWrap(true);
      sineHelp->setStyleSheet(
          BlopTheme::themed(QStringLiteral("color:#bbb;font-weight:normal;")));
      sineParamsLay->addWidget(sineHelp);

      auto mkSpin = [](double minV, double maxV, int decimals, double step) {
        QDoubleSpinBox *s = new QDoubleSpinBox;
        s->setRange(minV, maxV);
        s->setDecimals(decimals);
        s->setSingleStep(step);
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        s->setMinimumWidth(80);
        return s;
      };
      QDoubleSpinBox *spA = mkSpin(0.01, 2.0, 4, 0.05);
      QDoubleSpinBox *spB = mkSpin(0.05, 12.0, 4, 0.1);
      QDoubleSpinBox *spC = mkSpin(-12.57, 12.57, 4, 0.1);
      QDoubleSpinBox *spD = mkSpin(-1.5, 1.5, 4, 0.05);
      spA->setValue(m_config.shapeMathA);
      spB->setValue(m_config.shapeMathB);
      spC->setValue(m_config.shapeMathC);
      spD->setValue(m_config.shapeMathD);

      auto onSineSpin = [this, spA, spB, spC, spD]() {
        m_config.shapeMathA = spA->value();
        m_config.shapeMathB = spB->value();
        m_config.shapeMathC = spC->value();
        m_config.shapeMathD = spD->value();
        updatePreview();
        apply();
      };
      connect(spA, &QDoubleSpinBox::valueChanged, this, onSineSpin);
      connect(spB, &QDoubleSpinBox::valueChanged, this, onSineSpin);
      connect(spC, &QDoubleSpinBox::valueChanged, this, onSineSpin);
      connect(spD, &QDoubleSpinBox::valueChanged, this, onSineSpin);

      QGridLayout *sineGrid = new QGridLayout;
      sineGrid->setColumnStretch(1, 1);
      sineGrid->addWidget(new QLabel(QStringLiteral("a (Amplitude):")), 0, 0);
      sineGrid->addWidget(spA, 0, 1);
      sineGrid->addWidget(new QLabel(QStringLiteral("b (Perioden):")), 1, 0);
      sineGrid->addWidget(spB, 1, 1);
      sineGrid->addWidget(new QLabel(QStringLiteral("c (Phase, rad):")), 2, 0);
      sineGrid->addWidget(spC, 2, 1);
      sineGrid->addWidget(new QLabel(QStringLiteral("d (Offset):")), 3, 0);
      sineGrid->addWidget(spD, 3, 1);
      sineParamsLay->addLayout(sineGrid);
      sineLay->addWidget(m_shapeSineParamsWidget);

      auto applySineModeHint = [lblSineMode](bool fixed) {
        if (fixed) {
          lblSineMode->setText(
              QStringLiteral("Fest: Breiter ziehen zeigt mehr Perioden (b je 100 px); "
                             "Amplitude skaliert nicht mit der Rechteckh\u00f6he."));
        } else {
          lblSineMode->setText(QStringLiteral(
              "Flexibel: Aufziehen skaliert Amplitude und Perioden mit dem Rechteck."));
        }
      };
      applySineModeHint(m_config.shapeSineFixedParams);
      m_shapeSineParamsWidget->setVisible(m_config.shapeSineFixedParams);

      connect(chkSineFixed, &QCheckBox::toggled, this,
              [this, applySineModeHint](bool on) {
                m_config.shapeSineFixedParams = on;
                m_shapeSineParamsWidget->setVisible(on);
                applySineModeHint(on);
                updatePreview();
                apply();
              });

      layout->addWidget(m_shapeSinePanel);

      m_shapeGraphPanel = new QWidget(this);
      QVBoxLayout *graphLay = new QVBoxLayout(m_shapeGraphPanel);
      graphLay->setContentsMargins(0, 0, 0, 0);
      graphLay->setSpacing(6);
      graphLay->addWidget(new QLabel(QStringLiteral("Koordinatensystem")));
      QGridLayout *rangeGrid = new QGridLayout;
      auto mkRange = [](double v) {
        QDoubleSpinBox *s = new QDoubleSpinBox;
        s->setRange(-10000.0, 10000.0);
        s->setDecimals(2);
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        s->setValue(v);
        return s;
      };
      QDoubleSpinBox *sxMin = mkRange(m_config.graphXMin);
      QDoubleSpinBox *sxMax = mkRange(m_config.graphXMax);
      QDoubleSpinBox *syMin = mkRange(m_config.graphYMin);
      QDoubleSpinBox *syMax = mkRange(m_config.graphYMax);
      rangeGrid->addWidget(new QLabel("xMin"), 0, 0); rangeGrid->addWidget(sxMin, 0, 1);
      rangeGrid->addWidget(new QLabel("xMax"), 0, 2); rangeGrid->addWidget(sxMax, 0, 3);
      rangeGrid->addWidget(new QLabel("yMin"), 1, 0); rangeGrid->addWidget(syMin, 1, 1);
      rangeGrid->addWidget(new QLabel("yMax"), 1, 2); rangeGrid->addWidget(syMax, 1, 3);
      graphLay->addLayout(rangeGrid);
      auto onRange = [this, sxMin, sxMax, syMin, syMax]() {
        m_config.graphXMin = sxMin->value();
        m_config.graphXMax = sxMax->value();
        m_config.graphYMin = syMin->value();
        m_config.graphYMax = syMax->value();
        updatePreview();
        apply();
      };
      connect(sxMin, &QDoubleSpinBox::valueChanged, this, onRange);
      connect(sxMax, &QDoubleSpinBox::valueChanged, this, onRange);
      connect(syMin, &QDoubleSpinBox::valueChanged, this, onRange);
      connect(syMax, &QDoubleSpinBox::valueChanged, this, onRange);

      QHBoxLayout *addRow = new QHBoxLayout;
      QLineEdit *edExpr = new QLineEdit;
      edExpr->setPlaceholderText(QStringLiteral("f(x), z.B. sin(x)+x^2 oder y=cos(x)"));
      QPushButton *btnAddExpr = new QPushButton("+");
      btnAddExpr->setFixedWidth(32);
      addRow->addWidget(edExpr, 1);
      addRow->addWidget(btnAddExpr);
      graphLay->addLayout(addRow);

      QListWidget *lst = new QListWidget;
      graphLay->addWidget(lst);
      auto refreshFns = [this, lst]() {
        lst->clear();
        for (int i = 0; i < m_config.graphFunctions.size(); ++i) {
          const auto& f = m_config.graphFunctions[i];
          QString txt = QStringLiteral("%1: %2").arg(i + 1).arg(f.expression);
          if (!f.visible) txt += QStringLiteral(" [hidden]");
          lst->addItem(txt);
        }
        if (m_config.graphSelectedFunction >= 0 && m_config.graphSelectedFunction < lst->count())
          lst->setCurrentRow(m_config.graphSelectedFunction);
      };
      refreshFns();

      connect(btnAddExpr, &QPushButton::clicked, this, [this, edExpr, refreshFns]() {
        const QString expr = edExpr->text().trimmed();
        if (expr.isEmpty()) return;
        GraphFunctionSpec g;
        g.expression = expr;
        m_config.graphFunctions.push_back(g);
        m_config.graphSelectedFunction = m_config.graphFunctions.size() - 1;
        edExpr->clear();
        refreshFns();
        updatePreview();
        apply();
      });
      connect(lst, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_config.graphFunctions.size()) {
          m_config.graphSelectedFunction = row;
          apply();
        }
      });

      QHBoxLayout *actRow = new QHBoxLayout;
      QPushButton *btnDer = new QPushButton(QStringLiteral("Ableitung"));
      QPushButton *btnRoots = new QPushButton(QStringLiteral("Nullstellen"));
      QPushButton *btnExt = new QPushButton(QStringLiteral("Extrema"));
      QPushButton *btnDel = new QPushButton(QStringLiteral("Löschen"));
      actRow->addWidget(btnDer); actRow->addWidget(btnRoots); actRow->addWidget(btnExt); actRow->addWidget(btnDel);
      graphLay->addLayout(actRow);
      auto toggleFlag = [this, lst, refreshFns](int mode) {
        const int row = lst->currentRow();
        if (row < 0 || row >= m_config.graphFunctions.size()) return;
        auto& f = m_config.graphFunctions[row];
        if (mode == 0) f.showDerivative = !f.showDerivative;
        if (mode == 1) f.showRoots = !f.showRoots;
        if (mode == 2) f.showExtrema = !f.showExtrema;
        refreshFns();
        updatePreview();
        apply();
      };
      connect(btnDer, &QPushButton::clicked, this, [toggleFlag]() { toggleFlag(0); });
      connect(btnRoots, &QPushButton::clicked, this, [toggleFlag]() { toggleFlag(1); });
      connect(btnExt, &QPushButton::clicked, this, [toggleFlag]() { toggleFlag(2); });
      connect(btnDel, &QPushButton::clicked, this, [this, lst, refreshFns]() {
        const int row = lst->currentRow();
        if (row < 0 || row >= m_config.graphFunctions.size()) return;
        m_config.graphFunctions.removeAt(row);
        if (m_config.graphFunctions.isEmpty()) m_config.graphFunctions.push_back(GraphFunctionSpec{});
        m_config.graphSelectedFunction = qBound(0, m_config.graphSelectedFunction, m_config.graphFunctions.size() - 1);
        refreshFns();
        updatePreview();
        apply();
      });
      layout->addWidget(m_shapeGraphPanel);

      syncShapeOptionPanels();

      layout->addWidget(new QLabel("Halten → Form (Stift & Co.)"));
      layout->addWidget(new QLabel("Empfindlichkeit:"));
      QSlider *slHold = new QSlider(Qt::Horizontal);
      slHold->setRange(0, 100);
      slHold->setValue(m_config.holdShapeSensitivity);
      connect(slHold, &QSlider::valueChanged, [this](int v) {
        m_config.holdShapeSensitivity = v;
        apply();
      });
      layout->addWidget(slHold);

      layout->addWidget(new QLabel("Haltezeit (ms):"));
      QSlider *slDelay = new QSlider(Qt::Horizontal);
      slDelay->setRange(200, 900);
      slDelay->setSingleStep(20);
      slDelay->setValue(m_config.holdStillDelayMs);
      connect(slDelay, &QSlider::valueChanged, [this](int v) {
        m_config.holdStillDelayMs = v;
        apply();
      });
      layout->addWidget(slDelay);

      QCheckBox *chkCirc = new QCheckBox("Kreis-Erkennung");
      chkCirc->setChecked(m_config.holdEnableCircle);
      connect(chkCirc, &QCheckBox::toggled, [this](bool on) {
        m_config.holdEnableCircle = on;
        apply();
      });
      layout->addWidget(chkCirc);

      QCheckBox *chkTri = new QCheckBox("Dreieck-Erkennung");
      chkTri->setChecked(m_config.holdEnableTriangle);
      connect(chkTri, &QCheckBox::toggled, [this](bool on) {
        m_config.holdEnableTriangle = on;
        apply();
      });
      layout->addWidget(chkTri);
    }
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(37, 37, 38));
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);
  }

private:
  void syncShapeOptionPanels() {
    if (!m_shapeTicksPanel || !m_shapeSinePanel || !m_shapeGraphPanel)
      return;
    m_shapeTicksPanel->setVisible(m_config.shapeToolKind == ShapeToolKind::Axes2D);
    const bool sineMode =
        m_config.shapeToolKind == ShapeToolKind::SineGraph;
    m_shapeSinePanel->setVisible(sineMode);
    m_shapeGraphPanel->setVisible(false);
    if (m_shapeSineParamsWidget)
      m_shapeSineParamsWidget->setVisible(sineMode && m_config.shapeSineFixedParams);
  }
  void updatePreview() {
    if (m_previewWidget)
      m_previewWidget->updateConfig(m_mode, m_config);
  }
  void apply() { ToolManager::instance().updateConfig(m_config); }
  ToolConfig m_config;
  ToolMode m_mode;
  ToolPreviewWidget *m_previewWidget;
  int m_selectedQuickIndex{-1};
  QWidget *m_shapeTicksPanel{nullptr};
  QWidget *m_shapeSinePanel{nullptr};
  QWidget *m_shapeSineParamsWidget{nullptr};
  QWidget *m_shapeGraphPanel{nullptr};
};

// v3.16.0: legacy ToolSettingsPopup (desktop QDialog with Qt::Popup flag)
// removed. The unified morph-tray path in showSettingsPopup() now handles
// both Android and desktop with an in-window MorphTray container, which is
// what "no Windows-like chrome" requires (the popup was the last QDialog-
// driven floating window in the editor surface).

// =============================================================================
// 4. ModernToolbar Implementation
// =============================================================================

ModernToolbar::ModernToolbar(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_AcceptTouchEvents, true);
  setWindowFlags(Qt::FramelessWindowHint);
  setMouseTracking(true);
  if (parent)
    parent->installEventFilter(this);

  m_radialCollapseTimer.setSingleShot(true);
  connect(&m_radialCollapseTimer, &QTimer::timeout, this, [this]() {
    if (m_style == Radial)
      setStyle(Normal);
  });

  // v3.16.0: sliding active-tool indicator pill (Material-3 style). Created
  // before the buttons so it sits behind them in the z-order and never
  // intercepts mouse / touch events.
  auto *indicator = new ActiveIndicatorPill(this, m_accentColor);
  indicator->resize(UiScale::dp(28), UiScale::dp(3));
  indicator->hide();
  m_activeIndicator = indicator;

  btnPen = new ToolbarBtn("pen", this);
  btnPencil = new ToolbarBtn("pencil", this);
  btnHighlighter = new ToolbarBtn("highlighter", this);
  btnEraser = new ToolbarBtn("eraser", this);
  btnLasso = new ToolbarBtn("lasso", this);
  btnRuler = new ToolbarBtn("ruler", this);
  btnShape = new ToolbarBtn("shape", this);
  btnStickyNote = new ToolbarBtn("stickynote", this);
  btnText = new ToolbarBtn("text", this);
  btnImage = new ToolbarBtn("image", this);
  btnHand = new ToolbarBtn("hand", this);
  btnUndo = new ToolbarBtn("undo", this);
  btnRedo = new ToolbarBtn("redo", this);
  btnBackOverview = new ToolbarBtn("overview", this);
  btnBackOverview->setToolTip(tr("Zur Übersicht"));
  connect(btnBackOverview, &ToolbarBtn::clicked, this,
          [this]() { emit backToOverviewRequested(); });

  btnDockToggle = new ToolbarBtn("dock_float", this);

  btnSave = new ToolbarBtn("save", this);
  btnPalette = new ToolbarBtn("palette", this);
  btnBrushSize = new ToolbarBtn("brush_size", this);

  m_dockedOnlyButtons = {btnSave, btnPalette, btnBrushSize};

  m_buttons = {btnSave,     btnPen,      btnPencil, btnHighlighter,
                 btnEraser,    btnLasso,    btnRuler,    btnShape,  btnStickyNote,
                 btnText,      btnImage,    btnHand,     btnBackOverview,
                 btnUndo,      btnRedo,     btnPalette,  btnBrushSize, btnDockToggle};

  m_customColors = {Qt::black, Qt::white,         Qt::red,
                    Qt::blue,  QColor(0, 150, 0), QColor(255, 140, 0)};

  for (auto *b : m_buttons) {
    if (b)
      b->setAccentColor(m_accentColor);
  }

  auto handleToolClick = [this](ToolMode m) {
    if (mode_ == m) {
      if (m_style == Radial) {
          if (!m_showRadialSettings)
              toggleRadialSettings();
      } else {
          showSettingsPopup();
      }
      return;
    }
    if (m_showRadialSettings) {
        toggleRadialSettings(); // Hide settings when switching tools
    }
    ToolbarBtn *btn = getButtonForMode(m);
    if (btn) {
      QPoint pos = btn->mapToGlobal(QPoint(btn->width() + 15, 0));
      ToolUIBridge::instance().setOverlayPosition(pos);
    }
    ToolManager::instance().selectTool(m);
    setToolMode(m);
    if (m_style == Radial)
      m_radialCollapseTimer.start(180);
  };
  
  // Initialize Secondary Settings Ring (Colors & Sizes)
  for (QColor c : m_customColors) {
    QPushButton *btn = new QPushButton(this);
    btn->setFixedSize(28, 28);
    btn->setStyleSheet(QString("background-color: %1; border-radius: 14px; border: 2px solid rgba(255,255,255,100);").arg(c.name()));
    btn->hide();
    m_radialSettingsBtns.append(btn);
    connect(btn, &QPushButton::clicked, this, [this, c]() {
       ToolUIBridge::instance().setPenColor(c);
       toggleRadialSettings();
    });
  }
  
  QList<int> sizes = {2, 6, 12, 24};
  for (int s : sizes) {
    QPushButton *btn = new QPushButton(this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(30, 30);
    btn->setStyleSheet("background-color: rgba(30, 28, 52, 200); border-radius: 15px; border: 1px solid rgba(255,255,255,50); color: white; font-weight: bold;");
    btn->setText(QString::number(s));
    btn->hide();
    m_radialSettingsBtns.append(btn);
    connect(btn, &QPushButton::clicked, this, [this, s]() {
       ToolUIBridge::instance().setPenWidth(s);
       toggleRadialSettings();
    });
  }

  connect(btnPen, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Pen); });
  connect(btnPencil, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Pencil); });
  connect(btnHighlighter, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Highlighter); });
  connect(btnEraser, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Eraser); });

  connect(btnLasso, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Lasso); });
  connect(btnRuler, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Ruler); });
  connect(btnShape, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Shape); });
  connect(btnStickyNote, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::StickyNote); });
  connect(btnText, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Text); });
  connect(btnHand, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Hand); });
  connect(btnImage, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Image); });

#if BLOP_TOOLBAR_LONGPRESS
  auto connectLongPressClose = [this](ToolbarBtn *btn, ToolMode mode) {
    connect(btn, &ToolbarBtn::longPressed, this, [this, mode]() {
      if (mode_ != mode)
        return;
      if (m_style == Radial && m_showRadialSettings) {
        toggleRadialSettings();
      }
      if (mode != ToolMode::Pen) {
        ToolManager::instance().selectTool(ToolMode::Pen);
        setToolMode(ToolMode::Pen);
      }
    });
  };
  connectLongPressClose(btnPen, ToolMode::Pen);
  connectLongPressClose(btnPencil, ToolMode::Pencil);
  connectLongPressClose(btnHighlighter, ToolMode::Highlighter);
  connectLongPressClose(btnEraser, ToolMode::Eraser);
  connectLongPressClose(btnLasso, ToolMode::Lasso);
  connectLongPressClose(btnRuler, ToolMode::Ruler);
  connectLongPressClose(btnShape, ToolMode::Shape);
  connectLongPressClose(btnStickyNote, ToolMode::StickyNote);
  connectLongPressClose(btnText, ToolMode::Text);
  connectLongPressClose(btnHand, ToolMode::Hand);
  connectLongPressClose(btnImage, ToolMode::Image);
#endif
  connect(btnUndo, &ToolbarBtn::clicked,
          [this]() { emit undoRequested(); });
  connect(btnRedo, &ToolbarBtn::clicked,
          [this]() { emit redoRequested(); });
  connect(btnDockToggle, &ToolbarBtn::clicked,
          [this]() { setDockMode(!m_isDockedMode); });
#if BLOP_TOOLBAR_LONGPRESS
  // v3.16.0: long-press on the dock-toggle button opens the radial wheel as
  // a quick-switcher. The wheel uses HalfEdge automatically when the toolbar
  // is anchored near a screen edge so the half not visible off-screen never
  // appears. Releasing on a button selects it; tapping elsewhere closes.
  connect(btnDockToggle, &ToolbarBtn::longPressed, this, [this]() {
    if (m_style == Radial) {
      setStyle(Normal);
      return;
    }
    // Pick HalfEdge when toolbar's center is within a wheel-radius of any
    // edge of its parent; otherwise FullCircle in the screen center area.
    if (auto *p = parentWidget()) {
      const QPoint c = mapToParent(rect().center());
      const int wheelR = UiScale::dp(180);
      const int edgeBand = UiScale::dp(40);
      const bool nearEdge =
          c.x() < wheelR + edgeBand ||
          c.x() > p->width() - wheelR - edgeBand ||
          c.y() < wheelR + edgeBand ||
          c.y() > p->height() - wheelR - edgeBand;
      setRadialType(nearEdge ? HalfEdge : FullCircle);
    }
    setStyle(Radial);
  });
#endif

  connect(btnPalette, &ToolbarBtn::clicked, this,
          [this]() { toggleRadialSettings(); });
  connect(btnBrushSize, &ToolbarBtn::clicked, this,
          [this]() { toggleRadialSettings(); });

  setStyle(Normal);
  setToolMode(ToolMode::Pen);
  ToolManager::instance().selectTool(ToolMode::Pen);
}

// --- WICHTIG: Event-Handler sind jetzt HIER OBEN, damit sie gefunden werden
// ---

void ModernToolbar::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  int cx = width() / 2;
  int cy = height() / 2;
  if (m_style == Normal) {
    int w = width();
    int h = height();

    if (m_orientation == Vertical || m_orientation == Horizontal) {
      if (m_isDockedMode) {
        // Notch appearance for all platforms: flush on top, rounded
        // bottom corners only. Drawing at y = -r lets the widget
        // bounds clip the top corners square so the toolbar visually
        // attaches to the parent's top edge.
        QLinearGradient grad(0, 0, w, h);
        grad.setColorAt(0, QColor(30, 28, 52, 255));
        grad.setColorAt(1, QColor(20, 18, 40, 255));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);

        int r = 16;
        p.drawRoundedRect(0, -r, w, h + r, r, r);

        p.setBrush(Qt::NoBrush);
        QColor accentBorder = m_accentColor;
        accentBorder.setAlpha(100);
        p.setPen(QPen(accentBorder, 2));
        p.drawRoundedRect(1, -r, w - 2, h + r - 1, r, r);

        p.setPen(QPen(QColor(255, 255, 255, 30), 1));
        for (int sx : m_separatorXPositions) {
          p.drawLine(sx, 12, sx, h - 12);
        }
      } else {
        // Floating pill container
        int radius = std::min(w, h) / 2;
        // Outer glow / shadow
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawRoundedRect(rect().adjusted(2, 4, -2, -2), radius, radius);
        
        // Container body
        QLinearGradient grad(0, 0, w, h);
        grad.setColorAt(0, QColor(30, 28, 52, 248));
        grad.setColorAt(1, QColor(20, 18, 40, 248));
        p.setBrush(grad);
        QColor floatingBorder = m_accentColor;
        floatingBorder.setAlpha(60);
        p.setPen(QPen(floatingBorder, 1));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius, radius);

        // Drag grip
        if (m_draggable) {
          if (m_orientation == Vertical) {
            p.setBrush(QColor(255, 255, 255, 55));
            p.setPen(Qt::NoPen);
            for (int i = -1; i <= 1; ++i)
              p.drawEllipse(w / 2 - 2, h / 2 + i * 7 - 2, 4, 4);
          } else {
            p.setPen(QPen(QColor(255, 255, 255, 50), 2));
            p.drawLine(10, h / 2 - 8, 10, h / 2 + 8);
            p.drawLine(14, h / 2 - 8, 14, h / 2 + 8);
          }
        }
      }
    }
  } else {
    int rMain = 135 * m_scale;
    if (m_radialType == HalfEdge) {
      int paintCx = m_isDockedLeft ? 0 : width();
      QRect box(paintCx - rMain, cy - rMain, rMain * 2, rMain * 2);
      
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, 60));
      p.drawPie(QRect(box.x(), box.y() + 4, box.width(), box.height()), (m_isDockedLeft ? -90 : 90) * 16, 180 * 16);
      
      QRadialGradient bgGrad(paintCx, cy, rMain * 0.8);
      bgGrad.setColorAt(0.0, QColor(35, 33, 58, 245));
      bgGrad.setColorAt(1.0, QColor(20, 18, 40, 245));
      p.setBrush(bgGrad);
      QColor radialBorder = m_accentColor;
      radialBorder.setAlpha(90);
      p.setPen(QPen(radialBorder, 1));
      p.drawPie(box, (m_isDockedLeft ? -90 : 90) * 16, 180 * 16);
    } else {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, 60));
      p.drawEllipse(QPoint(cx, cy + 4), rMain, rMain);
      
      QRadialGradient bgGrad(cx, cy, rMain * 0.8);
      bgGrad.setColorAt(0.0, QColor(35, 33, 58, 245));
      bgGrad.setColorAt(1.0, QColor(20, 18, 40, 245));
      p.setBrush(bgGrad);
      QColor radialBorder = m_accentColor;
      radialBorder.setAlpha(90);
      p.setPen(QPen(radialBorder, 1));
      p.drawEllipse(QPoint(cx, cy), rMain, rMain);

      int hole = static_cast<int>(55 * m_scale);
      p.setBrush(QColor(15, 14, 25, 255));
      p.setPen(QPen(QColor(0, 0, 0, 150), 2));
      p.drawEllipse(QPoint(cx, cy), hole, hole);
    }
  }
}

void ModernToolbar::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    m_pressedButton = nullptr;
    m_isScrolling = false;
    m_hasScrolled = false;
    m_isDragging = false;
    m_isDockedCenterDragging = false;
    if (m_style == Radial)
      m_pressedButton = getRadialButtonAt(e->pos());
    int dragR = 45 * m_scale;
    int cx = width() / 2;
    int cy = height() / 2;
    dragStartPos_ = e->pos();
    
    if (m_style == Radial && m_radialType == HalfEdge)
      cx = m_isDockedLeft ? 0 : width();
    int dx = e->pos().x() - cx;
    int dy = e->pos().y() - cy;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (m_style == Radial && m_radialType == HalfEdge) {
      if (dist > dragR) {
        m_dragStartAngle = std::atan2(-dy, dx) * 180.0 / 3.14159;
        m_scrollStartAngleVal = m_scrollAngle;
        m_isScrolling = true;
      }
    }
    if (m_isResizing)
      return;
    if (m_style == Normal && m_isDockedMode && m_orientation == Horizontal &&
        supportsAdaptiveDockedScroll() && m_dockedCenterOverflowPx > 0) {
      bool hitVisibleButton = false;
      for (auto *b : m_buttons) {
        if (b && b->isVisible() && b->geometry().contains(e->pos())) {
          hitVisibleButton = true;
          break;
        }
      }
      const bool inCenterLane =
          e->pos().x() >= m_dockedCenterAreaStartX &&
          e->pos().x() <= m_dockedCenterAreaEndX;
      if (!hitVisibleButton && inCenterLane) {
        m_isDockedCenterDragging = true;
        m_dockedCenterDragLastX = e->pos().x();
        e->accept();
        return;
      }
    }
    if (m_style == Normal && !m_isPreview) {
      int handleSize = 30;
      if (e->pos().x() > width() - handleSize &&
          e->pos().y() > height() - handleSize) {
        m_isResizing = true;
        dragStartPos_ = e->pos();
        resizeStartPos_ = e->pos();
        startSize_ = size();
        return;
      }
    }
    if (!m_isPreview && !m_pressedButton) {
      bool canDrag = false;
      if (m_style != Radial) {
        if (m_orientation == Vertical && e->pos().y() < 50)
          canDrag = true;
        if (m_orientation == Horizontal && e->pos().x() < 50)
          canDrag = true;
      }
      if (canDrag) {
        m_isDragging = true;
        dragStartPos_ = e->pos();
        m_isScrolling = false;
        m_dragOffset =
            e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
        clearMask();
        return;
      }
    }
  }
}

void ModernToolbar::mouseMoveEvent(QMouseEvent *e) {
  if (m_isDockedCenterDragging && m_style == Normal && m_isDockedMode &&
      m_orientation == Horizontal && supportsAdaptiveDockedScroll() &&
      m_dockedCenterOverflowPx > 0) {
    const int dx = e->pos().x() - m_dockedCenterDragLastX;
    m_dockedCenterDragLastX = e->pos().x();
    m_dockedCenterScrollPx =
        qBound(0, m_dockedCenterScrollPx - dx, m_dockedCenterOverflowPx);
    updateLayout(false);
    e->accept();
    return;
  }
  if (e->buttons() == Qt::NoButton && !m_isDragging && !m_isScrolling && e->source() == Qt::MouseEventNotSynthesized) return;

  if (m_style == Radial && !m_isDragging) {
     QPoint delta = e->pos() - dragStartPos_;
     
     if (m_pressedButton) {
         if (delta.manhattanLength() > 5) {
             m_isDragging = true;
             m_pressedButton = nullptr; 
             m_isScrolling = false;
             m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
             clearMask();
             return;
         }
     } else {
         if (m_radialType == HalfEdge) {
             if ((m_isDockedLeft && delta.x() > 10) || (!m_isDockedLeft && delta.x() < -10)) {
                 m_isDragging = true;
                 m_isScrolling = false;
                 m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
                 clearMask();
                 return;
             }
         } else if (m_radialType == FullCircle) {
             if (delta.manhattanLength() > 5) {
                 m_isDragging = true;
                 m_isScrolling = false;
                 m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
                 clearMask();
                 return;
             }
         }
     }
  }

  if (m_style == Radial && m_isScrolling && m_radialType == HalfEdge &&
      !m_pressedButton && !m_isDragging) {
    int cx = m_isDockedLeft ? 0 : width();
    int cy = height() / 2;
    int dx = e->pos().x() - cx;
    int dy = e->pos().y() - cy;
    double currentAngle = std::atan2(-dy, dx) * 180.0 / 3.14159;
    double delta = currentAngle - m_dragStartAngle;
    if (delta > 180)
      delta -= 360;
    if (delta < -180)
      delta += 360;
    if (std::abs(delta) > 1.0)
      m_hasScrolled = true;
    m_scrollAngle = m_scrollStartAngleVal - delta;
    double maxScroll = (m_buttons.size() * 30.0);
    if (m_scrollAngle > maxScroll)
      m_scrollAngle = maxScroll;
    if (m_scrollAngle < -maxScroll)
      m_scrollAngle = -maxScroll;
    updateLayout();
    return;
  }
  if (m_isDragging) {
    if (!parentWidget())
      return;
    QPoint newPosGlobal = e->globalPosition().toPoint() - m_dragOffset;
    QPoint newTopLeft = parentWidget()->mapFromGlobal(newPosGlobal);
    QPoint globalMousePosInParent =
        parentWidget()->mapFromGlobal(e->globalPosition().toPoint());
    int parentW = parentWidget()->width();
    int parentH = parentWidget()->height();
    int effectivePadding = 0;
    if (m_style == Radial) {
      int rVisual = 175 * m_scale;
      int rWidget = width() / 2;
      effectivePadding = rWidget - rVisual;

      int snapDist = 60 * m_scale;
      int tearDist = 120 * m_scale;
      if (m_radialType == FullCircle) {
        if (globalMousePosInParent.x() < snapDist) {
          m_isDockedLeft = true;
          setRadialType(HalfEdge);
          move(0, y());
          m_dragOffset =
              e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
          return;
        } else if (globalMousePosInParent.x() > (parentW - snapDist)) {
          m_isDockedLeft = false;
          setRadialType(HalfEdge);
          move(parentW - width(), y());
          m_dragOffset =
              e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
          return;
        }
      } else if (m_radialType == HalfEdge) {
        bool pullAway = false;
        if (m_isDockedLeft && globalMousePosInParent.x() > tearDist)
          pullAway = true;
        else if (!m_isDockedLeft &&
                 globalMousePosInParent.x() < (parentW - tearDist))
          pullAway = true;
        if (pullAway) {
          setRadialType(FullCircle);
          clearMask();
          int newX = globalMousePosInParent.x() - (width() / 2);
          move(newX, y());
          m_dragOffset =
              e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
          return;
        } else {
          int fixedX = m_isDockedLeft ? 0 : (parentW - width());
          int minSlidingY = -effectivePadding;
          int maxSlidingY = parentH - height() + effectivePadding;
          move(fixedX, qBound(minSlidingY, newTopLeft.y(), maxSlidingY));
          return;
        }
      }
    }
    int minX = -effectivePadding;
    int maxX = parentW - width() + effectivePadding;
    int maxY = parentH - height() + effectivePadding;
    
    // Allow dragging all the way to y=0 to merge with header
    int effectiveTop = (m_style == Radial) ? -effectivePadding : 0; 
    
    int newY = qBound(effectiveTop, newTopLeft.y(), maxY);
    int newX = qBound(minX, newTopLeft.x(), maxX);
    
    // Docking Previews
#ifdef Q_OS_ANDROID
    // Edge-snap is disabled on Android: dragging is already disabled
    // (setDraggable(false)) and the side-snap path produced the
    // broken vertical-sliver state shown in image 2 of the user's
    // report. Skip the entire preview/show pipeline.
    if (m_snapPreview) m_snapPreview->hide();
#else
    if (m_style == Normal) {
      if (!m_snapPreview) {
        m_snapPreview = new QWidget(parentWidget());
        m_snapPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
        // v3.16.1: stylesheet set ONCE here, not on every mouse-move. Setting
        // stylesheet on Windows triggers a full style-recompute; doing that
        // at native mouse polling rate (~125 Hz) starved the paint thread
        // during toolbar drag and was a major source of the choppiness.
        m_snapPreview->setStyleSheet(QString(
            "background-color: transparent;"
            "border: 2px dashed %1;"
            "border-radius: 20px;").arg(m_accentColor.name()));
      }

      int idealL = calculateMinLength() + 20;
      const int safeTop = UiScale::safeTopPx(parentWidget());
      const int safeBottom = UiScale::safeBottomPx(parentWidget());
      const int sidePad = UiScale::safeHorizontalPaddingPx(parentWidget());
      const int previewTop = safeTop + UiScale::dp(8);
      const int previewHeight = UiScale::dp(52);

      if (newY <= 50) {
        // Top Snap: Centered width
        m_snapPreview->setGeometry((parentW - idealL) / 2, previewTop, idealL, previewHeight);
        if (!m_snapPreview->isVisible()) {
          m_snapPreview->show();
          m_snapPreview->raise();
        }
      } else if (newX <= 50) {
        // Left Snap (Vertical Pill)
        const int usableH = qMax(UiScale::dp(120), parentH - safeTop - safeBottom);
        const int y = safeTop + qMax(0, (usableH - idealL) / 2);
        m_snapPreview->setGeometry(sidePad, y, UiScale::dp(65), idealL);
        if (!m_snapPreview->isVisible()) {
          m_snapPreview->show();
          m_snapPreview->raise();
        }
      } else if (newX >= parentW - 100) {
        // Right Snap (Vertical Pill)
        const int usableH = qMax(UiScale::dp(120), parentH - safeTop - safeBottom);
        const int y = safeTop + qMax(0, (usableH - idealL) / 2);
        m_snapPreview->setGeometry(parentW - sidePad - UiScale::dp(65), y, UiScale::dp(65), idealL);
        if (!m_snapPreview->isVisible()) {
          m_snapPreview->show();
          m_snapPreview->raise();
        }
      } else {
        if (m_snapPreview->isVisible())
          m_snapPreview->hide();
      }
    }
#endif

    move(newX, newY);
    return;
  }
  if (m_isResizing && m_style == Normal) {
    QPoint delta = e->pos() - resizeStartPos_;
    int newW = startSize_.width() + delta.x();
    int newH = startSize_.height() + delta.y();
    int minL = calculateMinLength();
    if (m_orientation == Vertical) {
      if (newH < minL)
        newH = minL;
      if (newW < 50)
        newW = 50;
      if (newW > 100)
        newW = 100;
    } else {
      if (newW < minL)
        newW = minL;
      if (newH < 50)
        newH = 50;
      if (newH > 100)
        newH = 100;
    }
    resize(newW, newH);
    constrainToParent();
    return;
  }
  if (m_style == Normal && !m_isPreview) {
    if (e->pos().x() > width() - 30 && e->pos().y() > height() - 30)
      setCursor(Qt::SizeFDiagCursor);
    else
      setCursor(Qt::ArrowCursor);
  }
}

void ModernToolbar::mouseReleaseEvent(QMouseEvent *e) {
  if (m_isDockedCenterDragging) {
    m_isDockedCenterDragging = false;
    e->accept();
    return;
  }
  if (m_pressedButton) {
    if (!(m_style == Radial && m_radialType == HalfEdge && m_hasScrolled))
      m_pressedButton->triggerClick();
    m_pressedButton = nullptr;
  }
  if (m_isDragging) {
    m_cachedMask = QRegion();
    updateHitbox();

#ifdef Q_OS_ANDROID
    // Edge-snap is disabled on Android (matches the disabled snap
    // preview in mouseMoveEvent) - the side-snap path produces a
    // broken vertical sliver. Just hide any stray preview and skip
    // both the snap commit and the orientation check.
    if (m_snapPreview) m_snapPreview->hide();
#else
    if (m_style == Normal && m_snapPreview && m_snapPreview->isVisible()) {
        QRect snapGeom = m_snapPreview->geometry();
        m_snapPreview->hide();
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        
        bool isTopSnap = (snapGeom.y() == 10 && snapGeom.width() > 100);
        bool isVertical = !isTopSnap;
        
        if (isTopSnap) {
            setMaximumSize(snapGeom.size());
            setFixedSize(snapGeom.size());
            setOrientation(Horizontal, false); // no internal animation, we'll do geom animation
        } else {
            setMaximumSize(snapGeom.size());
            setFixedSize(snapGeom.size());
            setOrientation(Vertical, false);
        }
        
        // Smooth snap animation
        QPropertyAnimation *snapAnim = new QPropertyAnimation(this, "geometry");
        snapAnim->setDuration(220);
        snapAnim->setStartValue(geometry());
        snapAnim->setEndValue(snapGeom);
        snapAnim->setEasingCurve(QEasingCurve::OutCubic);
        snapAnim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        if (m_snapPreview) m_snapPreview->hide();
        if (m_style == Normal) checkOrientation(e->globalPosition().toPoint());
    }
#endif
  }
  m_isDragging = false;
  m_isResizing = false;
  m_isScrolling = false;
  m_hasScrolled = false;
  if (m_style == Radial && m_radialType == HalfEdge)
    snapToEdge();
  else
    constrainToParent();
}

void ModernToolbar::resizeEvent(QResizeEvent *) {
  updateLayout();
  updateHitbox();
}
void ModernToolbar::leaveEvent(QEvent *) { setCursor(Qt::ArrowCursor); }

// --- RESTLICHE FUNKTIONEN ---

void ModernToolbar::showSettingsPopup() {
  if (mode_ != ToolMode::Pen && mode_ != ToolMode::Pencil &&
      mode_ != ToolMode::Highlighter && mode_ != ToolMode::Eraser &&
      mode_ != ToolMode::Lasso && mode_ != ToolMode::Ruler &&
      mode_ != ToolMode::Shape)
    return;
  if (!m_toolSettingsSheet.isNull()) {
    m_toolSettingsSheet->hide();
    m_toolSettingsSheet->deleteLater();
    m_toolSettingsSheet = nullptr;
  }
  ToolConfig currentConfig = ToolManager::instance().config();
  ToolbarBtn *btn = getButtonForMode(mode_);

  // v3.16.0: unified morph-tray. The tray grows out of the bottom edge of
  // the toolbar (or to the side, when the toolbar is in vertical orientation)
  // with a height + opacity animation, so the rounded-bottom shape visually
  // continues the notch. Both platforms share this path; only the scrim is
  // Android-only because phones benefit from the dim-everything-behind UX
  // while desktops do not.
  QWidget *host = window();
#ifdef Q_OS_ANDROID
  if (auto *mw = qobject_cast<QMainWindow *>(host)) {
    if (QWidget *cw = mw->centralWidget())
      host = cw;
  }
#endif
  if (!host)
    return;

  auto *layer = new QWidget(host);
  layer->setAttribute(Qt::WA_DeleteOnClose, true);
  layer->setAttribute(Qt::WA_TranslucentBackground);
  layer->setFocusPolicy(Qt::NoFocus);
  layer->setGeometry(host->rect());
  m_toolSettingsSheet = layer;

#ifdef Q_OS_ANDROID
  auto *scrim = new AndroidToolSettingsScrim(layer);
#else
  auto *scrim = new DesktopTraySCrim(layer);
#endif
  scrim->setGeometry(layer->rect());
  scrim->show();
  scrim->lower();

  auto *tray = new MorphTray(layer);
  auto *trayLayout = new QVBoxLayout(tray);
  trayLayout->setContentsMargins(UiScale::dp(6), UiScale::dp(2),
                                 UiScale::dp(6), UiScale::dp(14));
  trayLayout->setSpacing(0);
  auto *content = new ToolSettingsContent(mode_, currentConfig, tray);
  trayLayout->addWidget(content);
  tray->adjustSize();

  const int targetW = qMax(UiScale::dp(280), tray->sizeHint().width());
  const int targetH = tray->sizeHint().height();

  const QPoint toolbarTL = host->mapFromGlobal(mapToGlobal(QPoint(0, 0)));
  const QPoint toolbarBR =
      host->mapFromGlobal(mapToGlobal(QPoint(width(), height())));
  const int toolbarCx = (toolbarTL.x() + toolbarBR.x()) / 2;
  int targetX = toolbarCx - targetW / 2;
  int targetY = toolbarBR.y();

  // When the toolbar is in vertical orientation (floating side), grow the
  // tray to the right of the active button instead of below; keep it bound
  // inside the host with safe-area margins on all sides.
  if (m_orientation == Vertical && btn) {
    const QPoint btnTL = host->mapFromGlobal(btn->mapToGlobal(QPoint(0, 0)));
    targetX = btnTL.x() + btn->width() + UiScale::dp(8);
    targetY = btnTL.y();
  }

  const QRect hostR = host->rect();
  const int sidePad = UiScale::safeHorizontalPaddingPx(host);
  const int topPad = UiScale::safeTopPx(host) + UiScale::dp(8);
  const int bottomPad = UiScale::safeBottomPx(host) + UiScale::dp(8);
  if (targetX + targetW > hostR.width() - sidePad)
    targetX = hostR.width() - sidePad - targetW;
  if (targetX < sidePad)
    targetX = sidePad;
  if (targetY + targetH > hostR.height() - bottomPad)
    targetY = hostR.height() - bottomPad - targetH;
  if (targetY < topPad)
    targetY = topPad;

  const QRect finalGeom(targetX, targetY, targetW, targetH);
  // Start collapsed (height = 0) so the tray appears to grow out of the
  // notch's bottom edge. Both width and x start at their final values so
  // only the height animates.
  tray->setGeometry(QRect(targetX, targetY, targetW, 0));

  // v3.16.1: paint-based alpha instead of QGraphicsOpacityEffect. The effect
  // forces Qt to render the whole subtree (tray + content) into an offscreen
  // pixmap every frame. Combined with the geometry animation that destroyed
  // performance on Windows. Now the tray paints its own translucent fill
  // with this alpha; the inner content stays at 100% opacity (acceptable -
  // it gets revealed by the growing height anyway).
  tray->setAlpha(0.0);

  layer->show();
  tray->show();
  tray->raise();

  auto *geomAnim = new QPropertyAnimation(tray, "geometry", tray);
  geomAnim->setDuration(320);
  geomAnim->setStartValue(tray->geometry());
  geomAnim->setEndValue(finalGeom);
  geomAnim->setEasingCurve(QEasingCurve::OutCubic);
  geomAnim->start(QAbstractAnimation::DeleteWhenStopped);

  auto *alphaAnim = new QVariantAnimation(tray);
  alphaAnim->setDuration(160);
  alphaAnim->setStartValue(0.0);
  alphaAnim->setEndValue(1.0);
  alphaAnim->setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(alphaAnim, &QVariantAnimation::valueChanged, tray,
                   [tray](const QVariant &v) {
                     if (tray)
                       tray->setAlpha(v.toDouble());
                   });
  alphaAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

ToolbarBtn *ModernToolbar::getButtonForMode(ToolMode m) {
  switch (m) {
  case ToolMode::Pen:
    return btnPen;
  case ToolMode::Pencil:
    return btnPencil;
  case ToolMode::Highlighter:
    return btnHighlighter;
  case ToolMode::Eraser:
    return btnEraser;
  case ToolMode::Lasso:
    return btnLasso;
  case ToolMode::Ruler:
    return btnRuler;
  case ToolMode::Shape:
    return btnShape;
  case ToolMode::StickyNote:
    return btnStickyNote;
  case ToolMode::Text:
    return btnText;
  case ToolMode::Image:
    return btnImage;
  case ToolMode::Hand:
    return btnHand;
  default:
    return btnPen;
  }
}

ToolbarBtn *ModernToolbar::getRadialButtonAt(const QPoint &pos) {
  for (auto *b : m_buttons) {
    if (b->isVisible() && b->geometry().contains(pos))
      return b;
  }
  return nullptr;
}

void ModernToolbar::showEvent(QShowEvent *) {
  updateLayout();
  if (m_style == Radial && !m_isPreview)
    constrainToParent();
  updateHitbox();
}
bool ModernToolbar::supportsAdaptiveDockedScroll() const {
#ifdef Q_OS_ANDROID
  return true;
#else
  return false;
#endif
}
int ModernToolbar::effectiveButtonSize(int w, int h) const {
#ifdef Q_OS_ANDROID
  // On Android Normal-style horizontal toolbars we additionally cap the
  // button size based on the available horizontal width / total button count.
  // This guarantees the entire row fits on narrow phones instead of spilling
  // past the right edge (the existing center-scroll fallback then handles
  // even narrower viewports gracefully).
  if (m_style == Normal && m_orientation == Horizontal) {
    // Tighter inter-button gap on Android - the docked toolbar is now
    // dp(40) tall (instead of dp(48)), so buttons + gaps need to scale
    // down with it to keep the row balanced.
    const int gap = UiScale::dp(3);
    int totalBtns = 0;
    int extras = 0;
    if (m_isDockedMode) {
      QList<ToolbarBtn *> leftGroup = leftChromeButtons();
      const QList<ToolbarBtn *> rightGroup = {btnPalette, btnBrushSize, btnSave,
                                              btnDockToggle};
      const int centerGroupSize = qMax(
          0, m_buttons.size() - leftGroup.size() - rightGroup.size());
      totalBtns = leftGroup.size() + rightGroup.size() + centerGroupSize;
      extras = UiScale::dp(20) + UiScale::dp(30) + UiScale::dp(30) +
               UiScale::dp(20) + UiScale::dp(24);
      if (leftGroup.size() > 1) extras += (leftGroup.size() - 1) * gap;
      if (rightGroup.size() > 1) extras += (rightGroup.size() - 1) * gap;
      if (centerGroupSize > 1) extras += (centerGroupSize - 1) * gap;
    } else {
      for (auto *b : m_buttons)
        if (!m_dockedOnlyButtons.contains(b))
          ++totalBtns;
      extras = UiScale::dp(30) + 16; // drag handle + side margin
      if (totalBtns > 1) extras += (totalBtns - 1) * gap;
    }
    const int widthCap =
        (totalBtns > 0) ? qMax(22, (w - extras) / totalBtns) : 28;
    // Compact bounds: docked tools = 22..32 dp (was 26..38), floating tools
    // = 24..30 (was 28..36). Material guidance allows 24-dp action icons
    // in dense toolbars, so 22-32 stays comfortably tap-friendly.
    const int verticalCap = qBound(22, h - UiScale::dp(8), 32);
    const int lower = m_isDockedMode ? 22 : 24;
    const int upper = m_isDockedMode ? 32 : 30;
    return qBound(lower, qMin(verticalCap, widthCap), upper);
  }
  // Reached when m_orientation == Vertical on Android (horizontal handled
  // above). Keep the same compact 22..30 / 22..32 dp range as the
  // horizontal Android variant so an edge-snap or orientation flip
  // doesn't suddenly produce ~36 dp buttons that look oversized on a
  // phone.
  if (m_style == Normal && !m_isDockedMode) {
    const int axis = (m_orientation == Vertical) ? qMin(w, h) : h;
    return qBound(22, axis - UiScale::dp(8), 30);
  }
  if (m_style == Normal && m_isDockedMode) {
    const int axis = (m_orientation == Vertical) ? qMin(w, h) : h;
    return qBound(22, axis - UiScale::dp(6), 32);
  }
#endif
  if (m_orientation == Vertical)
    return std::max(30, (w < h ? w - 14 : h - 14));
  return std::max(30, (h < w ? h - 12 : w - 12));
}
int ModernToolbar::effectiveGap() const {
#ifdef Q_OS_ANDROID
  return (m_style == Normal && !m_isDockedMode) ? UiScale::dp(4) : UiScale::dp(5);
#else
  return UiScale::dp(6);
#endif
}
bool ModernToolbar::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event->type() == QEvent::Resize &&
      !m_isPreview) {
    updateLayout(false);
    constrainToParent();
  }
  return QWidget::eventFilter(watched, event);
}
void ModernToolbar::setTopBound(int top) {
  m_topBound = top;
  constrainToParent();
}
void ModernToolbar::requestAdaptiveReflow() {
  updateLayout(false);
  if (!m_isPreview)
    constrainToParent();
  update();
}
void ModernToolbar::setScale(double s) {
  if (s < 0.5)
    s = 0.5;
  if (s > 2.0)
    s = 2.0;
  m_scale = s;
  if (m_style == Radial) {
    int btnSize = 40 * m_scale;
    for (auto *b : m_buttons)
      b->setBtnSize(btnSize);
    int size = 460 * m_scale;
    setFixedSize(size, size);
    updateLayout();
    updateHitbox();
    update();
    if (!m_isPreview)
      constrainToParent();
  }
}

void ModernToolbar::setAccentColor(const QColor &color) {
  if (!color.isValid() || m_accentColor == color)
    return;
  m_accentColor = color;
  for (auto *b : m_buttons) {
    if (b)
      b->setAccentColor(m_accentColor);
  }
  update();
}
void ModernToolbar::constrainToParent() {
  if (!parentWidget() || m_isPreview)
    return;
  QRect pRect = parentWidget()->rect();
  int padding = 0;
  if (m_style == Radial) {
    int rVisual = 175 * m_scale;
    int rWidget = width() / 2;
    padding = rWidget - rVisual;
  }
  int maxX = pRect.width() - width() + padding;
  int maxY = pRect.height() - height() + padding;
  int minX = -padding;
  int newX = x();
  int newY = y();
  int effectiveTop = (m_style == Radial) ? -padding : m_topBound;
  if (newY < effectiveTop)
    newY = effectiveTop;
  if (newY > maxY)
    newY = maxY;
  if (m_style == Radial && m_radialType == HalfEdge) {
    snapToEdge();
    move(x(), newY);
  } else {
    if (newX < minX)
      newX = minX;
    if (newX > maxX)
      newX = maxX;
    move(newX, newY);
  }
}
void ModernToolbar::toggleRadialSettings() {
  m_showRadialSettings = !m_showRadialSettings;
  updateLayout(true);
  updateHitbox();
  update();
}
void ModernToolbar::reorderButtons() {
  // Deprecated: We no longer mutate the m_buttons array to prevent destroying structural layouts across Docked/Linear modes.
}
void ModernToolbar::setToolMode(ToolMode mode) {
  bool changed = (mode_ != mode);
  mode_ = mode;
  for (auto *b : m_buttons)
    b->setActive(false);
  ToolbarBtn *activeBtn = getButtonForMode(mode);
  if (activeBtn) {
    activeBtn->setActive(true);
    activeBtn->animateSelect();
  }
  if (changed) {
    emit rulerToggled(mode == ToolMode::Ruler);
  }
  if (m_style == Radial && m_radialType == FullCircle && changed) {
    updateLayout(true);
  }
  updateActiveIndicator(true);
  update();
  updateHitbox();
}

void ModernToolbar::updateActiveIndicator(bool animate) {
  if (!m_activeIndicator)
    return;
  // Indicator only makes sense for the linear (Normal) toolbar; hide in Radial.
  if (m_style != Normal) {
    m_activeIndicator->hide();
    return;
  }
  ToolbarBtn *btn = getButtonForMode(mode_);
  if (!btn || !btn->isVisible() || btn->geometry().isEmpty()) {
    m_activeIndicator->hide();
    return;
  }

  const QPoint btnTL = btn->pos();
  const int btnW = btn->width();
  const int btnH = btn->height();
  const int indW = UiScale::dp(28);
  const int indH = UiScale::dp(3);

  QPoint target;
  if (m_orientation == Horizontal) {
    // Pill sits inside the bottom of the button row, ~4 dp above the pill's
    // bottom edge so the active button visually anchors the indicator.
    target = QPoint(btnTL.x() + (btnW - indW) / 2,
                    btnTL.y() + btnH - indH - UiScale::dp(4));
  } else {
    target = QPoint(btnTL.x() + btnW - indH - UiScale::dp(4),
                    btnTL.y() + (btnH - indW) / 2);
  }

  // Swap dimensions for vertical orientation.
  if (m_orientation == Horizontal)
    m_activeIndicator->resize(indW, indH);
  else
    m_activeIndicator->resize(indH, indW);

  if (!m_activeIndicator->isVisible()) {
    m_activeIndicator->move(target);
    m_activeIndicator->show();
    m_activeIndicator->raise();
    return;
  }
  if (!animate) {
    m_activeIndicator->move(target);
    m_activeIndicator->raise();
    return;
  }
  if (!m_activeIndicatorAnim) {
    m_activeIndicatorAnim =
        new QPropertyAnimation(m_activeIndicator, "pos", this);
  } else {
    m_activeIndicatorAnim->stop();
  }
  m_activeIndicatorAnim->setDuration(240);
  m_activeIndicatorAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_activeIndicatorAnim->setStartValue(m_activeIndicator->pos());
  m_activeIndicatorAnim->setEndValue(target);
  m_activeIndicatorAnim->start();
  m_activeIndicator->raise();
}

// v3.16.0: applyActiveLift is folded into setToolMode (the existing loop
// over m_buttons + setActive on the matched button already drives the
// per-button QPropertyAnimation(liftOffset)).
void ModernToolbar::applyActiveLift(ToolbarBtn *target) {
  for (auto *b : m_buttons) {
    if (!b)
      continue;
    b->setActive(b == target);
  }
}
void ModernToolbar::setStyle(Style style) {
  if (style == Radial)
    m_radialCollapseTimer.stop();
  // v3.16.1: cancel any in-flight per-button position animations from a
  // previous fan-out / collapse. Without this, switching styles mid-anim
  // leaves the QSequentialAnimationGroups racing the new layout pass, which
  // produced the "buttons drift while the new layout snaps in" stutter the
  // user reported.
  for (auto *b : m_buttons) {
    if (!b) continue;
    for (QPropertyAnimation *anim : b->findChildren<QPropertyAnimation *>())
      anim->stop();
    for (QSequentialAnimationGroup *seq :
         b->findChildren<QSequentialAnimationGroup *>())
      seq->stop();
  }
  if (m_activeIndicatorAnim)
    m_activeIndicatorAnim->stop();
  m_style = style;
  m_cachedMask = QRegion();
  m_showRadialSettings = false; // Hide settings rings when changing main styles
  bool buttonsIgnoreMouse = (style == Radial);
  for (auto *b : m_buttons) {
    b->setAttribute(Qt::WA_TransparentForMouseEvents, buttonsIgnoreMouse);
  }
  if (m_style == Normal) {
    setMinimumSize(UiScale::dp(50), UiScale::dp(65));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (m_orientation == Vertical) {
      setFixedWidth(UiScale::dp(65));
      setMinimumHeight(calculateMinLength());
      setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
      setFixedHeight(UiScale::dp(65));
      setMinimumWidth(calculateMinLength());
      setMaximumWidth(QWIDGETSIZE_MAX);
    }
  } else {
    int size = UiScale::dp(380) * m_scale;
    setFixedSize(size, size);
    reorderButtons();
  }
  for (auto b : m_buttons)
    b->show();
  if (style == Radial && m_radialType == HalfEdge)
    snapToEdge();
  else
    constrainToParent();
  if (style == Radial) {
    m_radialColdFan = true;
    updateLayout(true);
    m_radialColdFan = false;
  } else {
    updateLayout();
  }
  updateHitbox();
  update();
}
void ModernToolbar::setRadialType(RadialType type) {
  if (m_radialType == type)
    return;
  m_radialType = type;
  m_scrollAngle = 0.0;
  if (m_style == Radial) {
    if (m_radialType == HalfEdge && !m_isDragging)
      snapToEdge();
    else {
      if (!m_isPreview && parentWidget() && !m_isDragging) {
        constrainToParent();
      }
    }
    updateLayout(true);
    updateHitbox();
    update();
  }
}
void ModernToolbar::updateHitbox() {
  if (m_isDragging)
    return;
  if (m_style != Radial) {
    clearMask();
    m_cachedMask = QRegion();
    return;
  }
  int r = m_showRadialSettings ? 200 * m_scale : 145 * m_scale;
  int maskR = r + 4;
  QRegion newMask;
  if (m_radialType == FullCircle) {
    int cx = width() / 2;
    int cy = height() / 2;
    newMask =
        QRegion(cx - maskR, cy - maskR, 2 * maskR, 2 * maskR, QRegion::Ellipse);
  } else {
    int cy = height() / 2;
    int cx = m_isDockedLeft ? 0 : width();
    QRegion circleReg(cx - maskR, cy - maskR, 2 * maskR, 2 * maskR,
                      QRegion::Ellipse);
    QRegion widgetReg(rect());
    newMask = circleReg.intersected(widgetReg);
  }
  if (newMask == m_cachedMask)
    return;
  setMask(newMask);
  m_cachedMask = newMask;
}
void ModernToolbar::snapToEdge() {
  if (!parentWidget() || m_isPreview)
    return;
  int parentW = parentWidget()->width();
  int mid = parentW / 2;
  int targetX = 0;
  if (x() + width() / 2 < mid) {
    targetX = 0;
    m_isDockedLeft = true;
  } else {
    targetX = parentW - width();
    m_isDockedLeft = false;
  }
  QPropertyAnimation *anim = new QPropertyAnimation(this, "pos");
  anim->setDuration(300);
  anim->setStartValue(pos());
  anim->setEndValue(QPoint(targetX, y()));
  anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(anim, &QPropertyAnimation::finished, this, [this]() {
    updateLayout();
    updateHitbox();
    update();
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}
void ModernToolbar::wheelEvent(QWheelEvent *e) {
  if (m_style == Normal && m_isDockedMode && m_orientation == Horizontal &&
      supportsAdaptiveDockedScroll() && m_dockedCenterOverflowPx > 0) {
    const int step = qRound(e->angleDelta().y() / 12.0);
    m_dockedCenterScrollPx =
        qBound(0, m_dockedCenterScrollPx - step, m_dockedCenterOverflowPx);
    updateLayout(false);
    e->accept();
  } else if (m_style == Radial && m_radialType == HalfEdge) {
    double delta = e->angleDelta().y() / 8.0;
    m_scrollAngle += delta;
    double maxScroll = (m_buttons.size() * 30.0);
    if (m_scrollAngle > maxScroll)
      m_scrollAngle = maxScroll;
    if (m_scrollAngle < -maxScroll)
      m_scrollAngle = -maxScroll;
    updateLayout();
    e->accept();
  } else {
    QWidget::wheelEvent(e);
  }
}
void ModernToolbar::checkOrientation(const QPoint &globalPos) {
  Q_UNUSED(globalPos);
  if (!parentWidget() || m_style != Normal)
    return;
  QRect myRect = geometry();
  int parentW = parentWidget()->width();
  int parentH = parentWidget()->height();
  int distLeft = myRect.left();
  int distRight = parentW - myRect.right();
  int distTop = myRect.top();
  int distBottom = parentH - myRect.bottom();
  int threshold = UiScale::dp(80);
  bool horizontal = false;
  if (distTop < threshold || distBottom < threshold)
    horizontal = true;
  else if (distLeft < threshold || distRight < threshold)
    horizontal = false;
  else
    return;
  setOrientation(horizontal ? Horizontal : Vertical, true);
}
void ModernToolbar::setOrientation(Orientation o, bool animate) {
  if (m_orientation == o)
    return;
  m_orientation = o;
  m_cachedMask = QRegion();
  QSize currentS = size();
  QSize targetS(currentS.height(), currentS.width());
  if (animate) {
    QPropertyAnimation *anim = new QPropertyAnimation(this, "size");
    anim->setDuration(250);
    anim->setStartValue(currentS);
    anim->setEndValue(targetS);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::valueChanged, this,
            [this](const QVariant &) { updateLayout(); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    resize(targetS);
    updateLayout();
  }
}
void ModernToolbar::setDockMode(bool docked) {
  if (m_isDockedMode == docked) return;
  m_isDockedMode = docked;
  if (btnDockToggle)
    btnDockToggle->setIcon(docked ? QStringLiteral("dock_fixed")
                                  : QStringLiteral("dock_float"));
  m_draggable = !docked;

  // Clear any size constraints set by edge snapping
  setMinimumSize(0, 0);
  setMaximumSize(16777215, 16777215);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  if (QWidget *pw = parentWidget()) {
    QRect targetGeom;
    int idealW = calculateMinLength();
#ifdef Q_OS_ANDROID
    // Cap the parent extents to the real device usable viewport. The usable
    // viewport already excludes a dp(12) safety margin against system
    // cutouts/curved-edge insets that availableGeometry doesn't always
    // report, which prevents the right-most chrome icon from clipping.
    const int usableW = UiScale::androidUsableViewportWidthPx(pw);
    const int parentVisibleW =
        (pw->width() > 0) ? qMin(pw->width(), usableW) : usableW;
    // If the parent reports wider than the usable viewport (e.g. status bar
    // / cutout overlap), shift the toolbar right by half the difference so
    // it appears truly centered on the *visible* viewport.
    const int visibleOffset =
        qMax(0, (pw->width() - parentVisibleW) / 2);
#else
    const int parentVisibleW = pw->width();
    const int visibleOffset = 0;
#endif
    if (docked) {
      m_orientation = Horizontal;
#ifdef Q_OS_ANDROID
      // Anchored top-edge "notch": flush against the parent's top
      // (y=0) so the painter's negative-y rounded rect clips the top
      // corners square and only the bottom corners read as rounded.
      // Width follows the natural button-row, capped at usable width
      // minus dp(8) on each side so it never overflows the right edge
      // (the regression in image 3 of the user's report).
      const int sideMargin = UiScale::dp(8);
      const int maxBarW = qMax(UiScale::dp(180),
                               parentVisibleW - 2 * sideMargin);
      const int barW = qMin(idealW, maxBarW);
      const int xPos = visibleOffset + (parentVisibleW - barW) / 2;
      // Compact 40 dp container - matches the touch-friendly Material
      // 22..32 dp button bounds in effectiveButtonSize().
      targetGeom = QRect(xPos, 0, barW, UiScale::dp(40));
#else
      targetGeom = QRect((parentVisibleW - idealW) / 2, 0,
                         idealW, UiScale::dp(48));
#endif
    } else {
      m_orientation = Horizontal;
      int idealW = calculateMinLength();
#ifdef Q_OS_ANDROID
      idealW = qMin(idealW, parentVisibleW - UiScale::dp(24));
      const int xCentered = visibleOffset + (parentVisibleW - idealW) / 2;
      const int xPos = qMax(xCentered, UiScale::dp(8));
      targetGeom = QRect(xPos, UiScale::dp(56), idealW, UiScale::dp(46));
#else
      targetGeom = QRect((parentVisibleW - idealW) / 2,
                         UiScale::dp(60), idealW, UiScale::dp(52));
#endif
    }

#ifdef Q_OS_ANDROID
    // Clear any leftover setFixedSize / size-cap constraints planted by
    // a previous edge-snap so the new targetGeom can actually take
    // effect. setFixedSize() pins both min AND max, so we also call
    // it explicitly here with QWIDGETSIZE_MAX before clearing min/max
    // again - otherwise the next docked geometry can't widen back
    // (image 3 of the user's report: re-docked toolbar overflowed
    // the right edge because a leftover snap-fixed-size pinned it).
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    // Snap directly to the target geometry on Android - the 300 ms
    // QPropertyAnimation produced a half-collapsed mid-frame layout
    // when transitioning between docked and floating orientations,
    // which is what the user described as "verglitcht" with missing
    // space + visibility. An instant snap matches Material's discrete
    // state transitions and feels native on touch devices.
    setGeometry(targetGeom);
    updateLayout(false);
    update();
#else
    QPropertyAnimation *anim = new QPropertyAnimation(this, "geometry");
    anim->setDuration(300);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setEndValue(targetGeom);
    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
      updateLayout(false);
      update();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
#endif
  } else {
    updateLayout(false);
    update();
  }
}

QList<ToolbarBtn *> ModernToolbar::leftChromeButtons() const {
  QList<ToolbarBtn *> out;
  if (btnBackOverview)
    out.append(btnBackOverview);
  out.append(btnUndo);
  out.append(btnRedo);
  return out;
}

int ModernToolbar::calculateMinLength() {
  int btnS = UiScale::dp(40);
  int minGap = UiScale::dp(6);
  
  if (m_isDockedMode) {
    QList<ToolbarBtn *> leftGroup = leftChromeButtons();
    QList<ToolbarBtn *> rightGroup = {btnPalette, btnBrushSize, btnSave,
                                      btnDockToggle};
    int centerGroupSize = m_buttons.size() - leftGroup.size() - rightGroup.size();
    
    int leftW = leftGroup.size() * btnS + (leftGroup.size() - 1) * minGap;
    int rightW = rightGroup.size() * btnS + (rightGroup.size() - 1) * minGap;
    int centerW = centerGroupSize * btnS + (centerGroupSize - 1) * minGap +
                  UiScale::dp(24); // 24 = 3 separators * 8px
    
    return UiScale::dp(20) + leftW + UiScale::dp(30) + centerW + UiScale::dp(30) +
           rightW + UiScale::dp(20); // margins + group gaps
  } else {
    int dragH = UiScale::dp(30);
    int numButtons = 0;
    for (auto *b : m_buttons) {
      if (m_dockedOnlyButtons.contains(b)) continue;
      numButtons++;
    }
    return dragH + (numButtons * btnS) + ((numButtons - 1) * minGap) + 16;
  }
}
void ModernToolbar::updateLayout(bool animate) {
  if (m_isDragging)
    animate = false;
  if (m_style == Normal) {
    int w = width();
    int h = height();
    int btnS = effectiveButtonSize(w, h);
    
    for (auto *b : m_buttons) {
      b->setBtnSize(btnS);
    }
    
    int numVisible = 0;
    for (auto *b : m_buttons) {
      if (m_dockedOnlyButtons.contains(b)) {
        b->setVisible(m_isDockedMode);
      } else {
        b->setVisible(true);
      }
      if (b->isVisible()) numVisible++;
    }
    
    int dragSize = (m_draggable && !m_isDockedMode) ? UiScale::dp(14) : UiScale::dp(8);
    int gap = effectiveGap();
    m_separatorXPositions.clear();
    
    if (m_isDockedMode) {
      for (ToolbarBtn *b : leftChromeButtons()) {
        if (b->parentWidget() != this) {
          QPoint g = b->parentWidget()->mapToGlobal(b->pos());
          QPoint l = mapFromGlobal(g);
          b->setParent(this);
          b->move(l);
          b->setDrawFloatingBg(false);
          b->show();
        }
      }

      QList<ToolbarBtn *> leftGroup = leftChromeButtons();
      QList<ToolbarBtn *> rightGroup = {btnPalette, btnBrushSize, btnSave,
                                        btnDockToggle};
      QList<ToolbarBtn *> centerGroup;
      for (auto *b : m_buttons) {
          if (!leftGroup.contains(b) && !rightGroup.contains(b)) {
              centerGroup.append(b);
          }
      }
      
      const bool adaptiveCenterScroll = supportsAdaptiveDockedScroll();
      // Calculate widths and adaptive center scroll area.
      int leftStartX = 20;
      const int rightGroupW =
          rightGroup.size() * btnS + (rightGroup.size() - 1) * gap;
      const int rightStartXBase = w - 20 - rightGroupW;

      int centerContentW =
          centerGroup.size() * btnS + (centerGroup.size() - 1) * gap + (3 * 20);
      int leftEndX = leftStartX + leftGroup.size() * btnS +
                     (leftGroup.size() - 1) * gap;
      int centerAreaStart = leftEndX + 20;
      int centerAreaEnd = rightStartXBase - 20;
      if (centerAreaEnd < centerAreaStart)
        centerAreaEnd = centerAreaStart;
      int centerAreaW = centerAreaEnd - centerAreaStart;
      m_dockedCenterOverflowPx =
          adaptiveCenterScroll ? qMax(0, centerContentW - centerAreaW) : 0;
      m_dockedCenterScrollPx =
          qBound(0, m_dockedCenterScrollPx, m_dockedCenterOverflowPx);
      m_dockedCenterAreaStartX = centerAreaStart;
      m_dockedCenterAreaEndX = centerAreaEnd;

      int centerStartX =
          adaptiveCenterScroll
              ? (centerAreaStart - m_dockedCenterScrollPx)
              : ((w - centerContentW) / 2);
      int rightStartX = rightStartXBase;
      
      for (int i = 0; i < m_buttons.size(); ++i) {
        auto *b = m_buttons[i];
        if (!b->isVisible()) continue;
        
        int bx = 0;
        int by = (h - btnS) / 2;

        if (leftGroup.contains(b)) {
            bx = leftStartX;
            leftStartX += btnS + gap;
        } else if (rightGroup.contains(b)) {
            bx = rightStartX;
            rightStartX += btnS + gap;
        } else {
            bx = centerStartX;
            centerStartX += btnS + gap;
            
            if (b == btnRuler || b == btnImage || b == btnHand) {
              centerStartX += 4;
              m_separatorXPositions.append(centerStartX);
              centerStartX += gap + 4;
            }
            if (adaptiveCenterScroll) {
              const bool inVisibleCenterWindow =
                  (bx + btnS >= centerAreaStart) && (bx <= centerAreaEnd);
              b->setVisible(inVisibleCenterWindow);
            }
        }
        
        if (!b->isVisible())
          continue;

        if (animate) {
          QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
          anim->setDuration(200);
          anim->setEndValue(QPoint(bx, by));
          anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
          b->move(bx, by);
        }
      }
    } else {
      m_dockedCenterOverflowPx = 0;
      m_dockedCenterScrollPx = 0;
      m_dockedCenterAreaStartX = 0;
      m_dockedCenterAreaEndX = 0;
      int currentPos = dragSize;
      const QList<ToolbarBtn *> chromeRow = leftChromeButtons();

      // Undock: back + undo + redo in one row on the editor surface
      int floaterX = 16;
      const int floaterY = 18;
      for (ToolbarBtn *b : chromeRow) {
        if (QWidget *pw = parentWidget()) {
          if (b->parentWidget() == this) {
            QPoint g = mapToGlobal(b->pos());
            QPoint lp = pw->mapFromGlobal(g);
            b->setParent(pw);
            b->move(lp);
            b->setDrawFloatingBg(true);
            b->show();
          }
          QPoint targetPos(floaterX, floaterY);
          if (animate) {
            QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
            anim->setDuration(200);
            anim->setEndValue(targetPos);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
          } else {
            b->move(targetPos);
          }
          b->raise();
          floaterX += btnS + gap;
        }
      }

      for (auto *b : m_buttons) {
        if (chromeRow.contains(b)) continue;
        if (!b->isVisible()) continue;
        int bx, by;
        if (m_orientation == Vertical) {
          bx = (w - btnS) / 2;
          by = currentPos;
        } else {
          bx = currentPos;
          by = (h - btnS) / 2;
        }
        currentPos += btnS + gap;
        if (animate) {
          QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
          anim->setDuration(200);
          anim->setEndValue(QPoint(bx, by));
          anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
          b->move(bx, by);
        }
        b->show();
      }
    }
  } else {
    int cx = width() / 2;
    int cy = height() / 2;
    int paintCx = cx;
    if (m_radialType == HalfEdge) {
        paintCx = m_isDockedLeft ? 0 : width();
    }
    int btnS = 40 * m_scale;
    const int gap = 6;
    
    QList<ToolbarBtn*> radialBtns = {
      btnPen, btnPencil, btnHighlighter, btnEraser,
      btnLasso, btnRuler, btnShape, btnStickyNote,
      btnText, btnImage, btnHand
    };
    
    // Undock: back + undo + redo in one row
    {
      int floaterX = 16;
      const int floaterY = 18;
      for (ToolbarBtn *b : leftChromeButtons()) {
        b->setBtnSize(btnS);
        if (QWidget *pw = parentWidget()) {
          if (b->parentWidget() == this) {
            QPoint g = mapToGlobal(b->pos());
            QPoint lp = pw->mapFromGlobal(g);
            b->setParent(pw);
            b->move(lp);
            b->setDrawFloatingBg(true);
            b->show();
          }
          QPoint targetPos(floaterX, floaterY);
          if (animate) {
            QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
            anim->setDuration(200);
            anim->setEndValue(targetPos);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
          } else {
            b->move(targetPos);
          }
          b->raise();
          floaterX += btnS + gap;
        }
      }
    }
    
    ToolbarBtn *activeBtn = getButtonForMode(mode_);
    if (!activeBtn || !radialBtns.contains(activeBtn)) {
        activeBtn = btnPen;
    }
    
    const QList<ToolbarBtn *> chrome = leftChromeButtons();
    for (auto *b : m_buttons) {
      if (chrome.contains(b))
        continue;
      if (!radialBtns.contains(b)) {
        b->hide();
      } else {
        b->setBtnSize(btnS);
      }
    }
    
    activeBtn->setBtnSize(btnS + 12);

    if (animate && m_radialColdFan) {
      for (auto *b : radialBtns) {
        if (!b)
          continue;
        b->move(cx - b->width() / 2, cy - b->height() / 2);
      }
    }

    // v3.16.0 staggered fan-out: when entering radial mode (animate=true),
    // each button briefly waits its index*30ms before its OutBack position
    // animation runs. The slight stagger reads as a wave that "spreads"
    // out from the center, instead of all icons popping out at once.
    auto spreadTo = [this](ToolbarBtn *b, QPoint target, int idx, bool doAnim,
                           int cx, int cy) {
      const QPoint center(cx - b->width() / 2, cy - b->height() / 2);
      if (!doAnim) {
        b->move(target);
        return;
      }
      if (b->isHidden())
        b->move(center);
      auto *seq = new QSequentialAnimationGroup(b);
      if (idx > 0)
        seq->addPause(idx * 30);
      auto *anim = new QPropertyAnimation(b, "pos");
      anim->setDuration(260);
      anim->setEasingCurve(QEasingCurve::OutBack);
      anim->setStartValue(b->isHidden() ? center : b->pos());
      anim->setEndValue(target);
      seq->addAnimation(anim);
      seq->start(QAbstractAnimation::DeleteWhenStopped);
    };

    if (m_radialType == FullCircle) {
      const QPoint activeTarget(cx - (btnS + 12) / 2, cy - (btnS + 12) / 2);
      spreadTo(activeBtn, activeTarget, 0, animate, cx, cy);
      activeBtn->show();
    }

    if (m_radialType == HalfEdge) {
      int r = 110 * m_scale;
      for (int i = 0; i < radialBtns.size(); ++i) {
        auto *b = radialBtns[i];
        double angle =
            (m_isDockedLeft ? 0.0 : 180.0) + ((i - 2) * 35.0) + m_scrollAngle;
        double rad = angle * 3.14159 / 180.0;
        int currentSize = (b == activeBtn) ? (btnS + 12) : btnS;
        int targetX = paintCx + r * cos(rad) - currentSize / 2;
        int targetY = cy + r * sin(rad) - currentSize / 2;
        spreadTo(b, QPoint(targetX, targetY), i, animate, paintCx, cy);
        b->setVisible(m_isDockedLeft ? cos(rad) > 0.05 : cos(rad) < -0.05);
      }
    } else {
      int r = 90 * m_scale;
      double angleStep = 360.0 / radialBtns.size();
      int visibleIdx = 0;
      for (int i = 0; i < radialBtns.size(); ++i) {
        if (radialBtns[i] == activeBtn)
          continue;
        double rad = (-90.0 + i * angleStep) * 3.14159 / 180.0;
        int targetX = cx + r * cos(rad) - btnS / 2;
        int targetY = cy + r * sin(rad) - btnS / 2;
        // visibleIdx instead of i so the stagger sees a contiguous sequence
        // (active button counts as idx=0 above, others start at idx=1).
        spreadTo(radialBtns[i], QPoint(targetX, targetY), ++visibleIdx,
                 animate, cx, cy);
        radialBtns[i]->show();
      }
    }
    
    // Position the Setting Ring
    if (m_showRadialSettings) {
        int outerR = 155 * m_scale;
        
        if (m_radialType == HalfEdge) {
            double angleStep = 180.0 / (m_radialSettingsBtns.size() - 1);
            for (int i = 0; i < m_radialSettingsBtns.size(); ++i) {
                auto *btn = m_radialSettingsBtns[i];
                double angle = (m_isDockedLeft ? -90.0 : 90.0) + (i * angleStep);
                double rad = angle * 3.14159 / 180.0;
                int targetX = paintCx + outerR * cos(rad) - btn->width() / 2;
                int targetY = cy + outerR * sin(rad) - btn->height() / 2;
                
                if (animate) {
                    QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos");
                    anim->setDuration(250);
                    anim->setEasingCurve(QEasingCurve::OutBack);
                    if (btn->isHidden()) {
                        btn->move(paintCx - btn->width() / 2, cy - btn->height() / 2);
                    }
                    anim->setEndValue(QPoint(targetX, targetY));
                    anim->start(QAbstractAnimation::DeleteWhenStopped);
                } else {
                    btn->move(targetX, targetY);
                }
                btn->show();
                btn->raise();
                btn->setVisible(m_isDockedLeft ? cos(rad) > 0.05 : cos(rad) < -0.05);
            }
        } else {
            double outerAngleStep = 360.0 / m_radialSettingsBtns.size();
            for (int i = 0; i < m_radialSettingsBtns.size(); ++i) {
                auto *btn = m_radialSettingsBtns[i];
                double rad = (-90.0 + i * outerAngleStep) * 3.14159 / 180.0;
                int targetX = cx + outerR * cos(rad) - btn->width() / 2;
                int targetY = cy + outerR * sin(rad) - btn->height() / 2;
                
                if (animate) {
                    QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos");
                    anim->setDuration(250);
                    anim->setEasingCurve(QEasingCurve::OutBack);
                    if (btn->isHidden()) {
                        btn->move(cx - btn->width() / 2, cy - btn->height() / 2);
                    }
                    anim->setEndValue(QPoint(targetX, targetY));
                    anim->start(QAbstractAnimation::DeleteWhenStopped);
                } else {
                    btn->move(targetX, targetY);
                }
                btn->show();
                btn->raise();
            }
        }
    } else {
        for (auto *btn : m_radialSettingsBtns) {
            btn->hide();
        }
    }
  }

  // v3.16.1: ALWAYS reposition the indicator instantly from updateLayout.
  // Previously this passed `animate` through, so every resize / drag / dock
  // flip kept restarting the 240ms pos-animation and the indicator looked
  // like it was lagging behind the buttons. The animated path is only
  // triggered from setToolMode() / setStyle() where the user explicitly
  // changes the active tool or layout style.
  updateActiveIndicator(false);
}
