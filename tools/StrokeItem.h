#pragma once
#include <QGraphicsPathItem>
#include <QPainter>

// Eine spezialisierte Item-Klasse, die Blend-Modes unterstützt
class StrokeItem : public QGraphicsPathItem {
public:
    enum Type { Normal, Highlighter, Eraser };

    StrokeItem(QPainterPath path, QPen pen, Type type = Normal)
        : QGraphicsPathItem(path), m_type(type)
    {
        setPen(pen);
        // Performance-Optimierung
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        if (m_type == Highlighter) {
            // MARKER: Multiplizieren-Modus (dunkelt ab, statt zu überdecken)
            painter->setCompositionMode(QPainter::CompositionMode_Multiply);
        }
        else if (m_type == Eraser) {
            // RADIERER: Stanzt ein Loch in alles (DestinationOut)
            painter->setCompositionMode(QPainter::CompositionMode_DestinationOut);
        }

        // Normales Zeichnen des Pfads
        QGraphicsPathItem::paint(painter, option, widget);
    }

    int type() const override {
        // Eindeutige ID für unsere Items (UserType + 1)
        return QGraphicsItem::UserType + 1;
    }

private:
    Type m_type;
};
