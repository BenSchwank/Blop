#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include "StrokeItem.h"
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QLineF>
#include <QRectF>
#include <QVector>
#include <QtMath>
#include <cmath>
#include <QPen>
#include <QList>
#include <QPointF>
#include <QTabletEvent>
#include <QTimer>
#include <algorithm>
#include <array>

class AbstractStrokeTool : public AbstractTool {
    Q_OBJECT

public:
    explicit AbstractStrokeTool(QObject* parent = nullptr)
        : AbstractTool(parent) {
        m_holdStillTimer = new QTimer(this);
        m_holdStillTimer->setSingleShot(true);
        m_holdStillTimer->setInterval(qBound(200, m_config.holdStillDelayMs, 900));
        connect(m_holdStillTimer, &QTimer::timeout, this,
                &AbstractStrokeTool::onHoldStillTimeout);
    }

    void setConfig(const ToolConfig& config) override {
        AbstractTool::setConfig(config);
        if (m_holdStillTimer)
            m_holdStillTimer->setInterval(qBound(200, config.holdStillDelayMs, 900));
    }

    int holdStillMs() const { return qBound(200, m_config.holdStillDelayMs, 900); }

    // We store the last pressure so that mouseMove events (if synthesized) have a fallback,
    // though native TabletEvents will use their own pressure.
    qreal m_lastPressure{1.0};

    /// Call from the view before each tablet gesture event so strokes are added to the scene
    /// (tablet callbacks pass no QGraphicsScene* to handleMoveOrPress).
    void setStrokeSceneForTablet(QGraphicsScene* scene) override {
        if (scene)
            m_sceneRef = scene;
    }

    bool handleTabletEvent(QTabletEvent* event, const QPointF& scenePos) override {
        // Tablet events give us robust pressure [0.0 - 1.0]
        m_lastPressure = event->pressure();

        QGraphicsScene* sc = m_sceneRef;
        if (event->type() == QEvent::TabletPress) {
            handleMoveOrPress(scenePos, true, sc);
            return true;
        } else if (event->type() == QEvent::TabletMove) {
            handleMoveOrPress(scenePos, false, sc);
            return true;
        } else if (event->type() == QEvent::TabletRelease) {
            return handleMouseRelease(nullptr, nullptr);
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
        if (scene)
            m_sceneRef = scene;
        return handleMoveOrPress(event->scenePos(), false, scene);
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_currentItem) {
            if (m_holdStillTimer)
                m_holdStillTimer->stop();
            m_currentItem->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
            m_lastCompletedItem = m_currentItem;
            m_currentItem = nullptr;
            m_pointsBuffer.clear();
            m_longPressStraightMode = false;
            m_holdShapeCommitted = false;
            m_holdShapeKind = HoldShapeKind::None;
            m_holdShapeConfidence = 0.0;
            m_holdTriValid = false;
            m_holdTriDragIndex = -1;
            m_holdArcValid = false;
            m_isSnapping = false;
            m_rulerRef = nullptr;
            m_sceneRef = nullptr;
            emit contentModified();
            return true;
        }
        return false;
    }

private slots:
    void onHoldStillTimeout() {
        if (!m_currentItem || m_holdShapeCommitted || m_pointsBuffer.size() < 6)
            return;
        if (QLineF(m_lastKnownScenePos, m_pressStartPos).length() < 5.0)
            return;
        m_longPressStraightMode = true;
        if (!commitHoldShapeIfReady(m_lastKnownScenePos)) {
            m_holdShapeKind = HoldShapeKind::None;
            m_holdShapeConfidence = 0.0;
            m_longPressStraightMode = false;
            rebuildFreehandPathFromPoints();
        }
    }

private:
    QTimer* m_holdStillTimer{nullptr};
    QPointF m_lastKnownScenePos;

protected:
    enum class HoldShapeKind { None, Line, Arc, Rectangle, Triangle, Circle };

    virtual QPen createPen() const = 0;
    virtual qreal getZValue() const { return 0; }
    virtual StrokeItem::StrokeStyle strokeStyle() const { return StrokeItem::Normal; }

    QGraphicsPathItem* m_currentItem{nullptr};
    QPainterPath m_currentPath;
    QList<StrokePoint> m_pointsBuffer;
    QElapsedTimer m_lastMotionTimer;
    QPointF m_lastMotionPos;
    QPointF m_pressStartPos;
    bool m_longPressStraightMode{false};
    bool m_holdShapeCommitted{false};
    QPointF m_holdLastScenePos;
    HoldShapeKind m_holdShapeKind{HoldShapeKind::None};
    qreal m_holdShapeConfidence{0.0};
    bool m_isSnapping{false};
    RulerItem* m_rulerRef{nullptr};
    QGraphicsScene* m_sceneRef{nullptr};

    QPointF m_holdStrokeStart;
    QPointF m_holdStrokeEnd;
    QPointF m_holdCircleCenter;
    QPointF m_holdRectAnchorCorner;
    bool m_holdIsSquare{false};
    std::array<QPointF, 3> m_holdTriangleVerts{};
    bool m_holdTriValid{false};
    int m_holdTriDragIndex{-1}; ///< Vertex that follows the cursor (fixed after commit; avoids flip while dragging)
    QPointF m_holdArcStart;
    qreal m_arcHOverChord{0};
    bool m_holdArcValid{false};

