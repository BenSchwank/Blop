#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include "StrokeItem.h"
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QLineF>
#include <QPen>
#include <QList>
#include <QPointF>
#include <QTabletEvent>

class AbstractStrokeTool : public AbstractTool {
    Q_OBJECT

public:
    using AbstractTool::AbstractTool;

    // We store the last pressure so that mouseMove events (if synthesized) have a fallback,
    // though native TabletEvents will use their own pressure.
    qreal m_lastPressure{1.0};

    bool handleTabletEvent(QTabletEvent* event, const QPointF& scenePos) override {
        // Tablet events give us robust pressure [0.0 - 1.0]
        m_lastPressure = event->pressure();
        
        // We simulate a mouse event call to keep the logic unified. 
        // The actual drawing loop uses m_lastPressure.
        if (event->type() == QEvent::TabletPress) {
            handleMoveOrPress(scenePos, true, nullptr);
            return true;
        } else if (event->type() == QEvent::TabletMove) {
            handleMoveOrPress(scenePos, false, nullptr);
            return true;
        } else if (event->type() == QEvent::TabletRelease) {
            // End stroke
            return handleMouseRelease(nullptr, nullptr); // scene is stored conceptually
        }
        return false;
    }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        // Mouse clicks default to max pressure if not overriden by tablet prior
        m_sceneRef = scene;
        m_lastPressure = 1.0; 
        return handleMoveOrPress(event->scenePos(), true, scene);
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        return handleMoveOrPress(event->scenePos(), false, scene);
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentItem) {
            m_currentItem->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
            m_lastCompletedItem = m_currentItem;
            m_currentItem = nullptr;
            m_pointsBuffer.clear();
            m_longPressStraightMode = false;
            m_isSnapping = false; 
            m_rulerRef = nullptr;
            m_sceneRef = nullptr;
            emit contentModified();
            return true;
        }
        return false;
    }

protected:
    virtual QPen createPen() const = 0;
    virtual qreal getZValue() const { return 0; }
    virtual StrokeItem::StrokeStyle strokeStyle() const { return StrokeItem::Normal; }

    QGraphicsPathItem* m_currentItem{nullptr};
    QPainterPath m_currentPath;
    QList<StrokePoint> m_pointsBuffer;
    QElapsedTimer m_pressTimer;
    QPointF m_pressStartPos;
    bool m_longPressStraightMode{false};
    bool m_isSnapping{false};
    RulerItem* m_rulerRef{nullptr};
    QGraphicsScene* m_sceneRef{nullptr};

    bool handleMoveOrPress(QPointF scenePos, bool isPress, QGraphicsScene* scene) {
        if (isPress) {
            m_currentPath = QPainterPath();
            QPointF startPos = scenePos;
            m_pressStartPos = scenePos;
            m_pressTimer.restart();
            m_longPressStraightMode = false;
            m_isSnapping = false;
            m_rulerRef = nullptr;
            if (scene) m_sceneRef = scene;

            if (m_config.rulerSnap && m_sceneRef) {
                m_rulerRef = findRuler(m_sceneRef);
                if (m_rulerRef && m_rulerRef->isVisible()) {
                    startPos = m_rulerRef->snapPoint(startPos);
                    m_isSnapping = true;
                }
            }

            m_currentPath.moveTo(startPos);
            m_pointsBuffer.clear();
            m_pointsBuffer.append({startPos, m_lastPressure});

            auto* typedItem = new StrokeItem(m_currentPath, createPen(), QVector<StrokePoint>(), strokeStyle());
            typedItem->addPoint({startPos, m_lastPressure});
            m_currentItem = typedItem;
            m_currentItem->setZValue(getZValue());

            if (m_sceneRef) m_sceneRef->addItem(m_currentItem);
            return true;
        } else {
            if (m_currentItem) {
                QPointF newPos = scenePos;
                if (!m_longPressStraightMode && m_pressTimer.isValid()) {
                    const bool heldLongEnough = m_pressTimer.elapsed() >= 360;
                    const bool draggedEnough = QLineF(scenePos, m_pressStartPos).length() >= 8.0;
                    if (heldLongEnough && draggedEnough) {
                        m_longPressStraightMode = true;
                    }
                }

                if (m_isSnapping && m_rulerRef) {
                    QPointF start = m_pointsBuffer.first().pos;
                    newPos = m_rulerRef->snapPoint(newPos, start);
                    
                    // Bei Snapping wollen wir keine Glättung und den Punkt normal hinzufügen
                    if (m_pointsBuffer.isEmpty() || QLineF(newPos, m_pointsBuffer.last().pos).length() > 0.5) {
                        m_pointsBuffer.append({newPos, m_lastPressure});
                        m_currentPath.lineTo(newPos);
                        m_currentItem->setPath(m_currentPath);
                        auto* typedItem = static_cast<StrokeItem*>(m_currentItem);
                        typedItem->addPoint({newPos, m_lastPressure});
                    }
                    return true;
                }

                if (m_longPressStraightMode && !m_pointsBuffer.isEmpty()) {
                    const QPointF start = m_pointsBuffer.first().pos;
                    m_currentPath = QPainterPath();
                    m_currentPath.moveTo(start);
                    m_currentPath.lineTo(newPos);
                    m_currentItem->setPath(m_currentPath);
                    auto* typedItem = static_cast<StrokeItem*>(m_currentItem);
                    QVector<StrokePoint> lockedPoints;
                    lockedPoints.reserve(2);
                    lockedPoints.append({start, m_pointsBuffer.first().pressure});
                    lockedPoints.append({newPos, m_lastPressure});
                    typedItem->setPoints(lockedPoints);
                    return true;
                }

                if (m_config.smoothing > 0) {
                    double factor = m_config.smoothing / 120.0;
                    if (factor > 0.95) factor = 0.95;
                    QPointF lastPos = m_pointsBuffer.last().pos;
                    newPos = lastPos * factor + newPos * (1.0 - factor);
                }

                if (m_pointsBuffer.isEmpty() || QLineF(newPos, m_pointsBuffer.last().pos).length() > 1.0) {
                    m_pointsBuffer.append({newPos, m_lastPressure});
                    m_currentPath.lineTo(newPos);
                    
                    m_currentItem->setPath(m_currentPath);
                    auto* typedItem = static_cast<StrokeItem*>(m_currentItem);
                    typedItem->addPoint({newPos, m_lastPressure});
                }
                return true;
            }
        }
        return false;
    }

    RulerItem* findRuler(QGraphicsScene* scene) {
        if (!scene) return nullptr;
        for (QGraphicsItem* item : scene->items()) {
            if (item->type() == RulerItem::Type) return static_cast<RulerItem*>(item);
        }
        return nullptr;
    }
};
