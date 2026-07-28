// Headless pen + eraser event sequence (Phase 1 / Phase 3 tool ctest).
// Never linked into the Blop GUI target.

#include "EraserTool.h"
#include "StrokeItem.h"
#include "WritingTools.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPointingDevice>
#include <QTabletEvent>

#include <cstdio>

namespace {

int g_failures = 0;

void fail(const char *msg) {
  std::fprintf(stderr, "FAIL: %s\n", msg);
  ++g_failures;
}

void expect(bool cond, const char *msg) {
  if (!cond)
    fail(msg);
}

void sendMouse(AbstractTool *tool, QGraphicsScene *scene, QEvent::Type type,
               const QPointF &scenePos) {
  QGraphicsSceneMouseEvent ev(type);
  ev.setScenePos(scenePos);
  ev.setPos(scenePos);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
                                                          : Qt::LeftButton);
  if (type == QEvent::GraphicsSceneMousePress)
    tool->handleMousePress(&ev, scene);
  else if (type == QEvent::GraphicsSceneMouseMove)
    tool->handleMouseMove(&ev, scene);
  else
    tool->handleMouseRelease(&ev, scene);
}

StrokeItem *makeInkStroke(QGraphicsScene *scene, const QPointF &a,
                          const QPointF &b, qreal z = 10.0) {
  QPainterPath path(a);
  path.lineTo(b);
  QPen pen(QColor(20, 40, 200), 8.0, Qt::SolidLine, Qt::RoundCap,
           Qt::RoundJoin);
  auto *stroke = new StrokeItem(path, pen);
  stroke->setZValue(z);
  scene->addItem(stroke);
  return stroke;
}

void testPenCreatesStroke() {
  QGraphicsScene scene;
  PenTool pen;
  ToolConfig cfg;
  cfg.penWidth = 4;
  cfg.penColor = QColor(10, 10, 10);
  cfg.opacity = 1.0;
  pen.setConfig(cfg);

  sendMouse(&pen, &scene, QEvent::GraphicsSceneMousePress, {10, 10});
  sendMouse(&pen, &scene, QEvent::GraphicsSceneMouseMove, {40, 30});
  sendMouse(&pen, &scene, QEvent::GraphicsSceneMouseMove, {70, 20});
  sendMouse(&pen, &scene, QEvent::GraphicsSceneMouseRelease, {70, 20});

  auto *item = dynamic_cast<StrokeItem *>(pen.lastCompletedItem());
  expect(item != nullptr, "pen: lastCompletedItem is StrokeItem");
  expect(item && !item->path().isEmpty(), "pen: path non-empty");
  expect(scene.items().contains(item), "pen: stroke is on scene");
  expect(item && qFuzzyCompare(item->zValue(), 10.0), "pen: z≈10");
}

void testPencilCreatesStroke() {
  QGraphicsScene scene;
  PencilTool pencil;
  ToolConfig cfg;
  cfg.penWidth = 5;
  cfg.penColor = QColor(40, 40, 40);
  cfg.opacity = 1.0;
  cfg.hardness = 40;
  cfg.texture = QStringLiteral("Fein");
  pencil.setConfig(cfg);

  sendMouse(&pencil, &scene, QEvent::GraphicsSceneMousePress, {5, 5});
  sendMouse(&pencil, &scene, QEvent::GraphicsSceneMouseMove, {35, 25});
  sendMouse(&pencil, &scene, QEvent::GraphicsSceneMouseRelease, {55, 15});

  auto *item = dynamic_cast<StrokeItem *>(pencil.lastCompletedItem());
  expect(item != nullptr, "pencil: lastCompletedItem is StrokeItem");
  expect(item && !item->path().isEmpty(), "pencil: path non-empty");
  expect(scene.items().contains(item), "pencil: stroke is on scene");
  expect(item && qFuzzyCompare(item->zValue(), 15.0), "pencil: z≈15");
  expect(item && item->pen().brush().style() != Qt::NoBrush,
         "pencil: textured brush");
}

