#pragma once
#include "AbstractTool.h"
#include <QGraphicsItem>

class EraserTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Eraser; }
    QString name() const override { return "Radierer"; }
    QString iconName() const override { return "eraser"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        eraseAt(event->scenePos(), scene);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (event->buttons() & Qt::LeftButton) {
            eraseAt(event->scenePos(), scene);
            return true;
        }
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        return true;
    }

private:
    void eraseAt(const QPointF& pos, QGraphicsScene* scene) {
        QRectF eraserRect(pos.x() - 10, pos.y() - 10, 20, 20);
        QList<QGraphicsItem*> items = scene->items(eraserRect);
        bool erased = false;
        for (auto* item : items) {
            if (item->zValue() != -100) { // Hintergrund ignorieren
                scene->removeItem(item);
                delete item;
                erased = true;
            }
        }
        if (erased) emit contentModified();
    }
};
