#pragma once
#include "AbstractTool.h"
#include "GraphCanvasItem.h"
#include "ToolManager.h"
#include <QElapsedTimer>
#include <QFont>
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
        case ShapeToolKind::Ellipse: {
            QPainterPath p;
            p.addEllipse(rn.normalized());
            return p;
        }
        case ShapeToolKind::Line:
        case ShapeToolKind::DashedLine: {
            QPainterPath p;
            p.moveTo(dragRect.topLeft());
            p.lineTo(dragRect.bottomRight());
            return p;
        }
        case ShapeToolKind::Arrow:
        case ShapeToolKind::DoubleArrow: {
            QPainterPath p;
            const QPointF a = dragRect.topLeft();
            const QPointF b = dragRect.bottomRight();
            p.moveTo(a);
            p.lineTo(b);
            QLineF stem(a, b);
            if (stem.length() < 1.0)
                return p;
            const qreal head = qBound(8.0, stem.length() * 0.22, 28.0);
            QLineF u = stem.unitVector();
            const QPointF dir(u.dx(), u.dy());
            const QPointF n(-dir.y(), dir.x());
            auto arrowHead = [&](const QPointF &tip, const QPointF &d) {
                p.moveTo(tip);
                p.lineTo(tip - d * head + n * head * 0.45);
                p.moveTo(tip);
                p.lineTo(tip - d * head - n * head * 0.45);
            };
            arrowHead(b, dir);
            if (cfg.shapeToolKind == ShapeToolKind::DoubleArrow)
                arrowHead(a, -dir);
            return p;
        }
        case ShapeToolKind::ElbowConnector: {
            QPainterPath p;
            const QPointF a = dragRect.topLeft();
            const QPointF b = dragRect.bottomRight();
            const QPointF mid(b.x(), a.y());
            p.moveTo(a);
            p.lineTo(mid);
            p.lineTo(b);
            return p;
        }
        case ShapeToolKind::RoundedRect: {
            QPainterPath p;
            const qreal r = qMin(14.0, qMin(rn.width(), rn.height()) * 0.18);
            p.addRoundedRect(rn, r, r);
            return p;
        }
        case ShapeToolKind::Triangle: {
            QPainterPath p;
            p.moveTo(rn.center().x(), rn.top());
            p.lineTo(rn.bottomRight());
            p.lineTo(rn.bottomLeft());
            p.closeSubpath();
            return p;
        }
        case ShapeToolKind::Polyline: {
            QPainterPath p;
            p.moveTo(rn.left(), rn.center().y());
            p.lineTo(rn.center().x(), rn.top() + rn.height() * 0.25);
            p.lineTo(rn.right(), rn.bottom());
            return p;
        }
        case ShapeToolKind::Brace: {
            QPainterPath p;
            const qreal x0 = rn.left();
            const qreal x1 = rn.right();
            const qreal y0 = rn.top();
            const qreal y1 = rn.bottom();
            const qreal ym = rn.center().y();
            const qreal mid = x0 + rn.width() * 0.55;
            p.moveTo(x1, y0);
            p.cubicTo(mid, y0, mid, ym - rn.height() * 0.08, x0, ym);
            p.cubicTo(mid, ym + rn.height() * 0.08, mid, y1, x1, y1);
            return p;
        }
        case ShapeToolKind::Axes2D:
        case ShapeToolKind::AxesPositive:
        case ShapeToolKind::ComplexPlane: {
            QPainterPath p;
            const bool posOnly = cfg.shapeToolKind == ShapeToolKind::AxesPositive;
            const bool complex = cfg.shapeToolKind == ShapeToolKind::ComplexPlane;
            const qreal mx = posOnly ? rn.left() : (rn.left() + rn.right()) * 0.5;
            const qreal my = posOnly ? rn.bottom() : (rn.top() + rn.bottom()) * 0.5;
            p.moveTo(rn.left(), my);
            p.lineTo(rn.right(), my);
            p.moveTo(mx, rn.top());
            p.lineTo(mx, rn.bottom());
            // Filled triangular arrowheads on +X / +Y.
            const qreal ah = qMax(8.0, qMin(rn.width(), rn.height()) * 0.055);
            QPainterPath tipX;
            tipX.moveTo(rn.right(), my);
            tipX.lineTo(rn.right() - ah, my - ah * 0.55);
            tipX.lineTo(rn.right() - ah, my + ah * 0.55);
            tipX.closeSubpath();
            p.addPath(tipX);
            QPainterPath tipY;
            tipY.moveTo(mx, rn.top());
            tipY.lineTo(mx - ah * 0.55, rn.top() + ah);
            tipY.lineTo(mx + ah * 0.55, rn.top() + ah);
            tipY.closeSubpath();
            p.addPath(tipY);
            const int nt = qBound(2, cfg.shapeAxisTicks, 12);
            const qreal dx = (rn.right() - mx) / qMax(1, nt - 1);
            const qreal dy = (my - rn.top()) / qMax(1, nt - 1);
            const qreal tickH = qMax(3.0, qMin(rn.width(), rn.height()) * 0.04);
            for (int i = 0; i < nt; ++i) {
                const qreal x = mx + i * dx;
                p.moveTo(x, my - tickH * 0.5);
                p.lineTo(x, my + tickH * 0.5);
            }
            for (int i = 0; i < nt; ++i) {
                const qreal y = my - i * dy;
                p.moveTo(mx - tickH * 0.5, y);
                p.lineTo(mx + tickH * 0.5, y);
            }
            QFont labelFont(QStringLiteral("Segoe UI"), 10, QFont::Normal, true);
            labelFont.setPixelSize(qMax(10, int(qMin(rn.width(), rn.height()) * 0.07)));
            if (complex) {
                // Open unit-circle arc (stroke only — avoid filled ellipse).
                const qreal rad = qMin(rn.width(), rn.height()) * 0.22;
                p.arcMoveTo(QRectF(mx - rad, my - rad, 2 * rad, 2 * rad), 0);
                p.arcTo(QRectF(mx - rad, my - rad, 2 * rad, 2 * rad), 0, 360);
                p.addText(QPointF(rn.right() - ah * 3.6, my + ah * 2.8),
                          labelFont, QStringLiteral("Re"));
                p.addText(QPointF(mx + ah * 1.4, rn.top() + ah * 2.2),
                          labelFont, QStringLiteral("j·Im"));
            } else {
                p.addText(QPointF(rn.right() - ah * 2.2, my + ah * 2.4),
                          labelFont, QStringLiteral("x"));
                p.addText(QPointF(mx + ah * 1.2, rn.top() + ah * 2.0),
                          labelFont, QStringLiteral("y"));
            }
            return p;
        }
        case ShapeToolKind::Axes3D: {
            QPainterPath p;
            const QPointF o = rn.center();
            const qreal L = qMin(rn.width(), rn.height()) * 0.42;
            // Isometric-ish axes: x right-down, y left-down, z up
            const QPointF xEnd(o.x() + L * 0.95, o.y() + L * 0.35);
            const QPointF yEnd(o.x() - L * 0.85, o.y() + L * 0.35);
            const QPointF zEnd(o.x(), o.y() - L);
            auto arrow = [&](QPointF a, QPointF b) {
                p.moveTo(a);
                p.lineTo(b);
                QLineF ln(a, b);
                const qreal ang = ln.angle();
                const qreal ah = qMax(7.0, L * 0.09);
                QLineF l1 = QLineF::fromPolar(ah, ang + 155);
                QLineF l2 = QLineF::fromPolar(ah, ang - 155);
                l1.translate(b);
                l2.translate(b);
                QPainterPath tip;
                tip.moveTo(b);
                tip.lineTo(l1.p2());
                tip.lineTo(l2.p2());
                tip.closeSubpath();
                p.addPath(tip);
            };
            arrow(o, xEnd);
            arrow(o, yEnd);
            arrow(o, zEnd);
            QFont labelFont(QStringLiteral("Segoe UI"), 10, QFont::Normal, true);
            labelFont.setPixelSize(qMax(10, int(L * 0.16)));
            p.addText(xEnd + QPointF(4, 10), labelFont, QStringLiteral("x"));
            p.addText(yEnd + QPointF(-12, 10), labelFont, QStringLiteral("y"));
            p.addText(zEnd + QPointF(6, -2), labelFont, QStringLiteral("z"));
            return p;
        }
        case ShapeToolKind::UnitCircle: {
            QPainterPath p;
            const QPointF c = rn.center();
            const qreal rad = qMin(rn.width(), rn.height()) * 0.42;
            // Open arc so axis brush fill does not paint a disc.
            p.arcMoveTo(QRectF(c.x() - rad, c.y() - rad, 2 * rad, 2 * rad), 0);
            p.arcTo(QRectF(c.x() - rad, c.y() - rad, 2 * rad, 2 * rad), 0, 360);
            p.moveTo(c.x() - rad * 1.15, c.y());
            p.lineTo(c.x() + rad * 1.15, c.y());
            p.moveTo(c.x(), c.y() - rad * 1.15);
            p.lineTo(c.x(), c.y() + rad * 1.15);
            // 45° marks
            for (int deg : {0, 45, 90, 135, 180, 225, 270, 315}) {
                const qreal a = qDegreesToRadians(qreal(deg));
                const QPointF o(c.x() + rad * std::cos(a), c.y() - rad * std::sin(a));
                const QPointF i(c.x() + rad * 0.9 * std::cos(a),
                               c.y() - rad * 0.9 * std::sin(a));
                p.moveTo(i);
                p.lineTo(o);
            }
            return p;
        }
        case ShapeToolKind::StampResistor: {
            QPainterPath p;
            const qreal y = rn.center().y();
            const qreal x0 = rn.left();
            const qreal x1 = rn.right();
            const qreal mid = rn.center().x();
            const qreal zigW = rn.width() * 0.5;
            const qreal zigH = rn.height() * 0.28;
            p.moveTo(x0, y);
            p.lineTo(mid - zigW * 0.5, y);
            const int segs = 6;
            for (int i = 0; i < segs; ++i) {
                const qreal t0 = qreal(i) / segs;
                const qreal t1 = qreal(i + 1) / segs;
                const qreal xa = mid - zigW * 0.5 + zigW * t0;
                const qreal xb = mid - zigW * 0.5 + zigW * t1;
                const qreal ya = (i % 2 == 0) ? y - zigH : y + zigH;
                p.lineTo(xa, ya);
                p.lineTo(xb, y);
            }
            p.lineTo(x1, y);
            return p;
        }
        case ShapeToolKind::StampGnd: {
            QPainterPath p;
            const qreal cx = rn.center().x();
            const qreal y0 = rn.top() + rn.height() * 0.2;
            const qreal y1 = rn.center().y();
            p.moveTo(cx, y0);
            p.lineTo(cx, y1);
            const qreal w = rn.width() * 0.45;
            p.moveTo(cx - w, y1);
            p.lineTo(cx + w, y1);
            p.moveTo(cx - w * 0.65, y1 + rn.height() * 0.12);
            p.lineTo(cx + w * 0.65, y1 + rn.height() * 0.12);
            p.moveTo(cx - w * 0.35, y1 + rn.height() * 0.24);
            p.lineTo(cx + w * 0.35, y1 + rn.height() * 0.24);
            return p;
        }
        case ShapeToolKind::StampCapacitor: {
            QPainterPath p;
            const qreal y = rn.center().y();
            const qreal gap = rn.width() * 0.08;
            const qreal plateH = rn.height() * 0.45;
            p.moveTo(rn.left(), y);
            p.lineTo(rn.center().x() - gap, y);
            p.moveTo(rn.center().x() - gap, y - plateH);
            p.lineTo(rn.center().x() - gap, y + plateH);
            p.moveTo(rn.center().x() + gap, y - plateH);
            p.lineTo(rn.center().x() + gap, y + plateH);
            p.moveTo(rn.center().x() + gap, y);
            p.lineTo(rn.right(), y);
            return p;
        }
        case ShapeToolKind::StampInductor: {
            QPainterPath p;
            const qreal y = rn.center().y();
            const qreal x0 = rn.left() + rn.width() * 0.12;
            const qreal x1 = rn.right() - rn.width() * 0.12;
            p.moveTo(rn.left(), y);
            p.lineTo(x0, y);
            const int loops = 4;
            const qreal loopW = (x1 - x0) / loops;
            const qreal r = qMin(loopW * 0.45, rn.height() * 0.28);
            for (int i = 0; i < loops; ++i) {
                const qreal cx = x0 + loopW * (i + 0.5);
                p.moveTo(cx - r, y);
                p.arcTo(cx - r, y - r, 2 * r, 2 * r, 180, -180);
            }
            p.moveTo(x1, y);
            p.lineTo(rn.right(), y);
            return p;
        }
        case ShapeToolKind::StampVoltageSrc: {
            QPainterPath p;
            const QPointF c = rn.center();
            const qreal r = qMin(rn.width(), rn.height()) * 0.32;
            p.addEllipse(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
            p.moveTo(rn.left(), c.y());
            p.lineTo(c.x() - r, c.y());
            p.moveTo(c.x() + r, c.y());
            p.lineTo(rn.right(), c.y());
            // + / −
            p.moveTo(c.x() - r * 0.35, c.y() - r * 0.35);
            p.lineTo(c.x() + r * 0.05, c.y() - r * 0.35);
            p.moveTo(c.x() - r * 0.15, c.y() - r * 0.5);
            p.lineTo(c.x() - r * 0.15, c.y() - r * 0.2);
            p.moveTo(c.x() - r * 0.25, c.y() + r * 0.35);
            p.lineTo(c.x() + r * 0.05, c.y() + r * 0.35);
            return p;
        }
        case ShapeToolKind::StampCurrentSrc: {
            QPainterPath p;
            const QPointF c = rn.center();
            const qreal r = qMin(rn.width(), rn.height()) * 0.32;
            p.addEllipse(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
            p.moveTo(c.x(), rn.top());
            p.lineTo(c.x(), c.y() - r);
            p.moveTo(c.x(), c.y() + r);
            p.lineTo(c.x(), rn.bottom());
            p.moveTo(c.x(), c.y() + r * 0.45);
            p.lineTo(c.x(), c.y() - r * 0.45);
            p.moveTo(c.x(), c.y() - r * 0.45);
            p.lineTo(c.x() - r * 0.22, c.y() - r * 0.15);
            p.moveTo(c.x(), c.y() - r * 0.45);
            p.lineTo(c.x() + r * 0.22, c.y() - r * 0.15);
            return p;
        }
        case ShapeToolKind::StampNode: {
            QPainterPath p;
            const QPointF c = rn.center();
            const qreal r = qMin(rn.width(), rn.height()) * 0.18;
            p.addEllipse(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
            return p;
        }
        case ShapeToolKind::FlowDecision: {
            QPainterPath p;
            const QPointF c = rn.center();
            p.moveTo(c.x(), rn.top());
            p.lineTo(rn.right(), c.y());
            p.lineTo(c.x(), rn.bottom());
            p.lineTo(rn.left(), c.y());
            p.closeSubpath();
            return p;
        }
        case ShapeToolKind::FlowProcess: {
            QPainterPath p;
            const qreal rad = qMin(rn.width(), rn.height()) * 0.08;
            p.addRoundedRect(rn, rad, rad);
            return p;
        }
        case ShapeToolKind::FlowIO: {
            QPainterPath p;
            const qreal skew = rn.width() * 0.18;
            p.moveTo(rn.left() + skew, rn.top());
            p.lineTo(rn.right(), rn.top());
            p.lineTo(rn.right() - skew, rn.bottom());
            p.lineTo(rn.left(), rn.bottom());
            p.closeSubpath();
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
        const Qt::PenStyle style =
            m_config.shapeToolKind == ShapeToolKind::DashedLine ? Qt::DashLine
                                                                : Qt::SolidLine;
        pathItem->setPen(QPen(pc, m_config.penWidth, style, Qt::RoundCap,
                              Qt::RoundJoin));
        const auto kind = m_config.shapeToolKind;
        const bool axisFamily =
            kind == ShapeToolKind::Axes2D || kind == ShapeToolKind::AxesPositive ||
            kind == ShapeToolKind::ComplexPlane || kind == ShapeToolKind::Axes3D;
        if (axisFamily) {
          // Fill closed arrowhead triangles (and text glyphs); open arcs stay stroke-only.
          pathItem->setBrush(pc);
        } else if (m_config.fillColor.isValid() && m_config.fillColor.alpha() > 0) {
          QColor fc = m_config.fillColor;
          fc.setAlphaF(qBound(0.0, m_config.opacity, 1.0) * (fc.alphaF()));
          pathItem->setBrush(fc);
        } else {
          pathItem->setBrush(Qt::NoBrush);
        }
        pathItem->setZValue(5);
        pathItem->setFlags(QGraphicsItem::ItemIsSelectable |
                           QGraphicsItem::ItemIsMovable);
        pathItem->setData(0, QStringLiteral("shape_item"));

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