void testHighlighterCreatesStroke() {
  QGraphicsScene scene;
  HighlighterTool marker;
  ToolConfig cfg;
  cfg.penWidth = 6;
  cfg.penColor = QColor(255, 230, 0);
  cfg.opacity = 1.0;
  cfg.drawBehind = true;
  cfg.tipType = HighlighterTip::Round;
  marker.setConfig(cfg);

  sendMouse(&marker, &scene, QEvent::GraphicsSceneMousePress, {20, 40});
  sendMouse(&marker, &scene, QEvent::GraphicsSceneMouseMove, {80, 40});
  sendMouse(&marker, &scene, QEvent::GraphicsSceneMouseRelease, {80, 40});

  auto *item = dynamic_cast<StrokeItem *>(marker.lastCompletedItem());
  expect(item != nullptr, "highlighter: lastCompletedItem is StrokeItem");
  expect(item && !item->path().isEmpty(), "highlighter: path non-empty");
  expect(scene.items().contains(item), "highlighter: stroke is on scene");
  expect(item && item->strokeStyle() == StrokeItem::Highlighter,
         "highlighter: drawBehind → Highlighter style");
  expect(item && qFuzzyCompare(item->zValue(), -10.0),
         "highlighter: drawBehind z≈-10");
  expect(item && item->pen().width() >= 10, "highlighter: wide tip");
}

void testPixelEraseCutsStroke() {
  QGraphicsScene scene;
  StrokeItem *stroke = makeInkStroke(&scene, {0, 50}, {200, 50});

  EraserTool eraser;
  ToolConfig cfg;
  cfg.penWidth = 24;
  cfg.eraserMode = EraserMode::Pixel;
  cfg.eraserKeepInk = false;
  eraser.setConfig(cfg);

  int sessions = 0;
  QObject::connect(&eraser, &EraserTool::eraseSessionFinished,
                   [&](const QList<QGraphicsItem *> &,
                       const QHash<QGraphicsPathItem *, ErasePathSnapshot> &) {
                     ++sessions;
                   });
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMousePress, {100, 50});
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMouseMove, {110, 50});
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMouseRelease, {110, 50});

  expect(sessions == 1, "pixel: eraseSessionFinished emitted");
  expect(stroke->pen().style() == Qt::NoPen,
         "pixel: first cut converts stroke to NoPen outline");
  expect(stroke->points().isEmpty(), "pixel: points cleared after cut");
  // Outline after stroker+subtract can be wider than the centerline bounds.
  expect(!stroke->path().isEmpty() || !scene.items().contains(stroke),
         "pixel: cut left outline or removed empty stroke");
}

void testObjectEraseRemovesStroke() {
  QGraphicsScene scene;
  StrokeItem *stroke = makeInkStroke(&scene, {0, 20}, {120, 20});

  EraserTool eraser;
  ToolConfig cfg;
  cfg.penWidth = 8;
  cfg.eraserMode = EraserMode::Object;
  eraser.setConfig(cfg);

  QList<QGraphicsItem *> removed;
  QObject::connect(&eraser, &EraserTool::eraseSessionFinished,
                   [&](const QList<QGraphicsItem *> &r,
                       const QHash<QGraphicsPathItem *, ErasePathSnapshot> &) {
                     removed = r;
                   });

  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMousePress, {60, 20});
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMouseRelease, {60, 20});

  expect(!scene.items().contains(stroke), "object: stroke removed from scene");
  expect(removed.contains(stroke), "object: stroke listed in removed");
  qDeleteAll(removed);
}

void testTaggedTextPixelVsObject() {
  QGraphicsScene scene;
  auto *textLike = new QGraphicsPathItem();
  QPainterPath box;
  box.addRect(QRectF(40, 40, 40, 20));
  textLike->setPath(box);
  textLike->setPen(QPen(Qt::black, 2));
  textLike->setData(0, QStringLiteral("text"));
  scene.addItem(textLike);

  EraserTool eraser;
  ToolConfig cfg;
  cfg.penWidth = 30;
  cfg.eraserMode = EraserMode::Pixel;
  eraser.setConfig(cfg);

  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMousePress, {60, 50});
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMouseRelease, {60, 50});
  expect(scene.items().contains(textLike),
         "pixel: tagged text ignored");

  cfg.eraserMode = EraserMode::Object;
  eraser.setConfig(cfg);
  QList<QGraphicsItem *> removed;
  QObject::connect(&eraser, &EraserTool::eraseSessionFinished,
                   [&](const QList<QGraphicsItem *> &r,
                       const QHash<QGraphicsPathItem *, ErasePathSnapshot> &) {
                     removed = r;
                   });
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMousePress, {60, 50});
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMouseRelease, {60, 50});
  expect(!scene.items().contains(textLike),
         "object: tagged text removed");
  qDeleteAll(removed);
}

