#pragma once
#include <QGraphicsRectItem>
#include <QPainter>

enum class PageBackgroundType {
    Blank,
    Lined,
    Grid,
    Dotted
};

class PageItem : public QGraphicsRectItem {
public:
    PageItem(qreal x, qreal y, qreal w, qreal h, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(x, y, w, h, parent) {
        setZValue(0);
    }

    void setType(PageBackgroundType type) {
        m_type = type;
        update();
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        // 1. Schlagschatten
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 40));
        painter->drawRect(rect().translated(3, 3));

        // 2. Papierblatt
        painter->setBrush(Qt::white);
        painter->setPen(Qt::NoPen);
        painter->drawRect(rect());

        // 3. Muster (Gitter)
        if (m_type == PageBackgroundType::Grid) {
            painter->setPen(QPen(QColor(230, 230, 245), 1));
            qreal left = rect().left();
            qreal right = rect().right();
            qreal top = rect().top();
            qreal bottom = rect().bottom();
            int spacing = 40;

            for (qreal x = left + spacing; x < right; x += spacing)
                painter->drawLine(QPointF(x, top), QPointF(x, bottom));
            for (qreal y = top + spacing; y < bottom; y += spacing)
                painter->drawLine(QPointF(left, y), QPointF(right, y));
        }
    }

private:
    PageBackgroundType m_type{PageBackgroundType::Grid};
};
