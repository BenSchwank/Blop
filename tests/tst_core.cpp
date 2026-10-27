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
#include <QtTest>

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
    ink.points = {QPointF(1, 2), QPointF(3, 4)};
    ink.path.moveTo(1, 2);
    ink.path.lineTo(3, 4);
    n.pages[0].strokes.append(ink);

    Stroke remnant;
    remnant.filled = true;
    remnant.color = QColor(0, 0, 255, 200);
    remnant.path.addRect(QRectF(10, 20, 30, 40));
    n.pages[0].strokes.append(remnant);

    QVERIFY(NoteManager::saveNote(n, path));
    Note out;
    QVERIFY(NoteManager::loadNote(path, out));
    QCOMPARE(out.pages.size(), 1);
    QCOMPARE(out.pages[0].strokes.size(), 2);

    const Stroke &a = out.pages[0].strokes[0];
    QCOMPARE(a.width, 2.5);
    QCOMPARE(a.color.red(), 255);
    QCOMPARE(a.color.alpha(), 128);
    QCOMPARE(a.points.size(), 2);
    QVERIFY(!a.filled);

    const Stroke &b = out.pages[0].strokes[1];
    QVERIFY(b.filled);
    QCOMPARE(b.color.blue(), 255);
    QCOMPARE(b.color.alpha(), 200);
    QVERIFY(b.path.elementCount() > 0);
    QCOMPARE(b.path.boundingRect().size(), QSizeF(30, 40));
  }
};

QTEST_GUILESS_MAIN(CoreTests)
#include "tst_core.moc"