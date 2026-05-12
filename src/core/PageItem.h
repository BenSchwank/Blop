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
#ifdef Q_OS_ANDROID
        // ItemCoordinateCache renders the page once into an item-local
        // pixmap of ~A4-at-1.0x. View-zoom only blits/scales that pixmap
        // (cheap GPU op), and pan does the same — no cache invalidation
        // per pinch step (which DeviceCoordinateCache would force).
        setCacheMode(QGraphicsItem::ItemCoordinateCache, QSize(800, 1100));
#endif
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

/// "Plus"-Karte direkt unter der letzten Seite. Sichtbar als Affordance fuer
/// "Neue Seite", dient gleichzeitig als Ankerleiste fuer die Pages-UI. Wird
/// per Klick / Tap von MultiPageNoteView in addNewPage() gewandelt.
///
/// v3.16.1: vorher war das nur ein dotted-border Schlitz; jetzt zeichnen wir
/// ein zentriertes "+" plus Caption "Neue Seite", damit User auf den ersten
/// Blick sehen, dass hier eine Aktion lebt. Der Pull-Up-Zustand (m_pullProgress
/// 0..1) modifiziert Label und Skalierung.
class SkeletonPageItem : public QGraphicsRectItem {
public:
    SkeletonPageItem(qreal x, qreal y, qreal w, qreal h, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(0, 0, w, h, parent), m_w(w), m_h(h) {
        setPos(x, y);
        setZValue(0);
        // Akzeptiere Mouse-Events so dass MultiPageNoteView::mousePressEvent
        // den Klick detektieren kann (ueber QGraphicsView->itemAt).
        setAcceptedMouseButtons(Qt::LeftButton);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, m_w, m_h); }

    QPainterPath shape() const override {
        QPainterPath p;
        p.addRect(boundingRect());
        return p;
    }

    void setPullProgress(double p) {
        const double v = qBound(0.0, p, 1.0);
        if (qFuzzyCompare(v + 1.0, m_pullProgress + 1.0)) return;
        m_pullProgress = v;
        update();
    }
    double pullProgress() const { return m_pullProgress; }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        const QRectF r = boundingRect();
        const qreal rad = 14.0;
        painter->setRenderHint(QPainter::Antialiasing);

        // v3.16.1: hover/pull-progress tint -> the card lights up as the user
        // pulls past the page bottom, signalling "release to create".
        const qreal pull = m_pullProgress;
        QColor border = pull > 0.0
                            ? QColor::fromRgbF(0.49, 0.36, 0.99, 0.55 + 0.45 * pull)
                            : QColor(QStringLiteral("#9A9AA8"));
        QPen borderPen(border, 1.0 + pull * 1.4, Qt::DashLine);
        borderPen.setCosmetic(true);
        painter->setPen(borderPen);

        // Soft fill that fades in with the pull.
        if (pull > 0.0) {
            QColor fill = QColor::fromRgbF(0.49, 0.36, 0.99, 0.06 * pull);
            painter->setBrush(fill);
        } else {
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);

        // Centered "+" glyph + caption. Painted in scene units so it scales
        // with the rest of the page. The caption changes once pull crosses
        // the threshold (>= 0.7 ~ 80dp pull).
        const QString caption = (pull >= 0.7)
                                    ? QStringLiteral("Loslassen f\u00FCr neue Seite")
                                    : QStringLiteral("Neue Seite");
        const QColor txtColor = pull > 0.0
                                    ? QColor::fromRgbF(0.49, 0.36, 0.99, 1.0)
                                    : QColor(QStringLiteral("#7A7A88"));
        painter->setPen(txtColor);
        QFont plusFont = painter->font();
        plusFont.setPointSizeF(qMax(14.0, r.height() * 0.32));
        plusFont.setWeight(QFont::Bold);
        painter->setFont(plusFont);
        const QRectF plusRect(r.left(), r.top() + r.height() * 0.10,
                              r.width(), r.height() * 0.55);
        painter->drawText(plusRect, Qt::AlignCenter, QStringLiteral("+"));

        QFont capFont = painter->font();
        capFont.setPointSizeF(qMax(9.0, r.height() * 0.12));
        capFont.setWeight(QFont::Medium);
        painter->setFont(capFont);
        const QRectF capRect(r.left(), r.top() + r.height() * 0.60,
                             r.width(), r.height() * 0.30);
        painter->drawText(capRect, Qt::AlignCenter, caption);
    }

private:
    qreal m_w;
    qreal m_h;
    double m_pullProgress{0.0};
};
