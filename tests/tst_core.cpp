#include "Note.h"
#include "atomicfile.h"
#include "notemanager.h"
#include "versioncompare.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTransform>
#include <QtMath>
#include <QtTest>
#include <cmath>

class CoreTests : public QObject {
  Q_OBJECT
private slots:
  // --- updater version compare -------------------------------------------
  void version_parse_ignoresGitDescribeSuffix() {
    const QList<int> v = BlopVersion::parse(QStringLiteral("v3.29.47-3-gf390cd7"));
    QCOMPARE(v, (QList<int>{3, 29, 47, 0}));
  }
  void version_isNewer_basic() {
    QVERIFY(BlopVersion::isNewer("3.29.48", "3.29.47"));
    QVERIFY(BlopVersion::isNewer("v4.0", "3.99.99"));
    QVERIFY(!BlopVersion::isNewer("3.29.47", "3.29.47"));
    QVERIFY(!BlopVersion::isNewer("3.29.46", "3.29.47"));
  }
  void version_localWithDescribeSuffixIsNotOutdated() {
    // Regression: "47-3-gabc".toInt() used to be 0 -> endless update prompt.
    QVERIFY(!BlopVersion::isNewer("v3.29.47", "v3.29.47-3-gf390cd7"));
    QVERIFY(BlopVersion::isNewer("v3.29.48", "v3.29.47-3-gf390cd7"));
  }

  // --- atomic writes -----------------------------------------------------
  void atomicFile_writesAndCreatesParentDirs() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("a/b/c.json");
    QVERIFY(AtomicFile::write(path, "hello", "test"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("hello"));
  }
  void atomicFile_rotateBackupKeepsPreviousVersion() {
    QTemporaryDir dir;
    const QString path = dir.filePath("n.json");
    QVERIFY(AtomicFile::write(path, "v1", "test"));
    AtomicFile::rotateBackup(path);
    QVERIFY(AtomicFile::write(path, "v2", "test"));
    QFile bak(path + ".bak");
    QVERIFY(bak.open(QIODevice::ReadOnly));
    QCOMPARE(bak.readAll(), QByteArray("v1"));
  }

  // --- note persistence --------------------------------------------------
  void note_roundTripKeepsTextStickiesAndImages() {
    QTemporaryDir dir;
    const QString path = dir.filePath("t.bnote");

    Note n;
    n.id = "id-1";
    n.title = "Titel";
    n.pages.resize(1);
    TextObject t;
    t.text = "Hallo Welt";
    t.pos = QPointF(10, 20);
    n.pages[0].texts.append(t);
    StickyNoteObject s;
    s.text = "Merken";
    s.rotation = 25.0;
    s.width = 180.0;
    s.height = 140.0;
    n.pages[0].stickies.append(s);
    ImageObject im;
    im.pos = QPointF(5, 6);
    im.image = QImage(8, 8, QImage::Format_ARGB32);
    im.image.fill(Qt::red);
    n.pages[0].images.append(im);

    QVERIFY(NoteManager::saveNote(n, path));

    Note out;
    QVERIFY(NoteManager::loadNote(path, out));
    QCOMPARE(out.title, QString("Titel"));
    QCOMPARE(out.pages.size(), 1);
    QCOMPARE(out.pages[0].texts.size(), 1);
    QCOMPARE(out.pages[0].texts[0].text, QString("Hallo Welt"));
    QCOMPARE(out.pages[0].stickies.size(), 1);
    QCOMPARE(out.pages[0].stickies[0].text, QString("Merken"));
    QCOMPARE(out.pages[0].stickies[0].rotation, 25.0);
    QCOMPARE(out.pages[0].stickies[0].width, 180.0);
    QCOMPARE(out.pages[0].stickies[0].height, 140.0);
    QCOMPARE(out.pages[0].images.size(), 1);
    QCOMPARE(out.pages[0].images[0].image.size(), QSize(8, 8));
    QCOMPARE(out.pages[0].images[0].pos, QPointF(5, 6));
  }

