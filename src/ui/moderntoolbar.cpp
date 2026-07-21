#include "moderntoolbar.h"
#include "blop_theme.h"
#include "blop_dialogs.h"
#include "blopripple.h"
#include "notechrome.h"
#include "UIStyles.h"
#include "toolpickeroverlay.h"
#include "markuplibrarystore.h"
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
#include <QFrame>
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
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QFont>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QSettings>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPauseAnimation>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QPushButton>
#include <QRegion>
#include <QSlider>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QToolButton>
#include <QPixmap>
#include <QIcon>
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

// 64×64 logical coords — Drawboard-faithful Favorites glyphs.
// `color` is the functional tip tint when saturated (ink / highlighter / measure);
// otherwise icons render as clean light outlines like Drawboard's dark rail.
void drawToolbarGlyph64(QPainter *p, const QString &name, const QColor &color) {
  p->setRenderHint(QPainter::Antialiasing);
  const bool hasTip =
      color.isValid() && color.saturationF() > 0.18 && color.valueF() > 0.18;
  const QColor primary(245, 247, 250, 235);
  const QColor tip = hasTip ? color : primary;
  const QColor accent = tip;
  QColor accentSoft = tip;
  accentSoft.setAlpha(hasTip ? 200 : 120);

  const QPen primaryPen(primary, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen primaryThin(primary, 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen accentPen(accent, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen accentThick(accent, 5.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

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
    // Drawboard felt-tip: body outline + colored tip.
    QPainterPath body;
    body.moveTo(20, 46);
    body.lineTo(38, 20);
    body.lineTo(46, 26);
    body.lineTo(28, 52);
    body.closeSubpath();
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(body);
    QPainterPath nib;
    nib.moveTo(38, 20);
    nib.lineTo(48, 14);
    nib.lineTo(46, 26);
    nib.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(hasTip ? tip : primary);
    p->drawPath(nib);
    p->setPen(QPen(hasTip ? tip : primary, 3.0, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(22, 48, 18, 54);
    return;
  }
  if (name == QLatin1String("pencil")) {
    QPainterPath shaft;
    shaft.moveTo(18, 46);
    shaft.lineTo(40, 20);
    shaft.lineTo(48, 28);
    shaft.lineTo(26, 54);
    shaft.closeSubpath();
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(shaft);
    p->setPen(primaryThin);
    p->drawLine(24, 48, 42, 26);
    QPainterPath tipPath;
    tipPath.moveTo(40, 20);
    tipPath.lineTo(50, 14);
    tipPath.lineTo(48, 28);
    tipPath.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(hasTip ? tip : QColor(230, 210, 170, 230));
    p->drawPath(tipPath);
    return;
  }
  if (name == QLatin1String("highlighter")) {
    // Drawboard chisel highlighter — wide tip in tip color (default lime).
    const QColor hi = hasTip ? tip : QColor(90, 220, 70);
    QPainterPath body;
    body.moveTo(16, 42);
    body.lineTo(34, 18);
    body.lineTo(48, 30);
    body.lineTo(30, 54);
    body.closeSubpath();
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(body);
    QPainterPath chisel;
    chisel.moveTo(34, 18);
    chisel.lineTo(42, 12);
    chisel.lineTo(56, 26);
    chisel.lineTo(48, 30);
    chisel.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(hi);
    p->drawPath(chisel);
    return;
  }
  if (name == QLatin1String("eraser")) {
    // Drawboard eraser block (angled).
    QTransform t;
    t.translate(32, 32);
    t.rotate(-28);
    t.translate(-32, -32);
    p->save();
    p->setTransform(t, true);
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(16, 22, 32, 20), 4, 4);
    p->setPen(primaryThin);
    p->drawLine(16, 32, 48, 32);
    p->restore();
    return;
  }
  if (name == QLatin1String("select") || name == QLatin1String("lasso")) {
    // Drawboard rectangle-select: dashed marquee + cursor.
    QPen dash(primary, 2.6, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
    dash.setDashPattern({3.5, 2.8});
    p->setPen(dash);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(12, 12, 30, 28), 3, 3);
    // Cursor arrow
    QPainterPath cursor;
    cursor.moveTo(34, 30);
    cursor.lineTo(34, 52);
    cursor.lineTo(40, 46);
    cursor.lineTo(46, 56);
    cursor.lineTo(50, 54);
    cursor.lineTo(44, 44);
    cursor.lineTo(52, 44);
    cursor.closeSubpath();
    p->setPen(QPen(primary, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(primary);
    p->drawPath(cursor);
    return;
  }
  if (name == QLatin1String("ruler") || name == QLatin1String("measure")) {
    // Drawboard measure: dimension line with end ticks (red when tipped).
    const QColor mcol = hasTip ? tip : QColor(235, 70, 70);
    QPen mp(mcol, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(mp);
    p->drawLine(14, 40, 50, 22);
    p->drawLine(14, 40, 14, 48);
    p->drawLine(50, 22, 50, 30);
    p->drawLine(14, 40, 20, 34);
    p->drawLine(14, 40, 20, 46);
    p->drawLine(50, 22, 44, 16);
    p->drawLine(50, 22, 44, 28);
    return;
  }
  if (name == QLatin1String("rect") || name == QLatin1String("shape")) {
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawRect(QRectF(16, 16, 32, 32));
    return;
  }
  if (name == QLatin1String("ellipse") || name == QLatin1String("circle")) {
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawEllipse(QRectF(15, 15, 34, 34));
    return;
  }
  if (name == QLatin1String("line")) {
    p->setPen(QPen(primary, 3.4, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(16, 48, 48, 16);
    return;
  }
  if (name == QLatin1String("arrow")) {
    p->setPen(QPen(primary, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawLine(16, 48, 48, 16);
    p->drawLine(48, 16, 34, 18);
    p->drawLine(48, 16, 46, 30);
    return;
  }
  if (name == QLatin1String("axes") || name == QLatin1String("axes2d")) {
    p->setPen(QPen(primary, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawLine(14, 50, 50, 50);
    p->drawLine(14, 50, 14, 14);
    p->drawLine(50, 50, 44, 46);
    p->drawLine(50, 50, 44, 54);
    p->drawLine(14, 14, 10, 20);
    p->drawLine(14, 14, 18, 20);
    return;
  }
  if (name == QLatin1String("sine") || name == QLatin1String("sinegraph")) {
    p->setPen(QPen(primary, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath wave;
    wave.moveTo(12, 32);
    wave.cubicTo(20, 12, 28, 12, 32, 32);
    wave.cubicTo(36, 52, 44, 52, 52, 32);
    p->drawPath(wave);
    return;
  }
  if (name == QLatin1String("graph") || name == QLatin1String("coordinategraph")) {
    p->setPen(primaryThin);
    p->drawLine(14, 50, 50, 50);
    p->drawLine(14, 50, 14, 14);
    p->setPen(QPen(primary, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath curve;
    curve.moveTo(18, 44);
    curve.cubicTo(26, 44, 30, 18, 38, 22);
    curve.cubicTo(44, 24, 46, 36, 50, 30);
    p->drawPath(curve);
    return;
  }
  if (name == QLatin1String("bookmark") || name == QLatin1String("bookmarks")) {
    QPainterPath ribbon;
    ribbon.moveTo(20, 12);
    ribbon.lineTo(44, 12);
    ribbon.lineTo(44, 52);
    ribbon.lineTo(32, 42);
    ribbon.lineTo(20, 52);
    ribbon.closeSubpath();
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(ribbon);
    return;
  }
  if (name == QLatin1String("history") || name == QLatin1String("clock")) {
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawEllipse(QRectF(14, 14, 36, 36));
    p->setPen(QPen(primary, 3.0, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(32, 22, 32, 34);
    p->drawLine(32, 34, 42, 34);
    return;
  }
  if (name == QLatin1String("stickynote") || name == QLatin1String("note")) {
    // Document / sticky note page.
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    QPainterPath page;
    page.moveTo(20, 12);
    page.lineTo(40, 12);
    page.lineTo(48, 20);
    page.lineTo(48, 52);
    page.lineTo(16, 52);
    page.lineTo(16, 12);
    page.closeSubpath();
    p->drawPath(page);
    p->drawLine(40, 12, 40, 22);
    p->drawLine(40, 22, 48, 22);
    p->setPen(primaryThin);
    p->drawLine(22, 30, 42, 30);
    p->drawLine(22, 38, 38, 38);
    return;
  }
  if (name == QLatin1String("text")) {
    QFont f = p->font();
    f.setPixelSize(34);
    f.setBold(true);
    f.setFamily(QStringLiteral("Segoe UI"));
    p->setFont(f);
    p->setPen(primary);
    p->drawText(QRect(0, 0, 64, 64), Qt::AlignCenter, QStringLiteral("T"));
    return;
  }
  if (name == QLatin1String("image")) {
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(12, 16, 40, 32), 4, 4);
    p->drawEllipse(QRectF(18, 22, 8, 8));
    QPainterPath mountain;
    mountain.moveTo(14, 42);
    mountain.lineTo(26, 28);
    mountain.lineTo(34, 36);
    mountain.lineTo(44, 24);
    mountain.lineTo(50, 42);
    p->drawPath(mountain);
    return;
  }
  if (name == QLatin1String("hand")) {
    // Drawboard pan hand — open outline.
    QPainterPath hand;
    hand.moveTo(24, 34);
    hand.lineTo(24, 22);
    hand.cubicTo(24, 18, 28, 16, 30, 20);
    hand.lineTo(30, 30);
    hand.lineTo(32, 16);
    hand.cubicTo(32, 13, 36, 13, 36, 17);
    hand.lineTo(36, 30);
    hand.lineTo(40, 18);
    hand.cubicTo(40, 15, 44, 15, 44, 19);
    hand.lineTo(44, 34);
    hand.cubicTo(44, 46, 36, 52, 28, 50);
    hand.cubicTo(22, 48, 20, 42, 24, 34);
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(hand);
    return;
  }
  if (name == QLatin1String("library")) {
    // Drawboard markup library — three books.
    p->setPen(primaryPen);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(QRectF(12, 18, 12, 32), 2, 2);
    p->drawRoundedRect(QRectF(26, 14, 12, 36), 2, 2);
    p->drawRoundedRect(QRectF(40, 20, 12, 30), 2, 2);
    p->setPen(primaryThin);
    p->drawLine(15, 26, 21, 26);
    p->drawLine(29, 22, 35, 22);
    p->drawLine(43, 28, 49, 28);
    return;
  }
  if (name == QLatin1String("chevron_rail") || name == QLatin1String("chevron_right")) {
    p->setPen(QPen(primary, 3.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawLine(26, 18, 40, 32);
    p->drawLine(40, 32, 26, 46);
    return;
  }
  if (name == QLatin1String("chevron_left")) {
    p->setPen(QPen(primary, 3.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawLine(38, 18, 24, 32);
    p->drawLine(24, 32, 38, 46);
    return;
  }
  if (name == QLatin1String("zoom_in")) {
    p->setPen(primaryPen);
    p->drawEllipse(QPointF(28, 28), 14, 14);
    p->setPen(QPen(primary, 3.6, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(38, 38, 50, 50);
    p->drawLine(28, 21, 28, 35);
    p->drawLine(21, 28, 35, 28);
    return;
  }
  if (name == QLatin1String("zoom_out")) {
    p->setPen(primaryPen);
    p->drawEllipse(QPointF(28, 28), 14, 14);
    p->setPen(QPen(primary, 3.6, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(38, 38, 50, 50);
    p->drawLine(21, 28, 35, 28);
    return;
  }
  if (name == QLatin1String("fit_width")) {
    p->setPen(primaryThin);
    p->drawRoundedRect(14, 20, 36, 24, 3, 3);
    p->setPen(QPen(primary, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->drawLine(18, 32, 46, 32);
    p->drawLine(18, 32, 24, 26);
    p->drawLine(18, 32, 24, 38);
    p->drawLine(46, 32, 40, 26);
    p->drawLine(46, 32, 40, 38);
    return;
  }
  if (name == QLatin1String("fit_page")) {
    p->setPen(primaryPen);
    p->drawRoundedRect(18, 14, 28, 36, 4, 4);
    p->setPen(accentPen);
    p->drawLine(24, 24, 40, 24);
    p->drawLine(24, 32, 40, 32);
    p->drawLine(24, 40, 34, 40);
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
  if (name == QLatin1String("add") || name == QLatin1String("plus")) {
    p->setPen(QPen(primary, 4.0, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(QPointF(20, 32), QPointF(44, 32));
    p->drawLine(QPointF(32, 20), QPointF(32, 44));
    return;
  }
  if (name == QLatin1String("layout_rows")) {
    // Two-row markup layout glyph.
    p->setPen(primaryThin);
    p->drawRoundedRect(14, 16, 36, 12, 3, 3);
    p->drawRoundedRect(14, 36, 36, 12, 3, 3);
    p->setPen(accentPen);
    p->drawLine(20, 22, 44, 22);
    p->drawLine(20, 42, 36, 42);
    return;
  }
  if (name == QLatin1String("layout_single")) {
    p->setPen(primaryThin);
    p->drawRoundedRect(14, 24, 36, 16, 4, 4);
    p->setPen(accentPen);
    p->drawLine(20, 32, 44, 32);
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
// Drawboard Markup category tab (icon + label)
// =============================================================================
MarkupCategoryTab::MarkupCategoryTab(const QString &iconName,
                                     const QString &label, QWidget *parent)
    : QWidget(parent), m_icon(iconName), m_label(label) {
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFixedHeight(UiScale::dp(34));
  setMinimumWidth(UiScale::dp(72));
}

void MarkupCategoryTab::setActive(bool active) {
  if (m_active == active)
    return;
  m_active = active;
  update();
}

void MarkupCategoryTab::setAccentColor(const QColor &c) {
  if (!c.isValid() || m_accent == c)
    return;
  m_accent = c;
  update();
}

void MarkupCategoryTab::setCompact(bool compact) {
  if (m_compact == compact)
    return;
  m_compact = compact;
  setMinimumWidth(compact ? UiScale::dp(36) : UiScale::dp(72));
  update();
}

void MarkupCategoryTab::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRect r = rect().adjusted(1, 1, -1, -1);
  if (m_active || m_hover) {
    QColor fill = m_active ? QColor(m_accent.red(), m_accent.green(),
                                    m_accent.blue(), 48)
                           : QColor(255, 255, 255, 14);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(r, UiScale::dp(8), UiScale::dp(8));
  }

  const int iconBox = m_compact ? qMin(width(), height()) - 4
                                : UiScale::dp(18);
  const int iconX = m_compact ? (width() - iconBox) / 2 : UiScale::dp(8);
  const int iconY = (height() - iconBox) / 2;
  p.save();
  p.translate(iconX, iconY);
  p.scale(iconBox / 64.0, iconBox / 64.0);
  const QColor glyph =
      m_active ? m_accent : NoteChrome::textPrimary();
  blopDrawToolbarGlyph64(&p, m_icon, glyph);
  p.restore();

  if (!m_compact) {
    QFont f = p.font();
    f.setPixelSize(UiScale::sp(11));
    f.setWeight(m_active ? QFont::DemiBold : QFont::Medium);
    p.setFont(f);
    p.setPen(m_active ? m_accent : NoteChrome::textSecondary());
    const QRect textR(iconX + iconBox + UiScale::dp(6), 0,
                      width() - iconX - iconBox - UiScale::dp(10), height());
    p.drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, m_label);
  }

  if (m_active) {
    p.setPen(Qt::NoPen);
    p.setBrush(m_accent);
    p.drawRoundedRect(UiScale::dp(10), height() - UiScale::dp(3),
                      width() - UiScale::dp(20), UiScale::dp(2), 1, 1);
  }
}

void MarkupCategoryTab::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton)
    emit clicked();
  QWidget::mousePressEvent(e);
}

void MarkupCategoryTab::enterEvent(QEnterEvent *e) {
  m_hover = true;
  update();
  QWidget::enterEvent(e);
}

void MarkupCategoryTab::leaveEvent(QEvent *e) {
  m_hover = false;
  update();
  QWidget::leaveEvent(e);
}

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
void ToolbarBtn::setBtnCell(int w, int h) {
  m_size = qMin(w, h);
  setFixedSize(w, h);
  update();
}
void ToolbarBtn::setIcon(const QString &name) {
  m_iconName = name;
  update();
}
void ToolbarBtn::setDrawFloatingBg(bool draw) {
  m_drawFloatingBg = draw;
  update();
}
void ToolbarBtn::setRailSlotStyle(bool enable) {
  if (m_railSlotStyle == enable)
    return;
  m_railSlotStyle = enable;
  if (enable) {
    // Drawboard rail: no Samsung-style lift.
    if (m_liftAnim)
      m_liftAnim->stop();
    m_liftOffset = 0.0;
  } else {
    m_railFooterStyle = false;
  }
  update();
}
void ToolbarBtn::setRailFooterStyle(bool enable) {
  if (m_railFooterStyle == enable)
    return;
  m_railFooterStyle = enable;
  update();
}
void ToolbarBtn::setGlyphColor(const QColor &c) {
  if (m_glyphColor == c)
    return;
  m_glyphColor = c;
  update();
}
void ToolbarBtn::setShowChevron(bool show) {
  if (m_showChevron == show)
    return;
  m_showChevron = show;
  update();
}
void ToolbarBtn::setBadgeText(const QString &text) {
  if (m_badgeText == text)
    return;
  m_badgeText = text;
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

  if (m_railSlotStyle) {
    if (m_liftAnim)
      m_liftAnim->stop();
    m_liftOffset = 0.0;
    update();
    return;
  }

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

void ToolbarBtn::contextMenuEvent(QContextMenuEvent *e) {
  if (!m_railSlotStyle || m_railFooterStyle || m_railSlotIndex < 0) {
    QWidget::contextMenuEvent(e);
    return;
  }
  QMenu menu(this);
  menu.setStyleSheet(QStringLiteral(
      "QMenu { background: %1; color: %2; border: 1px solid %3; padding: 4px; }"
      "QMenu::item { padding: 6px 16px; }"
      "QMenu::item:selected { background: rgba(91,157,255,0.28); }")
                         .arg(NoteChrome::panelElevated().name(),
                              NoteChrome::textPrimary().name(),
                              NoteChrome::border().name()));
  QAction *up = menu.addAction(tr("Nach oben"));
  QAction *down = menu.addAction(tr("Nach unten"));
  menu.addSeparator();
  QAction *remove = menu.addAction(tr("Aus Symbolleiste entfernen"));
  QAction *picked = menu.exec(e->globalPos());
  if (picked == up)
    emit moveInRailRequested(-1);
  else if (picked == down)
    emit moveInRailRequested(+1);
  else if (picked == remove)
    emit removeFromRailRequested();
  e->accept();
}
void ToolbarBtn::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton && e->button() != Qt::NoButton) {
    QWidget::mousePressEvent(e);
    return;
  }
  m_pressing = true;
  m_longPressTriggered = false;
  m_railDragging = false;
  m_pressPos = e->pos();
  m_holdProgress = 0.0;

  // Drawboard rail slots: defer click to release so drag-reorder / chevron work.
  if (m_railSlotStyle) {
#if BLOP_TOOLBAR_LONGPRESS
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
#endif
    update();
    e->accept();
    return;
  }

#if BLOP_TOOLBAR_LONGPRESS
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
#else
  BlopRipple::spawn(this, mapToGlobal(rect().center()), m_accentColor);
  emit clicked();
#endif
}

void ToolbarBtn::mouseMoveEvent(QMouseEvent *e) {
  if (m_railSlotStyle && m_pressing && m_railSlotIndex >= 0 &&
      !m_railDragging) {
    const QPoint d = e->pos() - m_pressPos;
    if (d.manhattanLength() >= QApplication::startDragDistance()) {
      m_railDragging = true;
      m_longPressTriggered = true;
      if (m_holdAnim)
        m_holdAnim->stop();
      m_holdProgress = 0.0;
      emit railDragStarted(m_railSlotIndex);
    }
  }
  if (m_railDragging) {
    emit railDragMoved(mapToGlobal(e->pos()).y());
    e->accept();
    return;
  }
  QWidget::mouseMoveEvent(e);
}

void ToolbarBtn::mouseReleaseEvent(QMouseEvent *e) {
  const bool inside = rect().contains(e->pos());
  if (m_holdAnim)
    m_holdAnim->stop();

  if (m_railDragging) {
    emit railDragFinished(mapToGlobal(e->pos()).y());
    m_railDragging = false;
    m_pressing = false;
    m_longPressTriggered = false;
    m_holdProgress = 0.0;
    update();
    e->accept();
    return;
  }

  const bool shouldClick = m_pressing && !m_longPressTriggered && inside &&
                           (e->button() == Qt::LeftButton ||
                            e->button() == Qt::NoButton);
  m_pressing = false;
  m_longPressTriggered = false;
  m_holdProgress = 0.0;
  update();

  if (shouldClick && m_railSlotStyle) {
    // Bottom chevron hit zone → flyout affordance.
    if (m_showChevron && e->pos().y() >= height() - UiScale::dp(14)) {
      BlopRipple::spawn(this, mapToGlobal(rect().center()), m_accentColor);
      emit chevronClicked();
      e->accept();
      return;
    }
    BlopRipple::spawn(this, mapToGlobal(rect().center()), m_accentColor);
    emit clicked();
    e->accept();
    return;
  }

#if BLOP_TOOLBAR_LONGPRESS
  if (shouldClick) {
    BlopRipple::spawn(this, mapToGlobal(rect().center()), m_accentColor);
    emit clicked();
    e->accept();
    return;
  }
#endif
  QWidget::mouseReleaseEvent(e);
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
    p.setBrush(NoteChrome::panelElevated());
    p.setPen(QPen(NoteChrome::border(), 1));
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));
  }

  const int w = width();
  const int h = height();
  const int r = m_size / 2 - 4;

  if (m_railSlotStyle) {
    // Drawboard vertical Favorites rail: full-width active slot + accent edge.
    // Footer affordances stay quieter (no accent edge, softer glyphs).
    if (m_pressing && !m_railDragging) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(255, 255, 255, NoteChrome::isDark() ? 28 : 40));
      p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), UiScale::dp(4),
                        UiScale::dp(4));
    } else if (m_active && !m_railFooterStyle) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, NoteChrome::isDark() ? 170 : 36));
      p.drawRoundedRect(rect().adjusted(0, 1, 0, -1), UiScale::dp(4),
                        UiScale::dp(4));
      p.setBrush(m_accentColor.isValid() ? m_accentColor : NoteChrome::accent());
      p.drawRoundedRect(QRect(0, UiScale::dp(6), UiScale::dp(3),
                              h - UiScale::dp(12)),
                        UiScale::dp(1), UiScale::dp(1));
    } else if (m_hover) {
      p.setPen(Qt::NoPen);
      const int a = m_railFooterStyle
                        ? (NoteChrome::isDark() ? 12 : 20)
                        : (NoteChrome::isDark() ? 16 : 28);
      p.setBrush(QColor(255, 255, 255, a));
      p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), UiScale::dp(4),
                        UiScale::dp(4));
    }

    QColor tip =
        m_glyphColor.isValid() ? m_glyphColor : NoteChrome::textPrimary();
    if (m_railFooterStyle && !m_glyphColor.isValid()) {
      tip = NoteChrome::textSecondary();
      if (m_hover || m_pressing)
        tip = NoteChrome::textPrimary();
    } else if (!m_active && !m_railFooterStyle && !m_glyphColor.isValid()) {
      tip = NoteChrome::textPrimary();
      tip.setAlphaF(NoteChrome::isDark() ? 0.55 : 0.48);
    }
    // Reserve space for the flyout chevron so the glyph never overlaps it.
    const int chevronReserve =
        (m_showChevron && !m_railFooterStyle) ? UiScale::dp(14) : 0;
    const int iconPad = m_railFooterStyle ? UiScale::dp(14) : UiScale::dp(12);
    const int iconBoxH = qMax(UiScale::dp(18), h - chevronReserve);
    const int icon = qMin(w - iconPad, iconBoxH - UiScale::dp(4));
    p.save();
    p.translate(w / 2.0, iconBoxH / 2.0);
    p.scale(m_animScale, m_animScale);
    const qreal g = icon / 64.0;
    p.scale(g, g);
    p.translate(-32, -32);
    drawToolbarGlyph64(&p, m_iconName, tip);
    p.restore();

    if (m_showChevron && !m_railFooterStyle) {
      p.setPen(QPen(NoteChrome::textSecondary(), 1.7, Qt::SolidLine,
                    Qt::RoundCap, Qt::RoundJoin));
      const int cx = w / 2;
      const int cy = h - UiScale::dp(8);
      p.drawLine(cx - 4, cy - 2, cx, cy + 2);
      p.drawLine(cx, cy + 2, cx + 4, cy - 2);
    }

    if (!m_badgeText.isEmpty()) {
      const int bw =
          qMax(UiScale::dp(12), 4 + m_badgeText.size() * UiScale::dp(5));
      const int bh = UiScale::dp(12);
      const QRectF badge(w - bw - UiScale::dp(1), UiScale::dp(1), bw, bh);
      p.setPen(NoteChrome::textPrimary());
      QFont f = p.font();
      f.setPixelSize(UiScale::sp(9));
      f.setBold(true);
      p.setFont(f);
      p.drawText(badge, Qt::AlignRight | Qt::AlignVCenter, m_badgeText);
    }
    return;
  }

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
    p.setBrush(NoteChrome::accent());
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect().center(), r, r);
  } else if (m_hover) {
    p.setBrush(QColor(255, 255, 255, 28));
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect().center(), r, r);
  }

  const QColor fg =
      m_active ? QColor(255, 255, 255, 255) : NoteChrome::textPrimary();
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

  // Quiet size badge (Drawboard keeps properties elsewhere — keep subtle).
  if (!m_badgeText.isEmpty()) {
    const int bw = qMax(UiScale::dp(14), 6 + m_badgeText.size() * UiScale::dp(5));
    const int bh = UiScale::dp(13);
    const QRectF badge(w - bw - 1, 1, bw, bh);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawRoundedRect(badge, bh / 2.0, bh / 2.0);
    p.setPen(NoteChrome::textSecondary());
    QFont f = p.font();
    f.setPixelSize(UiScale::sp(8));
    f.setBold(true);
    p.setFont(f);
    p.drawText(badge, Qt::AlignCenter, m_badgeText);
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

    // Drawboard-like tool options header.
    {
      QString title = QStringLiteral("Werkzeug");
      switch (m_mode) {
      case ToolMode::Pen:
        title = QStringLiteral("Stift");
        break;
      case ToolMode::Pencil:
        title = QStringLiteral("Bleistift");
        break;
      case ToolMode::Highlighter:
        title = QStringLiteral("Textmarker");
        break;
      case ToolMode::Eraser:
        title = QStringLiteral("Radiergummi");
        break;
      case ToolMode::Lasso:
        title = QStringLiteral("Auswahl");
        break;
      case ToolMode::Ruler:
        title = QStringLiteral("Lineal / Messen");
        break;
      case ToolMode::Shape:
        title = QStringLiteral("Formen");
        break;
      default:
        break;
      }
      auto *header = new QLabel(title, this);
      header->setStyleSheet(QStringLiteral(
          "color: #F4F5FB; font-size: 16px; font-weight: 800; "
          "letter-spacing: -0.2px; background: transparent;"));
      layout->addWidget(header);
      auto *sub = new QLabel(QStringLiteral("Tooloptionen"), this);
      sub->setStyleSheet(QStringLiteral(
          "color: rgba(180,188,215,0.70); font-size: 11px; font-weight: 600; "
          "background: transparent;"));
      layout->addWidget(sub);
    }

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
        if (!BlopDialogs::confirm(this, QStringLiteral("Seite leeren"),
                                  QStringLiteral("Alles auf dieser Seite löschen?"),
                                  QStringLiteral("Löschen"),
                                  QStringLiteral("Abbrechen")))
          return;
        // Prefer the currently focused / active graphics view over a
        // heuristic walk of all top-level widgets (Study WebView etc.).
        QGraphicsView *view = nullptr;
        if (QWidget *fw = QApplication::focusWidget()) {
          view = qobject_cast<QGraphicsView *>(fw);
          if (!view)
            view = fw->findChild<QGraphicsView *>();
          if (!view)
            if (QWidget *win = fw->window())
              view = win->findChild<QGraphicsView *>();
        }
        if (!view) {
          for (QWidget *w : QApplication::topLevelWidgets()) {
            view = w->findChild<QGraphicsView *>();
            if (view)
              break;
          }
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
          {QStringLiteral("Ellipse"), ShapeToolKind::Ellipse},
          {QStringLiteral("Linie"), ShapeToolKind::Line},
          {QStringLiteral("Pfeil"), ShapeToolKind::Arrow},
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
  // Stay a child QWidget (not a native top-level window). FramelessWindowHint
  // alone promotes the toolbar to an override-redirect window and breaks
  // parent-relative docking — especially under software GL / no compositor.
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_AcceptTouchEvents, true);
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
  btnLasso = new ToolbarBtn("select", this); // Drawboard rectangle-select look
  btnRuler = new ToolbarBtn("measure", this);
  btnShape = new ToolbarBtn("rect", this);
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
  btnAddTool = new ToolbarBtn("add", this);
  btnAddTool->setToolTip(tr("Neues Tool wählen"));
  btnLibrary = new ToolbarBtn("library", this);
  btnLibrary->setToolTip(tr("Auszeichnungsbibliothek"));
  btnLibrary->setShowChevron(false);
  btnRailChevron = new ToolbarBtn("chevron_rail", this);
  btnRailChevron->setToolTip(tr("Eigenschaften ein-/ausblenden"));
  btnMoreProps = new ToolbarBtn("more", this);
  btnMoreProps->setToolTip(tr("More options"));
  btnLayoutToggle = new ToolbarBtn("layout_rows", this);
  btnLayoutToggle->setToolTip(tr("Switch markup toolbar layout"));
  btnLasso->setShowChevron(true);
  btnLasso->setToolTip(tr("Auswahl"));
  btnShape->setShowChevron(true);
  btnShape->setToolTip(tr("Formen"));

  btnSave = new ToolbarBtn("save", this);
  btnPalette = new ToolbarBtn("palette", this);
  btnBrushSize = new ToolbarBtn("brush_size", this);

  m_dockedOnlyButtons = {btnSave, btnPalette, btnBrushSize};

  m_buttons = {btnSave,     btnPen,      btnPencil, btnHighlighter,
                 btnEraser,    btnLasso,    btnRuler,    btnShape,  btnStickyNote,
                 btnText,      btnImage,    btnHand,     btnBackOverview,
                 btnUndo,      btnRedo,     btnPalette,  btnBrushSize, btnDockToggle,
                 btnAddTool, btnLibrary, btnRailChevron, btnMoreProps,
                 btnLayoutToggle};

  m_customColors = {Qt::black, Qt::white,         Qt::red,
                    Qt::blue,  QColor(0, 150, 0), QColor(255, 140, 0)};

  // Drawboard Markup category tabs (desktop). Hidden until markup mode.
  m_catSelect = new MarkupCategoryTab(QStringLiteral("hand"),
                                      tr("Select"), this);
  m_catFreeform = new MarkupCategoryTab(QStringLiteral("pen"),
                                        tr("Free-form"), this);
  m_catShapes = new MarkupCategoryTab(QStringLiteral("rect"),
                                      tr("Shapes"), this);
  m_catReview = new MarkupCategoryTab(QStringLiteral("text"),
                                      tr("Review"), this);
  m_catInsert = new MarkupCategoryTab(QStringLiteral("image"),
                                      tr("Insert"), this);
  m_categoryTabs = {m_catSelect, m_catFreeform, m_catShapes, m_catReview,
                    m_catInsert};
  for (auto *tab : m_categoryTabs) {
    tab->hide();
    tab->setAccentColor(m_accentColor);
  }
  connect(m_catSelect, &MarkupCategoryTab::clicked, this, [this]() {
    setMarkupCategory(CatSelect, true);
  });
  connect(m_catFreeform, &MarkupCategoryTab::clicked, this, [this]() {
    setMarkupCategory(CatFreeform, true);
  });
  connect(m_catShapes, &MarkupCategoryTab::clicked, this, [this]() {
    setMarkupCategory(CatShapes, true);
  });
  connect(m_catReview, &MarkupCategoryTab::clicked, this, [this]() {
    setMarkupCategory(CatReview, true);
  });
  connect(m_catInsert, &MarkupCategoryTab::clicked, this, [this]() {
    setMarkupCategory(CatInsert, true);
  });

  for (auto *b : m_buttons) {
    if (b)
      b->setAccentColor(m_accentColor);
  }

  loadRailTools();

  auto handleToolClick = [this](ToolMode m) {
    if (mode_ == m) {
#ifndef Q_OS_ANDROID
      // Desktop: flyout for family tools, otherwise properties panel — never MorphTray.
      if (toolHasFlyout(m)) {
        showToolFlyout(m);
        return;
      }
      emit toolOptionsRequested();
      return;
#else
      if (m_style == Radial) {
          if (!m_showRadialSettings)
              toggleRadialSettings();
      } else {
          showSettingsPopup();
      }
      return;
#endif
    }
    if (m_showRadialSettings) {
        toggleRadialSettings(); // Hide settings when switching tools
    }
    ToolbarBtn *btn = getButtonForMode(m);
    if (btn) {
      QPoint pos = btn->mapToGlobal(QPoint(btn->width() + 15, 0));
      ToolUIBridge::instance().setOverlayPosition(pos);
    }
    if (m_markupBarMode != MarkupOff)
      m_markupCategory = categoryForTool(m);
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

  auto wireTool = [this, handleToolClick](ToolbarBtn *btn, ToolMode m) {
    connect(btn, &ToolbarBtn::clicked, this, [=]() { handleToolClick(m); });
    connect(btn, &ToolbarBtn::removeFromRailRequested, this, [this, m]() {
      removeToolFromRail(m);
    });
    connect(btn, &ToolbarBtn::moveInRailRequested, this, [this, m](int delta) {
      moveRailTool(m, delta);
    });
  };
  wireTool(btnPen, ToolMode::Pen);
  wireTool(btnPencil, ToolMode::Pencil);
  wireTool(btnHighlighter, ToolMode::Highlighter);
  wireTool(btnEraser, ToolMode::Eraser);
  wireTool(btnLasso, ToolMode::Lasso);
  wireTool(btnRuler, ToolMode::Ruler);
  wireTool(btnShape, ToolMode::Shape);
  wireTool(btnStickyNote, ToolMode::StickyNote);
  wireTool(btnText, ToolMode::Text);
  wireTool(btnHand, ToolMode::Hand);
  wireTool(btnImage, ToolMode::Image);

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
  if (btnAddTool) {
    connect(btnAddTool, &ToolbarBtn::clicked, this,
            &ModernToolbar::showToolPicker);
  }
  if (btnLibrary) {
    connect(btnLibrary, &ToolbarBtn::clicked, this,
            &ModernToolbar::showMarkupLibrary);
  }
  if (btnRailChevron) {
    connect(btnRailChevron, &ToolbarBtn::clicked, this, [this]() {
      // Drawboard edge chrome: toggle the pinned properties panel.
      emit propertiesPanelToggleRequested();
    });
  }
  if (btnMoreProps) {
    connect(btnMoreProps, &ToolbarBtn::clicked, this, [this]() {
      emit toolOptionsRequested();
    });
  }
  if (btnLayoutToggle) {
    connect(btnLayoutToggle, &ToolbarBtn::clicked, this, [this]() {
      if (m_markupBarMode == MarkupOff)
        return;
      setMarkupBarMode(m_markupBarMode == MarkupTwoRow ? MarkupSingleRow
                                                       : MarkupTwoRow);
    });
  }
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &cfg) {
            // Persist property edits into the active Favorites preset slot.
            if (m_activeRailSlot >= 0 &&
                m_activeRailSlot < m_railSlots.size() &&
                m_railSlots[m_activeRailSlot].mode ==
                    ToolManager::instance().activeToolMode()) {
              RailSlot &s = m_railSlots[m_activeRailSlot];
              s.color = cfg.penColor;
              s.width = qMax(1, cfg.penWidth);
              s.opacity = cfg.opacity;
              s.shapeKind = static_cast<int>(cfg.shapeToolKind);
              s.lassoMode = static_cast<int>(cfg.lassoMode);
              saveRailTools();
            }
            syncToolBadges();
            syncDrawboardToolIcons();
            syncInlinePropertyControls();
          });

  setStyle(Normal);
  setToolMode(ToolMode::Pen);
  ToolManager::instance().selectTool(ToolMode::Pen);
  syncToolBadges();
  syncDrawboardToolIcons();
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
        // Drawboard Markup Toolbar: flat charcoal bar, soft bottom corners.
        QLinearGradient grad(0, 0, 0, h);
        grad.setColorAt(0, NoteChrome::toolbarFill());
        grad.setColorAt(1, NoteChrome::toolbarFillEnd());
        p.setBrush(grad);
        p.setPen(Qt::NoPen);

        const int r = UiScale::dp(10);
        p.drawRoundedRect(0, -r, w, h + r, r, r);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(NoteChrome::borderSoft(), 1));
        p.drawRoundedRect(0, -r, w - 1, h + r - 1, r, r);

        p.setPen(QPen(QColor(255, 255, 255, 22), 1));
        for (int sx : m_separatorXPositions) {
          const int y0 = (m_markupBarMode == MarkupTwoRow && m_markupRowDividerY > 0)
                             ? m_markupRowDividerY + UiScale::dp(6)
                             : UiScale::dp(10);
          const int y1 = h - UiScale::dp(10);
          if (y1 > y0)
            p.drawLine(sx, y0, sx, y1);
        }
        // Two-row Drawboard Markup: divider between category and tool rows.
        if (m_markupBarMode == MarkupTwoRow && m_markupRowDividerY > 0) {
          p.setPen(QPen(NoteChrome::borderSoft(), 1));
          p.drawLine(UiScale::dp(8), m_markupRowDividerY, w - UiScale::dp(8),
                     m_markupRowDividerY);
        }
      } else if (isDrawboardVerticalRail()) {
        // Edge-flush Favorites rail: square charcoal plate, no float shadow.
        QLinearGradient grad(0, 0, 0, h);
        grad.setColorAt(0, NoteChrome::toolbarFill());
        grad.setColorAt(1, NoteChrome::toolbarFillEnd());
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawRect(rect());
        p.setPen(QPen(NoteChrome::borderSoft(), 1));
        p.drawLine(0, 0, 0, h);
        p.setPen(QPen(QColor(255, 255, 255, 22), 1));
        for (int sy : m_separatorYPositions)
          p.drawLine(UiScale::dp(8), sy, w - UiScale::dp(8), sy);
        // Scroll fades when Favorites slots overflow the rail viewport.
        if (m_railMaxScroll > 0) {
          const int zone = UiScale::dp(28);
          const int a = NoteChrome::isDark() ? 150 : 70;
          if (m_railScrollPx > 0) {
            QLinearGradient topFade(0, m_railContentTop, 0,
                                    m_railContentTop + zone);
            topFade.setColorAt(0, QColor(0, 0, 0, a));
            topFade.setColorAt(1, Qt::transparent);
            p.fillRect(0, m_railContentTop, w, zone, topFade);
          }
          if (m_railScrollPx < m_railMaxScroll) {
            QLinearGradient botFade(0, m_railContentBottom - zone, 0,
                                    m_railContentBottom);
            botFade.setColorAt(0, Qt::transparent);
            botFade.setColorAt(1, QColor(0, 0, 0, a));
            p.fillRect(0, m_railContentBottom - zone, w, zone, botFade);
          }
        }
        // Drag-reorder drop indicator.
        if (m_railDragFrom >= 0 && m_railDragGhostY >= 0) {
          p.setPen(QPen(NoteChrome::accent(), 2, Qt::SolidLine, Qt::RoundCap));
          const int gy = qBound(UiScale::dp(4), m_railDragGhostY, h - UiScale::dp(4));
          p.drawLine(UiScale::dp(6), gy, w - UiScale::dp(6), gy);
        }
      } else {
        // Floating charcoal tool plate.
        const int radius = (m_orientation == Vertical)
                               ? UiScale::dp(14)
                               : qMin(w, h) / 2;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 55));
        p.drawRoundedRect(rect().adjusted(1, 3, -1, 0), radius, radius);

        QLinearGradient grad(0, 0, 0, h);
        grad.setColorAt(0, NoteChrome::toolbarFill());
        grad.setColorAt(1, NoteChrome::toolbarFillEnd());
        p.setBrush(grad);
        p.setPen(QPen(NoteChrome::border(), 1));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius, radius);

        if (m_orientation == Vertical) {
          p.setPen(QPen(QColor(255, 255, 255, 22), 1));
          for (int sy : m_separatorYPositions) {
            p.drawLine(UiScale::dp(10), sy, w - UiScale::dp(10), sy);
          }
          if (m_draggable) {
            p.setBrush(QColor(255, 255, 255, 45));
            p.setPen(Qt::NoPen);
            const int gy = UiScale::dp(8);
            for (int i = -1; i <= 1; ++i)
              p.drawEllipse(w / 2 + i * 6 - 2, gy, 4, 4);
          }
        } else if (m_draggable) {
          p.setPen(QPen(QColor(255, 255, 255, 40), 2, Qt::SolidLine, Qt::RoundCap));
          p.drawLine(11, h / 2 - 7, 11, h / 2 + 7);
          p.drawLine(15, h / 2 - 7, 15, h / 2 + 7);
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
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        // Prefer geometry shape over fragile y==10 checks (DPI/safe-area
        // offsets broke top-snap → left a tall vertical pill on Windows).
        const bool isTopSnap = snapGeom.width() >= snapGeom.height();

        // Desktop Drawboard: always prefer the vertical Favorites rail.
        // Top-edge snaps still resolve to a right-side vertical rail.
        applyDrawboardVerticalRail();
        {
          const int w = preferredRailWidth();
          const int h = qMax(calculateMinLength(), snapGeom.height());
          snapGeom.setWidth(w);
          snapGeom.setHeight(h);
          if (isTopSnap || snapGeom.x() > parentWidget()->width() / 2)
            snapGeom.moveLeft(parentWidget()->width() - w -
                              UiScale::safeHorizontalPaddingPx(parentWidget()));
          else
            snapGeom.moveLeft(
                UiScale::safeHorizontalPaddingPx(parentWidget()));
          setFixedSize(w, h);
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

void ModernToolbar::openToolOptions() {
  emit toolOptionsRequested();
#ifndef Q_OS_ANDROID
  // Desktop Drawboard: always use the persistent right properties panel.
  return;
#else
  showSettingsPopup();
#endif
}

void ModernToolbar::showToolPicker() {
  QWidget *host = parentWidget() ? parentWidget()->window() : window();
  if (!host)
    host = window();
#ifdef Q_OS_ANDROID
  if (auto *mw = qobject_cast<QMainWindow *>(host)) {
    if (QWidget *cw = mw->centralWidget())
      host = cw;
  }
#endif
  QSet<ToolMode> already;
  for (const RailSlot &s : m_railSlots)
    already.insert(s.mode);
  ToolPickerOverlay::present(
      host, m_accentColor, already, [this](ToolMode mode) {
        // Hand is unique; other tools may appear multiple times as presets.
        if (mode == ToolMode::Hand && railContains(ToolMode::Hand)) {
          for (int i = 0; i < m_railSlots.size(); ++i) {
            if (m_railSlots[i].mode == ToolMode::Hand) {
              applyRailSlot(i);
              return;
            }
          }
          return;
        }
        addToolToRail(mode);
        // Select the newly added slot (last matching mode).
        for (int i = m_railSlots.size() - 1; i >= 0; --i) {
          if (m_railSlots[i].mode == mode) {
            applyRailSlot(i);
            break;
          }
        }
#ifndef Q_OS_ANDROID
        // Desktop: properties panel is the options surface.
        emit toolOptionsRequested();
        return;
#else
        if (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
            mode == ToolMode::Highlighter || mode == ToolMode::Eraser ||
            mode == ToolMode::Lasso || mode == ToolMode::Ruler ||
            mode == ToolMode::Shape) {
          QTimer::singleShot(120, this, [this]() { showSettingsPopup(); });
        }
#endif
      });
}

void ModernToolbar::syncToolBadges() {
  auto clearAll = [this]() {
    for (ToolbarBtn *b :
         {btnPen, btnPencil, btnHighlighter, btnEraser, btnText, btnShape,
          btnRuler, btnAddTool, btnLibrary, btnRailChevron, btnLasso, btnHand,
          btnImage, btnStickyNote}) {
      if (b)
        b->setBadgeText(QString());
    }
  };
#ifndef Q_OS_ANDROID
  // Drawboard Favorites rail: clean icons without width chips.
  if (isDrawboardVerticalRail()) {
    clearAll();
    return;
  }
#endif
  auto widthBadge = [](ToolMode m) -> QString {
    const ToolConfig &cfg = ToolManager::instance().configFor(m);
    if (m == ToolMode::Highlighter)
      return QString::number(qMax(8, cfg.penWidth * 4));
    return QString::number(qMax(1, cfg.penWidth));
  };
  auto setBadge = [](ToolbarBtn *b, const QString &t) {
    if (b)
      b->setBadgeText(t);
  };
  setBadge(btnPen, widthBadge(ToolMode::Pen));
  setBadge(btnPencil, widthBadge(ToolMode::Pencil));
  setBadge(btnEraser, widthBadge(ToolMode::Eraser));
  setBadge(btnHighlighter, widthBadge(ToolMode::Highlighter));
  setBadge(btnText, widthBadge(ToolMode::Text));
  setBadge(btnShape, widthBadge(ToolMode::Shape));
  setBadge(btnRuler, widthBadge(ToolMode::Ruler));
  if (btnAddTool)
    btnAddTool->setBadgeText(QString());
  if (btnLibrary)
    btnLibrary->setBadgeText(QString());
  if (btnRailChevron)
    btnRailChevron->setBadgeText(QString());
}

void ModernToolbar::syncDrawboardToolIcons() {
  if (btnLasso) {
    btnLasso->setIcon(QStringLiteral("select"));
    btnLasso->setShowChevron(true);
  }
  if (btnRuler) {
    btnRuler->setIcon(QStringLiteral("measure"));
    btnRuler->setGlyphColor(QColor(235, 70, 70));
  }
  if (btnShape) {
    btnShape->setIcon(iconForSlot(RailSlot::fromTool(
        ToolMode::Shape, ToolManager::instance().configFor(ToolMode::Shape))));
    btnShape->setGlyphColor(QColor());
    btnShape->setShowChevron(true);
  }
  if (btnPen) {
    const QColor c = ToolManager::instance().configFor(ToolMode::Pen).penColor;
    btnPen->setGlyphColor(c);
  }
  if (btnPencil) {
    const QColor c =
        ToolManager::instance().configFor(ToolMode::Pencil).penColor;
    btnPencil->setGlyphColor(c);
  }
  if (btnHighlighter) {
    QColor c =
        ToolManager::instance().configFor(ToolMode::Highlighter).penColor;
    // Drawboard highlighters read as saturated neon tips.
    if (c.saturationF() < 0.25)
      c = QColor(90, 220, 70);
    btnHighlighter->setGlyphColor(c);
  }
  if (btnEraser)
    btnEraser->setGlyphColor(QColor());
  if (btnLibrary) {
    btnLibrary->setIcon(QStringLiteral("library"));
    btnLibrary->setShowChevron(false);
  }
  if (btnRailChevron)
    btnRailChevron->setIcon(m_propertiesPanelOpen
                                ? QStringLiteral("chevron_left")
                                : QStringLiteral("chevron_rail"));
  for (int i = 0; i < m_slotButtons.size(); ++i)
    syncSlotButtonAppearance(i);
}

void ModernToolbar::showMarkupLibrary() {
  // Drawboard "Auszeichnungsbibliothek" — device store + insert / delete.
  QWidget *host = parentWidget() ? parentWidget()->window() : window();
  if (!host)
    host = window();
  const QVector<MarkupLibraryItem> items = MarkupLibraryStore::load();
  const int deviceCount = items.size();

  auto *layer = new QWidget(host);
  layer->setAttribute(Qt::WA_DeleteOnClose, true);
  layer->setAttribute(Qt::WA_StyledBackground, true);
  layer->setStyleSheet(QStringLiteral("background: rgba(0,0,0,0.55);"));
  layer->setGeometry(host->rect());

  auto *card = new QFrame(layer);
  card->setObjectName(QStringLiteral("MarkupLibraryCard"));
  card->setStyleSheet(
      QStringLiteral("QFrame#MarkupLibraryCard {"
                     "  background: %1; border: 1px solid %2; border-radius: 14px;"
                     "}"
                     "QLabel { color: %3; background: transparent; }"
                     "QPushButton {"
                     "  background: %4; color: white; border: none;"
                     "  border-radius: 8px; padding: 10px 16px; font-weight: 700;"
                     "}"
                     "QPushButton#LibTab {"
                     "  background: transparent; color: %5; border: none;"
                     "  border-bottom: 2px solid transparent; border-radius: 0;"
                     "  padding: 8px 12px;"
                     "}"
                     "QPushButton#LibTab:checked {"
                     "  color: %3; border-bottom: 2px solid %4;"
                     "}"
                     "QToolButton {"
                     "  background: %6; color: %3; border: 1px solid %2;"
                     "  border-radius: 10px; padding: 8px;"
                     "}"
                     "QToolButton:hover { border-color: %4; }")
          .arg(NoteChrome::panelElevated().name(QColor::HexRgb),
               NoteChrome::border().name(QColor::HexRgb),
               NoteChrome::textPrimary().name(QColor::HexRgb),
               NoteChrome::accent().name(QColor::HexRgb),
               NoteChrome::textSecondary().name(QColor::HexRgb),
               NoteChrome::panelBg().name(QColor::HexRgb)));
  const int cardW = qMin(560, qMax(420, int(host->width() * 0.48)));
  const int cardH = qMin(480, qMax(340, int(host->height() * 0.55)));
  card->setGeometry((layer->width() - cardW) / 2,
                    (layer->height() - cardH) / 2, cardW, cardH);
  auto *lay = new QVBoxLayout(card);
  lay->setContentsMargins(UiScale::dp(22), UiScale::dp(18), UiScale::dp(22),
                          UiScale::dp(18));
  lay->setSpacing(UiScale::dp(12));
  auto *title = new QLabel(tr("Auszeichnungsbibliothek"), card);
  title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 800;"));
  lay->addWidget(title);

  auto *tabRow = new QHBoxLayout;
  auto *deviceTab = new QPushButton(
      tr("Device (%1)").arg(deviceCount), card);
  auto *cloudTab = new QPushButton(tr("Cloud"), card);
  deviceTab->setObjectName(QStringLiteral("LibTab"));
  cloudTab->setObjectName(QStringLiteral("LibTab"));
  deviceTab->setCheckable(true);
  cloudTab->setCheckable(true);
  QSettings libSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const bool openCloud =
      libSettings.value(QStringLiteral("ui/markup_library_last_tab"), 0).toInt() ==
      1;
  deviceTab->setChecked(!openCloud);
  cloudTab->setChecked(openCloud);
  cloudTab->setToolTip(tr("Cloud-Sync folgt später — Tippen für Infos."));
  tabRow->addWidget(deviceTab);
  tabRow->addWidget(cloudTab);
  tabRow->addStretch(1);
  lay->addLayout(tabRow);

  auto *scroll = new QScrollArea(card);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *pagesHost = new QWidget(scroll);
  auto *pagesLay = new QVBoxLayout(pagesHost);
  pagesLay->setContentsMargins(0, 0, 0, 0);
  pagesLay->setSpacing(0);

  auto *devicePage = new QWidget(pagesHost);
  auto *deviceLay = new QVBoxLayout(devicePage);
  deviceLay->setContentsMargins(0, 0, 0, 0);
  deviceLay->setSpacing(UiScale::dp(10));

  auto *cloudPage = new QWidget(pagesHost);
  auto *cloudLay = new QVBoxLayout(cloudPage);
  cloudLay->setContentsMargins(UiScale::dp(8), UiScale::dp(20), UiScale::dp(8),
                               UiScale::dp(8));
  cloudLay->setSpacing(UiScale::dp(12));
  // Visibility set after pages are built (respect last-tab preference).

  {
    QPixmap cloudIcon(UiScale::dp(56), UiScale::dp(56));
    cloudIcon.fill(Qt::transparent);
    {
      QPainter ip(&cloudIcon);
      ip.setRenderHint(QPainter::Antialiasing);
      ip.setPen(QPen(NoteChrome::textSecondary(), 2.4, Qt::SolidLine,
                     Qt::RoundCap, Qt::RoundJoin));
      ip.setBrush(Qt::NoBrush);
      ip.drawEllipse(8, 22, 18, 16);
      ip.drawEllipse(18, 14, 24, 20);
      ip.drawEllipse(34, 22, 16, 16);
      ip.drawLine(10, 36, 48, 36);
    }
    auto *cloudIconLbl = new QLabel(cloudPage);
    cloudIconLbl->setPixmap(cloudIcon);
    cloudIconLbl->setAlignment(Qt::AlignHCenter);
    cloudLay->addWidget(cloudIconLbl);
    auto *cloudTitle = new QLabel(tr("Cloud-Bibliothek"), cloudPage);
    cloudTitle->setAlignment(Qt::AlignHCenter);
    cloudTitle->setStyleSheet(
        QStringLiteral("font-size: 15px; font-weight: 700; color: %1;")
            .arg(NoteChrome::textPrimary().name(QColor::HexRgb)));
    cloudLay->addWidget(cloudTitle);
    auto *cloudBody = new QLabel(
        tr("Synchronisierte Auszeichnungen über Geräte folgen in einem "
           "späteren Update. Bis dahin speichert die Device-Bibliothek lokal "
           "auf diesem Gerät."),
        cloudPage);
    cloudBody->setWordWrap(true);
    cloudBody->setAlignment(Qt::AlignHCenter);
    cloudBody->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px;")
            .arg(NoteChrome::textSecondary().name(QColor::HexRgb)));
    cloudLay->addWidget(cloudBody);
    auto *useDevice = new QPushButton(tr("Gerät nutzen"), cloudPage);
    useDevice->setCursor(Qt::PointingHandCursor);
    cloudLay->addWidget(useDevice, 0, Qt::AlignHCenter);
    connect(useDevice, &QPushButton::clicked, deviceTab, &QPushButton::click);
    auto *notifyChk = new QCheckBox(
        tr("Benachrichtigen, wenn Cloud verfügbar ist"), cloudPage);
    notifyChk->setChecked(
        libSettings.value(QStringLiteral("ui/markup_library_cloud_notify"), false)
            .toBool());
    notifyChk->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;")
            .arg(NoteChrome::textSecondary().name(QColor::HexRgb)));
    connect(notifyChk, &QCheckBox::toggled, notifyChk, [](bool on) {
      QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
      s.setValue(QStringLiteral("ui/markup_library_cloud_notify"), on);
    });
    cloudLay->addWidget(notifyChk, 0, Qt::AlignHCenter);
    cloudLay->addStretch(1);
  }

  if (items.isEmpty()) {
    auto *emptyHost = new QWidget(devicePage);
    auto *emptyLay = new QVBoxLayout(emptyHost);
    emptyLay->setContentsMargins(UiScale::dp(12), UiScale::dp(16),
                                 UiScale::dp(12), UiScale::dp(12));
    emptyLay->setSpacing(UiScale::dp(10));
    QPixmap libIcon(UiScale::dp(48), UiScale::dp(48));
    libIcon.fill(Qt::transparent);
    {
      QPainter ip(&libIcon);
      ip.setRenderHint(QPainter::Antialiasing);
      const qreal s = libIcon.width() / 64.0;
      ip.scale(s, s);
      drawToolbarGlyph64(&ip, QStringLiteral("library"),
                         NoteChrome::textSecondary());
    }
    auto *iconLbl = new QLabel(emptyHost);
    iconLbl->setPixmap(libIcon);
    iconLbl->setAlignment(Qt::AlignHCenter);
    emptyLay->addWidget(iconLbl);
    auto *emptyTitle = new QLabel(tr("Noch keine Auszeichnungen"), emptyHost);
    emptyTitle->setAlignment(Qt::AlignHCenter);
    emptyTitle->setStyleSheet(
        QStringLiteral("font-size: 14px; font-weight: 700; color: %1;")
            .arg(NoteChrome::textPrimary().name(QColor::HexRgb)));
    emptyLay->addWidget(emptyTitle);
    auto *steps = new QLabel(
        tr("1. Annotationen mit der Auswahl markieren\n"
           "2. In der Auswahlleiste „Zur Bibliothek“ wählen\n"
           "3. Hier tippen, um sie erneut einzufügen"),
        emptyHost);
    steps->setWordWrap(true);
    steps->setAlignment(Qt::AlignHCenter);
    steps->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px;")
            .arg(NoteChrome::textSecondary().name(QColor::HexRgb)));
    emptyLay->addWidget(steps);
    emptyHost->setStyleSheet(
        QStringLiteral("background: %1; border-radius: 12px;")
            .arg(NoteChrome::panelBg().name(QColor::HexRgb)));
    deviceLay->addWidget(emptyHost);
    deviceLay->addStretch(1);
  } else {
    auto *tilesHost = new QWidget(devicePage);
    auto *tilesGrid = new QGridLayout(tilesHost);
    tilesGrid->setContentsMargins(0, 0, 0, 0);
    tilesGrid->setSpacing(UiScale::dp(10));
    int col = 0;
    int row = 0;
    for (const MarkupLibraryItem &item : items) {
      auto *tile = new QToolButton(tilesHost);
      tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
      tile->setCursor(Qt::PointingHandCursor);
      tile->setFixedSize(UiScale::dp(140), UiScale::dp(120));
      QPixmap pm(UiScale::dp(72), UiScale::dp(72));
      pm.fill(Qt::transparent);
      {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(pm.rect(), NoteChrome::panelBg());
        QRectF bounds;
        for (const Stroke &s : item.strokes)
          bounds = bounds.united(s.path.boundingRect());
        if (!bounds.isEmpty()) {
          const qreal pad = 8.0;
          const qreal sx =
              (pm.width() - 2 * pad) / qMax(1.0, bounds.width());
          const qreal sy =
              (pm.height() - 2 * pad) / qMax(1.0, bounds.height());
          const qreal scale = qMin(sx, sy);
          p.translate(pm.width() / 2.0, pm.height() / 2.0);
          p.scale(scale, scale);
          p.translate(-bounds.center().x(), -bounds.center().y());
          for (const Stroke &s : item.strokes) {
            QColor c = s.color;
            if (s.isHighlighter)
              c.setAlpha(90);
            p.setPen(QPen(c, qMax(1.0, s.width), Qt::SolidLine, Qt::RoundCap,
                          Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(s.path);
          }
        } else {
          p.setPen(QPen(item.previewColor, 3, Qt::SolidLine, Qt::RoundCap));
          p.drawLine(18, pm.height() - 22, pm.width() - 18, 22);
        }
      }
      tile->setIcon(QIcon(pm));
      tile->setIconSize(pm.size());
      tile->setText(item.name);
      const QString itemId = item.id;
      connect(tile, &QToolButton::clicked, layer, [this, itemId, layer]() {
        QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
        s.setValue(QStringLiteral("ui/markup_library_pending_insert"), itemId);
        layer->close();
        emit markupLibraryRequested();
      });
      tile->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(tile, &QWidget::customContextMenuRequested, tile,
              [tile, itemId, layer](const QPoint &pos) {
                QMenu menu(tile);
                QAction *del =
                    menu.addAction(ModernToolbar::tr("Entfernen"));
                if (menu.exec(tile->mapToGlobal(pos)) == del) {
                  MarkupLibraryStore::removeItem(itemId);
                  layer->close();
                }
              });
      tilesGrid->addWidget(tile, row, col);
      if (++col >= 3) {
        col = 0;
        ++row;
      }
    }
    deviceLay->addWidget(tilesHost);
    deviceLay->addStretch(1);
  }

  pagesLay->addWidget(devicePage);
  pagesLay->addWidget(cloudPage);
  if (openCloud) {
    devicePage->hide();
    cloudPage->show();
  } else {
    devicePage->show();
    cloudPage->hide();
  }
  scroll->setWidget(pagesHost);
  lay->addWidget(scroll, 1);

  auto *closeBtn = new QPushButton(tr("Schließen"), card);
  closeBtn->setStyleSheet(
      QStringLiteral("background: transparent; color: %1; border: 1px solid %2;")
          .arg(NoteChrome::textPrimary().name(QColor::HexRgb),
               NoteChrome::border().name(QColor::HexRgb)));
  lay->addWidget(closeBtn, 0, Qt::AlignRight);
  connect(closeBtn, &QPushButton::clicked, layer, &QWidget::close);
  auto showDevice = [deviceTab, cloudTab, devicePage, cloudPage]() {
    deviceTab->setChecked(true);
    cloudTab->setChecked(false);
    devicePage->show();
    cloudPage->hide();
    QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    s.setValue(QStringLiteral("ui/markup_library_last_tab"), 0);
  };
  auto showCloud = [deviceTab, cloudTab, devicePage, cloudPage]() {
    cloudTab->setChecked(true);
    deviceTab->setChecked(false);
    devicePage->hide();
    cloudPage->show();
    QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    s.setValue(QStringLiteral("ui/markup_library_last_tab"), 1);
    s.setValue(QStringLiteral("ui/markup_library_cloud_ack"), true);
  };
  connect(deviceTab, &QPushButton::clicked, card, showDevice);
  connect(cloudTab, &QPushButton::clicked, card, showCloud);

  layer->show();
  layer->raise();
  class BackdropCloser : public QObject {
  public:
    explicit BackdropCloser(QWidget *layer) : QObject(layer), m_layer(layer) {
      m_layer->installEventFilter(this);
    }
  protected:
    bool eventFilter(QObject *o, QEvent *e) override {
      if (o == m_layer && e->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(e);
        for (QObject *c : m_layer->children()) {
          if (auto *w = qobject_cast<QWidget *>(c)) {
            if (w->objectName() == QStringLiteral("MarkupLibraryCard") &&
                w->geometry().contains(me->pos()))
              return QObject::eventFilter(o, e);
          }
        }
        m_layer->close();
        return true;
      }
      return QObject::eventFilter(o, e);
    }
    QWidget *m_layer{nullptr};
  };
  new BackdropCloser(layer);
}

void ModernToolbar::showSettingsPopup() {
#ifndef Q_OS_ANDROID
  // Desktop never opens MorphTray — right properties panel owns tool options.
  emit toolOptionsRequested();
  return;
#endif
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
  for (auto *b : m_slotButtons) {
    if (b)
      b->setAccentColor(m_accentColor);
  }
  for (ToolbarBtn *b : {btnLibrary, btnAddTool, btnRailChevron}) {
    if (b)
      b->setAccentColor(m_accentColor);
  }
  for (auto *tab : m_categoryTabs) {
    if (tab)
      tab->setAccentColor(m_accentColor);
  }
  syncInlinePropertyControls();
  update();
}

void ModernToolbar::setPropertiesPanelOpen(bool open) {
  if (m_propertiesPanelOpen == open)
    return;
  m_propertiesPanelOpen = open;
  if (btnRailChevron) {
    btnRailChevron->setIcon(open ? QStringLiteral("chevron_left")
                                 : QStringLiteral("chevron_rail"));
    btnRailChevron->setToolTip(open ? tr("Eigenschaften ausblenden")
                                    : tr("Eigenschaften einblenden"));
  }
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
  for (int i = 0; i < m_slotButtons.size(); ++i) {
    if (!m_slotButtons[i])
      continue;
    const bool on = (m_activeRailSlot >= 0) ? (i == m_activeRailSlot)
                                            : (i < m_railSlots.size() &&
                                               m_railSlots[i].mode == mode);
    m_slotButtons[i]->setActive(on);
  }
  ToolbarBtn *activeBtn = getButtonForMode(mode);
  if (activeBtn && m_slotButtons.isEmpty()) {
    activeBtn->setActive(true);
    activeBtn->animateSelect();
  }
  if (m_markupBarMode != MarkupOff) {
    const MarkupCategory cat = categoryForTool(mode);
    if (cat != m_markupCategory) {
      m_markupCategory = cat;
      syncCategoryTabs();
    } else {
      syncCategoryTabs();
    }
  }
  if (changed) {
    emit rulerToggled(mode == ToolMode::Ruler);
  }
  if (m_markupBarMode != MarkupOff && changed) {
    updateLayout(false);
  } else if (m_style == Radial && m_radialType == FullCircle && changed) {
    updateLayout(true);
  }
  syncInlinePropertyControls();
  syncDrawboardToolIcons();
  updateActiveIndicator(true);
  update();
  updateHitbox();
}

void ModernToolbar::updateActiveIndicator(bool animate) {
  if (!m_activeIndicator)
    return;
  // Indicator only makes sense for the linear (Normal) toolbar; hide in Radial
  // and in the Drawboard vertical Favorites rail (slot highlight is enough).
  if (m_style != Normal || isDrawboardVerticalRail()) {
    m_activeIndicator->hide();
    return;
  }
  ToolbarBtn *btn = getButtonForMode(mode_);
  if (!btn || !btn->isVisible() || btn->geometry().isEmpty() ||
      btn->railSlotStyle()) {
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
      setFixedWidth(preferredRailWidth());
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
  if (isDrawboardVerticalRail() && m_orientation == Vertical) {
    const int step = qRound(e->angleDelta().y() / 4.0);
    m_railScrollPx = qMax(0, m_railScrollPx - step);
    updateLayout(false);
    e->accept();
    return;
  }
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
#ifndef Q_OS_ANDROID
  // Desktop Drawboard default is the right vertical rail; only flip when
  // the user explicitly snaps to a horizontal edge.
#endif
  setOrientation(horizontal ? Horizontal : Vertical, true);
}
void ModernToolbar::setOrientation(Orientation o, bool animate) {
  if (m_orientation == o)
    return;
  m_orientation = o;
  m_cachedMask = QRegion();
  // Clear stale min/max from the previous orientation before applying new
  // locks — leftover Vertical fixed-width is what produced the packed pill.
  setMinimumSize(0, 0);
  setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

  QSize currentS = size();
  QSize targetS;
  if (m_orientation == Vertical) {
    const int w = preferredRailWidth();
    const int h = qMax(calculateMinLength(), currentS.width());
    targetS = QSize(w, h);
    setFixedWidth(w);
    setMinimumHeight(h);
  } else {
    const int h = UiScale::dp(56);
    const int w = qMax(calculateMinLength(), currentS.height());
    targetS = QSize(w, h);
    setFixedHeight(h);
    setMinimumWidth(w);
  }
  if (animate) {
    QPropertyAnimation *anim = new QPropertyAnimation(this, "size");
    anim->setDuration(250);
    anim->setStartValue(currentS);
    anim->setEndValue(targetS);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::valueChanged, this,
            [this](const QVariant &) { updateLayout(); });
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
      updateLayout();
      update();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    resize(targetS);
    updateLayout();
  }
}
void ModernToolbar::applyDrawboardMarkupToolbar() {
#ifndef Q_OS_ANDROID
  m_style = Normal;
  m_isDockedMode = true;
  m_draggable = false;
  m_orientation = Horizontal;
  if (m_markupBarMode == MarkupOff)
    m_markupBarMode = MarkupTwoRow;
  m_markupCategory = categoryForTool(mode_);
  if (btnDockToggle) {
    btnDockToggle->setIcon(QStringLiteral("dock_fixed"));
    btnDockToggle->hide();
  }
  if (btnLayoutToggle) {
    btnLayoutToggle->setIcon(m_markupBarMode == MarkupTwoRow
                                 ? QStringLiteral("layout_single")
                                 : QStringLiteral("layout_rows"));
  }
  ensureInlinePropertyControls();
  setMinimumSize(0, 0);
  setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  const int barH = preferredMarkupHeight();
  const int barW = qMax(calculateMinLength(), UiScale::dp(360));
  setFixedHeight(barH);
  resize(barW, barH);
  updateLayout(false);
  update();
#endif
}

void ModernToolbar::setMarkupBarMode(MarkupBarMode mode) {
#ifndef Q_OS_ANDROID
  if (m_markupBarMode == mode)
    return;
  m_markupBarMode = mode;
  if (mode != MarkupOff) {
    applyDrawboardMarkupToolbar();
    emit dockModeChanged(m_isDockedMode);
  } else {
    for (auto *tab : m_categoryTabs)
      if (tab)
        tab->hide();
    for (auto *b : m_inlineColorBtns)
      if (b)
        b->hide();
    for (auto *b : m_inlineWidthBtns)
      if (b)
        b->hide();
    if (btnMoreProps)
      btnMoreProps->hide();
    if (btnLayoutToggle)
      btnLayoutToggle->hide();
    updateLayout(false);
  }
#else
  Q_UNUSED(mode);
#endif
}

int ModernToolbar::preferredMarkupHeight() const {
#ifndef Q_OS_ANDROID
  if (m_markupBarMode == MarkupTwoRow)
    return UiScale::dp(86);
  if (m_markupBarMode == MarkupSingleRow)
    return UiScale::dp(48);
#endif
  return UiScale::dp(48);
}

ModernToolbar::MarkupCategory
ModernToolbar::categoryForTool(ToolMode m) const {
  switch (m) {
  case ToolMode::Hand:
  case ToolMode::Lasso:
  case ToolMode::Eraser:
    return CatSelect;
  case ToolMode::Pen:
  case ToolMode::Pencil:
  case ToolMode::Highlighter:
    return CatFreeform;
  case ToolMode::Shape:
  case ToolMode::Ruler:
    return CatShapes;
  case ToolMode::Text:
    return CatReview;
  case ToolMode::StickyNote:
  case ToolMode::Image:
    return CatInsert;
  default:
    return CatFreeform;
  }
}

QList<ToolbarBtn *>
ModernToolbar::toolsForCategory(MarkupCategory cat) const {
  switch (cat) {
  case CatSelect:
    return {btnHand, btnLasso, btnEraser};
  case CatFreeform:
    return {btnPen, btnPencil, btnHighlighter};
  case CatShapes:
    return {btnShape, btnRuler};
  case CatReview:
    return {btnText};
  case CatInsert:
    return {btnStickyNote, btnImage};
  }
  return {};
}

void ModernToolbar::setMarkupCategory(MarkupCategory cat,
                                      bool selectDefaultTool) {
  m_markupCategory = cat;
  syncCategoryTabs();
  if (selectDefaultTool) {
    const auto tools = toolsForCategory(cat);
    if (!tools.isEmpty()) {
      ToolMode target = mode_;
      bool keep = false;
      for (auto *b : tools) {
        if (b == getButtonForMode(mode_)) {
          keep = true;
          break;
        }
      }
      if (!keep) {
        if (cat == CatSelect)
          target = ToolMode::Hand;
        else if (cat == CatFreeform)
          target = ToolMode::Pen;
        else if (cat == CatShapes)
          target = ToolMode::Shape;
        else if (cat == CatReview)
          target = ToolMode::Text;
        else
          target = ToolMode::StickyNote;
        ToolManager::instance().selectTool(target);
        setToolMode(target);
        emit toolChanged(target);
      }
    }
  }
  updateLayout(false);
  update();
}

void ModernToolbar::syncCategoryTabs() {
  if (m_catSelect)
    m_catSelect->setActive(m_markupCategory == CatSelect);
  if (m_catFreeform)
    m_catFreeform->setActive(m_markupCategory == CatFreeform);
  if (m_catShapes)
    m_catShapes->setActive(m_markupCategory == CatShapes);
  if (m_catReview)
    m_catReview->setActive(m_markupCategory == CatReview);
  if (m_catInsert)
    m_catInsert->setActive(m_markupCategory == CatInsert);
}

void ModernToolbar::ensureInlinePropertyControls() {
#ifndef Q_OS_ANDROID
  if (!m_inlineColorBtns.isEmpty())
    return;
  const QList<QColor> swatches = {
      QColor(20, 20, 20),    QColor(230, 50, 50),  QColor(40, 120, 255),
      QColor(40, 170, 80),   QColor(255, 180, 0),  QColor(160, 80, 220)};
  for (const QColor &c : swatches) {
    auto *btn = new QPushButton(this);
    btn->setFixedSize(UiScale::dp(18), UiScale::dp(18));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border-radius: %2px; "
                       "border: 1px solid rgba(255,255,255,40); }"
                       "QPushButton:hover { border: 2px solid %3; }")
            .arg(c.name(QColor::HexRgb))
            .arg(UiScale::dp(9))
            .arg(m_accentColor.name(QColor::HexRgb)));
    btn->hide();
    connect(btn, &QPushButton::clicked, this, [this, c]() {
      ToolUIBridge::instance().setPenColor(c);
      syncInlinePropertyControls();
    });
    m_inlineColorBtns.append(btn);
  }
  const QList<int> widths = {2, 6, 12};
  for (int w : widths) {
    auto *btn = new QPushButton(this);
    btn->setFixedSize(UiScale::dp(22), UiScale::dp(22));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setText(QString::number(w));
    btn->hide();
    connect(btn, &QPushButton::clicked, this, [this, w]() {
      ToolUIBridge::instance().setPenWidth(w);
      syncInlinePropertyControls();
    });
    m_inlineWidthBtns.append(btn);
  }
#endif
}

void ModernToolbar::syncInlinePropertyControls() {
#ifndef Q_OS_ANDROID
  if (m_markupBarMode == MarkupOff)
    return;
  const ToolConfig cfg = ToolManager::instance().config();
  const bool showProps =
      mode_ == ToolMode::Pen || mode_ == ToolMode::Pencil ||
      mode_ == ToolMode::Highlighter || mode_ == ToolMode::Eraser ||
      mode_ == ToolMode::Shape || mode_ == ToolMode::Text ||
      mode_ == ToolMode::Ruler;
  for (auto *btn : m_inlineColorBtns) {
    if (!btn)
      continue;
    // Keep geometry from layout; only visibility here when not mid-layout.
    Q_UNUSED(showProps);
  }
  for (int i = 0; i < m_inlineWidthBtns.size(); ++i) {
    auto *btn = m_inlineWidthBtns[i];
    if (!btn)
      continue;
    const int w = (i == 0) ? 2 : (i == 1 ? 6 : 12);
    const bool active = cfg.penWidth == w;
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border-radius: %3px; "
        "border: 1px solid %4; font-size: %5px; font-weight: 600; }"
        "QPushButton:hover { border-color: %6; }")
                           .arg(active ? m_accentColor.name(QColor::HexRgb)
                                       : QStringLiteral("rgba(255,255,255,12)"))
                           .arg(active ? QStringLiteral("#101010")
                                       : NoteChrome::textPrimary().name())
                           .arg(UiScale::dp(6))
                           .arg(active ? m_accentColor.name(QColor::HexRgb)
                                       : QStringLiteral("rgba(255,255,255,28)"))
                           .arg(UiScale::sp(10))
                           .arg(m_accentColor.name(QColor::HexRgb)));
  }
#else
  Q_UNUSED(0);
#endif
}

void ModernToolbar::applyDrawboardVerticalRail() {
#ifndef Q_OS_ANDROID
  m_markupBarMode = MarkupOff;
  for (auto *tab : m_categoryTabs)
    if (tab)
      tab->hide();
  for (auto *b : m_inlineColorBtns)
    if (b)
      b->hide();
  for (auto *b : m_inlineWidthBtns)
    if (b)
      b->hide();
  if (btnMoreProps)
    btnMoreProps->hide();
  if (btnLayoutToggle)
    btnLayoutToggle->hide();
  m_isDockedMode = false;
  m_draggable = false; // Drawboard Favorites rail is edge-anchored, not free-drag.
  if (btnDockToggle) {
    btnDockToggle->setIcon(QStringLiteral("dock_float"));
    btnDockToggle->hide();
  }
  m_style = Normal;
  if (m_railSlots.isEmpty())
    loadRailTools();
  else
    rebuildSlotButtons();
  syncDrawboardToolIcons();
  syncToolBadges();
  if (m_orientation != Vertical)
    setOrientation(Vertical, false);
  else {
    const int w = preferredRailWidth();
    const int h = qMax(calculateMinLength(), UiScale::dp(200));
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setFixedWidth(w);
    resize(w, h);
    updateLayout(false);
  }
  update();
#endif
}

bool ModernToolbar::isDrawboardVerticalRail() const {
#ifndef Q_OS_ANDROID
  return m_style == Normal && m_orientation == Vertical &&
         m_markupBarMode == MarkupOff;
#else
  return false;
#endif
}

int ModernToolbar::preferredRailWidth() const { return UiScale::dp(60); }

void ModernToolbar::loadRailTools() {
  m_railSlots.clear();
  auto isKnown = [](ToolMode m) {
    switch (m) {
    case ToolMode::Pen:
    case ToolMode::Pencil:
    case ToolMode::Highlighter:
    case ToolMode::Eraser:
    case ToolMode::Lasso:
    case ToolMode::Image:
    case ToolMode::Ruler:
    case ToolMode::Shape:
    case ToolMode::StickyNote:
    case ToolMode::Text:
    case ToolMode::Hand:
      return true;
    }
    return false;
  };
  auto defaultCfg = [](ToolMode mode) -> ToolConfig {
    ToolConfig cfg = ToolManager::instance().configFor(mode);
    return cfg;
  };
  auto appendSlot = [&](ToolMode mode) {
    if (!isKnown(mode))
      return;
    m_railSlots.append(RailSlot::fromTool(mode, defaultCfg(mode)));
  };

  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QVariantList slotMaps =
      s.value(QStringLiteral("ui/drawboard_rail_slots")).toList();
  if (!slotMaps.isEmpty()) {
    for (const QVariant &v : slotMaps) {
      const QVariantMap m = v.toMap();
      bool ok = false;
      const int n = m.value(QStringLiteral("mode")).toInt(&ok);
      if (!ok)
        continue;
      const auto mode = static_cast<ToolMode>(n);
      if (!isKnown(mode))
        continue;
      RailSlot slot;
      slot.id = m.value(QStringLiteral("id")).toString();
      if (slot.id.isEmpty())
        slot.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      slot.mode = mode;
      const QString colorName = m.value(QStringLiteral("color")).toString();
      slot.color = QColor(colorName);
      if (!slot.color.isValid())
        slot.color = defaultCfg(mode).penColor;
      slot.width = qMax(1, m.value(QStringLiteral("width"), 3).toInt());
      slot.opacity = m.value(QStringLiteral("opacity"), 1.0).toDouble();
      slot.shapeKind = m.value(QStringLiteral("shapeKind"), 0).toInt();
      slot.lassoMode = m.value(QStringLiteral("lassoMode"), 0).toInt();
      m_railSlots.append(slot);
    }
  } else {
    // Migrate legacy mode-only list.
    const QVariantList saved =
        s.value(QStringLiteral("ui/drawboard_rail_tools")).toList();
    for (const QVariant &v : saved) {
      bool ok = false;
      const int n = v.toInt(&ok);
      if (!ok)
        continue;
      appendSlot(static_cast<ToolMode>(n));
    }
  }

  if (m_railSlots.isEmpty()) {
    // Drawboard-like Favorites defaults (top → bottom), includes Pencil.
    const QList<ToolMode> defaults = {
        ToolMode::Hand,        ToolMode::Lasso,       ToolMode::Pen,
        ToolMode::Pencil,      ToolMode::Highlighter, ToolMode::Eraser,
        ToolMode::Text,        ToolMode::Image,       ToolMode::StickyNote,
        ToolMode::Shape,       ToolMode::Ruler};
    for (ToolMode m : defaults)
      appendSlot(m);
  } else {
    // Ensure Pencil exists after older saves that omitted it.
    bool hasPencil = false;
    bool hasHand = false;
    for (const RailSlot &sl : m_railSlots) {
      if (sl.mode == ToolMode::Pencil)
        hasPencil = true;
      if (sl.mode == ToolMode::Hand)
        hasHand = true;
    }
    if (!hasHand) {
      RailSlot hand = RailSlot::fromTool(ToolMode::Hand, defaultCfg(ToolMode::Hand));
      m_railSlots.prepend(hand);
    }
    if (!hasPencil) {
      int penIdx = -1;
      for (int i = 0; i < m_railSlots.size(); ++i) {
        if (m_railSlots[i].mode == ToolMode::Pen) {
          penIdx = i;
          break;
        }
      }
      RailSlot pencil = RailSlot::fromTool(ToolMode::Pencil, defaultCfg(ToolMode::Pencil));
      if (penIdx >= 0)
        m_railSlots.insert(penIdx + 1, pencil);
      else
        m_railSlots.append(pencil);
    }
    if (!hasHand || !hasPencil)
      saveRailTools();
  }
  m_activeRailSlot = -1;
  rebuildSlotButtons();
}

void ModernToolbar::saveRailTools() const {
  QVariantList out;
  for (const RailSlot &slot : m_railSlots) {
    QVariantMap m;
    m.insert(QStringLiteral("id"), slot.id);
    m.insert(QStringLiteral("mode"), static_cast<int>(slot.mode));
    m.insert(QStringLiteral("color"), slot.color.name(QColor::HexArgb));
    m.insert(QStringLiteral("width"), slot.width);
    m.insert(QStringLiteral("opacity"), slot.opacity);
    m.insert(QStringLiteral("shapeKind"), slot.shapeKind);
    m.insert(QStringLiteral("lassoMode"), slot.lassoMode);
    out.append(m);
  }
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/drawboard_rail_slots"), out);
  // Keep legacy key in sync (unique modes) for older builds.
  QVariantList legacy;
  QSet<int> seen;
  for (const RailSlot &slot : m_railSlots) {
    const int n = static_cast<int>(slot.mode);
    if (seen.contains(n))
      continue;
    seen.insert(n);
    legacy.append(n);
  }
  s.setValue(QStringLiteral("ui/drawboard_rail_tools"), legacy);
}

bool ModernToolbar::railContains(ToolMode mode) const {
  for (const RailSlot &s : m_railSlots) {
    if (s.mode == mode)
      return true;
  }
  return false;
}

QList<ToolMode> ModernToolbar::railTools() const {
  QList<ToolMode> out;
  for (const RailSlot &s : m_railSlots) {
    if (!out.contains(s.mode))
      out.append(s.mode);
  }
  return out;
}

QString ModernToolbar::iconForSlot(const RailSlot &s) const {
  switch (s.mode) {
  case ToolMode::Pen:
    return QStringLiteral("pen");
  case ToolMode::Pencil:
    return QStringLiteral("pencil");
  case ToolMode::Highlighter:
    return QStringLiteral("highlighter");
  case ToolMode::Eraser:
    return QStringLiteral("eraser");
  case ToolMode::Lasso:
    return QStringLiteral("select");
  case ToolMode::Ruler:
    return QStringLiteral("measure");
  case ToolMode::Shape: {
    const auto kind = static_cast<ShapeToolKind>(s.shapeKind);
    if (kind == ShapeToolKind::Circle)
      return QStringLiteral("ellipse");
    if (kind == ShapeToolKind::Ellipse)
      return QStringLiteral("ellipse");
    if (kind == ShapeToolKind::Line)
      return QStringLiteral("line");
    if (kind == ShapeToolKind::Arrow)
      return QStringLiteral("arrow");
    if (kind == ShapeToolKind::Axes2D)
      return QStringLiteral("axes");
    if (kind == ShapeToolKind::SineGraph)
      return QStringLiteral("sine");
    if (kind == ShapeToolKind::CoordinateGraph)
      return QStringLiteral("graph");
    return QStringLiteral("rect");
  }
  case ToolMode::StickyNote:
    return QStringLiteral("stickynote");
  case ToolMode::Text:
    return QStringLiteral("text");
  case ToolMode::Image:
    return QStringLiteral("image");
  case ToolMode::Hand:
    return QStringLiteral("hand");
  }
  return QStringLiteral("pen");
}

void ModernToolbar::syncSlotButtonAppearance(int index) {
  if (index < 0 || index >= m_railSlots.size() || index >= m_slotButtons.size())
    return;
  ToolbarBtn *btn = m_slotButtons[index];
  const RailSlot &s = m_railSlots[index];
  if (!btn)
    return;
  btn->setIcon(iconForSlot(s));
  btn->setRailSlotIndex(index);
  btn->setShowChevron(toolHasFlyout(s.mode));
  btn->setRailSlotStyle(true);
  btn->setRailFooterStyle(false);
  QColor glyph;
  if (s.mode == ToolMode::Pen || s.mode == ToolMode::Pencil)
    glyph = s.color;
  else if (s.mode == ToolMode::Highlighter) {
    glyph = s.color;
    if (glyph.saturationF() < 0.25)
      glyph = QColor(90, 220, 70);
  } else if (s.mode == ToolMode::Ruler) {
    glyph = QColor(235, 70, 70);
  }
  btn->setGlyphColor(glyph);

  QString tip;
  switch (s.mode) {
  case ToolMode::Hand:
    tip = tr("Hand · Leertaste = vorübergehend schwenken");
    break;
  case ToolMode::Lasso:
    tip = tr("Auswahl (V)");
    break;
  case ToolMode::Pen:
    tip = tr("Stift (P)");
    break;
  case ToolMode::Pencil:
    tip = tr("Bleistift");
    break;
  case ToolMode::Highlighter:
    tip = tr("Textmarker (H)");
    break;
  case ToolMode::Eraser:
    tip = tr("Radierer (E)");
    break;
  case ToolMode::Text:
    tip = tr("Text (T)");
    break;
  case ToolMode::Shape:
    tip = tr("Form");
    break;
  case ToolMode::Ruler:
    tip = tr("Messen");
    break;
  case ToolMode::Image:
    tip = tr("Bild");
    break;
  case ToolMode::StickyNote:
    tip = tr("Notiz");
    break;
  default:
    tip = tr("Werkzeug");
    break;
  }
  if (s.mode == ToolMode::Pen || s.mode == ToolMode::Pencil ||
      s.mode == ToolMode::Highlighter || s.mode == ToolMode::Shape) {
    tip += QStringLiteral(" · %1px").arg(s.width);
  }
  btn->setToolTip(tip);
  btn->setActive(index == m_activeRailSlot ||
                 (m_activeRailSlot < 0 && s.mode == mode_));
}

void ModernToolbar::rebuildSlotButtons() {
  for (ToolbarBtn *b : m_slotButtons) {
    if (!b)
      continue;
    b->hide();
    b->deleteLater();
  }
  m_slotButtons.clear();

#ifndef Q_OS_ANDROID
  if (!isDrawboardVerticalRail())
    return;
#else
  return;
#endif

  for (int i = 0; i < m_railSlots.size(); ++i) {
    auto *btn = new ToolbarBtn(iconForSlot(m_railSlots[i]), this);
    btn->setAccentColor(m_accentColor);
    btn->setRailSlotStyle(true);
    btn->setRailSlotIndex(i);
    m_slotButtons.append(btn);
    syncSlotButtonAppearance(i);

    connect(btn, &ToolbarBtn::clicked, this, [this, i]() { applyRailSlot(i); });
    connect(btn, &ToolbarBtn::chevronClicked, this, [this, i]() {
      if (i < 0 || i >= m_railSlots.size())
        return;
      if (m_activeRailSlot != i || mode_ != m_railSlots[i].mode) {
        m_activeRailSlot = -2; // force apply without re-click flyout
        applyRailSlot(i);
      }
      if (toolHasFlyout(m_railSlots[i].mode))
        showToolFlyout(m_railSlots[i].mode);
    });
    connect(btn, &ToolbarBtn::removeFromRailRequested, this, [this, i]() {
      removeRailSlotAt(i);
    });
    connect(btn, &ToolbarBtn::moveInRailRequested, this, [this, i](int delta) {
      moveRailSlot(i, delta);
    });
    connect(btn, &ToolbarBtn::railDragStarted, this, [this](int idx) {
      m_railDragFrom = idx;
      m_railDragGhostY = -1;
    });
    connect(btn, &ToolbarBtn::railDragMoved, this, [this](int globalY) {
      m_railDragGhostY = mapFromGlobal(QPoint(0, globalY)).y();
      update();
    });
    connect(btn, &ToolbarBtn::railDragFinished, this, [this](int globalY) {
      if (m_railDragFrom < 0 || m_railDragFrom >= m_railSlots.size()) {
        m_railDragFrom = -1;
        m_railDragGhostY = -1;
        update();
        return;
      }
      const int localY = mapFromGlobal(QPoint(0, globalY)).y();
      int target = m_railDragFrom;
      for (int j = 0; j < m_slotButtons.size(); ++j) {
        ToolbarBtn *b = m_slotButtons[j];
        if (!b || !b->isVisible())
          continue;
        if (localY < b->y() + b->height() / 2) {
          target = j;
          break;
        }
        target = j;
      }
      if (target != m_railDragFrom) {
        m_railSlots.move(m_railDragFrom, target);
        if (m_activeRailSlot == m_railDragFrom)
          m_activeRailSlot = target;
        else if (m_railDragFrom < m_activeRailSlot && target >= m_activeRailSlot)
          --m_activeRailSlot;
        else if (m_railDragFrom > m_activeRailSlot && target <= m_activeRailSlot)
          ++m_activeRailSlot;
        saveRailTools();
        rebuildSlotButtons();
        updateLayout(false);
      }
      m_railDragFrom = -1;
      m_railDragGhostY = -1;
      update();
    });
  }
}

void ModernToolbar::applyRailSlot(int index) {
  if (index < 0 || index >= m_railSlots.size())
    return;
  const RailSlot slot = m_railSlots[index];

  if (m_activeRailSlot == index && mode_ == slot.mode) {
    if (toolHasFlyout(slot.mode)) {
      showToolFlyout(slot.mode);
      return;
    }
    emit toolOptionsRequested();
    return;
  }

  m_activeRailSlot = index;
  if (slot.mode == ToolMode::Hand) {
    ToolManager::instance().selectTool(ToolMode::Hand);
  } else {
    ToolConfig &cfg = ToolManager::instance().configFor(slot.mode);
    cfg.penColor = slot.color;
    cfg.penWidth = qMax(1, slot.width);
    cfg.opacity = slot.opacity;
    // ShapeToolKind goes through Ellipse (=7).
    cfg.shapeToolKind = static_cast<ShapeToolKind>(
        qBound(0, slot.shapeKind, 7));
    cfg.lassoMode = (slot.lassoMode == 1) ? LassoMode::Rectangle
                                          : LassoMode::Freehand;
    ToolManager::instance().selectTool(slot.mode);
    ToolManager::instance().setConfig(cfg);
  }
  setToolMode(slot.mode);
  for (int i = 0; i < m_slotButtons.size(); ++i) {
    if (m_slotButtons[i])
      m_slotButtons[i]->setActive(i == m_activeRailSlot);
  }
  emit toolChanged(slot.mode);
}

void ModernToolbar::addCurrentToolAsRailSlot() {
  const ToolMode mode = ToolManager::instance().activeToolMode();
  if (mode == ToolMode::Hand && railContains(ToolMode::Hand))
    return;
  const ToolConfig &cfg = ToolManager::instance().config();
  m_railSlots.append(RailSlot::fromTool(mode, cfg));
  saveRailTools();
  m_activeRailSlot = m_railSlots.size() - 1;
  rebuildSlotButtons();
  updateLayout(false);
  update();
}

void ModernToolbar::addToolToRail(ToolMode mode) {
  if (!getButtonForMode(mode) && mode != ToolMode::Hand)
    return;
  if (mode == ToolMode::Hand && railContains(ToolMode::Hand))
    return;
  RailSlot slot = RailSlot::fromTool(mode, ToolManager::instance().configFor(mode));
  if (mode == ToolMode::Hand) {
    m_railSlots.prepend(slot);
  } else if (mode == ToolMode::Lasso) {
    int idx = 0;
    for (int i = 0; i < m_railSlots.size(); ++i) {
      if (m_railSlots[i].mode == ToolMode::Hand) {
        idx = i + 1;
        break;
      }
    }
    m_railSlots.insert(idx, slot);
  } else {
    m_railSlots.append(slot);
  }
  saveRailTools();
  rebuildSlotButtons();
  updateLayout(false);
  update();
}

void ModernToolbar::removeRailSlotAt(int index) {
  if (index < 0 || index >= m_railSlots.size())
    return;
  if (m_railSlots.size() <= 2)
    return;
  if (m_railSlots[index].mode == ToolMode::Hand)
    return;
  const ToolMode removed = m_railSlots[index].mode;
  m_railSlots.removeAt(index);
  if (m_activeRailSlot == index)
    m_activeRailSlot = -1;
  else if (m_activeRailSlot > index)
    --m_activeRailSlot;
  saveRailTools();
  if (mode_ == removed && !railContains(removed)) {
    const ToolMode fallback = railContains(ToolMode::Pen)
                                  ? ToolMode::Pen
                                  : m_railSlots.first().mode;
    int fi = 0;
    for (int i = 0; i < m_railSlots.size(); ++i) {
      if (m_railSlots[i].mode == fallback) {
        fi = i;
        break;
      }
    }
    rebuildSlotButtons();
    applyRailSlot(fi);
  } else {
    rebuildSlotButtons();
  }
  updateLayout(false);
  update();
}

void ModernToolbar::removeToolFromRail(ToolMode mode) {
  if (mode == ToolMode::Hand)
    return;
  for (int i = 0; i < m_railSlots.size(); ++i) {
    if (m_railSlots[i].mode == mode) {
      removeRailSlotAt(i);
      return;
    }
  }
}

void ModernToolbar::moveRailSlot(int index, int delta) {
  if (index < 0 || index >= m_railSlots.size() || delta == 0)
    return;
  const int target = index + delta;
  if (target < 0 || target >= m_railSlots.size())
    return;
  m_railSlots.move(index, target);
  if (m_activeRailSlot == index)
    m_activeRailSlot = target;
  else if (index < m_activeRailSlot && target >= m_activeRailSlot)
    --m_activeRailSlot;
  else if (index > m_activeRailSlot && target <= m_activeRailSlot)
    ++m_activeRailSlot;
  saveRailTools();
  rebuildSlotButtons();
  updateLayout(false);
  update();
}

void ModernToolbar::moveRailTool(ToolMode mode, int delta) {
  for (int i = 0; i < m_railSlots.size(); ++i) {
    if (m_railSlots[i].mode == mode) {
      moveRailSlot(i, delta);
      return;
    }
  }
}

bool ModernToolbar::toolHasFlyout(ToolMode mode) const {
  return mode == ToolMode::Lasso || mode == ToolMode::Shape;
}

void ModernToolbar::showToolFlyout(ToolMode mode) {
  ToolbarBtn *anchor = nullptr;
  if (m_activeRailSlot >= 0 && m_activeRailSlot < m_slotButtons.size())
    anchor = m_slotButtons[m_activeRailSlot];
  if (!anchor)
    anchor = getButtonForMode(mode);
  if (!anchor)
    return;

  QMenu menu(this);
  menu.setStyleSheet(QStringLiteral(
      "QMenu { background: %1; color: %2; border: 1px solid %3; padding: 10px; }"
      "QMenu::item { padding: 10px 22px; border-radius: 8px; min-height: 28px; }"
      "QMenu::item:selected { background: rgba(91,157,255,0.28); }"
      "QMenu::item:checked { color: %4; font-weight: 700; }"
      "QMenu::separator { height: 1px; background: %3; margin: 8px 6px; }")
                         .arg(NoteChrome::panelElevated().name(),
                              NoteChrome::textPrimary().name(),
                              NoteChrome::border().name(),
                              NoteChrome::accent().name()));

  if (mode == ToolMode::Lasso) {
    auto *freehand = menu.addAction(tr("Freihand-Auswahl"));
    auto *rect = menu.addAction(tr("Rechteck-Auswahl"));
    freehand->setCheckable(true);
    rect->setCheckable(true);
    const LassoMode cur =
        ToolManager::instance().configFor(ToolMode::Lasso).lassoMode;
    freehand->setChecked(cur == LassoMode::Freehand);
    rect->setChecked(cur == LassoMode::Rectangle);
    QAction *picked = menu.exec(anchor->mapToGlobal(
        QPoint(-menu.sizeHint().width() - UiScale::dp(6), UiScale::dp(4))));
    if (!picked)
      return;
    ToolConfig &cfg = ToolManager::instance().configFor(ToolMode::Lasso);
    cfg.lassoMode =
        (picked == rect) ? LassoMode::Rectangle : LassoMode::Freehand;
    ToolManager::instance().selectTool(ToolMode::Lasso);
    ToolManager::instance().setConfig(cfg);
    setToolMode(ToolMode::Lasso);
    if (m_activeRailSlot >= 0 && m_activeRailSlot < m_railSlots.size() &&
        m_railSlots[m_activeRailSlot].mode == ToolMode::Lasso) {
      m_railSlots[m_activeRailSlot].lassoMode =
          static_cast<int>(cfg.lassoMode);
      saveRailTools();
    }
    syncDrawboardToolIcons();
    return;
  }

  if (mode == ToolMode::Shape) {
    auto *rect = menu.addAction(tr("Rechteck"));
    auto *circle = menu.addAction(tr("Kreis"));
    auto *ellipse = menu.addAction(tr("Ellipse"));
    auto *line = menu.addAction(tr("Linie"));
    auto *arrow = menu.addAction(tr("Pfeil"));
    menu.addSeparator();
    auto *axes = menu.addAction(tr("Achsen"));
    auto *sine = menu.addAction(tr("Sinus"));
    auto *graph = menu.addAction(tr("Graph"));
    for (QAction *a : {rect, circle, ellipse, line, arrow, axes, sine, graph})
      a->setCheckable(true);
    const ShapeToolKind cur =
        ToolManager::instance().configFor(ToolMode::Shape).shapeToolKind;
    rect->setChecked(cur == ShapeToolKind::Rectangle);
    circle->setChecked(cur == ShapeToolKind::Circle);
    ellipse->setChecked(cur == ShapeToolKind::Ellipse);
    line->setChecked(cur == ShapeToolKind::Line);
    arrow->setChecked(cur == ShapeToolKind::Arrow);
    axes->setChecked(cur == ShapeToolKind::Axes2D);
    sine->setChecked(cur == ShapeToolKind::SineGraph);
    graph->setChecked(cur == ShapeToolKind::CoordinateGraph);
    menu.addSeparator();
    auto *more = menu.addAction(tr("Weitere Eigenschaften…"));
    QAction *picked = menu.exec(anchor->mapToGlobal(
        QPoint(-menu.sizeHint().width() - UiScale::dp(6), UiScale::dp(4))));
    if (!picked)
      return;
    if (picked == more) {
      emit toolOptionsRequested();
      return;
    }
    ShapeToolKind kind = ShapeToolKind::Rectangle;
    if (picked == circle)
      kind = ShapeToolKind::Circle;
    else if (picked == ellipse)
      kind = ShapeToolKind::Ellipse;
    else if (picked == line)
      kind = ShapeToolKind::Line;
    else if (picked == arrow)
      kind = ShapeToolKind::Arrow;
    else if (picked == axes)
      kind = ShapeToolKind::Axes2D;
    else if (picked == sine)
      kind = ShapeToolKind::SineGraph;
    else if (picked == graph)
      kind = ShapeToolKind::CoordinateGraph;
    ToolConfig &cfg = ToolManager::instance().configFor(ToolMode::Shape);
    cfg.shapeToolKind = kind;
    ToolManager::instance().selectTool(ToolMode::Shape);
    ToolManager::instance().setConfig(cfg);
    setToolMode(ToolMode::Shape);
    if (m_activeRailSlot >= 0 && m_activeRailSlot < m_railSlots.size() &&
        m_railSlots[m_activeRailSlot].mode == ToolMode::Shape) {
      m_railSlots[m_activeRailSlot].shapeKind = static_cast<int>(kind);
      saveRailTools();
    }
    syncDrawboardToolIcons();
  }
}

QList<ToolbarBtn *> ModernToolbar::currentRailButtons() const {
  QList<ToolbarBtn *> out;
  if (!m_slotButtons.isEmpty()) {
    for (ToolbarBtn *b : m_slotButtons) {
      if (b)
        out.append(b);
    }
  } else {
    for (const RailSlot &s : m_railSlots) {
      if (ToolbarBtn *b =
              const_cast<ModernToolbar *>(this)->getButtonForMode(s.mode))
        out.append(b);
    }
  }
  // Drawboard footer: Markup Library → "+" → edge chevron.
  if (btnLibrary)
    out.append(btnLibrary);
  if (btnAddTool)
    out.append(btnAddTool);
  if (btnRailChevron)
    out.append(btnRailChevron);
  return out;
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

#ifndef Q_OS_ANDROID
  // Desktop Drawboard default: vertical Favorites rail on the right.
  // "Docked" still means edge-anchored vertical rail (not Windows top bar).
  if (docked && m_style == Normal) {
    applyDrawboardVerticalRail();
    emit dockModeChanged(true);
    return;
  }
  if (!docked && m_markupBarMode != MarkupOff)
    m_markupBarMode = MarkupOff;
#endif

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
    Q_UNUSED(visibleOffset);
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
  emit dockModeChanged(m_isDockedMode);
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

#ifndef Q_OS_ANDROID
  if (m_markupBarMode != MarkupOff) {
    // Category tabs + tools of active category + inline props + chrome.
    const int catW = (m_markupBarMode == MarkupTwoRow)
                         ? UiScale::dp(92) * 5 + UiScale::dp(8) * 4
                         : UiScale::dp(40) * 5 + UiScale::dp(4) * 4;
    const int tools = qMax(1, toolsForCategory(m_markupCategory).size());
    const int toolsW = tools * btnS + (tools - 1) * minGap;
    const int propsW = UiScale::dp(170);
    const int chromeW = UiScale::dp(40) * 2 + minGap; // more + layout
    return UiScale::dp(24) + catW + UiScale::dp(16) + toolsW + UiScale::dp(16) +
           propsW + chromeW + UiScale::dp(16);
  }
#endif
  
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
#ifndef Q_OS_ANDROID
    if (m_orientation == Vertical) {
      const int n = qMax(1, m_railSlots.size()) + 3; // tools + library/+ /chevron
      const int cell = UiScale::dp(52);
      const int gap = UiScale::dp(8);
      // Two section dividers (select / ink / insert).
      return dragH + n * cell + (n - 1) * gap + UiScale::dp(48) + 16;
    }
#endif
    int numButtons = 0;
    for (auto *b : m_buttons) {
      if (m_dockedOnlyButtons.contains(b)) continue;
      if (b == btnMoreProps || b == btnLayoutToggle) continue;
      numButtons++;
    }
    return dragH + (numButtons * btnS) + ((numButtons - 1) * minGap) + 16;
  }
}

void ModernToolbar::updateMarkupLayout(bool animate) {
#ifndef Q_OS_ANDROID
  const int w = width();
  const int h = height();
  const bool twoRow = (m_markupBarMode == MarkupTwoRow);
  const int btnS = twoRow ? UiScale::dp(36) : UiScale::dp(34);
  const int gap = UiScale::dp(4);
  const int margin = UiScale::dp(10);
  m_separatorXPositions.clear();
  m_markupRowDividerY = -1;

  // Hide legacy chrome / clutter — utilities live in bottom bar.
  for (ToolbarBtn *b : leftChromeButtons()) {
    if (!b)
      continue;
    b->setDrawFloatingBg(false);
    b->hide();
    if (b->parentWidget() != this)
      b->setParent(this);
  }
  for (ToolbarBtn *b : m_dockedOnlyButtons) {
    if (b)
      b->hide();
  }
  if (btnDockToggle)
    btnDockToggle->hide();
  if (btnAddTool)
    btnAddTool->hide();

  // Hide all tool buttons first; show only active-category tools.
  const QList<ToolbarBtn *> markupTools = {btnHand,        btnLasso,
                                           btnEraser,      btnPen,
                                           btnPencil,      btnHighlighter,
                                           btnShape,       btnRuler,
                                           btnText,        btnStickyNote,
                                           btnImage};
  for (auto *b : markupTools) {
    if (b) {
      b->setBtnSize(btnS);
      b->hide();
    }
  }
  if (btnMoreProps)
    btnMoreProps->setBtnSize(btnS);
  if (btnLayoutToggle)
    btnLayoutToggle->setBtnSize(UiScale::dp(30));

  syncCategoryTabs();
  ensureInlinePropertyControls();

  const bool showProps =
      mode_ == ToolMode::Pen || mode_ == ToolMode::Pencil ||
      mode_ == ToolMode::Highlighter || mode_ == ToolMode::Eraser ||
      mode_ == ToolMode::Shape || mode_ == ToolMode::Text ||
      mode_ == ToolMode::Ruler;

  auto place = [animate](QWidget *b, int x, int y) {
    if (!b)
      return;
    if (animate) {
      auto *anim = new QPropertyAnimation(b, "pos");
      anim->setDuration(160);
      anim->setEasingCurve(QEasingCurve::OutCubic);
      anim->setEndValue(QPoint(x, y));
      anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
      b->move(x, y);
    }
    b->show();
    b->raise();
  };

  // Right chrome: layout toggle + more options
  int rightX = w - margin;
  if (btnLayoutToggle) {
    rightX -= btnLayoutToggle->width();
    const int y = twoRow ? (UiScale::dp(34) - btnLayoutToggle->height()) / 2
                         : (h - btnLayoutToggle->height()) / 2;
    place(btnLayoutToggle, rightX, y);
    rightX -= gap;
  }
  if (btnMoreProps) {
    rightX -= btnMoreProps->width();
    const int toolsRowY =
        twoRow ? UiScale::dp(38) + (UiScale::dp(48) - btnS) / 2
               : (h - btnS) / 2;
    place(btnMoreProps, rightX, toolsRowY);
    rightX -= UiScale::dp(10);
  }

  // Inline quick properties (color + width) just left of More.
  int propsEndX = rightX;
  if (showProps) {
    int px = propsEndX;
    for (int i = m_inlineWidthBtns.size() - 1; i >= 0; --i) {
      auto *b = m_inlineWidthBtns[i];
      if (!b)
        continue;
      px -= b->width();
      const int toolsRowY =
          twoRow ? UiScale::dp(38) + (UiScale::dp(48) - b->height()) / 2
                 : (h - b->height()) / 2;
      place(b, px, toolsRowY);
      px -= UiScale::dp(4);
    }
    px -= UiScale::dp(6);
    m_separatorXPositions.append(px);
    px -= UiScale::dp(6);
    for (int i = m_inlineColorBtns.size() - 1; i >= 0; --i) {
      auto *b = m_inlineColorBtns[i];
      if (!b)
        continue;
      px -= b->width();
      const int toolsRowY =
          twoRow ? UiScale::dp(38) + (UiScale::dp(48) - b->height()) / 2
                 : (h - b->height()) / 2;
      place(b, px, toolsRowY);
      px -= UiScale::dp(4);
    }
    propsEndX = px;
  } else {
    for (auto *b : m_inlineColorBtns)
      if (b)
        b->hide();
    for (auto *b : m_inlineWidthBtns)
      if (b)
        b->hide();
  }

  // Category tabs — shrink labels on narrow bars so tools still fit.
  int catX = margin;
  const int catH = twoRow ? UiScale::dp(34) : UiScale::dp(40);
  const int catY = twoRow ? UiScale::dp(4) : (h - catH) / 2;
  const int catCount = qMax(1, m_categoryTabs.size());
  int catW = twoRow ? UiScale::dp(92) : UiScale::dp(36);
  if (twoRow) {
    const int budget = qMax(UiScale::dp(200), propsEndX - margin - UiScale::dp(8));
    catW = qBound(UiScale::dp(56), budget / catCount - UiScale::dp(4),
                 UiScale::dp(100));
  }
  for (auto *tab : m_categoryTabs) {
    if (!tab)
      continue;
    // Compact (icon-only) when single-row, or when two-row tabs get narrow.
    tab->setCompact(!twoRow || catW < UiScale::dp(70));
    tab->setFixedHeight(catH);
    tab->setFixedWidth(twoRow ? catW : UiScale::dp(36));
    place(tab, catX, catY);
    catX += tab->width() + (twoRow ? UiScale::dp(4) : UiScale::dp(2));
  }
  m_separatorXPositions.append(catX + UiScale::dp(4));

  // Tools for active category
  QList<ToolbarBtn *> tools = toolsForCategory(m_markupCategory);
  int toolX = twoRow ? margin : (catX + UiScale::dp(12));
  const int toolsRowY =
      twoRow ? UiScale::dp(38) + (UiScale::dp(48) - btnS) / 2 : (h - btnS) / 2;
  if (twoRow)
    m_markupRowDividerY = UiScale::dp(38);

  const int maxToolX = propsEndX - UiScale::dp(12);
  for (auto *b : tools) {
    if (!b)
      continue;
    if (toolX + btnS > maxToolX)
      break;
    place(b, toolX, toolsRowY);
    toolX += btnS + gap;
  }

  syncInlinePropertyControls();
  Q_UNUSED(animate);
#else
  Q_UNUSED(animate);
#endif
}

void ModernToolbar::updateLayout(bool animate) {
  if (m_isDragging)
    animate = false;
#ifndef Q_OS_ANDROID
  if (m_style == Normal && m_markupBarMode != MarkupOff && m_isDockedMode &&
      m_orientation == Horizontal) {
    updateMarkupLayout(animate);
    updateActiveIndicator(false);
    return;
  }
#endif
  // Hide markup-only chrome unless the docked markup layout is active.
  const bool markupActive = (m_markupBarMode != MarkupOff && m_isDockedMode &&
                             m_orientation == Horizontal && m_style == Normal);
  if (!markupActive) {
    for (auto *tab : m_categoryTabs)
      if (tab)
        tab->hide();
    for (auto *b : m_inlineColorBtns)
      if (b)
        b->hide();
    for (auto *b : m_inlineWidthBtns)
      if (b)
        b->hide();
    if (btnMoreProps)
      btnMoreProps->hide();
    if (btnLayoutToggle)
      btnLayoutToggle->hide();
    if (!isDrawboardVerticalRail()) {
      if (btnLibrary)
        btnLibrary->hide();
      if (btnRailChevron)
        btnRailChevron->hide();
    }
  }
  if (m_style == Normal) {
    int w = width();
    int h = height();
    int btnS = effectiveButtonSize(w, h);
    
    for (auto *b : m_buttons) {
      b->setBtnSize(btnS);
    }
    
    int numVisible = 0;
    for (auto *b : m_buttons) {
      if (b == btnMoreProps || b == btnLayoutToggle || b == btnLibrary ||
          b == btnRailChevron) {
        b->hide();
        continue;
      }
      if (m_dockedOnlyButtons.contains(b)) {
        b->setVisible(m_isDockedMode);
      } else {
        b->setVisible(true);
      }
      if (b->isVisible()) numVisible++;
    }
    
    int dragSize = UiScale::dp(8);
    if (m_draggable && !m_isDockedMode)
      dragSize = (m_orientation == Vertical) ? UiScale::dp(22) : UiScale::dp(14);
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
          if (!leftGroup.contains(b) && !rightGroup.contains(b) &&
              b != btnMoreProps && b != btnLayoutToggle) {
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

#ifndef Q_OS_ANDROID
      // Desktop Drawboard vertical rail: undo/redo/back live in bottom/title
      // chrome — keep them out of the floating pill (no top-left floaters).
      if (m_orientation == Vertical) {
        for (ToolbarBtn *b : chromeRow) {
          if (!b)
            continue;
          b->setDrawFloatingBg(false);
          b->hide();
          if (b->parentWidget() != this)
            b->setParent(this);
        }
      } else
#endif
      {
      // Undock horizontal: back + undo + redo in one row on the editor surface
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
      }

      // Drawboard Favorites rail: persisted tool slots + "+" at bottom.
      QList<ToolbarBtn *> railOrder;
      m_separatorYPositions.clear();
      if (m_orientation == Vertical) {
        if (m_railSlots.isEmpty())
          loadRailTools();
        if (m_slotButtons.isEmpty())
          rebuildSlotButtons();
        railOrder = currentRailButtons();
        btnS = UiScale::dp(48);
        gap = UiScale::dp(6);
      } else {
        for (auto *b : m_buttons) {
          if (!chromeRow.contains(b) && b != btnMoreProps &&
              b != btnLayoutToggle && b != btnLibrary && b != btnRailChevron)
            railOrder.append(b);
        }
      }

      if (m_orientation == Vertical) {
        // Fixed footer (library / + / chevron); scrollable tool slots above.
        QList<ToolbarBtn *> slotBtns;
        QList<ToolbarBtn *> footerBtns;
        for (ToolbarBtn *b : railOrder) {
          if (!b)
            continue;
          if (b == btnLibrary || b == btnAddTool || b == btnRailChevron)
            footerBtns.append(b);
          else
            slotBtns.append(b);
        }
        const int footerGap = UiScale::dp(4);
        const int footerBtnS = UiScale::dp(40);
        const int footerH =
            footerBtns.size() * footerBtnS +
            qMax(0, footerBtns.size() - 1) * footerGap + UiScale::dp(14);
        const int contentTop = dragSize + UiScale::dp(8);
        const int contentBottom = h - footerH - UiScale::dp(8);
        const int contentH = qMax(btnS, contentBottom - contentTop);

        // Content height of all slots (+ soft dividers).
        int contentNeeded = 0;
        for (int i = 0; i < slotBtns.size(); ++i) {
          contentNeeded += btnS + gap;
          // Dividers after select-group / ink-group.
          if (i < m_railSlots.size()) {
            const ToolMode m = m_railSlots[i].mode;
            if (m == ToolMode::Lasso || m == ToolMode::Eraser)
              contentNeeded += UiScale::dp(10);
          }
        }
        const int maxScroll = qMax(0, contentNeeded - contentH);
        m_railScrollPx = qBound(0, m_railScrollPx, maxScroll);
        m_railContentTop = contentTop;
        m_railContentBottom = contentBottom;
        m_railMaxScroll = maxScroll;

        int y = contentTop - m_railScrollPx;
        int lastSlotBottom = contentTop;
        for (int i = 0; i < slotBtns.size(); ++i) {
          ToolbarBtn *b = slotBtns[i];
          b->setRailSlotStyle(true);
          b->setBtnCell(w - UiScale::dp(6), btnS);
          const int bx = UiScale::dp(3);
          const int by = y;
          y += b->height() + gap;
          if (i < m_railSlots.size()) {
            const ToolMode m = m_railSlots[i].mode;
            if (m == ToolMode::Lasso || m == ToolMode::Eraser) {
              y += UiScale::dp(4);
              const int sepY = y - gap / 2;
              if (sepY >= contentTop && sepY <= contentBottom)
                m_separatorYPositions.append(sepY);
              y += UiScale::dp(4);
            }
          }
          lastSlotBottom = by + b->height();
          const bool visible =
              (by + b->height() > contentTop) && (by < contentBottom);
          if (visible) {
            b->move(bx, by);
            b->show();
            b->raise();
          } else {
            b->hide();
          }
        }

        // Pack footer under the tool cluster when everything fits; otherwise
        // pin to the bottom edge (scrollable slots above).
        int fy = contentBottom + UiScale::dp(8);
        if (maxScroll == 0)
          fy = qMin(contentBottom + UiScale::dp(8),
                    lastSlotBottom + UiScale::dp(12));
        m_separatorYPositions.append(fy - UiScale::dp(6));
        for (ToolbarBtn *b : footerBtns) {
          b->setRailSlotStyle(true);
          b->setRailFooterStyle(true);
          b->setBtnCell(w - UiScale::dp(6), footerBtnS);
          b->move(UiScale::dp(3), fy);
          b->show();
          b->raise();
          fy += b->height() + footerGap;
        }

        for (auto *b : m_buttons) {
          if (!b || chromeRow.contains(b) || railOrder.contains(b))
            continue;
          b->setRailSlotStyle(false);
          b->hide();
        }
        if (btnDockToggle)
          btnDockToggle->hide();
        if (m_activeIndicator)
          m_activeIndicator->hide();
      } else {
        for (auto *b : railOrder) {
          if (!b)
            continue;
          if (chromeRow.contains(b))
            continue;
          if (m_dockedOnlyButtons.contains(b)) {
            b->hide();
            continue;
          }
          b->setRailSlotStyle(false);
          b->setBtnSize(btnS);
          const int bx = currentPos;
          const int by = (h - btnS) / 2;
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
