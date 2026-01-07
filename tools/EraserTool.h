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
        // Nutze die Stiftbreite als Radierergröße (mindestens 10px)
        double size = (m_config.penWidth > 0) ? m_config.penWidth * 2 : 20.0;
        double r = size / 2.0;
        QRectF eraserRect(pos.x() - r, pos.y() - r, size, size);

        QList<QGraphicsItem*> items = scene->items(eraserRect);
        bool erased = false;
        for (auto* item : items) {
            // WICHTIG: Nur Items löschen, die über dem Hintergrund liegen (Z > 0)
            // PageItem hat Z=0, Striche haben Z=0.5 oder Z=1.0
            if (item->zValue() > 0.001) {
                scene->removeItem(item);
                delete item;
                erased = true;
            }
        }
        if (erased) emit contentModified();
    }
};