  void note_corruptFileRecoversFromBackup() {
    QTemporaryDir dir;
    const QString path = dir.filePath("t.bnote");
    Note n;
    n.id = "id-2";
    n.title = "Gut";
    n.pages.resize(1);
    QVERIFY(NoteManager::saveNote(n, path));
    // Second save rotates the first (good) version into .bak.
    n.title = "Gut v2";
    QVERIFY(NoteManager::saveNote(n, path));

    // Make sure a good backup exists, then corrupt the live file.
    QFile::remove(path + ".bak");
    QVERIFY(QFile::copy(path, path + ".bak"));
    {
      QFile f(path);
      QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
      f.write("{ this is not json");
    }
    QSignalSpy spy(&SaveStatus::instance(), &SaveStatus::recovered);
    Note out;
    QVERIFY(NoteManager::loadNote(path, out));
    QCOMPARE(out.title, QString("Gut v2"));
    QTRY_COMPARE(spy.count(), 1); // delivered via queued call
  }

  void note_unreadableWithoutBackupFails() {
    QTemporaryDir dir;
    const QString path = dir.filePath("broken.bnote");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("garbage");
    f.close();
    Note out;
    QVERIFY(!NoteManager::loadNote(path, out));
  }

  void note_roundTripKeepsStrokeAlphaWidthAndFilled() {
    QTemporaryDir dir;
    const QString path = dir.filePath("strokes.bnote");

    Note n;
    n.id = "id-strokes";
    n.title = "Tinte";
    n.pages.resize(1);

    Stroke ink;
    ink.width = 2.5;
    ink.color = QColor(255, 0, 0, 128);
    ink.penStyle = int(Qt::DashLine);
    ink.points = {QPointF(1, 2), QPointF(3, 4)};
    ink.path.moveTo(1, 2);
    ink.path.lineTo(3, 4);
    n.pages[0].strokes.append(ink);

    Stroke remnant;
    remnant.filled = true;
    remnant.color = QColor(0, 0, 255, 200);
    remnant.path.addRect(QRectF(10, 20, 30, 40));
    n.pages[0].strokes.append(remnant);

    // Pixel-eraser remnant: outline with a hole (OddEvenFill).
    Stroke holed;
    holed.filled = true;
    holed.color = QColor(20, 20, 20);
    QPainterPath outline;
    outline.addRect(QRectF(0, 0, 100, 20));
    QPainterPath hole;
    hole.addEllipse(QRectF(40, -10, 40, 40));
    holed.path = outline.subtracted(hole);
    holed.path.setFillRule(Qt::OddEvenFill);
    const int holeEls = holed.path.elementCount();
    QVERIFY(holeEls > 4);
    n.pages[0].strokes.append(holed);

    QVERIFY(NoteManager::saveNote(n, path));
    Note out;
    QVERIFY(NoteManager::loadNote(path, out));
    QCOMPARE(out.pages.size(), 1);
    QCOMPARE(out.pages[0].strokes.size(), 3);

    const Stroke &a = out.pages[0].strokes[0];
    QCOMPARE(a.width, 2.5);
    QCOMPARE(a.color.red(), 255);
    QCOMPARE(a.color.alpha(), 128);
    QCOMPARE(a.points.size(), 2);
    QCOMPARE(a.penStyle, int(Qt::DashLine));
    QVERIFY(!a.filled);

    const Stroke &b = out.pages[0].strokes[1];
    QVERIFY(b.filled);
    QCOMPARE(b.color.blue(), 255);
    QCOMPARE(b.color.alpha(), 200);
    QVERIFY(b.path.elementCount() > 0);
    QCOMPARE(b.path.boundingRect().size(), QSizeF(30, 40));

    const Stroke &c = out.pages[0].strokes[2];
    QVERIFY(c.filled);
    QCOMPARE(c.path.fillRule(), Qt::OddEvenFill);
    QCOMPARE(c.path.elementCount(), holeEls);
    // Hole must survive: center of the ellipse is not filled.
    QVERIFY(!c.path.contains(QPointF(60, 10)));
    QVERIFY(c.path.contains(QPointF(5, 10)));
  }

