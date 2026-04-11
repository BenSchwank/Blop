#pragma once
#include "AbstractTool.h"
#include "GraphCanvasItem.h"
#include "ToolManager.h"
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QLineF>
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QtMath>
#include "math/MathExpressionParser.h"
#include "math/MathEvaluator.h"
#include "math/NumericAnalysis.h"
#include <cmath>

class ShapeTool : public AbstractTool {
    Q_OBJECT
public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Shape; }
    QString name() const override { return "Formen"; }
    QString iconName() const override { return "shape"; }

    static QPainterPath buildShapePath(const QRectF& dragRect, const ToolConfig& cfg) {
        const QRectF rn = dragRect.normalized();
        if (rn.width() < 1.0 && rn.height() < 1.0)
            return {};

        switch (cfg.shapeToolKind) {
        case ShapeToolKind::Rectangle: {
            QPainterPath p;
            p.addRect(rn);
            return p;
        }
        case ShapeToolKind::Circle: {
            const QPointF c = rn.center();
            const qreal rad = qMin(rn.width(), rn.height()) * 0.5;
            QPainterPath p;
            p.addEllipse(QRectF(c.x() - rad, c.y() - rad, 2.0 * rad, 2.0 * rad));
            return p;
        }
        case ShapeToolKind::Axes2D: {
            QPainterPath p;
            const qreal mx = (rn.left() + rn.right()) * 0.5;
            const qreal my = (rn.top() + rn.bottom()) * 0.5;
            p.moveTo(rn.left(), my);
            p.lineTo(rn.right(), my);
            p.moveTo(mx, rn.top());
            p.lineTo(mx, rn.bottom());
            const int nt = qBound(2, cfg.shapeAxisTicks, 12);
            const qreal dx = rn.width() / qMax(1, nt - 1);
            const qreal dy = rn.height() / qMax(1, nt - 1);
            const qreal tickH = qMax(3.0, qMin(rn.width(), rn.height()) * 0.04);
            for (int i = 0; i < nt; ++i) {
                const qreal x = rn.left() + i * dx;
                p.moveTo(x, my - tickH * 0.5);
                p.lineTo(x, my + tickH * 0.5);
            }
            for (int i = 0; i < nt; ++i) {
                const qreal y = rn.top() + i * dy;
                p.moveTo(mx - tickH * 0.5, y);
                p.lineTo(mx + tickH * 0.5, y);
            }
            return p;
        }
        case ShapeToolKind::SineGraph: {
            QPainterPath p;
            const qreal cy = rn.center().y();
            const qreal a = qBound(0.01, cfg.shapeMathA, 2.0);
            const qreal b = qBound(0.05, cfg.shapeMathB, 12.0);
            const qreal c = qBound(-12.57, cfg.shapeMathC, 12.57);
            const qreal d = qBound(-1.5, cfg.shapeMathD, 1.5);
            const qreal w = rn.width();
            if (cfg.shapeSineFixedParams) {
                constexpr qreal kLenRef = 100.0; ///< b = volle Perioden pro kLenRef Szeneneinheiten horizontal
                constexpr qreal kAmpRef = 45.0;  ///< a, d als Faktoren dieser Amplitude (px)
                const qreal omega = b * (2.0 * M_PI) / kLenRef;
                const int N = qBound(96, static_cast<int>(w / 3.0) + 1, 640);
                for (int i = 0; i <= N; ++i) {
                    const qreal t = static_cast<qreal>(i) / static_cast<qreal>(N);
                    const qreal x = rn.left() + t * w;
                    const qreal ang = omega * (x - rn.left()) + c;
                    const qreal y = cy - (a * kAmpRef * std::sin(ang) + d * kAmpRef);
                    if (i == 0)
                        p.moveTo(x, y);
                    else
                        p.lineTo(x, y);
                }
            } else {
                const int N = 96;
                const qreal halfH = rn.height() * 0.5;
                for (int i = 0; i <= N; ++i) {
                    const qreal t = static_cast<qreal>(i) / static_cast<qreal>(N);
                    const qreal x = rn.left() + t * w;
                    const qreal ang = b * (2.0 * M_PI) * t + c;
                    const qreal y = cy - halfH * (a * std::sin(ang) + d);
                    if (i == 0)
                        p.moveTo(x, y);
                    else
                        p.lineTo(x, y);
                }
            }
            return p;
        }
        case ShapeToolKind::CoordinateGraph: {
            QPainterPath p;
            p.addRect(rn);
            const qreal x0 = rn.left() + (0.0 - cfg.graphXMin) / qMax(1e-6, (cfg.graphXMax - cfg.graphXMin)) * rn.width();
            const qreal y0 = rn.bottom() - (0.0 - cfg.graphYMin) / qMax(1e-6, (cfg.graphYMax - cfg.graphYMin)) * rn.height();
            p.moveTo(rn.left(), y0); p.lineTo(rn.right(), y0);
            p.moveTo(x0, rn.top()); p.lineTo(x0, rn.bottom());

            auto mapX = [&](double x) { return rn.left() + (x - cfg.graphXMin) / qMax(1e-6, (cfg.graphXMax - cfg.graphXMin)) * rn.width(); };
            auto mapY = [&](double y) { return rn.bottom() - (y - cfg.graphYMin) / qMax(1e-6, (cfg.graphYMax - cfg.graphYMin)) * rn.height(); };

            // Graph starts empty; curves are added interactively on the graph item.
            return p;
        }
        default:
            return {};
        }
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
        pathItem->setPen(QPen(pc, m_config.penWidth));
        pathItem->setBrush(Qt::NoBrush);
        pathItem->setZValue(5);

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
            if (heldLongEnough && draggedEnough && m_config.shapeToolKind == ShapeToolKind::Rectangle)
                m_longPressLock = true;
        }

        QPointF endPos = event->scenePos();
        if (m_longPressLock) {
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
};
