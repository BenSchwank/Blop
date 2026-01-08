#pragma once
#include "AbstractTool.h"
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
        m_currentPath.moveTo(event->scenePos());

        // Puffer für Glättung zurücksetzen
        m_pointsBuffer.clear();
        m_pointsBuffer.append(event->scenePos());

        m_currentItem = new QGraphicsPathItem();
        m_currentItem->setPath(m_currentPath);
        m_currentItem->setPen(createPen());
        m_currentItem->setZValue(getZValue());

        // Performance Optimierung
        m_currentItem->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

        scene->addItem(m_currentItem);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentItem) {
            QPointF newPos = event->scenePos();

            // --- GLAETTUNG (Streamline) ---
            if (m_config.smoothing > 0) {
                // Einfacher Algorithmus: Wir bewegen uns nur einen Teil der Strecke zum neuen Punkt
                // Factor 0 = kein Smoothing, Factor 0.9 = sehr starkes Smoothing (Lag)
                double factor = m_config.smoothing / 120.0; // Skalierung auf sinnvolle Werte
                if (factor > 0.95) factor = 0.95;

                QPointF lastPos = m_pointsBuffer.last();

                // Berechne geglättete Position (Weighted Moving Average)
                newPos = lastPos * factor + newPos * (1.0 - factor);
            }

            // Nur hinzufügen, wenn wir uns genug bewegt haben (reduziert Punkte)
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
            // Optional: Pfad vereinfachen am Ende für schönere Kurven
            // m_currentItem->setPath(m_currentPath.simplified());

            m_currentItem = nullptr;
            m_pointsBuffer.clear();
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
    QList<QPointF> m_pointsBuffer; // Für Smoothing
};