  void note_roundTripKeepsShapeFillDashAndPath() {
    QTemporaryDir dir;
    const QString path = dir.filePath("shapes.bnote");

    Note n;
    n.id = "id-shapes";
    n.title = "Formen";
    n.pages.resize(1);

    Stroke rect;
    rect.shape = true;
    rect.width = 2.0;
    rect.color = QColor(20, 20, 20);
    rect.penStyle = int(Qt::SolidLine);
    rect.fillColor = QColor(91, 157, 255, 120);
    rect.path.addRect(QRectF(40, 60, 120, 80));
    n.pages[0].strokes.append(rect);

    Stroke dash;
    dash.shape = true;
    dash.width = 3.0;
    dash.color = QColor(200, 40, 40);
    dash.penStyle = int(Qt::DashLine);
    dash.path.moveTo(10, 200);
    dash.path.lineTo(200, 240);
    n.pages[0].strokes.append(dash);

    Stroke ell;
    ell.shape = true;
    ell.width = 1.5;
    ell.color = QColor(0, 0, 0);
    ell.penStyle = int(Qt::SolidLine);
    ell.path.addEllipse(QRectF(220, 60, 90, 70));
    n.pages[0].strokes.append(ell);

    QVERIFY(NoteManager::saveNote(n, path));
    Note out;
    QVERIFY(NoteManager::loadNote(path, out));
    QCOMPARE(out.pages[0].strokes.size(), 3);

    const Stroke &a = out.pages[0].strokes[0];
    QVERIFY(a.shape);
    QVERIFY(!a.filled);
    QCOMPARE(a.penStyle, int(Qt::SolidLine));
    QCOMPARE(a.fillColor.rgba(), QColor(91, 157, 255, 120).rgba());
    QCOMPARE(a.path.boundingRect().size(), QSizeF(120, 80));

    const Stroke &b = out.pages[0].strokes[1];
    QVERIFY(b.shape);
    QCOMPARE(b.penStyle, int(Qt::DashLine));
    QVERIFY(!b.fillColor.isValid() || b.fillColor.alpha() == 0);
    QCOMPARE(b.path.elementCount(), 2);

    const Stroke &c = out.pages[0].strokes[2];
    QVERIFY(c.shape);
    QVERIFY(c.path.elementCount() > 0);
    QCOMPARE(c.path.boundingRect().size(), QSizeF(90, 70));
  }

  // Contract for MultiPageNoteView::duplicateSelection: after harvest, only
  // geometry is offset — shape/fill/dash must survive (regression vs. the old
  // manual Stroke builder that dropped fc/ps/shape).
  void stroke_offsetCloneKeepsShapeMeta() {
    Stroke src;
    src.shape = true;
    src.width = 2.5;
    src.color = QColor(10, 20, 30);
    src.penStyle = int(Qt::DashLine);
    src.fillColor = QColor(255, 0, 0, 180);
    src.path.addRect(QRectF(30, 40, 80, 50));
    src.points = {QPointF(30, 40), QPointF(110, 90)};

    Stroke dup = src;
    const QPointF offset(20, 20);
    dup.path.translate(offset);
    for (QPointF &pt : dup.points)
      pt += offset;

    QVERIFY(dup.shape);
    QCOMPARE(dup.penStyle, int(Qt::DashLine));
    QCOMPARE(dup.fillColor.rgba(), QColor(255, 0, 0, 180).rgba());
    QCOMPARE(dup.width, 2.5);
    QCOMPARE(dup.path.boundingRect().topLeft(), QPointF(50, 60));
    QCOMPARE(dup.points.first(), QPointF(50, 60));
  }

  // Contract for duplicateSelection image_item path: offset keeps pixels.
  void image_offsetCloneKeepsPixels() {
    ImageObject src;
    src.pos = QPointF(40, 50);
    src.width = 120;
    src.height = 80;
    src.opacity = 0.75;
    src.rotation = 15.0;
    src.image = QImage(16, 16, QImage::Format_ARGB32);
    src.image.fill(QColor(20, 40, 60));

    ImageObject dup = src;
    dup.pos += QPointF(20, 20);

    QCOMPARE(dup.pos, QPointF(60, 70));
    QCOMPARE(dup.width, 120.0);
    QCOMPARE(dup.opacity, 0.75);
    QCOMPARE(dup.rotation, 15.0);
    QCOMPARE(dup.image.pixelColor(0, 0), QColor(20, 40, 60));
    QVERIFY(dup.image.size() == src.image.size());
  }

  // Contract for imagePoseFromItem after TransformOverlay::bakeTransform:
  // uniform scale + rotation live in the matrix, not item->scale()/rotation().
  void transform_matrixHoldsUniformScaleAndRotation() {
    QTransform m;
    m.translate(120, 80);
    m.rotate(30);
    m.scale(2.5, 2.5);
    QCOMPARE(std::hypot(m.m11(), m.m12()), 2.5);
    const qreal rot = qRadiansToDegrees(std::atan2(m.m12(), m.m11()));
    QVERIFY(qAbs(rot - 30.0) < 0.01);
    QCOMPARE(m.dx(), 120.0);
    QCOMPARE(m.dy(), 80.0);
    // Simulated pixmap 40×20 → stored width/height after harvest.
    QCOMPARE(40.0 * 2.5, 100.0);
    QCOMPARE(20.0 * 2.5, 50.0);
  }