void testKeepInkSkipsPenZ() {
  QGraphicsScene scene;
  StrokeItem *ink = makeInkStroke(&scene, {0, 80}, {160, 80}, 10.0);

  EraserTool eraser;
  ToolConfig cfg;
  cfg.penWidth = 28;
  cfg.eraserMode = EraserMode::Pixel;
  cfg.eraserKeepInk = true;
  eraser.setConfig(cfg);

  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMousePress, {80, 80});
  sendMouse(&eraser, &scene, QEvent::GraphicsSceneMouseRelease, {80, 80});

  expect(scene.items().contains(ink), "keepInk: z>=10 stroke preserved");
  expect(ink->pen().style() != Qt::NoPen,
         "keepInk: stroke pen unchanged");
}

void testPressureResponseCurve() {
  PenTool pen;
  ToolConfig cfg;
  cfg.pressureSensitivity = true;
  pen.setConfig(cfg);

  expect(qFuzzyCompare(pen.blopPressureResponse(1.0), 1.0),
         "pressure: full → 1.0");
  const qreal mid = pen.blopPressureResponse(0.5);
  expect(mid > 0.30 && mid < 1.0, "pressure: mid soft-knee");
  const qreal low = pen.blopPressureResponse(0.05);
  expect(low >= 0.30, "pressure: never below soft floor");

  cfg.pressureSensitivity = false;
  pen.setConfig(cfg);
  expect(qFuzzyCompare(pen.blopPressureResponse(0.2), 1.0),
         "pressure: disabled → 1.0");
}

void sendTablet(AbstractStrokeTool *tool, QEvent::Type type,
                const QPointF &scenePos, qreal pressure) {
  const QPointingDevice *dev = QPointingDevice::primaryPointingDevice();
  const Qt::MouseButton button =
      type == QEvent::TabletRelease ? Qt::NoButton : Qt::LeftButton;
  const Qt::MouseButtons buttons =
      type == QEvent::TabletRelease ? Qt::MouseButtons{} : Qt::LeftButton;
  QTabletEvent ev(type, dev, scenePos, scenePos, pressure, 0.f, 0.f, 0.f, 0.0,
                  0.f, Qt::NoModifier, button, buttons);
  tool->handleTabletEvent(&ev, scenePos);
}

void testTabletPenCreatesStroke() {
  QGraphicsScene scene;
  PenTool pen;
  ToolConfig cfg;
  cfg.penWidth = 4;
  cfg.penColor = QColor(5, 5, 5);
  cfg.opacity = 1.0;
  cfg.pressureSensitivity = true;
  pen.setConfig(cfg);
  pen.setStrokeSceneForTablet(&scene);

  sendTablet(&pen, QEvent::TabletPress, {15, 15}, 0.4);
  sendTablet(&pen, QEvent::TabletMove, {45, 35}, 0.7);
  sendTablet(&pen, QEvent::TabletMove, {75, 25}, 0.9);
  sendTablet(&pen, QEvent::TabletRelease, {75, 25}, 0.0);

  auto *item = dynamic_cast<StrokeItem *>(pen.lastCompletedItem());
  expect(item != nullptr, "tablet: lastCompletedItem");
  expect(item && !item->path().isEmpty(), "tablet: path non-empty");
  expect(scene.items().contains(item), "tablet: on scene");
  expect(item && item->points().size() >= 2, "tablet: pressure points");
}

} // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);

  testPenCreatesStroke();
  testPencilCreatesStroke();
  testHighlighterCreatesStroke();
  testPixelEraseCutsStroke();
  testObjectEraseRemovesStroke();
  testTaggedTextPixelVsObject();
  testKeepInkSkipsPenZ();
  testPressureResponseCurve();
  testTabletPenCreatesStroke();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d writing/eraser sequence check(s) failed\n",
                 g_failures);
    return 1;
  }
  std::fprintf(stdout, "eraser_tool_sequence: OK\n");
  return 0;
}
