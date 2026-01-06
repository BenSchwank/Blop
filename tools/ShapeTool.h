#pragma once
#include "AbstractTool.h"
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>

class ShapeTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Shape; }
    QString name() const override { return "Formen"; }
    QString iconName() const override { return "shape"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        m_startPos = event->scenePos();

        // Default: Rechteck (später über Config steuerbar)
        // Wir erstellen ein Rechteck mit Größe 0
        auto* item = new QGraphicsRectItem(QRectF(m_startPos, QSizeF(0,0)));

        QPen pen(m_config.penColor, 3);
        item->setPen(pen);
        item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);

        m_currentShape = item;
        scene->addItem(m_currentShape);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentShape) {
            // Rechteck aufziehen
            QRectF rect(m_startPos, event->scenePos());

            // Casten und setzen
            if (auto* r = dynamic_cast<QGraphicsRectItem*>(m_currentShape))
                r->setRect(rect.normalized());

            return true;
        }
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentShape) {
            m_currentShape = nullptr;
            emit contentModified();
            return true;
        }
        return false;
    }

private:
    QPointF m_startPos;
    QGraphicsItem* m_currentShape{nullptr};
};