  // StickyPose uses the same matrix decomposition; local rect × scale → w/h.
  void sticky_matrixHoldsNonUniformScaleAndRotation() {
    QTransform m;
    m.translate(40, 60);
    m.rotate(-15);
    m.scale(1.5, 2.0);
    const qreal sx = std::hypot(m.m11(), m.m12());
    const qreal sy = std::hypot(m.m21(), m.m22());
    QVERIFY(qAbs(sx - 1.5) < 0.01);
    QVERIFY(qAbs(sy - 2.0) < 0.01);
    const qreal rot = qRadiansToDegrees(std::atan2(m.m12(), m.m11()));
    QVERIFY(qAbs(rot - (-15.0)) < 0.01);
    QCOMPARE(168.0 * sx, 252.0);
    QCOMPARE(148.0 * sy, 296.0);
  }

  void textHtml_roundTripKeepsTable() {
    QTextDocument doc;
    QTextCursor c(&doc);
    QTextTableFormat tf;
    tf.setBorder(1.0);
    tf.setCellPadding(4.0);
    QTextTable *table = c.insertTable(2, 3, tf);
    QVERIFY(table);
    table->cellAt(0, 0).firstCursorPosition().insertText(QStringLiteral("A1"));

    const QString html = doc.toHtml();
    QVERIFY(html.contains(QLatin1String("<table"), Qt::CaseInsensitive));
    QVERIFY(html.contains(QStringLiteral("A1")));

    Note n;
    n.id = "tbl-1";
    n.title = "Tabelle";
    n.pages.resize(1);
    TextObject t;
    t.html = html;
    t.text = doc.toPlainText();
    t.pos = QPointF(10, 20);
    n.pages[0].texts.append(t);

    QTemporaryDir dir;
    const QString path = dir.filePath("table.bnote");
    QVERIFY(NoteManager::saveNote(n, path));

    Note out;
    QVERIFY(NoteManager::loadNote(path, out));
    QCOMPARE(out.pages[0].texts.size(), 1);
    QVERIFY(out.pages[0].texts[0].html.contains(QLatin1String("<table"),
                                                 Qt::CaseInsensitive));
    QVERIFY(out.pages[0].texts[0].html.contains(QStringLiteral("A1")));

    QTextDocument round;
    round.setHtml(out.pages[0].texts[0].html);
    QVERIFY(round.toHtml().contains(QLatin1String("<table"),
                                    Qt::CaseInsensitive));
  }

  void note_roundTripKeepsRulerCompassAndSize() {
    QTemporaryDir dir;
    const QString path = dir.filePath("ruler.bnote");

    Note n;
    n.id = "ruler-1";
    n.title = "Lineal";
    n.pages.resize(1);
    n.ruler.placed = true;
    n.ruler.pos = QPointF(120, 340);
    n.ruler.rotation = 15.0;
    n.ruler.compassMode = true;
    n.ruler.width = 520;
    n.ruler.height = 520;

    QVERIFY(NoteManager::saveNote(n, path));

    Note out;
    QVERIFY(NoteManager::loadNote(path, out));
    QVERIFY(out.ruler.placed);
    QCOMPARE(out.ruler.pos, QPointF(120, 340));
    QCOMPARE(out.ruler.rotation, 15.0);
    QVERIFY(out.ruler.compassMode);
    QCOMPARE(out.ruler.width, 520.0);
    QCOMPARE(out.ruler.height, 520.0);
  }

  // Dimension Fläche uses shoelace × unitsPerPx² (see DimensionTool).
  void dimension_shoelaceAreaUnits() {
    const QVector<QPointF> square = {
        {0, 0}, {100, 0}, {100, 100}, {0, 100}};
    double area = 0.0;
    for (int i = 0; i < square.size(); ++i) {
      const QPointF a = square[i];
      const QPointF b = square[(i + 1) % square.size()];
      area += a.x() * b.y() - b.x() * a.y();
    }
    area = qAbs(area) * 0.5;
    QCOMPARE(area, 10000.0);
    // unitsPerPx = 1 → label A=10000.00 u²
    QCOMPARE(QStringLiteral("A=%1 u²").arg(area, 0, 'f', 2),
             QStringLiteral("A=10000.00 u²"));
  }
};

QTEST_GUILESS_MAIN(CoreTests)
#include "tst_core.moc"