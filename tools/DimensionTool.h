#pragma once
#include "AbstractTool.h"
#include "ToolManager.h"

#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QtMath>

/// Technical dimensioning: length bar, 3-point angle, or area polygon.
class DimensionTool : public AbstractTool {
public:
  using AbstractTool::AbstractTool;

  ToolMode mode() const override { return ToolMode::Dimension; }
  QString name() const override { return QStringLiteral("Bemaßung"); }
  QString iconName() const override { return QStringLiteral("ruler"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene || !event)
      return false;
    m_points.clear();
    m_points.append(event->scenePos());
    if (m_preview) {
      scene->removeItem(m_preview);
      delete m_preview;
      m_preview = nullptr;
    }
    m_preview = scene->addPath(QPainterPath(),
                               QPen(QColor(91, 157, 255), 1.4, Qt::DashLine));
    m_preview->setZValue(900);
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
    m_points.append(event->scenePos());
    const DimensionKind kind = m_config.dimensionKind;
    const int need = (kind == DimensionKind::Length)   ? 2
                     : (kind == DimensionKind::Angle3Point) ? 3
                                                           : 3;
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
    auto *item = scene->addPath(buildFinal(m_points),
                                QPen(QColor(30, 30, 36), 1.6, Qt::SolidLine,
                                     Qt::RoundCap, Qt::RoundJoin));
    item->setZValue(8);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    if (auto *lbl = makeLabel(m_points)) {
      lbl->setParentItem(item);
      lbl->setZValue(9);
    }
    m_lastCompletedItem = item;
    m_points.clear();
    emit contentModified();
    return true;
  }

private:
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

  QGraphicsSimpleTextItem *makeLabel(const QVector<QPointF> &pts) const {
    if (pts.size() < 2)
      return nullptr;
    const DimensionKind kind = m_config.dimensionKind;
    QString text;
    QPointF anchor;
    if (kind == DimensionKind::Length) {
      const qreal lenU = QLineF(pts.first(), pts.last()).length() * unitsPerPx();
      text = QStringLiteral("%1").arg(lenU, 0, 'f', 2);
      anchor = QLineF(pts.first(), pts.last()).pointAt(0.5) + QPointF(0, -12);
    } else if (kind == DimensionKind::Angle3Point && pts.size() >= 3) {
      const qreal ang =
          QLineF(pts[0], pts[1]).angleTo(QLineF(pts[0], pts.last()));
      text = QStringLiteral("%1°").arg(ang, 0, 'f', 1);
      anchor = pts[0] + QPointF(8, -8);
    } else if (pts.size() >= 3) {
      // Shoelace area in user units²
      double area = 0.0;
      for (int i = 0; i < pts.size(); ++i) {
        const QPointF a = pts[i];
        const QPointF b = pts[(i + 1) % pts.size()];
        area += a.x() * b.y() - b.x() * a.y();
      }
      area = qAbs(area) * 0.5 * unitsPerPx() * unitsPerPx();
      text = QStringLiteral("A=%1").arg(area, 0, 'f', 2);
      QPointF c(0, 0);
      for (const QPointF &pt : pts)
        c += pt;
      anchor = c / pts.size();
    } else {
      return nullptr;
    }
    auto *lbl = new QGraphicsSimpleTextItem(text);
    lbl->setBrush(QColor(20, 20, 28));
    QFont f = lbl->font();
    f.setPointSizeF(10);
    f.setBold(true);
    lbl->setFont(f);
    lbl->setPos(anchor);
    return lbl;
  }

  QVector<QPointF> m_points;
  QGraphicsPathItem *m_preview{nullptr};
};
