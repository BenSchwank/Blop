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
#include <array>

class AbstractStrokeTool : public AbstractTool {
    Q_OBJECT

public:
    explicit AbstractStrokeTool(QObject* parent = nullptr)
        : AbstractTool(parent) {
        m_holdStillTimer = new QTimer(this);
        m_holdStillTimer->setSingleShot(true);
        m_holdStillTimer->setInterval(360);
        connect(m_holdStillTimer, &QTimer::timeout, this,
                &AbstractStrokeTool::onHoldStillTimeout);
    }

    // We store the last pressure so that mouseMove events (if synthesized) have a fallback,
    // though native TabletEvents will use their own pressure.
    qreal m_lastPressure{1.0};

    /// Call from the view before each tablet gesture event so strokes are added to the scene
    /// (tablet callbacks pass no QGraphicsScene* to handleMoveOrPress).
    void setStrokeSceneForTablet(QGraphicsScene* scene) {
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
    QPointF m_holdCircleCenter;
    QPointF m_holdRectAnchorCorner;
    bool m_holdIsSquare{false};
    std::array<QPointF, 3> m_holdTriangleVerts{};
    bool m_holdTriValid{false};
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
                m_holdStillTimer->start(360);
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
                            m_holdStillTimer->start(360);
                    }
                    const bool heldLongEnough = m_lastMotionTimer.elapsed() >= 360;
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
        m_holdTriValid = false;
        m_holdArcValid = false;
        switch (kind) {
        case HoldShapeKind::Line:
            break;
        case HoldShapeKind::Rectangle: {
            m_holdIsSquare = squareLike;
            const QPointF c[4] = {fitted[0].pos, fitted[1].pos, fitted[2].pos, fitted[3].pos};
            qreal bestD = 1e100;
            int bi = 0;
            for (int i = 0; i < 4; ++i) {
                const qreal d = QLineF(c[i], m_holdStrokeStart).length();
                if (d < bestD) {
                    bestD = d;
                    bi = i;
                }
            }
            m_holdRectAnchorCorner = c[bi];
            break;
        }
        case HoldShapeKind::Triangle:
            m_holdTriangleVerts[0] = fitted[0].pos;
            m_holdTriangleVerts[1] = fitted[1].pos;
            m_holdTriangleVerts[2] = fitted[2].pos;
            m_holdTriValid = true;
            break;
        case HoldShapeKind::Circle: {
            QRectF bb;
            for (const auto& sp : fitted)
                bb |= QRectF(sp.pos, QSizeF(1, 1));
            m_holdCircleCenter = bb.center();
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
        int mi = 0;
        qreal best = -1.0;
        for (int i = 0; i < 3; ++i) {
            const qreal d = QLineF(m_holdTriangleVerts[i], m_holdStrokeStart).length();
            if (d > best) {
                best = d;
                mi = i;
            }
        }
        m_holdTriangleVerts[mi] = cursor;
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
        return 0.48;
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

    static int estimateCornerCount(const QList<StrokePoint>& pts) {
        if (pts.size() < 10) return 0;
        int corners = 0;
        for (int i = 2; i < pts.size() - 2; i += 2) {
            const QPointF a = pts[i - 2].pos;
            const QPointF b = pts[i].pos;
            const QPointF c = pts[i + 2].pos;
            const QPointF v1 = b - a;
            const QPointF v2 = c - b;
            const qreal l1 = std::hypot(v1.x(), v1.y());
            const qreal l2 = std::hypot(v2.x(), v2.y());
            if (l1 < 1.0 || l2 < 1.0) continue;
            qreal dot = (v1.x() * v2.x() + v1.y() * v2.y()) / (l1 * l2);
            dot = qBound(-1.0, dot, 1.0);
            const qreal angDeg = qRadiansToDegrees(std::acos(dot));
            if (angDeg > 35.0 && angDeg < 155.0)
                ++corners;
        }
        return corners;
    }

    HoldShapeKind fitHoldShape(const QList<StrokePoint>& pts, QPainterPath& outPath,
                               QVector<StrokePoint>& outPoints, qreal& outConfidence) const {
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
        const qreal closure = QLineF(pStart, pEnd).length() / diag;
        const bool mostlyClosed = closure < 0.26;
        const int corners = estimateCornerCount(pts);

        qreal lineErr = 0.0;
        for (const auto& p : pts) lineErr += distancePointToLine(p.pos, pStart, pEnd);
        lineErr = (lineErr / pts.size()) / diag;
        const qreal lineClosedPenalty = mostlyClosed ? 0.22 : 1.0;
        qreal lineScore = clamp01((1.0 - lineErr * 5.5) * lineClosedPenalty);

        QPointF center = box.center();
        qreal rAvg = 0.0;
        for (const auto& p : pts) rAvg += QLineF(p.pos, center).length();
        rAvg /= pts.size();
        qreal rVar = 0.0;
        for (const auto& p : pts) {
            const qreal d = QLineF(p.pos, center).length() - rAvg;
            rVar += d * d;
        }
        rVar = std::sqrt(rVar / pts.size()) / qMax<qreal>(1.0, rAvg);
        const qreal aspect = qMin(w, h) / qMax(w, h);
        const qreal circleClosedBoost = mostlyClosed ? 1.18 : 0.2;
        qreal circleScore = clamp01((1.0 - rVar * 2.15) * (0.35 + 0.65 * aspect) * circleClosedBoost);

        qreal edgeNear = 0.0;
        for (const auto& p : pts) {
            const qreal dL = std::abs(p.pos.x() - box.left());
            const qreal dR = std::abs(p.pos.x() - box.right());
            const qreal dT = std::abs(p.pos.y() - box.top());
            const qreal dB = std::abs(p.pos.y() - box.bottom());
            const qreal dMin = qMin(qMin(dL, dR), qMin(dT, dB));
            if (dMin < diag * 0.08) edgeNear += 1.0;
        }
        const qreal rectClosedBoost = mostlyClosed ? 1.15 : 0.1;
        qreal rectScore = clamp01((edgeNear / pts.size()) * rectClosedBoost * (mostlyClosed ? 1.08 : 1.0));
        if (corners < 3) rectScore *= mostlyClosed ? 0.72 : 0.55;

        qreal triScore = clamp01((mostlyClosed ? 1.12 : 0.15) * (1.0 - std::abs(corners - 3) * 0.18));

        qreal arcScore = 0.0;
        if (!mostlyClosed) {
            QPointF mid = pts[pts.size() / 2].pos;
            qreal arcBulge = distancePointToLine(mid, pStart, pEnd) / diag;
            arcScore = clamp01((arcBulge - 0.08) * 3.0) * clamp01((0.7 - lineErr) * 1.8);
        }

        HoldShapeKind best = HoldShapeKind::Line;
        qreal bestScore = lineScore;
        auto pick = [&](HoldShapeKind k, qreal s) {
            if (s > bestScore) { best = k; bestScore = s; }
        };
        pick(HoldShapeKind::Rectangle, rectScore);
        pick(HoldShapeKind::Triangle, triScore);
        pick(HoldShapeKind::Circle, circleScore);
        pick(HoldShapeKind::Arc, arcScore);

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
            const QPointF top((box.left() + box.right()) * 0.5, box.top());
            const QPointF bl(box.left(), box.bottom());
            const QPointF br(box.right(), box.bottom());
            addPolyline({top, br, bl, top});
        } else if (best == HoldShapeKind::Circle) {
            outPath.addEllipse(box);
            const int seg = 40;
            for (int i = 0; i <= seg; ++i) {
                const qreal t = (2.0 * M_PI * i) / seg;
                const QPointF p(center.x() + 0.5 * w * std::cos(t),
                                center.y() + 0.5 * h * std::sin(t));
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
