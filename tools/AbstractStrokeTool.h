#pragma once
#include "AbstractTool.h"
#include <QGraphicsPathItem>
#include <QPen>

class AbstractStrokeTool : public AbstractTool {
    Q_OBJECT

public:
    using AbstractTool::AbstractTool;

    // --- Input Handling ---
    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        // Wir nutzen die übergebene 'scene', da event->scene() null sein kann
        if (!scene) return false;

        m_currentPath = QPainterPath();
        m_currentPath.moveTo(event->scenePos());

        m_currentItem = new QGraphicsPathItem();
        m_currentItem->setPath(m_currentPath);
        m_currentItem->setPen(createPen());
        m_currentItem->setZValue(getZValue());

        m_currentItem->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

        scene->addItem(m_currentItem);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentItem) {
            m_currentPath.lineTo(event->scenePos());
            m_currentItem->setPath(m_currentPath);
            return true;
        }
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentItem) {
            m_currentItem = nullptr;
            emit contentModified();
            return true;
        }
        return false;
    }

protected:
    virtual QPen createPen() const = 0;
    virtual qreal getZValue() const { return 0; }

    QGraphicsPathItem* m_currentItem{nullptr};
    QPainterPath m_currentPath;
};
