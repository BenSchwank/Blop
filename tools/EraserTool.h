#pragma once
#include "AbstractStrokeTool.h"
#include "StrokeItem.h"
#include "UIStyles.h"
#include <QBrush>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QSet>
#include <QVector>

/// Snapshot of a path item before an erase gesture touched it.
struct ErasePathSnapshot {
    QPainterPath path;
    QPen pen;
    QBrush brush;
    QVector<StrokePoint> points;
    QPointF pos;
    QGraphicsItem *parent{nullptr};
};

class EraserTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;
    ToolMode mode() const override { return ToolMode::Eraser; }
    QString name() const override { return "Radierer"; }
    QString iconName() const override { return "eraser"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        m_sceneRef = scene;
        eraseAt(event->scenePos(), scene);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (scene)
            m_sceneRef = scene;
        eraseAt(event->scenePos(), scene ? scene : m_sceneRef);
        return true;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent*, QGraphicsScene* scene) override {
        finishSession(scene ? scene : m_sceneRef);
        return true;
    }

    bool handleTabletEvent(QTabletEvent* event, const QPointF& scenePos) override {
        if (m_sceneRef) {
            if (event->type() == QEvent::TabletPress ||
                event->type() == QEvent::TabletMove)
                eraseAt(scenePos, m_sceneRef);
            else if (event->type() == QEvent::TabletRelease)
                finishSession(m_sceneRef);
        }
        // Do not create an eraser stroke overlay via AbstractStrokeTool —
        // object/pixel erase is handled entirely in eraseAt().
        if (event->type() == QEvent::TabletPress ||
            event->type() == QEvent::TabletMove ||
            event->type() == QEvent::TabletRelease)
            return true;
        return AbstractStrokeTool::handleTabletEvent(event, scenePos);
    }

signals:
    /// Fired once per erase gesture. `removed` items are off-scene; the
    /// receiver owns them via an undo command. `pathBefore` holds pre-erase
    /// geometry for items that were cut but not fully deleted.
    void eraseSessionFinished(const QList<QGraphicsItem *> &removed,
                              const QHash<QGraphicsPathItem *, ErasePathSnapshot> &pathBefore);

protected:
    QPen createPen() const override {
        return QPen(Qt::white, m_config.penWidth, Qt::SolidLine, Qt::RoundCap,
                    Qt::RoundJoin);
    }
    StrokeItem::StrokeStyle strokeStyle() const override {
        return StrokeItem::Eraser;
    }

private:
    void rememberPath(QGraphicsPathItem *pathItem) {
        if (!pathItem || m_pathBefore.contains(pathItem))
            return;
        ErasePathSnapshot snap;
        snap.path = pathItem->path();
        snap.pen = pathItem->pen();
        snap.brush = pathItem->brush();
        snap.pos = pathItem->pos();
        snap.parent = pathItem->parentItem();
        if (auto *si = dynamic_cast<StrokeItem *>(pathItem))
            snap.points = si->points();
        m_pathBefore.insert(pathItem, snap);
    }

    void eraseAt(QPointF pos, QGraphicsScene* scene) {
        if (!scene) return;

        double r = (m_config.eraserMode == EraserMode::Pixel)
                       ? (m_config.penWidth / 2.0)
                       : 5.0;

        QRectF rect(pos.x() - r, pos.y() - r, 2 * r, 2 * r);
        QList<QGraphicsItem *> items =
            scene->items(rect, Qt::IntersectsItemBoundingRect);

        QPainterPath eraserShape;
        eraserShape.addEllipse(pos, r, r);

        QList<QGraphicsItem *> toDelete;
        QSet<QGraphicsItem *> toDeleteSet;
        bool pathChanged = false;

        for (QGraphicsItem *item : items) {
            if (toDeleteSet.contains(item))
                continue;
            if (!item->shape().intersects(item->mapFromScene(eraserShape)))
                continue;

            const QString tag = item->data(0).toString();
            if (tag == QLatin1String("text") || tag == QLatin1String("image") ||
                tag == QLatin1String("sticky_note") ||
                tag == QLatin1String("shape")) {
                if (m_config.eraserMode == EraserMode::Object) {
                    toDelete.append(item);
                    toDeleteSet.insert(item);
                }
                continue;
            }

            QGraphicsPathItem *pathItem =
                dynamic_cast<QGraphicsPathItem *>(item);
            if (!pathItem)
                continue;
            if (m_currentItem && pathItem == m_currentItem)
                continue;
            if (m_config.eraserKeepInk && pathItem->zValue() >= 10)
                continue;

            if (m_config.eraserMode == EraserMode::Object) {
                rememberPath(pathItem);
                toDelete.append(item);
                toDeleteSet.insert(item);
            } else {
                rememberPath(pathItem);
                QPainterPath currentPath = pathItem->path();

                if (pathItem->pen().style() != Qt::NoPen) {
                    QPainterPathStroker stroker;
                    stroker.setWidth(pathItem->pen().widthF());
                    stroker.setCapStyle(pathItem->pen().capStyle());
                    stroker.setJoinStyle(pathItem->pen().joinStyle());
                    stroker.setMiterLimit(pathItem->pen().miterLimit());

                    QPainterPath outline = stroker.createStroke(currentPath);
                    QPainterPath newPath = outline.subtracted(eraserShape);

                    pathItem->setPath(newPath);
                    pathItem->setBrush(pathItem->pen().brush());
                    pathItem->setPen(Qt::NoPen);
                    if (auto *strokeItem = dynamic_cast<StrokeItem *>(pathItem))
                        strokeItem->setPoints({});
                } else {
                    QPainterPath newPath = currentPath.subtracted(eraserShape);
                    pathItem->setPath(newPath);
                }
                pathChanged = true;

                if (pathItem->path().isEmpty()) {
                    if (auto *strokeItem = dynamic_cast<StrokeItem *>(pathItem))
                        strokeItem->setPoints({});
                    toDelete.append(item);
                    toDeleteSet.insert(item);
                }
            }
        }

        for (QGraphicsItem *item : toDelete) {
            if (auto *pathItem = dynamic_cast<QGraphicsPathItem *>(item))
                rememberPath(pathItem);
            scene->removeItem(item);
            if (!m_sessionRemoved.contains(item))
                m_sessionRemoved.append(item);
        }
        if (!toDelete.isEmpty() || pathChanged)
            emit contentModified();
    }

    void finishSession(QGraphicsScene *scene) {
        Q_UNUSED(scene);
        if (m_sessionRemoved.isEmpty() && m_pathBefore.isEmpty())
            return;

    // Drop pathBefore entries that only exist for bookkeeping of removed
    // items — keep them so undo can restore original stroke geometry.
    const QList<QGraphicsItem *> removed = m_sessionRemoved;
    const QHash<QGraphicsPathItem *, ErasePathSnapshot> pathBefore = m_pathBefore;
    m_sessionRemoved.clear();
    m_pathBefore.clear();
    emit eraseSessionFinished(removed, pathBefore);
  }

    QList<QGraphicsItem *> m_sessionRemoved;
    QHash<QGraphicsPathItem *, ErasePathSnapshot> m_pathBefore;
};
