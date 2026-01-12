#pragma once
#include <QGraphicsPathItem>
#include <QPainter>

class StrokeItem : public QGraphicsPathItem {
public:
    enum Type { Normal, Highlighter, Eraser };

    StrokeItem(QPainterPath path, QPen pen, Type type = Normal)
        : QGraphicsPathItem(path), m_type(type)
    {
        setPen(pen);
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);

        // WICHTIG: Damit das Lasso greift UND man es bewegen kann
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        if (m_type == Highlighter) {
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
        }
        else if (m_type == Eraser) {
            painter->setCompositionMode(QPainter::CompositionMode_DestinationOut);
        }

        QGraphicsPathItem::paint(painter, option, widget);
    }

    int type() const override {
        return QGraphicsItem::UserType + 1;
    }

private:
    Type m_type;
};
