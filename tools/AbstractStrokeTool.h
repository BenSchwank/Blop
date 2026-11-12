#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include "RulerTool.h"
#include "StrokeItem.h"

#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QTabletEvent>
#include <QVector>
#include <cmath>

/// Clean-room stroke base (v2): collect raw points, render a smoothed path,
/// finish on release. No gesture recognition, no timers, no surprises —
/// what you draw is what you get.
///
/// Features kept:
///  - per-point stylus pressure (width scaling inside StrokeItem)
///  - ruler edge snapping while drawing along a visible ruler
///  - simple exponential smoothing (config.smoothing)
///  - highlighter "smart line" (straight-line preview while dragging)
class AbstractStrokeTool : public AbstractTool {
  Q_OBJECT

public:
  using AbstractTool::AbstractTool;

  // ── Tablet (stylus) ────────────────────────────────────────────────
  void setStrokeSceneForTablet(QGraphicsScene *scene) override {
    if (scene)
      m_sceneRef = scene;
  }

  bool handleTabletEvent(QTabletEvent *event,
                         const QPointF &scenePos) override {
    if (event->type() == QEvent::TabletPress) {
      m_lastPressure = pressureFrom(event->pressure());
      return beginStroke(scenePos, m_sceneRef);
    }
    if (event->type() == QEvent::TabletMove) {
      m_lastPressure = pressureFrom(event->pressure());
      return extendStroke(scenePos);
    }
    if (event->type() == QEvent::TabletRelease)
      return finishStroke();
    return false;
  }

  // ── Mouse ──────────────────────────────────────────────────────────
  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene || !event)
      return false;
    m_lastPressure = 1.0;
    return beginStroke(event->scenePos(), scene);
  }

  bool handleMouseMove(QGraphicsSceneMouseEvent *event,
                       QGraphicsScene *scene) override {
    if (scene)
      m_sceneRef = scene;
    if (!event)
      return false;
    return extendStroke(event->scenePos());
  }

  bool handleMouseRelease(QGraphicsSceneMouseEvent *,
                          QGraphicsScene *) override {
    return finishStroke();
  }

protected:
  virtual QPen createPen() const = 0;
  virtual qreal getZValue() const { return 0; }
  virtual StrokeItem::StrokeStyle strokeStyle() const {
    return StrokeItem::Normal;
  }

  qreal m_lastPressure{1.0};
  QGraphicsScene *m_sceneRef{nullptr};
  StrokeItem *m_currentItem{nullptr};
  QVector<StrokePoint> m_points;
  RulerItem *m_rulerRef{nullptr};
  bool m_snapping{false};

  qreal pressureFrom(qreal raw) const {
    if (!m_config.pressureSensitivity || raw <= 0.0)
      return 1.0;
    // Soft knee: never thinner than 30% of configured width.
    return 0.30 + 0.70 * std::pow(qBound(0.0, raw, 1.0), 0.65);
  }

  bool beginStroke(const QPointF &pos, QGraphicsScene *scene) {
    if (!scene)
      return false;
    m_sceneRef = scene;
    QPointF start = pos;

    m_snapping = false;
    m_rulerRef = nullptr;
    if (m_config.rulerSnap) {
      m_rulerRef = findActiveRuler(scene);
      if (m_rulerRef && m_rulerRef->isVisible() &&
          m_rulerRef->isNearSnapEdge(start, 28.0)) {
        start = m_rulerRef->snapPoint(start);
        m_snapping = true;
      }
    }

    m_points.clear();
    m_points.append({start, m_lastPressure});
    m_moveClock.start();
    m_fittedShape = false;

    QPainterPath p;
    p.moveTo(start);
    m_currentItem = new StrokeItem(p, createPen(), {}, strokeStyle());
    m_currentItem->setPoints(m_points);
    m_currentItem->setZValue(getZValue());
    scene->addItem(m_currentItem);
    return true;
  }

  bool extendStroke(const QPointF &pos) {
    if (!m_currentItem || m_points.isEmpty())
      return false;

    QPointF p = pos;
    if (m_snapping && m_rulerRef) {
      const QPointF snapped = m_rulerRef->snapPoint(p, m_points.first().pos);
      // Release the snap when the cursor is pulled far off the edge —
      // otherwise the whole stroke glues to the ruler line.
      if (QLineF(p, snapped).length() > 48.0) {
        m_snapping = false;
      } else {
        p = snapped;
      }
    } else if (m_config.smoothing > 0) {
      const qreal f = qMin(0.95, m_config.smoothing / 120.0);
      p = m_points.last().pos * f + p * (1.0 - f);
    }

    if (mode() == ToolMode::Highlighter && m_config.smartLine) {
      // Straight-line highlighter: preview collapses to start → cursor.
      m_points.resize(1);
      m_points.append({p, m_lastPressure});
    } else {
      if (QLineF(p, m_points.last().pos).length() < 1.0)
        return true;
      m_points.append({p, m_lastPressure});
      m_moveClock.restart();
    }
    commitPath();
    return true;
  }

  bool finishStroke() {
    if (!m_currentItem)
      return false;
    // Ink-to-shape: holding still before release fits a clean figure.
    // Freehand strokes (no pause) are never touched.
    if (m_points.size() >= 8 && m_moveClock.elapsed() >= 350) {
      const QVector<QPointF> fit = fitShape(m_points);
      if (!fit.isEmpty()) {
        m_points.clear();
        for (const QPointF &pt : fit)
          m_points.append({pt, 1.0});
        m_fittedShape = true;
        m_currentItem->setData(0, QStringLiteral("shape_item"));
      }
    }
    commitPath();
    m_lastCompletedItem = m_currentItem;
    m_currentItem = nullptr;
    m_snapping = false;
    m_rulerRef = nullptr;
    m_sceneRef = nullptr;
    emit contentModified();
    return true;
  }

  void commitPath() {
    if (!m_currentItem)
      return;
    QPainterPath path;
    if (m_fittedShape && !m_points.isEmpty()) {
      // Crisp polygon for fitted figures (no quad smoothing).
      path.moveTo(m_points.first().pos);
      for (int i = 1; i < m_points.size(); ++i)
        path.lineTo(m_points[i].pos);
      path.closeSubpath();
    } else {
      QVector<QPointF> pts;
      pts.reserve(m_points.size());
      for (const StrokePoint &sp : m_points)
        pts.append(sp.pos);
      path = smoothStrokePath(pts);
    }
    m_currentItem->setPath(path);
    m_currentItem->setPoints(m_points);
  }