    bool handleMoveOrPress(QPointF scenePos, bool isPress, QGraphicsScene* scene) {
        if (isPress) {
            m_currentPath = QPainterPath();
            QPointF startPos = scenePos;
            m_pressStartPos = scenePos;
            m_lastMotionPos = scenePos;
            m_lastMotionTimer.restart();
            m_longPressStraightMode = false;
            m_holdShapeCommitted = false;
            m_holdShapeKind = HoldShapeKind::None;
            m_holdShapeConfidence = 0.0;
            m_holdTriValid = false;
            m_holdTriDragIndex = -1;
            m_holdArcValid = false;
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
            m_lastKnownScenePos = startPos;
            if (m_holdStillTimer)
                m_holdStillTimer->start(holdStillMs());
            return true;
        } else {
            if (m_currentItem) {
                m_lastKnownScenePos = scenePos;
                QPointF newPos = scenePos;
                if (!m_longPressStraightMode && m_lastMotionTimer.isValid()) {
                    const qreal movementSinceLastSample = QLineF(scenePos, m_lastMotionPos).length();
                    if (movementSinceLastSample > 4.0) {
                        m_lastMotionPos = scenePos;
                        m_lastMotionTimer.restart();
                        if (m_holdStillTimer)
                            m_holdStillTimer->start(holdStillMs());
                    }
                    const bool heldLongEnough = m_lastMotionTimer.elapsed() >= holdStillMs();
                    const bool draggedEnough = QLineF(scenePos, m_pressStartPos).length() >= 5.0;
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
                    if (m_holdShapeCommitted) {
                        adjustHoldShape(scenePos);
                        return true;
                    }
                    if (commitHoldShapeIfReady(scenePos))
                        return true;
                    m_holdShapeKind = HoldShapeKind::None;
                    m_holdShapeConfidence = 0.0;
                    m_longPressStraightMode = false;
                    rebuildFreehandPathFromPoints();
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

    bool commitHoldShapeIfReady(QPointF scenePos) {
        if (!m_currentItem || m_pointsBuffer.size() < 6)
            return false;
        if (m_holdShapeCommitted)
            return true;
        auto* typedItem = static_cast<StrokeItem*>(m_currentItem);
        QPainterPath fittedPath;
        QVector<StrokePoint> fittedPoints;
        qreal confidence = 0.0;
        bool squareLike = false;
        QPointF arcMidCaptured;
        HoldShapeKind kind = fitHoldShape(m_pointsBuffer, fittedPath, fittedPoints, confidence, &squareLike,
                                          &arcMidCaptured);
        const qreal thresh = holdConfidenceThreshold(kind);
        if (kind == HoldShapeKind::None || confidence < thresh || fittedPoints.size() < 2)
            return false;
        m_holdShapeKind = kind;
        m_holdShapeConfidence = confidence;
        m_currentPath = fittedPath;
        m_currentItem->setPath(m_currentPath);
        typedItem->setPoints(fittedPoints);
        captureHoldAnchorsAfterFirstFit(kind, fittedPoints, squareLike, arcMidCaptured);
        m_holdShapeCommitted = true;
        if (m_holdStillTimer)
            m_holdStillTimer->stop();
        adjustHoldShape(scenePos);
        return true;
    }

    void adjustHoldShape(QPointF scenePos) {
        if (!m_currentItem)
            return;
        const qreal pr = m_lastPressure;
        switch (m_holdShapeKind) {
        case HoldShapeKind::Line:
            rebuildHoldLine(scenePos, pr);
            break;
        case HoldShapeKind::Rectangle:
            rebuildHoldRect(scenePos, pr);
            break;
        case HoldShapeKind::Triangle:
            if (m_holdTriValid)
                rebuildHoldTriangle(scenePos, pr);
            break;
        case HoldShapeKind::Circle:
            rebuildHoldCircle(scenePos, pr);
            break;
        case HoldShapeKind::Arc:
            if (m_holdArcValid)
                rebuildHoldArc(scenePos, pr);
            break;
        default:
            break;
        }
        m_holdLastScenePos = scenePos;
    }

    void captureHoldAnchorsAfterFirstFit(HoldShapeKind kind, const QVector<StrokePoint>& fitted,
                                         bool squareLike, const QPointF& arcMid) {
        m_holdStrokeStart = m_pointsBuffer.first().pos;
        m_holdStrokeEnd = m_pointsBuffer.last().pos;
        m_holdTriValid = false;
        m_holdArcValid = false;
        switch (kind) {
        case HoldShapeKind::Line:
            break;
        case HoldShapeKind::Rectangle: {
            m_holdIsSquare = squareLike;
            const QPointF c[4] = {fitted[0].pos, fitted[1].pos, fitted[2].pos, fitted[3].pos};
            qreal bestD = -1.0;
            int bi = 0;
            // Fixed corner = farthest from where the stroke ended (opposite side); cursor drags the near side.
            for (int i = 0; i < 4; ++i) {
                const qreal d = QLineF(c[i], m_holdStrokeEnd).length();
                if (d > bestD) {
                    bestD = d;
                    bi = i;
                }
            }
            m_holdRectAnchorCorner = c[bi];
            break;
        }
        case HoldShapeKind::Triangle: {
            m_holdTriangleVerts[0] = fitted[0].pos;
            m_holdTriangleVerts[1] = fitted[1].pos;
            m_holdTriangleVerts[2] = fitted[2].pos;
            m_holdTriDragIndex = 0;
            qreal bd = 1e100;
            for (int i = 0; i < 3; ++i) {
                const qreal d = QLineF(m_holdTriangleVerts[i], m_holdStrokeEnd).length();
                if (d < bd) {
                    bd = d;
                    m_holdTriDragIndex = i;
                }
            }
            m_holdTriValid = true;
            break;
        }
        case HoldShapeKind::Circle: {
            QPointF sum(0.0, 0.0);
            for (const auto& sp : fitted) {
                sum += sp.pos;
            }
            m_holdCircleCenter = sum / fitted.size();
            break;
        }
        case HoldShapeKind::Arc: {
            const QPointF p0 = fitted.first().pos;
            const QPointF p2 = fitted.last().pos;
            m_holdArcStart = p0;
            const QPointF M((p0.x() + p2.x()) * 0.5, (p0.y() + p2.y()) * 0.5);
            const qreal len = QLineF(p0, p2).length();
            if (len > 1e-6) {
                QPointF perp(-(p2.y() - p0.y()) / len, (p2.x() - p0.x()) / len);
                const QPointF am = arcMid - M;
                m_arcHOverChord = (am.x() * perp.x() + am.y() * perp.y()) / len;
                m_holdArcValid = true;
            }
            break;
        }
        default:
            break;
        }
    }

    void rebuildHoldLine(const QPointF& end, qreal pressure) {
        QVector<StrokePoint> pts;
        pts.append({m_holdStrokeStart, pressure});
        pts.append({end, pressure});
        m_currentPath = QPainterPath();
        m_currentPath.moveTo(m_holdStrokeStart);
        m_currentPath.lineTo(end);
        m_currentItem->setPath(m_currentPath);
        static_cast<StrokeItem*>(m_currentItem)->setPoints(pts);
    }

    void rebuildHoldRect(const QPointF& oppositeCorner, qreal pressure) {
        QPointF a = m_holdRectAnchorCorner;
        QPointF b = oppositeCorner;
        if (m_holdIsSquare) {
            const qreal sx = (b.x() >= a.x()) ? 1.0 : -1.0;
            const qreal sy = (b.y() >= a.y()) ? 1.0 : -1.0;
            const qreal side = qMax(qMax(std::abs(b.x() - a.x()), std::abs(b.y() - a.y())), 1.0);
            b = QPointF(a.x() + sx * side, a.y() + sy * side);
        }
        const QRectF r = QRectF(a, b).normalized();
        const QVector<QPointF> poly{r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft(), r.topLeft()};
        m_currentPath = QPainterPath();
        m_currentPath.moveTo(poly.first());
        QVector<StrokePoint> pts;
        pts.append({poly.first(), pressure});
        for (int i = 1; i < poly.size(); ++i) {
            m_currentPath.lineTo(poly[i]);
            pts.append({poly[i], pressure});
        }
        m_currentItem->setPath(m_currentPath);
        static_cast<StrokeItem*>(m_currentItem)->setPoints(pts);
    }

    void rebuildHoldTriangle(const QPointF& cursor, qreal pressure) {
        if (m_holdTriDragIndex < 0 || m_holdTriDragIndex > 2)
            return;
        m_holdTriangleVerts[m_holdTriDragIndex] = cursor;
        const QVector<QPointF> poly{m_holdTriangleVerts[0], m_holdTriangleVerts[1], m_holdTriangleVerts[2],
                                      m_holdTriangleVerts[0]};
        m_currentPath = QPainterPath();
        m_currentPath.moveTo(poly.first());
        QVector<StrokePoint> pts;
        pts.append({poly.first(), pressure});
        for (int i = 1; i < poly.size(); ++i) {
            m_currentPath.lineTo(poly[i]);
            pts.append({poly[i], pressure});
        }
        m_currentItem->setPath(m_currentPath);
        static_cast<StrokeItem*>(m_currentItem)->setPoints(pts);
    }

    void rebuildHoldCircle(const QPointF& cursor, qreal pressure) {
        qreal r = QLineF(m_holdCircleCenter, cursor).length();
        r = qMax(r, 1.0);
        const QRectF box(m_holdCircleCenter.x() - r, m_holdCircleCenter.y() - r, 2.0 * r, 2.0 * r);
        m_currentPath = QPainterPath();
        m_currentPath.addEllipse(box);
        QVector<StrokePoint> pts;
        const int seg = 40;
        for (int i = 0; i <= seg; ++i) {
            const qreal t = (2.0 * M_PI * i) / seg;
            const QPointF p(m_holdCircleCenter.x() + r * std::cos(t), m_holdCircleCenter.y() + r * std::sin(t));
            pts.append({p, pressure});
        }
        m_currentItem->setPath(m_currentPath);
        static_cast<StrokeItem*>(m_currentItem)->setPoints(pts);
    }

    void rebuildHoldArc(const QPointF& end, qreal pressure) {
        const QPointF p0 = m_holdArcStart;
        const QPointF p2 = end;
        const QPointF M((p0.x() + p2.x()) * 0.5, (p0.y() + p2.y()) * 0.5);
        const qreal len = QLineF(p0, p2).length();
        if (len < 1e-4)
            return;
        QPointF perp(-(p2.y() - p0.y()) / len, (p2.x() - p0.x()) / len);
        const QPointF midCtrl = M + perp * (m_arcHOverChord * len);
        m_currentPath = QPainterPath();
        m_currentPath.moveTo(p0);
        m_currentPath.quadTo(midCtrl, p2);
        QVector<StrokePoint> pts;
        const int seg = 24;
        for (int i = 0; i <= seg; ++i) {
            const qreal t = static_cast<qreal>(i) / seg;
            const qreal u = 1.0 - t;
            const QPointF p = (u * u) * p0 + (2.0 * u * t) * midCtrl + (t * t) * p2;
            pts.append({p, pressure});
        }
        m_currentItem->setPath(m_currentPath);
        static_cast<StrokeItem*>(m_currentItem)->setPoints(pts);
    }

    RulerItem* findRuler(QGraphicsScene* scene) {
        if (!scene) return nullptr;
        for (QGraphicsItem* item : scene->items()) {
            if (item->type() == RulerItem::Type) return static_cast<RulerItem*>(item);
        }
        return nullptr;
    }

    static qreal clamp01(qreal v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }

    static qreal holdConfidenceThreshold(HoldShapeKind k) {
        if (k == HoldShapeKind::Line)
            return 0.56;
        if (k == HoldShapeKind::Circle)
            return 0.38;
        return 0.43;
    }

    static qreal distancePointToLine(const QPointF& p, const QPointF& a, const QPointF& b) {
        const qreal dx = b.x() - a.x();
        const qreal dy = b.y() - a.y();
        const qreal len2 = dx * dx + dy * dy;
        if (len2 < 1e-6) return QLineF(p, a).length();
        const qreal t = ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / len2;
        const qreal tt = qBound(0.0, t, 1.0);
        const QPointF proj(a.x() + tt * dx, a.y() + tt * dy);
        return QLineF(p, proj).length();
    }

    static QVector<QPointF> subsampleStrokePositions(const QList<StrokePoint>& pts, int maxCount) {
        QVector<QPointF> out;
        if (pts.isEmpty())
            return out;
        if (pts.size() <= maxCount) {
            for (const auto& sp : pts)
                out.append(sp.pos);
            return out;
        }
        const int step = qMax(1, pts.size() / maxCount);
        for (int i = 0; i < pts.size(); i += step)
            out.append(pts[i].pos);
        if (QLineF(out.last(), pts.last().pos).length() > 0.5)
            out.append(pts.last().pos);
        return out;
    }

    static void douglasPeuckerRange(const QVector<QPointF>& pts, int i0, int i1, qreal eps,
                                    QVector<bool>& keep) {
        if (i1 <= i0 + 1)
            return;
        qreal dmax = 0.0;
        int im = i0;
        for (int i = i0 + 1; i < i1; ++i) {
            const qreal d = distancePointToLine(pts[i], pts[i0], pts[i1]);
            if (d > dmax) {
                dmax = d;
                im = i;
            }
        }
        if (dmax > eps) {
            keep[im] = true;
            douglasPeuckerRange(pts, i0, im, eps, keep);
            douglasPeuckerRange(pts, im, i1, eps, keep);
        }
    }

    static QVector<QPointF> douglasPeucker(const QVector<QPointF>& pts, qreal epsilon) {
        if (pts.size() < 3)
            return pts;
        QVector<bool> keep(pts.size(), false);
        keep[0] = true;
        keep[pts.size() - 1] = true;
        douglasPeuckerRange(pts, 0, static_cast<int>(pts.size()) - 1, epsilon, keep);
        QVector<QPointF> out;
        for (int i = 0; i < pts.size(); ++i) {
            if (keep[i])
                out.append(pts[i]);
        }
        return out;
    }

    /// Mean distance to axis-aligned rectangle outline, normalized by diag.
    static qreal meanDistToRectOutlineNorm(const QList<StrokePoint>& pts, const QRectF& b, qreal diag) {
        if (pts.isEmpty() || diag < 1e-6)
            return 1.0;
        qreal sum = 0.0;
        for (const auto& sp : pts) {
            const QPointF& p = sp.pos;
            const qreal inL = p.x() - b.left();
            const qreal inR = b.right() - p.x();
            const qreal inT = p.y() - b.top();
            const qreal inB = b.bottom() - p.y();
            sum += qMax(0.0, qMin(qMin(inL, inR), qMin(inT, inB)));
        }
        return (sum / pts.size()) / diag;
    }

    /// Mean distance to closed polyline edges (consecutive vertices), normalized by diag.
    static qreal meanDistToClosedPolyEdgesNorm(const QList<StrokePoint>& pts,
                                               const QVector<QPointF>& ring, qreal diag) {
        if (pts.isEmpty() || ring.size() < 3 || diag < 1e-6)
            return 1.0;
        qreal sum = 0.0;
        const int n = ring.size();
        for (const auto& sp : pts) {
            qreal md = 1e100;
            for (int i = 0; i < n; ++i) {
                const QPointF& a = ring[i];
                const QPointF& b = ring[(i + 1) % n];
                md = qMin(md, distancePointToLine(sp.pos, a, b));
            }
            sum += md;
        }
        return (sum / pts.size()) / diag;
    }

    static qreal meanRadialErrorNorm(const QList<StrokePoint>& pts, const QPointF& c, qreal rMean,
                                     qreal diag) {
        if (pts.isEmpty() || diag < 1e-6)
            return 1.0;
        qreal sum = 0.0;
        for (const auto& sp : pts) {
            sum += std::abs(QLineF(sp.pos, c).length() - rMean);
        }
        return (sum / pts.size()) / diag;
    }

    static QPointF strokeCentroid(const QList<StrokePoint>& pts) {
        qreal sx = 0.0;
        qreal sy = 0.0;
        for (const auto& sp : pts) {
            sx += sp.pos.x();
            sy += sp.pos.y();
        }
        const qreal inv = 1.0 / pts.size();
        return QPointF(sx * inv, sy * inv);
    }

    /// Few iterations: centroid then pull center toward a uniform-radius fit (better than box.center for hand circles).
    static void refineCircleCenterRadius(const QList<StrokePoint>& pts, QPointF& c, qreal& rMean) {
        for (int iter = 0; iter < 3; ++iter) {
            rMean = 0.0;
            for (const auto& sp : pts)
                rMean += QLineF(sp.pos, c).length();
            rMean /= pts.size();
            QPointF acc(0.0, 0.0);
            for (const auto& sp : pts) {
                const QPointF v = sp.pos - c;
                const qreal len = std::hypot(v.x(), v.y());
                if (len > 1e-4)
                    acc += c + v * (rMean / len);
                else
                    acc += sp.pos;
            }
            c = acc / pts.size();
        }
        rMean = 0.0;
        for (const auto& sp : pts)
            rMean += QLineF(sp.pos, c).length();
        rMean /= pts.size();
    }

    /// Algebraic (Kåsa) circle fit; z ≈ a x + b y + c with z = x²+y² → center (a/2,b/2).
    static bool algebraicCircleFitKasa(const QVector<QPointF>& pts, QPointF& outC, qreal& outR) {
        const int n = pts.size();
        if (n < 3)
            return false;
        double Sxx = 0, Syy = 0, Sxy = 0, Sx = 0, Sy = 0;
        double Sxz = 0, Syz = 0, Sz = 0;
        for (const QPointF& p : pts) {
            const double x = p.x();
            const double y = p.y();
            const double z = x * x + y * y;
            Sx += x;
            Sy += y;
            Sz += z;
            Sxx += x * x;
            Syy += y * y;
            Sxy += x * y;
            Sxz += x * z;
            Syz += y * z;
        }
        const double m00 = Sxx, m01 = Sxy, m02 = Sx;
        const double m10 = Sxy, m11 = Syy, m12 = Sy;
        const double m20 = Sx, m21 = Sy, m22 = static_cast<double>(n);
        const double r0 = Sxz, r1 = Syz, r2 = Sz;
        const double det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20)
                         + m02 * (m10 * m21 - m11 * m20);
        if (std::abs(det) < 1e-18)
            return false;
        auto cramerCol = [&](int col) {
            double a00 = m00, a01 = m01, a02 = m02;
            double a10 = m10, a11 = m11, a12 = m12;
            double a20 = m20, a21 = m21, a22 = m22;
            if (col == 0) {
                a00 = r0;
                a10 = r1;
                a20 = r2;
            } else if (col == 1) {
                a01 = r0;
                a11 = r1;
                a21 = r2;
            } else {
                a02 = r0;
                a12 = r1;
                a22 = r2;
            }
            return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20)
                 + a02 * (a10 * a21 - a11 * a20);
        };
        const double a = cramerCol(0) / det;
        const double b = cramerCol(1) / det;
        const double c = cramerCol(2) / det;
        const double xc = 0.5 * a;
        const double yc = 0.5 * b;
        const double rr = c + xc * xc + yc * yc;
        if (rr <= 1e-8)
            return false;
        outC = QPointF(xc, yc);
        outR = std::sqrt(rr);
        return outR > 1e-6;
    }

    static qreal meanAbsRadial(const QList<StrokePoint>& pts, const QPointF& c, qreal r) {
        qreal sum = 0.0;
        for (const auto& sp : pts)
            sum += std::abs(QLineF(sp.pos, c).length() - r);
        return sum;
    }

    /// Minimum mean edge distance over four bbox-aligned isosceles triangles (apex on each side).
    static qreal bestBBoxTriangleResidual(const QList<StrokePoint>& pts, const QRectF& bn, qreal diag,
                                          QVector<QPointF>& outRing) {
        const qreal l = bn.left();
        const qreal rgt = bn.right();
        const qreal t = bn.top();
        const qreal b = bn.bottom();
        const qreal mx = (l + rgt) * 0.5;
        const qreal my = (t + b) * 0.5;
        const QVector<QVector<QPointF>> rings{{QPointF(mx, t), QPointF(rgt, b), QPointF(l, b)},
                                              {QPointF(mx, b), QPointF(l, t), QPointF(rgt, t)},
                                              {QPointF(l, my), QPointF(rgt, t), QPointF(rgt, b)},
                                              {QPointF(rgt, my), QPointF(l, t), QPointF(l, b)}};
        qreal best = 1e100;
        outRing = rings[0];
        for (const QVector<QPointF>& ring : rings) {
            const qreal e = meanDistToClosedPolyEdgesNorm(pts, ring, diag);
            if (e < best) {
                best = e;
                outRing = ring;
            }
        }
        return best;
    }

    static int simplifiedPolyVertexCount(const QVector<QPointF>& simp, qreal diag) {
        if (simp.isEmpty())
            return 0;
        const qreal closeEps = qMax(2.0, 0.02 * diag);
        int n = simp.size();
        if (n >= 2 && QLineF(simp.first(), simp.last()).length() <= closeEps)
            n = n - 1;
        return qMax(1, n);
    }

    HoldShapeKind fitHoldShape(const QList<StrokePoint>& pts, QPainterPath& outPath,
                               QVector<StrokePoint>& outPoints, qreal& outConfidence,
                               bool* outSquareLike = nullptr, QPointF* outArcMid = nullptr) const {
        outPath = QPainterPath();
        outPoints.clear();
        outConfidence = 0.0;
        if (pts.size() < 6) return HoldShapeKind::None;

        QRectF box;
        for (int i = 0; i < pts.size(); ++i) {
            if (i == 0) box = QRectF(pts[i].pos, QSizeF(1, 1));
            else box = box.united(QRectF(pts[i].pos, QSizeF(1, 1)));
        }
        const qreal w = qMax<qreal>(1.0, box.width());
        const qreal h = qMax<qreal>(1.0, box.height());
        const qreal diag = std::hypot(w, h);
        if (diag < 10.0) return HoldShapeKind::None;

        const QPointF pStart = pts.first().pos;
        const QPointF pEnd = pts.last().pos;
        const qreal chord = QLineF(pStart, pEnd).length();
        const qreal closure = chord / diag;
        qreal polyLen = 0.0;
        for (int i = 1; i < pts.size(); ++i)
            polyLen += QLineF(pts[i - 1].pos, pts[i].pos).length();

        const QPointF boxCenter = box.center();
        qreal rAvgBox = 0.0;
        for (const auto& p : pts) rAvgBox += QLineF(p.pos, boxCenter).length();
        rAvgBox /= pts.size();
        qreal rVarBox = 0.0;
        for (const auto& p : pts) {
            const qreal d = QLineF(p.pos, boxCenter).length() - rAvgBox;
            rVarBox += d * d;
        }
        rVarBox = std::sqrt(rVarBox / pts.size()) / qMax<qreal>(1.0, rAvgBox);

        const qreal sens = 0.70 + 0.60 * (qBound(0, m_config.holdShapeSensitivity, 100) / 100.0);

        QPointF circleCenter = strokeCentroid(pts);
        qreal rFit = 0.0;
        refineCircleCenterRadius(pts, circleCenter, rFit);
        const QVector<QPointF> subsFit = subsampleStrokePositions(pts, 72);
        QPointF cK;
        qreal rK = 0.0;
        if (algebraicCircleFitKasa(subsFit, cK, rK)) {
            const qreal eK = meanAbsRadial(pts, cK, rK);
            const qreal eI = meanAbsRadial(pts, circleCenter, rFit);
            if (eK < eI)
                circleCenter = cK, rFit = rK;
        }
        qreal rVarC = 0.0;
        for (const auto& p : pts) {
            const qreal d = QLineF(p.pos, circleCenter).length() - rFit;
            rVarC += d * d;
        }
        rVarC = std::sqrt(rVarC / pts.size()) / qMax<qreal>(1.0, rFit);
        const qreal aspect = qMin(w, h) / qMax(w, h);
        qreal radialErrNorm = meanRadialErrorNorm(pts, circleCenter, rFit, diag);

        const bool closedByEndpoints = closure < 0.30 * sens;
        const bool closedLikeLoop = (polyLen > 2.12 * qMax(chord, diag * 0.012)) && (chord < 0.48 * diag * sens)
            && (rVarC < 0.142 * sens) && (aspect > qMax(0.30, 0.46 - 0.12 * sens));
        const bool mostlyClosed = closedByEndpoints || closedLikeLoop;

        qreal lineErr = 0.0;
        for (const auto& p : pts) lineErr += distancePointToLine(p.pos, pStart, pEnd);
        lineErr = (lineErr / pts.size()) / diag;
        const qreal lineClosedPenalty = mostlyClosed ? 0.22 : 1.0;
        const qreal lineScore = clamp01((1.0 - lineErr * 5.5) * lineClosedPenalty);

        qreal arcScore = 0.0;
        if (!mostlyClosed) {
            QPointF mid = pts[pts.size() / 2].pos;
            qreal arcBulge = distancePointToLine(mid, pStart, pEnd) / diag;
            arcScore = clamp01((arcBulge - 0.08) * 3.0) * clamp01((0.7 - lineErr) * 1.8);
        }

        HoldShapeKind best = HoldShapeKind::Line;
        qreal bestScore = lineScore;
        QVector<QPointF> triDrawRing;

        const qreal mTriRect = 0.10 / sens;
        const qreal mShapeLine = 0.055 / sens;
        const qreal minShapeQualVsLine = qMax(0.34, 0.42 - 0.10 * ((sens - 0.70) / 0.60));
        const qreal minGapShapeVsLineAlt = 0.03 / sens;
        static constexpr qreal kOpenArcLineMargin = 0.085;

        if (mostlyClosed) {
            const QRectF bn = box.normalized();
            QVector<QPointF> bestTriRing;
            const qreal triResNorm = bestBBoxTriangleResidual(pts, bn, diag, bestTriRing);
            triDrawRing = bestTriRing;

            const QVector<QPointF> dpIn = subsampleStrokePositions(pts, 72);
            const qreal dpEps = qMax(1.5, 0.011 * diag);
            const QVector<QPointF> simp = douglasPeucker(dpIn, dpEps);
            const int dpVerts = simplifiedPolyVertexCount(simp, diag);

            const qreal rectResNorm = meanDistToRectOutlineNorm(pts, bn, diag);

            qreal dpTriFactor = 0.52;
            if (dpVerts <= 4)
                dpTriFactor = 1.0;
            else if (dpVerts == 5)
                dpTriFactor = 0.8;
            else if (dpVerts == 6)
                dpTriFactor = 0.66;

            qreal dpRectFactor = 0.52;
            if (dpVerts >= 4 && dpVerts <= 5)
                dpRectFactor = 1.0;
            else if (dpVerts >= 6 && dpVerts <= 8)
                dpRectFactor = 0.86;

            const qreal triResBoost = clamp01(1.0 - triResNorm / 0.14);
            dpTriFactor = qMax(dpTriFactor, 0.35 + 0.65 * triResBoost);
            const qreal rectResBoost = clamp01(1.0 - rectResNorm / 0.13);
            dpRectFactor = qMax(dpRectFactor, 0.35 + 0.65 * rectResBoost);

            qreal triQ = clamp01(1.0 - triResNorm / 0.22) * dpTriFactor;
            if (!m_config.holdEnableTriangle)
                triQ = 0.0;
            qreal rectQ = clamp01(1.0 - rectResNorm / 0.195) * dpRectFactor;
            rectQ *= (1.0 - 0.52 * clamp01(1.0 - 4.0 * rVarBox));

            const qreal circleQual =
                clamp01(1.0 - rVarC / (0.105 * sens)) * clamp01((aspect - (0.48 - 0.06 * (sens - 0.70))) / 0.52);
            const bool circleHard = m_config.holdEnableCircle && (rVarC < 0.118 * sens) && (aspect > qMax(0.36, 0.48 - 0.09 * sens))
                && (radialErrNorm < 0.158 * sens) && (circleQual >= qMax(0.32, 0.40 - 0.08 * ((sens - 0.70) / 0.60)));
            const bool circleSoft = m_config.holdEnableCircle && !circleHard && (rVarC < 0.132 * sens)
                && (aspect > qMax(0.34, 0.46 - 0.09 * sens)) && (radialErrNorm < 0.178 * sens)
                && (circleQual >= qMax(0.26, 0.34 - 0.06 * ((sens - 0.70) / 0.60)))
                && (closedByEndpoints || closedLikeLoop);
            const bool circleWeak = m_config.holdEnableCircle && !circleHard && !circleSoft
                && (closedByEndpoints || closedLikeLoop) && (rVarC < 0.145 * sens)
                && (aspect > qMax(0.32, 0.43 - 0.09 * sens)) && (radialErrNorm < 0.198 * sens)
                && (circleQual >= 0.26) && (circleQual + 0.02 > qMax(triQ, rectQ));
            const bool circleRescue = m_config.holdEnableCircle && !circleHard && !circleSoft && !circleWeak
                && (closedByEndpoints || closedLikeLoop) && (rVarC < 0.125 * sens) && (radialErrNorm < 0.19 * sens)
                && (aspect > 0.30) && (circleQual >= 0.28)
                && (circleQual + 0.04 >= qMax(triQ, rectQ));

            if (circleHard) {
                best = HoldShapeKind::Circle;
                bestScore = circleQual;
            } else if (circleSoft) {
                best = HoldShapeKind::Circle;
                bestScore = qMax(0.41, circleQual * 0.92);
            } else if (circleWeak) {
                best = HoldShapeKind::Circle;
                bestScore = qMax(0.40, circleQual * 0.86);
            } else if (circleRescue) {
                best = HoldShapeKind::Circle;
                bestScore = qMax(0.39, circleQual * 0.88);
            } else {
                struct Qc {
                    HoldShapeKind k;
                    qreal q;
                };
                Qc cand[3] = {{HoldShapeKind::Triangle, clamp01(triQ)},
                              {HoldShapeKind::Rectangle, clamp01(rectQ)},
                              {HoldShapeKind::Line, lineScore}};
                std::sort(cand, cand + 3, [](const Qc& a, const Qc& b) { return a.q > b.q; });
                const qreal gap = cand[0].q - cand[1].q;
                const auto isShape = [](HoldShapeKind k) {
                    return k == HoldShapeKind::Triangle || k == HoldShapeKind::Rectangle;
                };
                if (cand[0].k == HoldShapeKind::Line) {
                    best = HoldShapeKind::Line;
                    bestScore = lineScore;
                } else if (isShape(cand[0].k) && cand[1].k == HoldShapeKind::Line) {
                    if (gap >= mShapeLine
                        || (cand[0].q >= minShapeQualVsLine && gap >= minGapShapeVsLineAlt)) {
                        best = cand[0].k;
                        bestScore = cand[0].q;
                    } else {
                        best = HoldShapeKind::Line;
                        bestScore = lineScore;
                    }
                } else if (isShape(cand[0].k) && isShape(cand[1].k)) {
                    if (gap >= mTriRect) {
                        best = cand[0].k;
                        bestScore = cand[0].q;
                    } else {
                        best = HoldShapeKind::Line;
                        bestScore = lineScore;
                    }
                } else {
                    best = cand[0].k;
                    bestScore = cand[0].q;
                }
            }
        } else {
            if (lineScore >= arcScore - kOpenArcLineMargin) {
                best = HoldShapeKind::Line;
                bestScore = lineScore;
            } else {
                best = HoldShapeKind::Arc;
                bestScore = arcScore;
            }
        }

        const qreal pressure = pts.last().pressure;
        auto addPolyline = [&](const QVector<QPointF>& poly) {
            if (poly.size() < 2) return;
            outPath.moveTo(poly.first());
            outPoints.append({poly.first(), pressure});
            for (int i = 1; i < poly.size(); ++i) {
                outPath.lineTo(poly[i]);
                outPoints.append({poly[i], pressure});
            }
        };

        if (best == HoldShapeKind::Line) {
            addPolyline({pStart, pEnd});
        } else if (best == HoldShapeKind::Rectangle) {
            const bool squareLike = aspect > 0.84;
            if (outSquareLike)
                *outSquareLike = squareLike;
            QRectF r = box.normalized();
            if (squareLike) {
                const qreal side = qMax(r.width(), r.height());
                r.setWidth(side);
                r.setHeight(side);
            }
            addPolyline({r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft(), r.topLeft()});
        } else if (best == HoldShapeKind::Triangle) {
            if (triDrawRing.size() >= 3)
                addPolyline({triDrawRing[0], triDrawRing[1], triDrawRing[2], triDrawRing[0]});
            else {
                const QPointF top((box.left() + box.right()) * 0.5, box.top());
                const QPointF bl(box.left(), box.bottom());
                const QPointF br(box.right(), box.bottom());
                addPolyline({top, br, bl, top});
            }
        } else if (best == HoldShapeKind::Circle) {
            const qreal d = 2.0 * rFit;
            const QRectF circleBox(circleCenter.x() - 0.5 * d, circleCenter.y() - 0.5 * d, d, d);
            outPath.addEllipse(circleBox);
            const int seg = 40;
            for (int i = 0; i <= seg; ++i) {
                const qreal t = (2.0 * M_PI * i) / seg;
                const QPointF p(circleCenter.x() + rFit * std::cos(t), circleCenter.y() + rFit * std::sin(t));
                outPoints.append({p, pressure});
            }
        } else if (best == HoldShapeKind::Arc) {
            const QPointF mid = pts[pts.size() / 2].pos;
            if (outArcMid)
                *outArcMid = mid;
            outPath.moveTo(pStart);
            outPath.quadTo(mid, pEnd);
            const int seg = 24;
            for (int i = 0; i <= seg; ++i) {
                const qreal t = static_cast<qreal>(i) / seg;
                const qreal u = 1.0 - t;
                const QPointF p = (u * u) * pStart + (2.0 * u * t) * mid + (t * t) * pEnd;
                outPoints.append({p, pressure});
            }
        }

        if (outPoints.size() < 2) return HoldShapeKind::None;
        if (best == HoldShapeKind::Circle || best == HoldShapeKind::Triangle
            || best == HoldShapeKind::Rectangle)
            outConfidence = qMin(1.0, bestScore + 0.04);
        else
            outConfidence = bestScore;
        return best;
    }

    void rebuildFreehandPathFromPoints() {
        if (!m_currentItem || m_pointsBuffer.isEmpty()) return;
        m_currentPath = QPainterPath();
        m_currentPath.moveTo(m_pointsBuffer.first().pos);
        QVector<StrokePoint> freePts;
        freePts.reserve(m_pointsBuffer.size());
        freePts.append(m_pointsBuffer.first());
        for (int i = 1; i < m_pointsBuffer.size(); ++i) {
            m_currentPath.lineTo(m_pointsBuffer[i].pos);
            freePts.append(m_pointsBuffer[i]);
        }
        m_currentItem->setPath(m_currentPath);
        auto* typedItem = static_cast<StrokeItem*>(m_currentItem);
        typedItem->setPoints(freePts);
    }
};
