#pragma once
#include <QGraphicsPathItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QVector>

struct StrokePoint {
    QPointF pos;
    qreal pressure;
};

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
            setCacheMode(QGraphicsItem::DeviceCoordinateCache);
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
        if (m_style == Highlighter) {
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
        }
        else if (m_style == Eraser) {
            painter->setCompositionMode(QPainter::CompositionMode_DestinationOut);
        }

        if (!m_points.isEmpty() && m_points.size() > 1 && m_style != Highlighter) {
            QPen basePen = pen();
            qreal baseWidth = basePen.widthF();

            for (int i = 0; i < m_points.size() - 1; ++i) {
                const StrokePoint& p1 = m_points[i];
                const StrokePoint& p2 = m_points[i+1];
                
                // Pressure usually 0.0 to 1.0. Wir cappen bei 0.1, damit es nicht unsichtbar wird.
                qreal avgPressure = (p1.pressure + p2.pressure) / 2.0;
                qreal w = baseWidth * qMax(0.1, avgPressure);
                
                QPen segPen = basePen;
                segPen.setWidthF(w);
                painter->setPen(segPen);
                painter->drawLine(p1.pos, p2.pos);
            }
        } else {
            QStyleOptionGraphicsItem opt = *option;
            opt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
            QGraphicsPathItem::paint(painter, &opt, widget);
        }
    }

private:
    QVector<StrokePoint> m_points;
    StrokeStyle m_style;
};
