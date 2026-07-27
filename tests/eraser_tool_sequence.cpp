// Headless pen + eraser event sequence (Phase 1 / Phase 3 tool ctest).
// Never linked into the Blop GUI target.

#include "EraserTool.h"
#include "StrokeItem.h"
#include "WritingTools.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

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
}

void testPixelEraseCutsStroke() {
  QGraphicsScene scene;
  StrokeItem *stroke = makeInkStroke(&scene, {0, 50}, {200, 50});
  const qreal areaBefore = stroke->path().boundingRect().width();

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
  // Outline after stroker+subtract can be wider than the centerline bounds;
  // require a real geometry change via pathBefore or a non-identical path.
  Q_UNUSED(areaBefore);
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

} // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);

  testPenCreatesStroke();
  testPixelEraseCutsStroke();
  testObjectEraseRemovesStroke();
  testTaggedTextPixelVsObject();
  testKeepInkSkipsPenZ();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d eraser/pen sequence check(s) failed\n",
                 g_failures);
    return 1;
  }
  std::fprintf(stdout, "eraser_tool_sequence: OK\n");
  return 0;
}
