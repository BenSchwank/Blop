#include "infinitecanvasstore.h"

#include "tools/StrokeItem.h"

#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QPainterPath>
#include <QPen>
#include <QTextDocument>
#include <QTextOption>

namespace InfiniteCanvasStore {
namespace {

bool isStrokeItem(QGraphicsItem *item) {
  return item && item->type() == QGraphicsItem::UserType + 1;
}

} // namespace

bool saveToFile(const QString &path, QGraphicsScene *scene, bool isInfinite,
                const QSet<QGraphicsItem *> &exclude) {
  if (!scene || path.isEmpty())
    return false;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  QDataStream out(&file);

  out << kMagicV5;
  out << isInfinite;

  const QList<QGraphicsItem *> items = scene->items(Qt::AscendingOrder);
  int count = 0;
  for (auto *item : items) {
    if (isStrokeItem(item) && !exclude.contains(item))
      ++count;
  }
  out << count;

  for (auto *item : items) {
    if (!isStrokeItem(item) || exclude.contains(item))
      continue;
    auto *strokeItem = static_cast<StrokeItem *>(item);
    out << strokeItem->pos() << strokeItem->pen().color()
        << (int)strokeItem->pen().width() << strokeItem->path();
    const QVector<StrokePoint> &pts = strokeItem->points();
    out << (qint32)pts.size();
    for (const auto &p : pts)
      out << p.pos << p.pressure;
  }

  QList<QGraphicsRectItem *> stickies;
  for (auto *item : items) {
    if (exclude.contains(item))
      continue;
    if (item->data(0).toString() != QLatin1String("sticky_note"))
      continue;
    if (auto *card = qgraphicsitem_cast<QGraphicsRectItem *>(item))
      stickies.append(card);
  }
  out << (qint32)stickies.size();
  for (QGraphicsRectItem *card : stickies) {
    QString text;
    int fontPt = 14;
    for (QGraphicsItem *ch : card->childItems()) {
      if (auto *ti = qgraphicsitem_cast<QGraphicsTextItem *>(ch)) {
        text = ti->toPlainText();
        fontPt = ti->font().pointSize() > 0 ? ti->font().pointSize() : 14;
        break;
      }
    }
    const QRectF r = card->rect();
    out << card->pos() << r.width() << r.height() << card->brush().color()
        << fontPt << text;
  }

  QList<QGraphicsPathItem *> shapes;
  QList<QGraphicsTextItem *> texts;
  QList<QGraphicsPixmapItem *> images;
  for (auto *item : items) {
    if (exclude.contains(item))
      continue;
    const QString tag = item->data(0).toString();
    if (tag == QLatin1String("shape")) {
      if (auto *p = qgraphicsitem_cast<QGraphicsPathItem *>(item))
        shapes.append(p);
    } else if (tag == QLatin1String("text")) {
      if (auto *t = qgraphicsitem_cast<QGraphicsTextItem *>(item)) {
        if (!(t->parentItem() &&
              t->parentItem()->data(0).toString() ==
                  QLatin1String("sticky_note")))
          texts.append(t);
      }
    } else if (tag == QLatin1String("image")) {
      if (auto *im = qgraphicsitem_cast<QGraphicsPixmapItem *>(item))
        images.append(im);
    }
  }

  out << (qint32)shapes.size();
  for (QGraphicsPathItem *p : shapes) {
    const QColor fill = (p->brush().style() == Qt::NoBrush)
                            ? QColor(0, 0, 0, 0)
                            : p->brush().color();
    out << p->pos() << p->pen().color() << p->pen().widthF() << fill
        << p->data(1).toInt() << p->path();
  }
  out << (qint32)texts.size();
  for (QGraphicsTextItem *t : texts) {
    out << t->pos() << t->toPlainText() << t->font().family()
        << t->font().pointSize() << t->defaultTextColor()
        << int(t->document()->defaultTextOption().alignment());
  }
  out << (qint32)images.size();
  for (QGraphicsPixmapItem *im : images) {
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    im->pixmap().toImage().save(&buf, "PNG");
    out << im->pos() << im->opacity() << im->scale() << png;
  }
  return true;
}

bool loadFromFile(const QString &path, QGraphicsScene *scene,
                  bool *isInfinite) {
  if (!scene || path.isEmpty())
    return false;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return false;
  QDataStream in(&file);
  quint32 magic = 0;
  in >> magic;

  bool infinite = true;
  if (magic == kMagicV1) {
    infinite = true;
  } else if (magic == kMagicV2 || magic == kMagicV3 || magic == kMagicV4 ||
             magic == kMagicV5) {
    in >> infinite;
  } else {
    return false;
  }
  if (isInfinite)
    *isInfinite = infinite;

  scene->clear();

  int count = 0;
  in >> count;
  const bool wasBlocked = scene->blockSignals(true);
  for (int i = 0; i < count; ++i) {
    QPointF pos;
    QColor color;
    int width = 2;
    QPainterPath path;
    in >> pos >> color >> width >> path;
    QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    QVector<StrokePoint> pts;
    if (magic >= kMagicV3) {
      qint32 ptCount = 0;
      in >> ptCount;
      for (int j = 0; j < ptCount; ++j) {
        QPointF ppos;
        qreal ppress = 1.0;
        in >> ppos >> ppress;
        pts.append({ppos, ppress});
      }
    }

    auto *item = new StrokeItem(path, pen, pts, StrokeItem::Normal);
    scene->addItem(item);
    item->setPos(pos);
    item->setZValue(color.alpha() < 255 ? 0.1 : 1.0);
  }

  if (magic >= kMagicV4) {
    qint32 stickyCount = 0;
    in >> stickyCount;
    for (qint32 i = 0; i < stickyCount; ++i) {
      QPointF pos;
      qreal w = 168, h = 148;
      QColor fill(255, 236, 120);
      int fontPt = 14;
      QString text;
      in >> pos >> w >> h >> fill >> fontPt >> text;
      auto *card = new QGraphicsRectItem(0, 0, w, h);
      card->setPos(pos);
      card->setBrush(fill);
      card->setPen(QPen(QColor(210, 175, 55), 1.2));
      card->setFlags(QGraphicsItem::ItemIsSelectable |
                     QGraphicsItem::ItemIsMovable |
                     QGraphicsItem::ItemSendsGeometryChanges);
      card->setZValue(6);
      card->setData(0, QStringLiteral("sticky_note"));
      auto *ti = new QGraphicsTextItem(card);
      ti->setPos(10, 10);
      ti->setTextWidth(w - 20);
      QFont font = ti->font();
      font.setPointSize(qBound(8, fontPt, 72));
      ti->setFont(font);
      ti->setDefaultTextColor(QColor(40, 36, 20));
      ti->setPlainText(text);
      ti->setTextInteractionFlags(Qt::TextEditorInteraction);
      ti->setFlag(QGraphicsItem::ItemIsFocusable, true);
      scene->addItem(card);
    }
  }

  if (magic >= kMagicV5) {
    qint32 shapeCount = 0;
    in >> shapeCount;
    for (qint32 i = 0; i < shapeCount; ++i) {
      QPointF pos;
      QColor penC, fillC;
      qreal penW = 2.0;
      int kind = 0;
      QPainterPath path;
      in >> pos >> penC >> penW >> fillC >> kind >> path;
      auto *pathItem = new QGraphicsPathItem(path);
      pathItem->setPos(pos);
      pathItem->setPen(
          QPen(penC, penW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      if (fillC.isValid() && fillC.alpha() > 0)
        pathItem->setBrush(fillC);
      else
        pathItem->setBrush(Qt::NoBrush);
      pathItem->setZValue(5);
      pathItem->setData(0, QStringLiteral("shape"));
      pathItem->setData(1, kind);
      pathItem->setFlags(QGraphicsItem::ItemIsSelectable |
                         QGraphicsItem::ItemIsMovable);
      scene->addItem(pathItem);
    }
    qint32 textCount = 0;
    in >> textCount;
    for (qint32 i = 0; i < textCount; ++i) {
      QPointF pos;
      QString text, family;
      int fontPt = 16;
      QColor color;
      int align = 0;
      in >> pos >> text >> family >> fontPt >> color >> align;
      auto *ti = new QGraphicsTextItem();
      ti->setPos(pos);
      ti->setPlainText(text);
      QFont font = ti->font();
      if (!family.isEmpty())
        font.setFamily(family);
      font.setPointSize(qBound(8, fontPt, 72));
      ti->setFont(font);
      ti->setDefaultTextColor(color);
      QTextOption opt = ti->document()->defaultTextOption();
      opt.setAlignment(Qt::Alignment(align));
      ti->document()->setDefaultTextOption(opt);
      ti->setTextInteractionFlags(Qt::TextEditorInteraction);
      ti->setFlags(QGraphicsItem::ItemIsSelectable |
                   QGraphicsItem::ItemIsFocusable |
                   QGraphicsItem::ItemIsMovable);
      ti->setData(0, QStringLiteral("text"));
      ti->setZValue(5);
      scene->addItem(ti);
    }
    qint32 imageCount = 0;
    in >> imageCount;
    for (qint32 i = 0; i < imageCount; ++i) {
      QPointF pos;
      qreal opacity = 1.0, scale = 1.0;
      QByteArray png;
      in >> pos >> opacity >> scale >> png;
      QPixmap pm;
      pm.loadFromData(png, "PNG");
      if (pm.isNull())
        continue;
      auto *im = new QGraphicsPixmapItem(pm);
      im->setPos(pos);
      im->setOpacity(qBound(0.1, opacity, 1.0));
      if (scale > 0.01)
        im->setScale(scale);
      im->setFlags(QGraphicsItem::ItemIsSelectable |
                   QGraphicsItem::ItemIsMovable);
      im->setData(0, QStringLiteral("image"));
      im->setZValue(5);
      scene->addItem(im);
    }
  }

  scene->blockSignals(wasBlocked);
  return true;
}

} // namespace InfiniteCanvasStore
