#include <QtTest>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QEvent>

#include "tools/ToolManager.h"
#include "tools/WritingTools.h"
#include "tools/EraserTool.h"
#include "tools/LassoTool.h"
#include "tools/RulerTool.h"
#include "tools/ShapeTool.h"
#include "tools/StrokeItem.h"
#include "PageItem.h"

namespace {

QGraphicsSceneMouseEvent *mouseEv(QEvent::Type t, const QPointF &scenePos,
                                  Qt::MouseButtons buttons = Qt::LeftButton) {
  auto *ev = new QGraphicsSceneMouseEvent(t);
  ev->setScenePos(scenePos);
  ev->setPos(scenePos);
  ev->setButton(Qt::LeftButton);
  ev->setButtons(buttons);
  return ev;
}

int strokeItemCount(QGraphicsScene *scene) {
  int n = 0;
  for (QGraphicsItem *it : scene->items())
    if (it->type() == StrokeItem::Type)
      ++n;
  return n;
}

} // namespace

class ToolTests : public QObject {
  Q_OBJECT
private slots:

  // ── Pen: press/move/release produces a stroke item with points ──
  void penDrawsStroke() {
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 800, 1100);
    PenTool tool;
    QVERIFY(mouseEv(QEvent::GraphicsSceneMousePress, QPointF(100, 100))
                    ? tool.handleMousePress(
                          mouseEv(QEvent::GraphicsSceneMousePress,
                                  QPointF(100, 100)),
                          &scene)
                    : false);
    for (int x = 110; x <= 300; x += 10)
      tool.handleMouseMove(mouseEv(QEvent::GraphicsSceneMouseMove,
                                   QPointF(x, 100 + x / 10)),
                             &scene);
    QVERIFY(tool.handleMouseRelease(
        mouseEv(QEvent::GraphicsSceneMouseRelease, QPointF(300, 130),
                Qt::NoButton),
        &scene));
    QCOMPARE(strokeItemCount(&scene), 1);
    auto *item = qgraphicsitem_cast<StrokeItem *>(
        tool.lastCompletedItem());
    QVERIFY(item);
    QVERIFY(item->points().size() > 10);
    QVERIFY(item->path().elementCount() > 1);
  }

  // ── Highlighter live item sits above the page (z > 0) ──
  void highlighterPreviewVisible() {
    QGraphicsScene scene;
    HighlighterTool tool;
    tool.handleMousePress(mouseEv(QEvent::GraphicsSceneMousePress,
                                  QPointF(50, 50)),
                          &scene);
    const auto items = scene.items();
    QCOMPARE(items.size(), 1);
    QVERIFY(items.first()->zValue() > 0.0);
    tool.handleMouseRelease(
        mouseEv(QEvent::GraphicsSceneMouseRelease, QPointF(60, 50),
                Qt::NoButton),
        &scene);
  }

  // ── Ruler appears via ensureRulerExists ──
  void rulerSpawns() {
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 800, 1100);
    RulerItem *r = RulerTool::ensureRulerExists(&scene, ToolConfig{});
    QVERIFY(r);
    QVERIFY(r->isVisible());
    QCOMPARE(findActiveRuler(&scene), r);

    // Compass mode applies live through setConfig.
    ToolConfig c;
    c.compassMode = true;
    r->setConfig(c);
    QCOMPARE(r->config().compassMode, true);
  }

  // ── Ruler rotate via wheel delta ──
  void rulerRotates() {
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 800, 1100);
    RulerItem *r = RulerTool::ensureRulerExists(&scene, ToolConfig{});
    ToolConfig c;
    c.rulerSnap = false;
    r->setConfig(c);
    const qreal before = r->rotation();
    r->applyWheelDelta(120, Qt::NoModifier);
    QVERIFY(!qFuzzyCompare(r->rotation() + 1.0, before + 1.0));
  }

  // ── Lasso: marquee removed after release, no leftovers ──
  void lassoCleansUp() {
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 800, 1100);
    LassoTool tool;
    tool.handleMousePress(mouseEv(QEvent::GraphicsSceneMousePress,
                                  QPointF(10, 10)),
                          &scene);
    tool.handleMouseMove(mouseEv(QEvent::GraphicsSceneMouseMove,
                                 QPointF(200, 200)),
                         &scene);
    tool.handleMouseRelease(
        mouseEv(QEvent::GraphicsSceneMouseRelease, QPointF(200, 200),
                Qt::NoButton),
        &scene);
    // No dashed marquee path left behind.
    QCOMPARE(scene.items().size(), 0);
  }

  // ── Eraser (pixel) subtracts from a pen stroke ──
  void eraserCuts() {
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 800, 1100);
    PenTool pen;
    pen.handleMousePress(mouseEv(QEvent::GraphicsSceneMousePress,
                                 QPointF(100, 100)),
                         &scene);
    for (int x = 110; x <= 400; x += 10)
      pen.handleMouseMove(mouseEv(QEvent::GraphicsSceneMouseMove,
                                  QPointF(x, 100)),
                          &scene);
    pen.handleMouseRelease(
        mouseEv(QEvent::GraphicsSceneMouseRelease, QPointF(400, 100),
                Qt::NoButton),
        &scene);
    QCOMPARE(strokeItemCount(&scene), 1);

    EraserTool eraser;
    ToolConfig ec;
    ec.eraserMode = EraserMode::Pixel;
    ec.penWidth = 60;
    eraser.setConfig(ec);
    eraser.handleMousePress(mouseEv(QEvent::GraphicsSceneMousePress,
                                    QPointF(250, 100)),
                            &scene);
    // Stroke was converted to a filled outline with a hole → still one
    // item, but its path is no longer a simple open stroke.
    const auto items = scene.items();
    QCOMPARE(items.size(), 1);
    auto *pi = dynamic_cast<QGraphicsPathItem *>(items.first());
    QVERIFY(pi);
    QVERIFY(pi->pen().style() == Qt::NoPen);
    QVERIFY(pi->path().elementCount() > 4);
  }

  // ── Ink-to-shape: drawn rectangle + hold → fitted shape_item ──
  void inkToShapeRect() {
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 800, 1100);
    PenTool tool;
    tool.handleMousePress(mouseEv(QEvent::GraphicsSceneMousePress,
                                  QPointF(100, 100)),
                          &scene);
    // Draw a rough rectangle.
    const QPointF corners[] = {{100, 100}, {300, 98}, {302, 250},
                               {98, 252}, {100, 100}};
    for (int c = 0; c < 4; ++c) {
      const QLineF seg(corners[c], corners[c + 1]);
      for (int i = 1; i <= 20; ++i)
        tool.handleMouseMove(mouseEv(QEvent::GraphicsSceneMouseMove,
                                     seg.pointAt(i / 20.0)),
                             &scene);
    }
    QTest::qWait(400); // hold still → fit fires on release
    tool.handleMouseRelease(
        mouseEv(QEvent::GraphicsSceneMouseRelease, corners[4], Qt::NoButton),
        &scene);
    auto *item = tool.lastCompletedItem();
    QVERIFY(item);
    QCOMPARE(item->data(0).toString(), QStringLiteral("shape_item"));
  }

  // ── Shape tool emits a selectable shape_item ──
  void shapeToolTags() {
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, 800, 1100);
    ShapeTool tool;
    tool.handleMousePress(mouseEv(QEvent::GraphicsSceneMousePress,
                                  QPointF(50, 50)),
                          &scene);
    tool.handleMouseMove(mouseEv(QEvent::GraphicsSceneMouseMove,
                                 QPointF(250, 200)),
                         &scene);
    tool.handleMouseRelease(
        mouseEv(QEvent::GraphicsSceneMouseRelease, QPointF(250, 200),
                Qt::NoButton),
        &scene);
    auto *item = tool.lastCompletedItem();
    QVERIFY(item);
    QCOMPARE(item->data(0).toString(), QStringLiteral("shape_item"));
  }

  // ── Grid page renders visibly: line pixels darker than paper ──
  void gridPageVisible() {
    QGraphicsScene scene;
    auto *page = new PageItem(0, 0, 800, 1120);
    page->setType(PageBackgroundType::Grid);
    scene.addItem(page);
    QImage img(800, 1120, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    scene.render(&p, QRectF(0, 0, 800, 1120), QRectF(0, 0, 800, 1120));
    p.end();

    // Debug: darkest pixel across the row.
    int minL = 255;
    int minX = -1;
    for (int x = 0; x < 800; ++x) {
      const int l = img.pixelColor(x, 400).lightness();
      if (l < minL) { minL = l; minX = x; }
    }
    qWarning() << "row400 min lightness" << minL << "at x" << minX;

    // First interior grid line sits at margin(24)+spacing(40) = 64.
    const QColor onLine = img.pixelColor(64, 400);
    const QColor paper = img.pixelColor(50, 400);
    QVERIFY2(onLine.lightness() < paper.lightness() - 15,
             qPrintable(QStringLiteral("line=%1 paper=%2")
                            .arg(onLine.name(), paper.name())));
    // Major line (5th minor, x=224) should be even stronger.
    const QColor major = img.pixelColor(224, 400);
    QVERIFY(major.lightness() <= onLine.lightness() + 4);
  }
};

QTEST_MAIN(ToolTests)
#include "tst_tools.moc"
