#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include <QGraphicsPathItem>
#include <QPen>
#include <QList>
#include <QPointF>

class AbstractStrokeTool : public AbstractTool {
    Q_OBJECT

public:
    using AbstractTool::AbstractTool;

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        m_currentPath = QPainterPath();
        QPointF startPos = event->scenePos();
        m_isSnapping = false;
        m_rulerRef = nullptr;

        if (m_config.rulerSnap) {
            m_rulerRef = findRuler(scene);
            if (m_rulerRef && m_rulerRef->isVisible()) {
                startPos = m_rulerRef->snapPoint(startPos);
                m_isSnapping = true;
            }
        }

        m_currentPath.moveTo(startPos);
        m_pointsBuffer.clear();
        m_pointsBuffer.append(startPos);

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
            QPointF newPos = event->scenePos();
            if (m_isSnapping && m_rulerRef) {
                newPos = m_rulerRef->snapPoint(newPos);
                QPointF start = m_pointsBuffer.first();
                QPainterPath snapPath = m_rulerRef->getSnapPath(start, newPos);
                m_currentItem->setPath(snapPath);
                m_currentPath = snapPath;
                return true;
            }

            if (m_config.smoothing > 0) {
                double factor = m_config.smoothing / 120.0;
                if (factor > 0.95) factor = 0.95;
                QPointF lastPos = m_pointsBuffer.last();
                newPos = lastPos * factor + newPos * (1.0 - factor);
            }

            if (m_pointsBuffer.isEmpty() || QLineF(newPos, m_pointsBuffer.last()).length() > 1.0) {
                m_pointsBuffer.append(newPos);
                m_currentPath.lineTo(newPos);
                m_currentItem->setPath(m_currentPath);
            }
            return true;
        }
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentItem) {
            m_currentItem = nullptr;
            m_pointsBuffer.clear();
            m_isSnapping = false; m_rulerRef = nullptr;
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
    QList<QPointF> m_pointsBuffer;
    bool m_isSnapping{false};
    RulerItem* m_rulerRef{nullptr};

    RulerItem* findRuler(QGraphicsScene* scene) {
        for (QGraphicsItem* item : scene->items()) {
            if (item->type() == RulerItem::Type) return static_cast<RulerItem*>(item);
        }
        return nullptr;
    }
};
