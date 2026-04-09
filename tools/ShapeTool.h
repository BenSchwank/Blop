#pragma once
#include "AbstractTool.h"
#include <QElapsedTimer>
#include <QLineF>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QPen>

class ShapeTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Shape; }
    QString name() const override { return "Formen"; }
    QString iconName() const override { return "shape"; }

    // --- Interaction ---
    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;

        m_startPos = event->scenePos();
        m_lastMotionPos = m_startPos;
        m_pressTimer.restart();
        m_longPressLock = false;

        // KORREKTUR: Wir erstellen das Item direkt als RectItem
        QGraphicsRectItem* rectItem = new QGraphicsRectItem();

        // Jetzt können wir setPen aufrufen, da rectItem den richtigen Typ hat
        rectItem->setPen(QPen(m_config.penColor, m_config.penWidth));
        rectItem->setZValue(5); // Liegt unter dem Stift (10) aber über Bildern

        // Initiale Größe 0
        rectItem->setRect(QRectF(m_startPos, QSizeF(0, 0)));

        // Speichern im Member-Pointer (der ist vom Typ QGraphicsItem*)
        m_currentShape = rectItem;

        scene->addItem(m_currentShape);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentShape) {
            if (!m_longPressLock && m_pressTimer.isValid()) {
                const qreal moved = QLineF(m_lastMotionPos, event->scenePos()).length();
                if (moved > 1.8) {
                    m_lastMotionPos = event->scenePos();
                    m_pressTimer.restart();
                }
                const bool heldLongEnough = m_pressTimer.elapsed() >= 360;
                const bool draggedEnough = QLineF(m_startPos, event->scenePos()).length() >= 8.0;
                if (heldLongEnough && draggedEnough)
                    m_longPressLock = true;
            }
            // Wir müssen zurückcasten, um setRect aufzurufen
            QGraphicsRectItem* rectItem = static_cast<QGraphicsRectItem*>(m_currentShape);

            QPointF endPos = event->scenePos();
            if (m_longPressLock) {
                // Long-press: keep shape clean/proportional (square lock).
                const QPointF delta = endPos - m_startPos;
                const qreal side = qMax(qAbs(delta.x()), qAbs(delta.y()));
                const qreal sx = (delta.x() >= 0) ? 1.0 : -1.0;
                const qreal sy = (delta.y() >= 0) ? 1.0 : -1.0;
                endPos = m_startPos + QPointF(side * sx, side * sy);
            }
            QRectF newRect(m_startPos, endPos);
            rectItem->setRect(newRect.normalized());
            return true;
        }
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentShape) {
            QGraphicsRectItem* rectItem = static_cast<QGraphicsRectItem*>(m_currentShape);
            QRectF rect = rectItem->rect();

            // Zu kleine Formen entfernen (versehentlicher Klick)
            if (rect.width() < 5 && rect.height() < 5) {
                scene->removeItem(m_currentShape);
                delete m_currentShape;
            } else {
                emit contentModified();
            }
            m_currentShape = nullptr;
            m_longPressLock = false;
            return true;
        }
        return false;
    }

private:
    QGraphicsItem* m_currentShape{nullptr};
    QPointF m_startPos;
    QPointF m_lastMotionPos;
    QElapsedTimer m_pressTimer;
    bool m_longPressLock{false};
};
