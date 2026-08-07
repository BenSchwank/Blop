/**
 * Headless InfiniteCanvasStore V5 round-trip (Phase 0.1).
 * Verifies stroke + sticky + shape + text + image survive save/load.
 */
#include "infinitecanvasstore.h"

#include "tools/StrokeItem.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QImage>
#include <QPainterPath>
#include <QPen>
#include <QTemporaryDir>
#include <QtGlobal>

#include <cstdio>

static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL: %s\n", msg);                                 \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  QTemporaryDir dir;
  CHECK(dir.isValid(), "temp dir");
  const QString path = dir.filePath(QStringLiteral("infinite_roundtrip.bin"));

  QGraphicsScene saveScene;
  {
    QPainterPath strokePath;
    strokePath.moveTo(10, 10);
    strokePath.lineTo(80, 40);
    strokePath.lineTo(40, 90);
    QPen strokePen(QColor(20, 90, 200), 3.0, Qt::SolidLine, Qt::RoundCap,
                   Qt::RoundJoin);
    QVector<StrokePoint> pts;
    pts.append({QPointF(10, 10), 1.0});
    pts.append({QPointF(80, 40), 0.8});
    pts.append({QPointF(40, 90), 0.6});
    auto *strokeItem = new StrokeItem(strokePath, strokePen, pts);
    strokeItem->setPos(5, 5);
    saveScene.addItem(strokeItem);

    auto *card = new QGraphicsRectItem(0, 0, 120, 90);
    card->setPos(200, 50);
    card->setBrush(QColor(255, 236, 150));
    card->setPen(QPen(QColor(210, 175, 55), 1.2));
    card->setData(0, QStringLiteral("sticky_note"));
    auto *stickyText = new QGraphicsTextItem(card);
    stickyText->setPos(10, 10);
    stickyText->setTextWidth(100);
    QFont sf = stickyText->font();
    sf.setPointSize(14);
    stickyText->setFont(sf);
    stickyText->setPlainText(QStringLiteral("Hello sticky"));
    saveScene.addItem(card);

    QPainterPath shapePath;
    shapePath.addEllipse(QRectF(0, 0, 60, 40));
    auto *shape = new QGraphicsPathItem(shapePath);
    shape->setPos(50, 150);
    shape->setPen(QPen(QColor(200, 40, 40), 2.0));
    shape->setBrush(QColor(255, 180, 180));
    shape->setData(0, QStringLiteral("shape"));
    shape->setData(1, 1);
    saveScene.addItem(shape);

    auto *text = new QGraphicsTextItem(QStringLiteral("Graph label"));
    text->setDefaultTextColor(QColor(30, 30, 30));
    text->setPos(100, 220);
    text->setData(0, QStringLiteral("text"));
    QFont tf = text->font();
    tf.setFamily(QStringLiteral("Sans Serif"));
    tf.setPointSize(16);
    text->setFont(tf);
    saveScene.addItem(text);

    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(QColor(10, 180, 90));
    auto *pix = new QGraphicsPixmapItem(QPixmap::fromImage(img));
    pix->setPos(300, 180);
    pix->setData(0, QStringLiteral("image"));
    saveScene.addItem(pix);
  }

  CHECK(InfiniteCanvasStore::saveToFile(path, &saveScene, true), "saveToFile");

  QGraphicsScene loadScene;
  bool infinite = false;
  CHECK(InfiniteCanvasStore::loadFromFile(path, &loadScene, &infinite),
        "loadFromFile");
  CHECK(infinite, "isInfinite flag");

  int strokes = 0, stickies = 0, shapes = 0, texts = 0, images = 0;
  QString stickyText;
  for (QGraphicsItem *it : loadScene.items()) {
    if (it->type() == QGraphicsItem::UserType + 1)
      ++strokes;
    const QString tag = it->data(0).toString();
    if (tag == QLatin1String("sticky_note")) {
      ++stickies;
      for (QGraphicsItem *ch : it->childItems()) {
        if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(ch)) {
          stickyText = ti->toPlainText();
          break;
        }
      }
    } else if (tag == QLatin1String("shape"))
      ++shapes;
    else if (tag == QLatin1String("text"))
      ++texts;
    else if (tag == QLatin1String("image"))
      ++images;
  }

  CHECK(strokes == 1, "stroke count");
  CHECK(stickies == 1, "sticky count");
  CHECK(shapes == 1, "shape count");
  CHECK(texts == 1, "text count");
  CHECK(images == 1, "image count");
  CHECK(stickyText == QLatin1String("Hello sticky"), "sticky text");

  if (g_failures) {
    std::fprintf(stderr, "infinite_persistence_roundtrip: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::fprintf(stdout, "infinite_persistence_roundtrip: OK\n");
  return 0;
}
