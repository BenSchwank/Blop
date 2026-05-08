#pragma once
#include <QGraphicsRectItem>
#include <QGraphicsItem>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QWidget>

enum class PageBackgroundType {
    Blank,
    Lined,
    Grid,
    Dotted,
    Legal
};

class PageItem : public QGraphicsRectItem {
public:
    PageItem(qreal x, qreal y, qreal w, qreal h, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(x, y, w, h, parent) {
        setZValue(0);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
        setFlag(QGraphicsItem::ItemIsMovable, false);
        // No cache mode: with the OpenGL viewport active in MultiPageNoteView
        // (Android), the bare paint is fast enough that caching adds zero
        // pan benefit while invalidating on every pinch-zoom step (which
        // made zoom janky in v3.15.4.111).
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

    void setPaperColor(const QColor &c) {
        m_paperColor = c.isValid() ? c : QColor(Qt::white);
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
        painter->setBrush(m_paperColor);
        painter->setPen(Qt::NoPen);
        painter->drawRect(rect());

        // 3. Hintergrundbild (z.B. importierte PDF-Seite)
        if (!m_backgroundImage.isNull()) {
#ifndef Q_OS_ANDROID
            // Skip bilinear filtering on Android - the page is rendered
            // through DeviceCoordinateCache anyway, and SmoothPixmapTransform
            // burns measurable frame time on phones with no visible benefit.
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
#endif
            painter->drawImage(rect(), m_backgroundImage);
        }

        // 4. Muster – nur wenn kein Hintergrundbild
        if (m_backgroundImage.isNull()) {
            const qreal left = rect().left();
            const qreal right = rect().right();
            const qreal top = rect().top();
            const qreal bottom = rect().bottom();
            const int L = m_paperColor.lightness();
            const QColor lineCol =
                L < 130 ? QColor(255, 255, 255, 50) : QColor(190, 190, 210, 110);

            switch (m_type) {
            case PageBackgroundType::Blank:
                break;
            case PageBackgroundType::Lined:
            case PageBackgroundType::Legal: {
                painter->setPen(QPen(lineCol, 1));
                const int lineSpacing = 40;
                for (qreal y = top + lineSpacing; y < bottom; y += lineSpacing)
                    painter->drawLine(QPointF(left, y), QPointF(right, y));
                if (m_type == PageBackgroundType::Legal) {
                    const qreal marginX = left + 72;
                    const QColor redMargin =
                        L < 130 ? QColor(255, 120, 120) : QColor(220, 80, 80);
                    painter->setPen(QPen(redMargin, 2));
                    painter->drawLine(QPointF(marginX, top), QPointF(marginX, bottom));
                }
                break;
            }
            case PageBackgroundType::Grid: {
                painter->setPen(QPen(lineCol, 1));
                const int spacing = 40;
                for (qreal x = left + spacing; x < right; x += spacing)
                    painter->drawLine(QPointF(x, top), QPointF(x, bottom));
                for (qreal y = top + spacing; y < bottom; y += spacing)
                    painter->drawLine(QPointF(left, y), QPointF(right, y));
                break;
            }
            case PageBackgroundType::Dotted: {
                painter->setPen(QPen(lineCol, 1));
                const int step = 28;
                for (qreal x = left + step; x < right; x += step)
                    for (qreal y = top + step; y < bottom; y += step)
                        painter->drawPoint(QPointF(x, y));
                break;
            }
            }
        }
    }

private:
    PageBackgroundType m_type{PageBackgroundType::Grid};
    QImage m_backgroundImage;
    QColor m_paperColor{Qt::white};
};

/// Leiste in der Szene direkt unter der letzten Seite (Anker für die Seiten-UI)
class SkeletonPageItem : public QGraphicsRectItem {
public:
    SkeletonPageItem(qreal x, qreal y, qreal w, qreal h, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(0, 0, w, h, parent), m_w(w), m_h(h) {
        setPos(x, y);
        setZValue(0);
        setAcceptedMouseButtons(Qt::NoButton);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, m_w, m_h); }

    QPainterPath shape() const override {
        QPainterPath p;
        p.addRect(boundingRect());
        return p;
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        const QRectF r = boundingRect();
        const qreal rad = 12.0;
        // Nur punktierter Rahmen (kleiner A4-Bereich) — kein Hintergrund
        painter->setRenderHint(QPainter::Antialiasing);
        QPen borderPen(QColor(QStringLiteral("#9A9AA8")), 1.0, Qt::DotLine);
        borderPen.setCosmetic(true);
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
    }

private:
    qreal m_w;
    qreal m_h;
};
