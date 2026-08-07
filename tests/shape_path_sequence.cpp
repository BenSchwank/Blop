// Headless ShapePath geometry checks (Phase 1 tool sequence).
// Never linked into the Blop GUI target.

#include "ShapePath.h"

#include <QCoreApplication>
#include <QPainterPath>
#include <QRectF>

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

void testTinyDragEmpty() {
  ToolConfig cfg;
  cfg.shapeToolKind = ShapeToolKind::Rectangle;
  const QPainterPath p = blopBuildShapePath(QRectF(10, 10, 0.2, 0.2), cfg);
  expect(p.isEmpty(), "tiny drag → empty path");
}

void testRectangle() {
  ToolConfig cfg;
  cfg.shapeToolKind = ShapeToolKind::Rectangle;
  const QRectF r(10, 20, 80, 40);
  const QPainterPath p = blopBuildShapePath(r, cfg);
  expect(!p.isEmpty(), "rect: non-empty");
  expect(p.boundingRect().contains(r.normalized().center()),
         "rect: contains center");
}

void testCircleInscribed() {
  ToolConfig cfg;
  cfg.shapeToolKind = ShapeToolKind::Circle;
  const QRectF r(0, 0, 100, 60);
  const QPainterPath p = blopBuildShapePath(r, cfg);
  expect(!p.isEmpty(), "circle: non-empty");
  const QRectF b = p.boundingRect();
  expect(qFuzzyCompare(b.width(), 60.0) && qFuzzyCompare(b.height(), 60.0),
         "circle: diameter = min side");
}

void testLineAndArrow() {
  ToolConfig cfg;
  cfg.shapeToolKind = ShapeToolKind::Line;
  const QRectF r(0, 0, 100, 50);
  const QPainterPath line = blopBuildShapePath(r, cfg);
  expect(line.elementCount() >= 2, "line: has endpoints");

  cfg.shapeToolKind = ShapeToolKind::Arrow;
  const QPainterPath arrow = blopBuildShapePath(r, cfg);
  expect(arrow.elementCount() > line.elementCount(),
         "arrow: stem + head elements");

  // Up/left drag must keep direction (not normalize to SE).
  const QRectF upLeft(QPointF(80, 60), QPointF(20, 10));
  const QPainterPath arrowUp = blopBuildShapePath(upLeft, cfg);
  expect(!arrowUp.isEmpty(), "arrow up-left: non-empty");
  expect(arrowUp.elementCount() >= 2, "arrow up-left: has stem");
  if (arrowUp.elementCount() >= 2) {
    const QPainterPath::Element e0 = arrowUp.elementAt(0);
    const QPainterPath::Element e1 = arrowUp.elementAt(1);
    auto near = [](qreal a, qreal b) { return qAbs(a - b) < 0.01; };
    expect(near(e0.x, 80.0) && near(e0.y, 60.0),
           "arrow up-left: starts at drag origin");
    expect(near(e1.x, 20.0) && near(e1.y, 10.0),
           "arrow up-left: ends at drag tip");
  }
}

void testAxesTicks() {
  ToolConfig cfg;
  cfg.shapeToolKind = ShapeToolKind::Axes2D;
  cfg.shapeAxisTicks = 4;
  const QPainterPath p = blopBuildShapePath(QRectF(0, 0, 200, 100), cfg);
  expect(!p.isEmpty(), "axes: non-empty");
  expect(p.elementCount() > 4, "axes: cross + ticks");
}

void testSineAndGraphFrame() {
  ToolConfig cfg;
  cfg.shapeToolKind = ShapeToolKind::SineGraph;
  cfg.shapeSineFixedParams = false;
  const QPainterPath sine = blopBuildShapePath(QRectF(0, 0, 120, 60), cfg);
  expect(sine.elementCount() > 10, "sine: sampled curve");

  cfg.shapeToolKind = ShapeToolKind::CoordinateGraph;
  cfg.graphXMin = -5;
  cfg.graphXMax = 5;
  cfg.graphYMin = -5;
  cfg.graphYMax = 5;
  const QPainterPath frame = blopBuildShapePath(QRectF(0, 0, 100, 100), cfg);
  expect(!frame.isEmpty(), "graph frame: non-empty");
}

} // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);

  testTinyDragEmpty();
  testRectangle();
  testCircleInscribed();
  testLineAndArrow();
  testAxesTicks();
  testSineAndGraphFrame();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d shape_path check(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "shape_path_sequence: OK\n");
  return 0;
}
