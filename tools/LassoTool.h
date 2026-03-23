#pragma once
#include "AbstractTool.h"
#include <QGraphicsPathItem>
#include <QPen>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QTransform>
#include "ToolManager.h"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsItem>
#include <QObject>
#include <QString>
#include <QPointF>

class LassoTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;
    ToolMode mode() const override { return ToolMode::Lasso; }
    QString name() const override { return "Lasso"; }
    QString iconName() const override { return "lasso"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        QGraphicsItem* itemUnderMouse = scene->itemAt(event->scenePos(), QTransform());

        // Wenn auf ein markiertes Objekt geklickt wurde: Nichts tun -> Verschieben!
        if (itemUnderMouse && itemUnderMouse->isSelected()) {
            return false; // -> Standard Qt Verschiebe-Logik greift!
        }

        // Sonst: Immer alte Auswahl verwerfen, da wir explizit eine neue Starten
        scene->clearSelection();

        // Neue Auswahl starten
        m_startPos = event->scenePos();
        m_currentPath = QPainterPath();

        if (m_config.lassoMode == LassoMode::Rectangle) {
            m_currentPath.addRect(QRectF(m_startPos, m_startPos));
        } else {
            m_currentPath.moveTo(m_startPos);
        }

        if (!m_selectionItem) {
            m_selectionItem = new QGraphicsPathItem();
            QPen p(Qt::black, 2, Qt::DashLine);
            p.setCosmetic(true);
            m_selectionItem->setPen(p);
            m_selectionItem->setZValue(9999);
            scene->addItem(m_selectionItem);
        }
        m_selectionItem->setPath(m_currentPath);
        m_selectionItem->show();

        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!m_selectionItem) return false;

        if (m_config.lassoMode == LassoMode::Rectangle) {
            QRectF rect(m_startPos, event->scenePos());
            rect = rect.normalized(); // FIX: Prevent degenerate shapes with negative dimensions
            if (m_config.aspectLock) {
                qreal w = rect.width();
                qreal h = rect.height();
                qreal size = qMax(qAbs(w), qAbs(h));
                w = (w < 0) ? -size : size;
                h = (h < 0) ? -size : size;
                rect = QRectF(m_startPos, QPointF(m_startPos.x() + w, m_startPos.y() + h));
            }
            m_currentPath = QPainterPath();
            m_currentPath.addRect(rect);
        } else {
            m_currentPath.lineTo(event->scenePos());
        }

        m_selectionItem->setPath(m_currentPath);
        scene->update();
        return true;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!m_selectionItem) return false;

        if (m_config.lassoMode == LassoMode::Freehand) {
            m_currentPath.closeSubpath();
            m_selectionItem->setPath(m_currentPath);
        }

        auto selectionMode = Qt::IntersectsItemShape;

        scene->setSelectionArea(m_currentPath, Qt::ReplaceSelection, selectionMode, QTransform());

        for (QGraphicsItem* item : scene->selectedItems()) {
            if (item->type() == RulerItem::Type) {
                item->setSelected(false);
            }
        }

        scene->removeItem(m_selectionItem);
        delete m_selectionItem;
        m_selectionItem = nullptr;

        scene->update();
        return true;
    }

private:
    QPointF m_startPos;
    QPainterPath m_currentPath;
    QGraphicsPathItem* m_selectionItem{nullptr};
};
