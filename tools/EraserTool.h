#pragma once
#include "AbstractTool.h"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsItem>

class EraserTool : public AbstractTool {
    Q_OBJECT

public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Eraser; }
    QString name() const override { return "Radierer"; }
    QString iconName() const override { return "eraser"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        eraseAt(event->scenePos(), scene);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if(event->buttons() & Qt::LeftButton) {
            eraseAt(event->scenePos(), scene);
            return true;
        }
        return false;
    }

    void drawOverlay(QPainter* painter, const QRectF& rect) override {
        // Optional: Cursor Kreis malen
    }

private:
    void eraseAt(const QPointF& pos, QGraphicsScene* scene) {
        if(!scene) return;

        qreal r = m_config.eraserWidth / 2.0;
        QRectF area(pos.x() - r, pos.y() - r, r*2, r*2);

        QList<QGraphicsItem*> items = scene->items(area, Qt::IntersectsItemShape, Qt::DescendingOrder);

        for (QGraphicsItem* item : items) {
            if (item->type() == QGraphicsPathItem::Type || item->type() == QGraphicsPixmapItem::Type) {
                scene->removeItem(item);
                delete item;
                emit contentModified();
            }
        }
    }
};
