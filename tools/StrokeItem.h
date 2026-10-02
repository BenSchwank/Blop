#pragma once
#include <QGraphicsPathItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QVector>

struct StrokePoint {
    QPointF pos;
    qreal pressure;
};

inline QPainterPath smoothStrokePath(const QVector<QPointF> &pts) {
    QPainterPath path;
    if (pts.isEmpty())
        return path;
    path.moveTo(pts[0]);
    if (pts.size() == 1)
        return path;
    if (pts.size() == 2) {
        path.lineTo(pts[1]);
        return path;
    }
    for (int i = 1; i < pts.size() - 1; ++i) {
        const QPointF mid((pts[i].x() + pts[i + 1].x()) * 0.5,
                          (pts[i].y() + pts[i + 1].y()) * 0.5);
        path.quadTo(pts[i], mid);
    }
    path.lineTo(pts.last());
    return path;
}

class StrokeItem : public QGraphicsPathItem {
public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    enum StrokeStyle { Normal, Highlighter, Eraser };

    StrokeItem(QPainterPath path, QPen pen, const QVector<StrokePoint>& points = QVector<StrokePoint>(), StrokeStyle style = Normal)
        : QGraphicsPathItem(path), m_points(points), m_style(style)
    {
        setPen(pen);
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
        
        // Performance Fix: Cache strokes as static images once fully constructed. 
        if (!m_points.isEmpty()) {
            // setCacheMode(QGraphicsItem::DeviceCoordinateCache); // DISABLED: causes infinite painter loops with DestinationOut/Eraser strokes!
        }
    }
    
    void addPoint(const StrokePoint& p) {
        m_points.append(p);
    }
    
    void setPoints(const QVector<StrokePoint>& points) {
        m_points = points;
    }
    
    QVector<StrokePoint> points() const {
        return m_points;
    }

    StrokeStyle strokeStyle() const {
        return m_style;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        painter->setRenderHint(QPainter::Antialiasing, true);
        if (m_style == Highlighter) {
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
        }
        else if (m_style == Eraser) {
            painter->setCompositionMode(QPainter::CompositionMode_DestinationOut);
        }

        if (pen().style() == Qt::NoPen || m_points.size() < 2 || m_style == Highlighter) {
            QStyleOptionGraphicsItem opt = *option;
            opt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
            QGraphicsPathItem::paint(painter, &opt, widget);
            return;
        }

        QPen segPen = pen();
        segPen.setCapStyle(Qt::RoundCap);
        segPen.setJoinStyle(Qt::RoundJoin);
        const qreal baseWidth = segPen.widthF();
        auto widthAt = [&](int seg) {
            const qreal avg = (m_points[seg].pressure + m_points[seg + 1].pressure) * 0.5;
            return baseWidth * qMax(0.1, avg);
        };

        painter->setBrush(Qt::NoBrush);
        const int lastSeg = m_points.size() - 2;
        for (int i = 0; i <= lastSeg; ) {
            const qreal w = widthAt(i);
            int j = i;
            while (j < lastSeg && qAbs(widthAt(j + 1) - w) <= 0.35)
                ++j;
            QVector<QPointF> pts;
            pts.reserve(j - i + 2);
            for (int k = i; k <= j + 1; ++k)
                pts.append(m_points[k].pos);
            segPen.setWidthF(w);
            painter->setPen(segPen);
            painter->drawPath(smoothStrokePath(pts));
            i = j + 1;
        }
    }

private:
    QVector<StrokePoint> m_points;
    StrokeStyle m_style;
};
