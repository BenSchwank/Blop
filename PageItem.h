#pragma once
#include <QGraphicsRectItem>
#include <QGraphicsItem>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>

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
        setFlag(QGraphicsItem::ItemIsSelectable, false);
        setFlag(QGraphicsItem::ItemIsMovable, false);
    }

    void setType(PageBackgroundType type) {
        m_type = type;
        update();
    }

    // Set a background image (e.g. a rendered PDF page)
    void setBackgroundImage(const QImage &img) {
        m_backgroundImage = img;
        update();
    }
    const QImage &backgroundImage() const { return m_backgroundImage; }

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

        // 3. Hintergrundbild (z.B. importierte PDF-Seite)
        if (!m_backgroundImage.isNull()) {
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter->drawImage(rect(), m_backgroundImage);
        }

        // 4. Muster (Gitter) - nur wenn kein Hintergrundbild
        if (m_backgroundImage.isNull() && m_type == PageBackgroundType::Grid) {
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
    QImage m_backgroundImage;
};
