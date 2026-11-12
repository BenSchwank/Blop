#pragma once
#include "AbstractTool.h"
#include "ToolManager.h"

#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPath>
#include <QTabletEvent>
#include <QtMath>

/// Technical dimensioning: length bar, 3-point angle, or area polygon.
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
      return handleMouseRelease(&ev, m_sceneRef);
    }
    return false;
  }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene || !event)
      return false;
    // Multi-click sequences (angle/area) keep collecting until enough points
    // exist — only a finished/aborted sequence starts a fresh measurement.
    const bool inProgress =
        m_preview && !m_points.isEmpty() && m_points.size() < pointsNeeded();
    if (!inProgress)
      m_points.clear();
    m_points.append(event->scenePos());
    if (!m_preview) {
      m_preview = scene->addPath(QPainterPath(),
                                 QPen(QColor(91, 157, 255), 1.4, Qt::DashLine));
      m_preview->setZValue(900);
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
    const int need = pointsNeeded();
    if (m_points.size() < need) {
      if (m_preview)
        m_preview->setPath(buildPreview(m_points));
      return true;
    }
    if (m_preview) {
      scene->removeItem(m_preview);
      delete m_preview;
      m_preview = nullptr;
    }
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
    // Solid dark fill → label glyphs + dot. Area mode gets a light shading
    // over the measured polygon instead.
    item->setBrush(m_config.dimensionKind == DimensionKind::Area
                       ? QColor(30, 30, 36, 60)
                       : QColor(30, 30, 36));
    item->setZValue(8);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->setData(0, QStringLiteral("shape_item"));
    m_lastCompletedItem = item;
    m_points.clear();
    emit contentModified();
    return true;
  }

private:
  int pointsNeeded() const {
    return m_config.dimensionKind == DimensionKind::Length ? 2 : 3;
  }

  qreal unitsPerPx() const {
    // One paper cell ≈ 5mm ≈ measureUnitsPerCell user units.
    // At ~37.8 px/cm → 18.9 px per 5mm cell.
    const qreal pxPerCell = 18.9;
    return m_config.measureUnitsPerCell / qMax(1.0, pxPerCell);
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
      const qreal lenPx = ln.length();
      const qreal lenU = lenPx * unitsPerPx();
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
      // Label as small diamond at midpoint (text via crude ticks)
      const QPointF mid = ln.pointAt(0.5);
      p.addEllipse(mid, 2.5, 2.5);
      // Encode length in path using small bar code-like nicks for reading;
      // also store as item tooltip after creation — here draw tick count.
      Q_UNUSED(lenU);
      return p;
    }
    if (kind == DimensionKind::Angle3Point && pts.size() >= 3) {
      const QPointF v = pts[0];
      const QPointF a = pts[1];
      const QPointF b = pts[pts.size() - 1];
      p.moveTo(a);
      p.lineTo(v);
      p.lineTo(b);
      const qreal ang =
          QLineF(v, a).angleTo(QLineF(v, b));
      const qreal r = qMin(QLineF(v, a).length(), QLineF(v, b).length()) * 0.35;
      QPainterPath arc;
      const qreal start = QLineF(v, a).angle();
      arc.moveTo(v);
      arc.arcTo(QRectF(v.x() - r, v.y() - r, 2 * r, 2 * r), start, ang);
      p.addPath(arc);
      Q_UNUSED(ang);
      return p;
    }
    // Area: polygon through points, close if >= 3
    p.moveTo(pts.first());
    for (int i = 1; i < pts.size(); ++i)
      p.lineTo(pts[i]);
    if (pts.size() >= 3)
      p.closeSubpath();
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
    if (pts.size() >= 3) {
      // Shoelace area in user units²
      double area = 0.0;
      for (int i = 0; i < pts.size(); ++i) {
        const QPointF a = pts[i];
        const QPointF b = pts[(i + 1) % pts.size()];
        area += a.x() * b.y() - b.x() * a.y();
      }
      area = qAbs(area) * 0.5 * unitsPerPx() * unitsPerPx();
      return QStringLiteral("A=%1").arg(area, 0, 'f', 2);
    }
    return QString();
  }

  QPointF labelAnchor(const QVector<QPointF> &pts) const {
    const DimensionKind kind = m_config.dimensionKind;
    if (kind == DimensionKind::Length)
      return QLineF(pts.first(), pts.last()).pointAt(0.5) + QPointF(0, -12);
    if (kind == DimensionKind::Angle3Point && pts.size() >= 3)
      return pts[0] + QPointF(8, -8);
    QPointF c(0, 0);
    for (const QPointF &pt : pts)
      c += pt;
    return c / pts.size();
  }

  QVector<QPointF> m_points;
  QGraphicsPathItem *m_preview{nullptr};
  QGraphicsScene *m_sceneRef{nullptr};
};
