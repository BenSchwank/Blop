#pragma once
#include "AbstractTool.h"
#include <QGraphicsPathItem>
#include <QPen>

class LassoTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Lasso; }
    QString name() const override { return "Lasso"; }
    QString iconName() const override { return "lasso"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;

        scene->clearSelection();

        m_selectionPath = QPainterPath();
        m_selectionPath.moveTo(event->scenePos());

        m_lassoVisual = new QGraphicsPathItem();
        m_lassoVisual->setPen(QPen(QColor(0x5E5CE6), 2, Qt::DashLine));
        m_lassoVisual->setPath(m_selectionPath);
        m_lassoVisual->setZValue(100);

        scene->addItem(m_lassoVisual);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_lassoVisual) {
            m_selectionPath.lineTo(event->scenePos());
            m_lassoVisual->setPath(m_selectionPath);
            return true;
        }
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_lassoVisual) {
            m_selectionPath.closeSubpath();
            m_lassoVisual->setPath(m_selectionPath);

            scene->setSelectionArea(m_selectionPath, QTransform());

            scene->removeItem(m_lassoVisual);
            delete m_lassoVisual;
            m_lassoVisual = nullptr;

            emit contentModified();
            return true;
        }
        return false;
    }

private:
    QGraphicsPathItem* m_lassoVisual{nullptr};
    QPainterPath m_selectionPath;
};
