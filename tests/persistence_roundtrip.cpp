// Headless A4 JSON persistence round-trip (Phase 0.1 of docs/release-roadmap.md).
// Never linked into the Blop GUI target.

#include "Note.h"
#include "notemanager.h"

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QPainterPath>
#include <QTemporaryDir>
#include <QtGlobal>

#include <cmath>
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

bool nearlyEqual(qreal a, qreal b, qreal eps = 1e-4) {
  return std::fabs(a - b) <= eps;
}

bool colorsEqual(const QColor &a, const QColor &b) {
  return a.red() == b.red() && a.green() == b.green() && a.blue() == b.blue() &&
         a.alpha() == b.alpha();
}

Stroke makeStroke() {
  Stroke s;
  s.width = 3.5;
  s.color = QColor(20, 40, 200, 255);
  s.isHighlighter = false;
  s.isEraser = false;
  s.pageIndex = 0;
  s.points = {QPointF(10, 10), QPointF(20, 30), QPointF(40, 25)};
  s.pressures = {0.4, 0.8, 0.6};
  s.path = QPainterPath(s.points.first());
  for (int i = 1; i < s.points.size(); ++i)
    s.path.lineTo(s.points.at(i));
  return s;
}

Note makeRichNote() {
  Note n;
  n.id = QStringLiteral("roundtrip-test");
  n.title = QStringLiteral("Persistenz-Test");
  n.tags = {QStringLiteral("qa"), QStringLiteral("phase0")};
  n.ensurePage(0);
  n.ensurePage(1);

  NotePage &p0 = n.pages[0];
  p0.title = QStringLiteral("Seite Eins");
  p0.backgroundType = 2;
  p0.paperColor = QColor(250, 248, 240);
  p0.rotationDegrees = 90;
  p0.bookmarked = true;
  p0.strokes.push_back(makeStroke());

  GraphObject g;
  g.rect = QRectF(12, 34, 280, 180);
  g.xMin = -5;
  g.xMax = 5;
  g.yMin = -2;
  g.yMax = 2;
  g.selectedFunction = 0;
  GraphFunction fn;
  fn.expression = QStringLiteral("x^2");
  fn.color = QColor(94, 92, 230);
  fn.visible = true;
  g.functions.push_back(fn);
  p0.graphs.push_back(g);

  StickyNoteObject sn;
  sn.pos = QPointF(50, 60);
  sn.width = 168;
  sn.height = 148;
  sn.text = QStringLiteral("Sticky Inhalt");
  sn.color = QColor(255, 236, 120);
  sn.fontPointSize = 14;
  p0.stickies.push_back(sn);

  ShapeObject sh;
  sh.pos = QPointF(100, 120);
  sh.path.addRect(QRectF(0, 0, 80, 40));
  sh.penWidth = 2.5;
  sh.penColor = QColor(30, 30, 30);
  sh.fillColor = QColor(0, 0, 0, 0); // outline-only
  sh.kind = 1;
  p0.shapes.push_back(sh);

  TextObject tx;
  tx.pos = QPointF(15, 200);
  tx.text = QStringLiteral("Hallo Text");
  tx.fontFamily = QStringLiteral("Sans Serif");
  tx.fontPointSize = 18;
  tx.color = QColor(10, 10, 10);
  tx.align = 1;
  p0.texts.push_back(tx);

  ImageObject im;
  im.pos = QPointF(200, 200);
  im.opacity = 0.85;
  im.scale = 1.25;
  QImage img(8, 8, QImage::Format_ARGB32);
  img.fill(QColor(255, 0, 0, 200));
  QByteArray png;
  {
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
  }
  im.png = png;
  p0.images.push_back(im);

  NotePage &p1 = n.pages[1];
  p1.title = QStringLiteral("Seite Zwei");
  p1.backgroundType = 0;
  Stroke hl = makeStroke();
  hl.isHighlighter = true;
  hl.color = QColor(255, 255, 0, 100);
  hl.width = 12;
  p1.strokes.push_back(hl);

  return n;
}

