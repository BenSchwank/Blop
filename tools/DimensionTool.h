#pragma once
#include "AbstractTool.h"
#include "ToolManager.h"

#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTabletEvent>
#include <QtMath>
#include <limits>

/// Technical dimensioning: length bar, 3-point angle, or N-gon area.
class DimensionTool : public AbstractTool {
public:
  using AbstractTool::AbstractTool;

  ToolMode mode() const override { return ToolMode::Dimension; }
  QString name() const override { return QStringLiteral("Bemaßung"); }
  QString iconName() const override { return QStringLiteral("ruler"); }

  void setStrokeSceneForTablet(QGraphicsScene *scene) override {
    m_sceneRef = scene;
  }

  bool handleTabletEvent(QTabletEvent *event,
                         const QPointF &scenePos) override {
    if (event->type() == QEvent::TabletPress) {
      QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMousePress);
      ev.setScenePos(scenePos);
      ev.setButton(Qt::LeftButton);
      return handleMousePress(&ev, m_sceneRef);
    }
    if (event->type() == QEvent::TabletMove) {
      QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMouseMove);
      ev.setScenePos(scenePos);
      return handleMouseMove(&ev, m_sceneRef);
    }
    if (event->type() == QEvent::TabletRelease) {
      QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMouseRelease);
      ev.setScenePos(scenePos);
      ev.setButton(Qt::LeftButton);
      return handleMouseRelease(&ev, m_sceneRef);
    }
    return false;
  }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene || !event)
      return false;

    if (m_config.dimensionKind == DimensionKind::Area && m_points.size() >= 3) {
      // Close: click near first vertex, or double-click (near last vertex).
      const QPointF pos = event->scenePos();
      if (QLineF(m_points.first(), pos).length() <= closeRadius() ||
          QLineF(m_points.last(), pos).length() <= 10.0) {
        commitMeasurement(scene);
        return true;
      }
    }

    // Multi-click sequences keep collecting until enough points exist —
    // only a finished/aborted sequence starts a fresh measurement.
    const bool inProgress =
        m_preview && !m_points.isEmpty() &&
        (m_config.dimensionKind == DimensionKind::Area ||
         m_points.size() < pointsNeeded());
    if (!inProgress)
      m_points.clear();
    m_points.append(event->scenePos());
    if (!m_preview) {
      m_preview = scene->addPath(QPainterPath(),
                                 QPen(QColor(91, 157, 255), 1.4, Qt::DashLine));
      m_preview->setZValue(900);
      m_preview->setBrush(QColor(91, 157, 255, 28));
    }
    return true;
  }

  bool handleMouseMove(QGraphicsSceneMouseEvent *event,
                       QGraphicsScene *scene) override {
    Q_UNUSED(scene);
    if (!m_preview || !event || m_points.isEmpty())
      return false;
    QVector<QPointF> pts = m_points;
    pts.append(event->scenePos());
    m_preview->setPath(buildPreview(pts));
    // Soft fill preview while dragging the next area edge.
    if (m_config.dimensionKind == DimensionKind::Area && pts.size() >= 3)
      m_preview->setBrush(QColor(91, 157, 255, 40));
    return true;
  }

  bool handleMouseRelease(QGraphicsSceneMouseEvent *event,
                          QGraphicsScene *scene) override {
    if (!scene || !event) {
      m_preview = nullptr;
      return false;
    }

    // Clicks re-fire press+release at the same spot — skip duplicate points.
    if (m_points.isEmpty() ||
        QLineF(m_points.last(), event->scenePos()).length() > 2.0)
      m_points.append(event->scenePos());

    if (m_config.dimensionKind == DimensionKind::Area) {
      // Keep collecting until close-click / right-click.
      if (m_preview)
        m_preview->setPath(buildPreview(m_points));
      return true;
    }

    const int need = pointsNeeded();
    if (m_points.size() < need) {
      if (m_preview)
        m_preview->setPath(buildPreview(m_points));
      return true;
    }
    commitMeasurement(scene);
    return true;
  }

