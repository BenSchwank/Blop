// Headless lasso selection + transform overlay move (Phase 1 tool sequences).
// Never linked into the Blop GUI target.

#include "LassoTool.h"
#include "TransformOverlay.h"

#include <QApplication>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>

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

bool sendToolMouse(AbstractTool *tool, QGraphicsScene *scene, QEvent::Type type,
                   const QPointF &scenePos) {
  QGraphicsSceneMouseEvent ev(type);
  ev.setScenePos(scenePos);
  ev.setPos(scenePos);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
                                                          : Qt::LeftButton);
  if (type == QEvent::GraphicsSceneMousePress)
    return tool->handleMousePress(&ev, scene);
  if (type == QEvent::GraphicsSceneMouseMove)
    return tool->handleMouseMove(&ev, scene);
  return tool->handleMouseRelease(&ev, scene);
}

void sendItemMouse(QGraphicsItem *item, QGraphicsScene *scene, QEvent::Type type,
                   const QPointF &scenePos) {
  QGraphicsSceneMouseEvent ev(type);
  ev.setScenePos(scenePos);
  ev.setPos(item->mapFromScene(scenePos));
  ev.setButton(Qt::LeftButton);
  ev.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
                                                          : Qt::LeftButton);
  scene->sendEvent(item, &ev);
}

QGraphicsRectItem *makeSelectable(QGraphicsScene *scene, const QRectF &r) {
  auto *item = new QGraphicsRectItem(r);
  item->setFlag(QGraphicsItem::ItemIsSelectable, true);
  item->setFlag(QGraphicsItem::ItemIsMovable, true);
  item->setBrush(QColor(80, 140, 220));
  scene->addItem(item);
  return item;
}

class FakeRulerItem : public QGraphicsRectItem {
public:
  using QGraphicsRectItem::QGraphicsRectItem;
  int type() const override { return kBlopRulerItemType; }
};

void testRectLassoSelectsInsideOnly() {
  QGraphicsScene scene;
  auto *inside = makeSelectable(&scene, QRectF(20, 20, 30, 30));
  auto *outside = makeSelectable(&scene, QRectF(200, 200, 30, 30));

  LassoTool lasso;
  ToolConfig cfg;
  cfg.lassoMode = LassoMode::Rectangle;
  cfg.aspectLock = false;
  lasso.setConfig(cfg);

  expect(sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMousePress, {10, 10}),
         "rect: press starts lasso");
  expect(sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseMove, {80, 80}),
         "rect: move updates");
  expect(sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseRelease, {80, 80}),
         "rect: release selects");

  expect(inside->isSelected(), "rect: inside item selected");
  expect(!outside->isSelected(), "rect: outside item not selected");
  expect(scene.selectedItems().size() == 1, "rect: exactly one selected");
}

void testFreehandLassoSelects() {
  QGraphicsScene scene;
  auto *target = makeSelectable(&scene, QRectF(40, 40, 20, 20));
  auto *miss = makeSelectable(&scene, QRectF(180, 40, 20, 20));

  LassoTool lasso;
  ToolConfig cfg;
  cfg.lassoMode = LassoMode::Freehand;
  lasso.setConfig(cfg);

  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMousePress, {30, 30});
  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseMove, {70, 30});
  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseMove, {70, 70});
  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseMove, {30, 70});
  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseRelease, {30, 30});

  expect(target->isSelected(), "freehand: enclosed item selected");
  expect(!miss->isSelected(), "freehand: distant item not selected");
}

void testPressOnSelectedReturnsFalse() {
  QGraphicsScene scene;
  auto *item = makeSelectable(&scene, QRectF(10, 10, 40, 40));
  item->setSelected(true);

  LassoTool lasso;
  ToolConfig cfg;
  cfg.lassoMode = LassoMode::Rectangle;
  cfg.aspectLock = false;
  lasso.setConfig(cfg);

  const bool handled =
      sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMousePress, {25, 25});
  expect(!handled, "selected press: tool yields to Qt move");
  expect(item->isSelected(), "selected press: selection kept");
}

void testRulerTypeExcluded() {
  QGraphicsScene scene;
  auto *ink = makeSelectable(&scene, QRectF(20, 20, 40, 40));
  auto *ruler = new FakeRulerItem(QRectF(25, 25, 30, 30));
  ruler->setFlag(QGraphicsItem::ItemIsSelectable, true);
  scene.addItem(ruler);

  LassoTool lasso;
  ToolConfig cfg;
  cfg.lassoMode = LassoMode::Rectangle;
  cfg.aspectLock = false;
  lasso.setConfig(cfg);

  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMousePress, {10, 10});
  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseMove, {80, 80});
  sendToolMouse(&lasso, &scene, QEvent::GraphicsSceneMouseRelease, {80, 80});

  expect(ink->isSelected(), "ruler-exclude: ink selected");
  expect(!ruler->isSelected(), "ruler-exclude: ruler type cleared");
}

void testTransformOverlayCenterMove() {
  QGraphicsScene scene;
  QGraphicsView view(&scene);
  view.resize(400, 300);
  view.show();

  auto *target = makeSelectable(&scene, QRectF(0, 0, 80, 60));
  target->setPos(100, 100);
  const QPointF startPos = target->pos();

  auto *overlay = new TransformOverlay(target);
  overlay->setZValue(99999);
  scene.addItem(overlay);

  const QPointF center = target->sceneBoundingRect().center();
  expect(overlay->handleAt(center) == TransformOverlay::Center,
         "transform: center handle detected");

  int started = 0;
  int ended = 0;
  QObject::connect(overlay, &TransformOverlay::interactionStarted,
                   [&]() { ++started; });
  QObject::connect(overlay, &TransformOverlay::interactionEnded,
                   [&]() { ++ended; });

  sendItemMouse(overlay, &scene, QEvent::GraphicsSceneMousePress, center);
  sendItemMouse(overlay, &scene, QEvent::GraphicsSceneMouseMove,
                center + QPointF(40, 25));
  sendItemMouse(overlay, &scene, QEvent::GraphicsSceneMouseRelease,
                center + QPointF(40, 25));

  expect(started == 1, "transform: interactionStarted");
  expect(ended == 1, "transform: interactionEnded");
  expect(target->pos() != startPos, "transform: target moved");
  expect(qAbs(target->pos().x() - (startPos.x() + 40)) < 1.0,
         "transform: dx≈40");
  expect(qAbs(target->pos().y() - (startPos.y() + 25)) < 1.0,
         "transform: dy≈25");
}

} // namespace

int main(int argc, char **argv) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QApplication app(argc, argv);

  testRectLassoSelectsInsideOnly();
  testFreehandLassoSelects();
  testPressOnSelectedReturnsFalse();
  testRulerTypeExcluded();
  testTransformOverlayCenterMove();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d lasso/transform check(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "lasso_tool_sequence: OK\n");
  return 0;
}