void checkRoundTrip(const Note &orig, const Note &loaded) {
  expect(loaded.id == orig.id, "note id");
  expect(loaded.title == orig.title, "note title");
  expect(loaded.tags == orig.tags, "note tags");
  expect(loaded.pages.size() == orig.pages.size(), "page count");
  if (loaded.pages.size() != orig.pages.size())
    return;

  for (int i = 0; i < orig.pages.size(); ++i) {
    const NotePage &a = orig.pages[i];
    const NotePage &b = loaded.pages[i];
    expect(b.title == a.title, "page title");
    expect(b.backgroundType == a.backgroundType, "page backgroundType");
    expect(b.rotationDegrees == a.rotationDegrees, "page rotation");
    expect(b.bookmarked == a.bookmarked, "page bookmarked");
    expect(colorsEqual(b.paperColor, a.paperColor) || i == 1,
           "page paperColor"); // page1 may use default white

    expect(b.strokes.size() == a.strokes.size(), "stroke count");
    if (!a.strokes.isEmpty() && b.strokes.size() == a.strokes.size()) {
      const Stroke &sa = a.strokes[0];
      const Stroke &sb = b.strokes[0];
      expect(nearlyEqual(sb.width, sa.width), "stroke width");
      expect(sb.isHighlighter == sa.isHighlighter, "stroke highlighter");
      expect(sb.points.size() == sa.points.size(), "stroke points");
      expect(sb.pressures.size() == sa.pressures.size() || sa.pressures.isEmpty(),
             "stroke pressures");
      if (!sa.points.isEmpty() && sb.points.size() == sa.points.size()) {
        expect(nearlyEqual(sb.points.first().x(), sa.points.first().x()),
               "stroke first x");
        expect(nearlyEqual(sb.points.first().y(), sa.points.first().y()),
               "stroke first y");
      }
    }

    expect(b.graphs.size() == a.graphs.size(), "graph count");
    if (!a.graphs.isEmpty() && b.graphs.size() == a.graphs.size()) {
      expect(b.graphs[0].functions.size() == a.graphs[0].functions.size(),
             "graph fn count");
      if (!a.graphs[0].functions.isEmpty() &&
          b.graphs[0].functions.size() == a.graphs[0].functions.size()) {
        expect(b.graphs[0].functions[0].expression ==
                   a.graphs[0].functions[0].expression,
               "graph expression");
      }
    }

    expect(b.stickies.size() == a.stickies.size(), "sticky count");
    if (!a.stickies.isEmpty() && b.stickies.size() == a.stickies.size())
      expect(b.stickies[0].text == a.stickies[0].text, "sticky text");

    expect(b.shapes.size() == a.shapes.size(), "shape count");
    if (!a.shapes.isEmpty() && b.shapes.size() == a.shapes.size()) {
      expect(b.shapes[0].kind == a.shapes[0].kind, "shape kind");
      expect(b.shapes[0].fillColor.alpha() == 0, "shape outline-only fill");
    }

    expect(b.texts.size() == a.texts.size(), "text count");
    if (!a.texts.isEmpty() && b.texts.size() == a.texts.size()) {
      expect(b.texts[0].text == a.texts[0].text, "text content");
      expect(b.texts[0].align == a.texts[0].align, "text align");
    }

    expect(b.images.size() == a.images.size(), "image count");
    if (!a.images.isEmpty() && b.images.size() == a.images.size()) {
      expect(!b.images[0].png.isEmpty(), "image png non-empty");
      expect(nearlyEqual(b.images[0].opacity, a.images[0].opacity),
             "image opacity");
      expect(nearlyEqual(b.images[0].scale, a.images[0].scale), "image scale");
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  Q_UNUSED(app);

  QTemporaryDir tmp;
  if (!tmp.isValid()) {
    fail("temp dir");
    return 1;
  }

  const QString path = tmp.filePath(QStringLiteral("roundtrip.blop.json"));
  const Note original = makeRichNote();

  expect(NoteManager::saveNote(original, path), "saveNote");
  Note loaded;
  expect(NoteManager::loadNote(path, loaded), "loadNote");
  checkRoundTrip(original, loaded);

  if (g_failures == 0) {
    std::printf("OK: A4 JSON persistence round-trip passed\n");
    return 0;
  }
  std::fprintf(stderr, "%d assertion(s) failed\n", g_failures);
  return 1;
}
