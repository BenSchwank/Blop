#pragma once
#include "AbstractTool.h"
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
            // Wir müssen zurückcasten, um setRect aufzurufen
            QGraphicsRectItem* rectItem = static_cast<QGraphicsRectItem*>(m_currentShape);

            QRectF newRect(m_startPos, event->scenePos());
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
            return true;
        }
        return false;
    }

private:
    QGraphicsItem* m_currentShape{nullptr};
    QPointF m_startPos;
};