private:
  QElapsedTimer m_moveClock;
  bool m_fittedShape{false};

  /// Distance from point p to the infinite line through a→b.
  static qreal distToLine(const QPointF &p, const QPointF &a,
                          const QPointF &b) {
    const qreal dx = b.x() - a.x(), dy = b.y() - a.y();
    const qreal len = std::hypot(dx, dy);
    if (len < 1e-6)
      return QLineF(a, p).length();
    return std::abs(dy * p.x() - dx * p.y() + b.x() * a.y() -
                    b.y() * a.x()) /
           len;
  }

  /// Douglas-Peucker simplification of a stroke polyline.
  static void rdpRecurse(const QVector<QPointF> &pts, int i0, int i1,
                         qreal eps, QVector<QPointF> &out) {
    qreal dMax = 0;
    int idx = i0;
    for (int i = i0 + 1; i < i1; ++i) {
      const qreal d = distToLine(pts[i], pts[i0], pts[i1]);
      if (d > dMax) {
        dMax = d;
        idx = i;
      }
    }
    if (dMax > eps && idx > i0 && idx < i1) {
      rdpRecurse(pts, i0, idx, eps, out);
      out.removeLast();
      rdpRecurse(pts, idx, i1, eps, out);
    } else {
      out.append(pts[i0]);
      out.append(pts[i1]);
    }
  }

  /// Fit line/triangle/rect/ellipse from a finished stroke. Empty = keep
  /// freehand. The result replaces stroke points (polygon vertices or a
  /// sampled ellipse) and is tagged "shape_item" for vector persistence.
  QVector<QPointF> fitShape(const QVector<StrokePoint> &in) const {
    QVector<QPointF> src;
    src.reserve(in.size());
    for (const StrokePoint &sp : in)
      src.append(sp.pos);
    if (src.size() < 8)
      return {};

    QRectF bb;
    for (const QPointF &p : src)
      bb = bb.united(QRectF(p, QSizeF(0.01, 0.01)));
    const qreal diag = std::hypot(bb.width(), bb.height());
    if (diag < 30.0)
      return {};

    QVector<QPointF> simp;
    rdpRecurse(src, 0, src.size() - 1, diag * 0.045, simp);
    if (simp.size() >= 2 && simp.first() == simp.last())
      simp.removeLast(); // closed ring: drop duplicated endpoint

    // LINE
    if (simp.size() <= 2)
      return {src.first(), src.last()};

    const qreal closeGap =
        QLineF(src.first(), src.last()).length() / qMax(1.0, diag);

    // TRIANGLE / RECTANGLE — need a roughly closed loop.
    if (closeGap < 0.30 && simp.size() >= 3 && simp.size() <= 5) {
      QVector<QPointF> poly = simp;
      if (poly.size() == 5)
        poly.removeLast(); // rare: endpoint counted twice
      if (poly.size() == 3 || poly.size() == 4) {
        poly.append(poly.first()); // close
        return poly;
      }
    }

    // ELLIPSE — closed, many vertices.
    if (closeGap < 0.30 && simp.size() > 4) {
      QPainterPath el;
      el.addEllipse(bb);
      const auto poly = el.toSubpathPolygons().first();
      return QVector<QPointF>(poly.begin(), poly.end());
    }

    return {};
  }
};