private:
  // Generous hit target so closing works at typical note zoom (~50–80%).
  static constexpr qreal closeRadius() { return 48.0; }

  int pointsNeeded() const {
    if (m_config.dimensionKind == DimensionKind::Length)
      return 2;
    if (m_config.dimensionKind == DimensionKind::Angle3Point)
      return 3;
    // Area finishes explicitly (close to start / right-click).
    return std::numeric_limits<int>::max();
  }

  qreal unitsPerPx() const {
    // One paper cell ≈ 5mm ≈ measureUnitsPerCell user units.
    // At ~37.8 px/cm → 18.9 px per 5mm cell.
    const qreal pxPerCell = 18.9;
    return m_config.measureUnitsPerCell / qMax(1.0, pxPerCell);
  }

  void clearPreview(QGraphicsScene *scene) {
    if (!m_preview)
      return;
    if (scene)
      scene->removeItem(m_preview);
    delete m_preview;
    m_preview = nullptr;
  }

  void commitMeasurement(QGraphicsScene *scene) {
    if (!scene || m_points.size() < 2) {
      clearPreview(scene);
      m_points.clear();
      return;
    }
    if (m_config.dimensionKind == DimensionKind::Area && m_points.size() < 3) {
      clearPreview(scene);
      m_points.clear();
      return;
    }
    clearPreview(scene);
    QPainterPath path = buildFinal(m_points);
    // Bake the measurement text into the path so it survives the note
    // harvest (children of items are not persisted separately).
    const QString label = labelText(m_points);
    if (!label.isEmpty()) {
      QFont f;
      f.setPointSizeF(10);
      f.setBold(true);
      path.addText(labelAnchor(m_points), f, label);
    }
    auto *item = scene->addPath(
        path, QPen(QColor(30, 30, 36), 1.6, Qt::SolidLine, Qt::RoundCap,
                   Qt::RoundJoin));
    // Dark brush fills label glyphs + hatch strips. Area outline is an
    // open subpath so the polygon itself is not flooded solid.
    item->setBrush(QColor(30, 30, 36));
    item->setZValue(8);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->setData(0, QStringLiteral("shape_item"));
    item->setToolTip(label);
    m_lastCompletedItem = item;
    m_points.clear();
    emit contentModified();
  }

  QPainterPath buildPreview(const QVector<QPointF> &pts) const {
    return buildFinal(pts);
  }

  QPainterPath buildFinal(const QVector<QPointF> &pts) const {
    QPainterPath p;
    if (pts.size() < 2)
      return p;
    const DimensionKind kind = m_config.dimensionKind;
    if (kind == DimensionKind::Length) {
      const QPointF a = pts[0];
      const QPointF b = pts[pts.size() - 1];
      QLineF ln(a, b);
      // Extension ticks
      QLineF n = ln.normalVector();
      n.setLength(10);
      p.moveTo(a + (n.p2() - n.p1()));
      p.lineTo(a - (n.p2() - n.p1()));
      p.moveTo(b + (n.p2() - n.p1()));
      p.lineTo(b - (n.p2() - n.p1()));
      p.moveTo(a);
      p.lineTo(b);
      // Arrowheads
      auto tip = [&](QPointF from, QPointF to) {
        QLineF l(from, to);
        const qreal ang = l.angle();
        QLineF a1 = QLineF::fromPolar(8, ang + 150);
        QLineF a2 = QLineF::fromPolar(8, ang - 150);
        a1.translate(to);
        a2.translate(to);
        p.moveTo(to);
        p.lineTo(a1.p2());
        p.moveTo(to);
        p.lineTo(a2.p2());
      };
      tip(b, a);
      tip(a, b);
      const QPointF mid = ln.pointAt(0.5);
      p.addEllipse(mid, 2.5, 2.5);
      return p;
    }
    if (kind == DimensionKind::Angle3Point && pts.size() >= 3) {
      const QPointF v = pts[0];
      const QPointF a = pts[1];
      const QPointF b = pts[pts.size() - 1];
      p.moveTo(a);
      p.lineTo(v);
      p.lineTo(b);
      const qreal ang = QLineF(v, a).angleTo(QLineF(v, b));
      const qreal r =
          qMin(QLineF(v, a).length(), QLineF(v, b).length()) * 0.35;
      QPainterPath arc;
      const qreal start = QLineF(v, a).angle();
      arc.moveTo(v);
      arc.arcTo(QRectF(v.x() - r, v.y() - r, 2 * r, 2 * r), start, ang);
      p.addPath(arc);
      return p;
    }
    // Area: open outline (pen-drawn) + diagonal hatch clipped to polygon.
    p.moveTo(pts.first());
    for (int i = 1; i < pts.size(); ++i)
      p.lineTo(pts[i]);
    if (pts.size() >= 3)
      p.lineTo(pts.first()); // visually closed, not a filled subpath

    if (pts.size() >= 3) {
      QPainterPath poly;
      poly.moveTo(pts.first());
      for (int i = 1; i < pts.size(); ++i)
        poly.lineTo(pts[i]);
      poly.closeSubpath();

      const QRectF br = poly.boundingRect();
      const qreal step = 8.0;
      QPainterPathStroker stroker;
      stroker.setWidth(0.95);
      stroker.setCapStyle(Qt::FlatCap);
      // Diagonals ↘ clipped to the polygon → persistent hatch after harvest.
      for (qreal t = br.left() - br.height(); t < br.right() + br.height();
           t += step) {
        QPainterPath line;
        line.moveTo(t, br.top());
        line.lineTo(t + br.height(), br.bottom());
        p.addPath(poly.intersected(stroker.createStroke(line)));
      }

      // Vertex markers
      for (const QPointF &pt : pts)
        p.addEllipse(pt, 2.2, 2.2);

      // Close-ring hint at first vertex while still collecting.
      if (m_preview)
        p.addEllipse(pts.first(), closeRadius() * 0.45, closeRadius() * 0.45);
    }
    return p;
  }

  QString labelText(const QVector<QPointF> &pts) const {
    if (pts.size() < 2)
      return QString();
    const DimensionKind kind = m_config.dimensionKind;
    if (kind == DimensionKind::Length) {
      const qreal lenU = QLineF(pts.first(), pts.last()).length() * unitsPerPx();
      return QStringLiteral("%1").arg(lenU, 0, 'f', 2);
    }
    if (kind == DimensionKind::Angle3Point && pts.size() >= 3) {
      const qreal ang =
          QLineF(pts[0], pts[1]).angleTo(QLineF(pts[0], pts.last()));
      return QStringLiteral("%1°").arg(ang, 0, 'f', 1);
    }
    if (kind == DimensionKind::Area && pts.size() >= 3) {
      const double area = shoelaceAreaUnits(pts);
      return QStringLiteral("A=%1 u²").arg(area, 0, 'f', 2);
    }
    return QString();
  }

  double shoelaceAreaUnits(const QVector<QPointF> &pts) const {
    double area = 0.0;
    for (int i = 0; i < pts.size(); ++i) {
      const QPointF a = pts[i];
      const QPointF b = pts[(i + 1) % pts.size()];
      area += a.x() * b.y() - b.x() * a.y();
    }
    const qreal u = unitsPerPx();
    return qAbs(area) * 0.5 * u * u;
  }

  QPointF labelAnchor(const QVector<QPointF> &pts) const {
    const DimensionKind kind = m_config.dimensionKind;
    if (kind == DimensionKind::Length)
      return QLineF(pts.first(), pts.last()).pointAt(0.5) + QPointF(0, -12);
    if (kind == DimensionKind::Angle3Point && pts.size() >= 3)
      return pts[0] + QPointF(8, -8);
    // Place area label just above the bounding box so hatch stays readable.
    QRectF br;
    for (const QPointF &pt : pts)
      br |= QRectF(pt, QSizeF(0.1, 0.1));
    return QPointF(br.center().x() - 18, br.top() - 14);
  }

  QVector<QPointF> m_points;
  QGraphicsPathItem *m_preview{nullptr};
  QGraphicsScene *m_sceneRef{nullptr};
};
