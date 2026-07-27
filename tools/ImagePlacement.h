#pragma once

/// Shared image-item construction for ImageTool + headless ctests (no file dialog).

#include "ToolSettings.h"

#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QPointF>
#include <QtGlobal>

inline QGraphicsPixmapItem *blopCreateImageItem(const QPixmap &pixmap,
                                                const QPointF &pos,
                                                const ToolConfig &cfg) {
  if (pixmap.isNull())
    return nullptr;

  QPixmap pm = pixmap;
  if (pm.width() > 800)
    pm = pm.scaledToWidth(800, Qt::SmoothTransformation);

  auto *item = new QGraphicsPixmapItem(pm);
  item->setPos(pos);
  item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
  item->setZValue(5);
  item->setData(0, QStringLiteral("image"));
  const qreal op =
      cfg.imageOpacity > 0.01 ? cfg.imageOpacity : cfg.opacity;
  item->setOpacity(qBound(0.1, op, 1.0));
  return item;
}
