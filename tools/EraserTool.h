#pragma once
#include "AbstractStrokeTool.h"
#include "StrokeItem.h"
#include "UIStyles.h"

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPointer>
#include <QSet>

class EraserTool : public AbstractStrokeTool {
  Q_OBJECT
public:
  using AbstractStrokeTool::AbstractStrokeTool;
  ToolMode mode() const override { return ToolMode::Eraser; }
  QString name() const override { return QStringLiteral("Radierer"); }
  QString iconName() const override { return QStringLiteral("eraser"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene)
      return false;
    m_previewPos = event->scenePos();
    m_previewActive = true;
    emit overlayChanged();
    eraseAt(event->scenePos(), scene);
    return true;
  }

  bool handleMouseMove(QGraphicsSceneMouseEvent *event,
                       QGraphicsScene *scene) override {
    if (!scene)
      return false;
    m_previewPos = event->scenePos();
    m_previewActive = true;
    emit overlayChanged();
    if (event->buttons() & Qt::LeftButton)
      eraseAt(event->scenePos(), scene);
    return true;
  }

  bool handleMouseRelease(QGraphicsSceneMouseEvent *,
                          QGraphicsScene *) override {
    m_previewActive = false;
    emit overlayChanged();
    return true;
  }

  bool handleTabletEvent(QTabletEvent *event,
                         const QPointF &scenePos) override {
    if (!m_sceneRef)
      return false;
    m_previewPos = scenePos;
    if (event->type() == QEvent::TabletRelease) {
      m_previewActive = false;
      emit overlayChanged();
      return true;
    }
    m_previewActive = true;
    emit overlayChanged();
    eraseAt(scenePos, m_sceneRef);
    return true;
  }

  void onDeactivated() override {
    m_previewActive = false;
    emit overlayChanged();
  }

  void drawOverlay(QPainter *painter, const QRectF &rect) override {
    Q_UNUSED(rect);
    if (!m_previewActive || m_config.eraserMode != EraserMode::Pixel)
      return;
    const qreal r = eraserRadius();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QColor fill(0xEF, 0x44, 0x44, 40);
    QPen pen(QColor(0xEF, 0x44, 0x44, 200), 1.5, Qt::SolidLine, Qt::RoundCap);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(fill);
    painter->drawEllipse(m_previewPos, r, r);
    painter->restore();
  }

protected:
  QPen createPen() const override {
    return QPen(Qt::white, m_config.penWidth, Qt::SolidLine, Qt::RoundCap,
                Qt::RoundJoin);
  }
  StrokeItem::StrokeStyle strokeStyle() const override {
    return StrokeItem::Eraser;
  }

private:
  qreal eraserRadius() const {
    double r = m_config.penWidth / 2.0;
    if (r < 1.0)
      r = 1.0;
    return r;
  }

  static bool isTextItem(QGraphicsItem *item) {
    if (!item)
      return false;
    if (qgraphicsitem_cast<QGraphicsTextItem *>(item))
      return true;
    return item->data(0).toString() == QLatin1String("text_item");
  }

  static bool isErasableGraphic(QGraphicsItem *item) {
    if (!item)
      return false;
    if (dynamic_cast<QGraphicsPathItem *>(item))
      return true;
    if (isTextItem(item))
      return true;
    return false;
  }

  void eraseAt(QPointF pos, QGraphicsScene *scene) {
    if (!scene)
      return;

    const double r = eraserRadius();
    QRectF rect(pos.x() - r, pos.y() - r, 2 * r, 2 * r);
    QList<QGraphicsItem *> items =
        scene->items(rect, Qt::IntersectsItemBoundingRect);

    QPainterPath eraserShape;
    eraserShape.addEllipse(pos, r, r);

    QList<QGraphicsItem *> toDelete;
    QSet<QGraphicsItem *> toDeleteSet;
    bool mutated = false;

    for (QGraphicsItem *item : items) {
      if (!isErasableGraphic(item))
        continue;
      if (m_currentItem && item == m_currentItem)
        continue;
      if (toDeleteSet.contains(item))
        continue;

      const QPainterPath eraserInItem = item->mapFromScene(eraserShape);
      if (!item->shape().intersects(eraserInItem))
        continue;

      // „Nur Textmarker löschen“ — skip ink, shapes, and text.
      if (m_config.eraserKeepInk) {
        if (isTextItem(item))
          continue;
        auto *si = dynamic_cast<StrokeItem *>(item);
        if (si) {
          if (si->strokeStyle() != StrokeItem::Highlighter)
            continue;
        } else if (auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item)) {
          // Non-Stroke path shapes (geometry) are not highlighter ink.
          Q_UNUSED(pathItem);
          continue;
        } else {
          continue;
        }
      }

      // Object eraser: delete whole element (stroke, shape, text).
      if (m_config.eraserMode == EraserMode::Object) {
        toDelete.append(item);
        toDeleteSet.insert(item);
        continue;
      }

      // Pixel eraser: only path strokes (not text).
      auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
      if (!pathItem || isTextItem(item))
        continue;

      QPainterPath currentPath = pathItem->path();
      if (auto *si = dynamic_cast<StrokeItem *>(pathItem)) {
        const auto pts = si->points();
        if (pts.size() > 1 && pathItem->pen().style() != Qt::NoPen) {
          QVector<QPointF> center;
          center.reserve(pts.size());
          for (const StrokePoint &sp : pts)
            center.append(sp.pos);
          currentPath = smoothStrokePath(center);
        }
      }

      if (pathItem->pen().style() != Qt::NoPen) {
        QPainterPathStroker stroker;
        stroker.setWidth(pathItem->pen().widthF());
        stroker.setCapStyle(pathItem->pen().capStyle());
        stroker.setJoinStyle(pathItem->pen().joinStyle());
        stroker.setMiterLimit(pathItem->pen().miterLimit());
        QPainterPath outline = stroker.createStroke(currentPath);
        QPainterPath newPath = outline.subtracted(eraserInItem);
        pathItem->setPath(newPath);
        pathItem->setBrush(pathItem->pen().brush());
        pathItem->setPen(Qt::NoPen);
        if (auto *strokeItem = dynamic_cast<StrokeItem *>(pathItem))
          strokeItem->setPoints({});
        // Drop stale Stroke snap (MultiPageNoteView kStrokeSnapKey = 0x5301)
        // so harvest uses the subtracted filled outline, not the old polyline.
        pathItem->setData(0x5301, QVariant());
        mutated = true;
      } else {
        QPainterPath newPath = currentPath.subtracted(eraserInItem);
        if (newPath != currentPath) {
          pathItem->setPath(newPath);
          mutated = true;
        }
      }

      if (pathItem->path().isEmpty()) {
        if (auto *strokeItem = dynamic_cast<StrokeItem *>(pathItem))
          strokeItem->setPoints({});
        toDelete.append(item);
        toDeleteSet.insert(item);
      }
    }

    for (QGraphicsItem *item : toDelete) {
      scene->removeItem(item);
      delete item;
      mutated = true;
    }
    if (mutated)
      emit contentModified();
  }

  QPointF m_previewPos;
  bool m_previewActive{false};
};
