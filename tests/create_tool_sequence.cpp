// Headless Text / Sticky / Image create sequences (Phase 1).
// Never linked into the Blop GUI target.

#include "ImagePlacement.h"
#include "StickyNoteTool.h"
#include "TextTool.h"

#include <QApplication>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QImage>
#include <QPixmap>

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

bool sendPress(AbstractTool *tool, QGraphicsScene *scene, const QPointF &pos) {
  QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMousePress);
  ev.setScenePos(pos);
  ev.setPos(pos);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::LeftButton);
  return tool->handleMousePress(&ev, scene);
}

void testTextCreatesItem() {
  QGraphicsScene scene;
  TextTool text;
  ToolConfig cfg;
  cfg.penWidth = 18;
  cfg.penColor = QColor(20, 20, 20);
  cfg.opacity = 1.0;
  cfg.fontFamily = QStringLiteral("Mono");
  cfg.textAlign = 1;
  text.setConfig(cfg);

  int modified = 0;
  QObject::connect(&text, &AbstractTool::contentModified, [&]() { ++modified; });

  expect(sendPress(&text, &scene, {50, 60}), "text: press creates");
  auto *item = dynamic_cast<QGraphicsTextItem *>(text.lastCompletedItem());
  expect(item != nullptr, "text: lastCompletedItem");
  expect(modified == 1, "text: contentModified");
  expect(scene.items().contains(item), "text: on scene");
  expect(item && item->data(0).toString() == QLatin1String("text"),
         "text: data tag");
  expect(item && (item->flags() & QGraphicsItem::ItemIsMovable),
         "text: movable");
  expect(item && (item->textInteractionFlags() & Qt::TextEditorInteraction),
         "text: editable");
  expect(item && item->pos() == QPointF(50, 60), "text: position");
}

void testEmptyTextRemovalSignal() {
  QGraphicsScene scene;
  TextTool text;
  ToolConfig cfg;
  cfg.penWidth = 14;
  text.setConfig(cfg);

  QGraphicsTextItem *removed = nullptr;
  QObject::connect(&text, &TextTool::emptyTextRemoved,
                   [&](QGraphicsTextItem *it) { removed = it; });

  sendPress(&text, &scene, {10, 10});
  auto *first = dynamic_cast<QGraphicsTextItem *>(text.lastCompletedItem());
  expect(first != nullptr, "empty-text: first created");

  // Second press outside clears empty first item.
  sendPress(&text, &scene, {200, 200});
  expect(removed == first, "empty-text: emptyTextRemoved fired");
  expect(!scene.items().contains(first), "empty-text: removed from scene");
  expect(text.lastCompletedItem() != nullptr, "empty-text: second item kept");
  delete removed;
}

void testStickyCreatesCardWithChildText() {
  QGraphicsScene scene;
  StickyNoteTool sticky;
  ToolConfig cfg;
  cfg.stickyBgColor = QColor(255, 220, 100);
  cfg.opacity = 1.0;
  cfg.penWidth = 14;
  sticky.setConfig(cfg);

  int modified = 0;
  QObject::connect(&sticky, &AbstractTool::contentModified,
                   [&]() { ++modified; });

  expect(sendPress(&sticky, &scene, {30, 40}), "sticky: press creates");
  auto *card = dynamic_cast<QGraphicsRectItem *>(sticky.lastCompletedItem());
  expect(card != nullptr, "sticky: card item");
  expect(modified == 1, "sticky: contentModified");
  expect(card && card->data(0).toString() == QLatin1String("sticky_note"),
         "sticky: data tag");
  expect(card && card->pos() == QPointF(30, 40), "sticky: position");

  QGraphicsTextItem *child = nullptr;
  for (QGraphicsItem *c : card->childItems()) {
    if ((child = dynamic_cast<QGraphicsTextItem *>(c)))
      break;
  }
  expect(child != nullptr, "sticky: child text");
  expect(child &&
             (child->textInteractionFlags() & Qt::TextEditorInteraction),
         "sticky: child editable");
}

void testImagePlacementHelper() {
  ToolConfig cfg;
  cfg.imageOpacity = 0.75;
  QImage img(64, 48, QImage::Format_ARGB32);
  img.fill(QColor(10, 120, 200));
  QPixmap pm = QPixmap::fromImage(img);

  auto *item = blopCreateImageItem(pm, QPointF(12, 18), cfg);
  expect(item != nullptr, "image: created");
  expect(item && item->data(0).toString() == QLatin1String("image"),
         "image: data tag");
  expect(item && item->pos() == QPointF(12, 18), "image: position");
  expect(item && qAbs(item->opacity() - 0.75) < 0.01, "image: opacity");
  expect(item && (item->flags() & QGraphicsItem::ItemIsMovable),
         "image: movable");

  QGraphicsScene scene;
  scene.addItem(item);
  expect(scene.items().contains(item), "image: on scene");
}

void testWideImageScaled() {
  ToolConfig cfg;
  cfg.opacity = 1.0;
  QImage img(1200, 40, QImage::Format_ARGB32);
  img.fill(Qt::green);
  auto *item = blopCreateImageItem(QPixmap::fromImage(img), {0, 0}, cfg);
  expect(item != nullptr, "wide-image: created");
  expect(item && item->pixmap().width() == 800, "wide-image: capped at 800");
  delete item;
}

} // namespace

int main(int argc, char **argv) {
  qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  QApplication app(argc, argv);

  testTextCreatesItem();
  testEmptyTextRemovalSignal();
  testStickyCreatesCardWithChildText();
  testImagePlacementHelper();
  testWideImageScaled();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d create-tool check(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "create_tool_sequence: OK\n");
  return 0;
}
