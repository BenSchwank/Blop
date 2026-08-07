#pragma once
#include "AbstractTool.h"
#include "GraphCanvasItem.h"
#include "ShapePath.h"
#include "ToolManager.h"
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QLineF>
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include "math/MathExpressionParser.h"
#include "math/MathEvaluator.h"
#include "math/NumericAnalysis.h"

class ShapeTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Shape; }
    QString name() const override { return "Formen"; }
    QString iconName() const override { return "shape"; }

    static QPainterPath buildShapePath(const QRectF& dragRect, const ToolConfig& cfg) {
        return blopBuildShapePath(dragRect, cfg);
    }

    void setStrokeSceneForTablet(QGraphicsScene* scene) override {
        m_sceneRef = scene;
    }

    bool handleTabletEvent(QTabletEvent* event, const QPointF& scenePos) override {
        if (event->type() == QEvent::TabletPress) {
            QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMousePress);
            mouseEvent.setScenePos(scenePos);
            return handleMousePress(&mouseEvent, m_sceneRef);
        } else if (event->type() == QEvent::TabletMove) {
            QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseMove);
            mouseEvent.setScenePos(scenePos);
            return handleMouseMove(&mouseEvent, m_sceneRef);
        } else if (event->type() == QEvent::TabletRelease) {
            QGraphicsSceneMouseEvent mouseEvent(QEvent::GraphicsSceneMouseRelease);
            mouseEvent.setScenePos(scenePos);
            return handleMouseRelease(&mouseEvent, m_sceneRef);
        }
        return false;
    }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene)
            return false;

        m_startPos = event->scenePos();
        m_lastMotionPos = m_startPos;
        m_pressTimer.restart();
        m_longPressLock = false;

        auto* pathItem = new QGraphicsPathItem();
        QColor pc = m_config.penColor;
        pc.setAlphaF(m_config.opacity);
        pathItem->setPen(QPen(pc, m_config.penWidth, Qt::SolidLine, Qt::RoundCap,
                              Qt::RoundJoin));
        if (m_config.fillColor.isValid() && m_config.fillColor.alpha() > 0) {
          QColor fc = m_config.fillColor;
          fc.setAlphaF(qBound(0.0, m_config.opacity, 1.0) * (fc.alphaF()));
          pathItem->setBrush(fc);
        } else {
          pathItem->setBrush(Qt::NoBrush);
        }
        pathItem->setZValue(5);
        pathItem->setData(0, QStringLiteral("shape"));
        pathItem->setData(1, int(m_config.shapeToolKind));
        pathItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
        pathItem->setFlag(QGraphicsItem::ItemIsMovable, true);

        m_currentShape = pathItem;
        scene->addItem(m_currentShape);
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        Q_UNUSED(scene);
        if (!m_currentShape)
            return false;

        if (!m_longPressLock && m_pressTimer.isValid()) {
            const qreal moved = QLineF(m_lastMotionPos, event->scenePos()).length();
            if (moved > 1.8) {
                m_lastMotionPos = event->scenePos();
                m_pressTimer.restart();
            }
            const bool heldLongEnough = m_pressTimer.elapsed() >= 360;
            const bool draggedEnough = QLineF(m_startPos, event->scenePos()).length() >= 8.0;
            // Square lock for rectangles; circle already inscribes — lock aspect
            // so the drag rubber-band matches the final circle diameter.
            if (heldLongEnough && draggedEnough &&
                (m_config.shapeToolKind == ShapeToolKind::Rectangle ||
                 m_config.shapeToolKind == ShapeToolKind::Circle))
                m_longPressLock = true;
        }

        QPointF endPos = event->scenePos();
        if (m_longPressLock || m_config.shapeToolKind == ShapeToolKind::Circle) {
            // Circle: always equal aspect from the drag start (avoids a
            // "chopped square" rubber-band that users mistake for the shape).
            const QPointF delta = endPos - m_startPos;
            const qreal side = qMax(qAbs(delta.x()), qAbs(delta.y()));
            const qreal sx = (delta.x() >= 0) ? 1.0 : -1.0;
            const qreal sy = (delta.y() >= 0) ? 1.0 : -1.0;
            endPos = m_startPos + QPointF(side * sx, side * sy);
        }

        const QRectF R(m_startPos, endPos);
        const QPainterPath path = buildShapePath(R, m_config);
        static_cast<QGraphicsPathItem*>(m_currentShape)->setPath(path);
        return true;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        Q_UNUSED(event);
        if (!scene) {
            m_currentShape = nullptr;
            m_longPressLock = false;
            return false;
        }
        if (m_currentShape) {
            const QPainterPath p = static_cast<QGraphicsPathItem*>(m_currentShape)->path();
            QRectF bb = p.boundingRect();
            if (bb.width() < 5.0 && bb.height() < 5.0) {
                scene->removeItem(m_currentShape);
                delete m_currentShape;
            } else {
                if (m_config.shapeToolKind == ShapeToolKind::CoordinateGraph) {
                    scene->removeItem(m_currentShape);
                    delete m_currentShape;
                    m_currentShape = nullptr;
                    auto* graphItem = new GraphCanvasItem(bb);
                    GraphObject d;
                    d.rect = bb;
                    d.selectedFunction = m_config.graphSelectedFunction;
                    d.xMin = m_config.graphXMin;
                    d.xMax = m_config.graphXMax;
                    d.yMin = m_config.graphYMin;
                    d.yMax = m_config.graphYMax;
                    // New graph objects start empty; functions are added via floating handwriting panel.
                    d.functions.clear();
                    d.selectedFunction = -1;
                    graphItem->fromData(d);
                    graphItem->setZValue(4.0);
                    scene->addItem(graphItem);
                    m_lastCompletedItem = graphItem;
                } else {
                    m_lastCompletedItem = m_currentShape;
                }
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
    QGraphicsScene* m_sceneRef{nullptr};
};
