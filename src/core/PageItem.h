#pragma once
#include "blop_theme.h"
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

        const bool light = !BlopTheme::instance().isDark();

        // 1. Soft layered shadow (no GraphicsEffect — keeps scene paint cheap).
        painter->setPen(Qt::NoPen);
        if (light) {
            painter->setBrush(QColor(15, 23, 42, int(0.04 * 255)));
            painter->drawRect(rect().translated(3, 4));
            painter->setBrush(QColor(15, 23, 42, int(0.07 * 255)));
            painter->drawRect(rect().translated(1, 2));
        } else {
            // ~0 10 30 rgba(0,0,0,0.35) as stacked fills.
            painter->setBrush(QColor(0, 0, 0, 30));
            painter->drawRect(rect().translated(6, 10));
            painter->setBrush(QColor(0, 0, 0, 50));
            painter->drawRect(rect().translated(3, 5));
            painter->setBrush(QColor(0, 0, 0, 70));
            painter->drawRect(rect().translated(1, 2));
        }

        // 2. Papierblatt
        painter->setBrush(m_paperColor);
        if (light)
            painter->setPen(QPen(QColor(0xCB, 0xD5, 0xE1), 1));
        else
            painter->setPen(QPen(QColor(0, 0, 0, 51), 1));
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
                L < 130 ? QColor(255, 255, 255, 60) : QColor(150, 160, 190, 150);

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
                // Engineer/millimeter paper: minor + every 5th major.
                const qreal margin = 24.0;
                const qreal gl = left + margin;
                const qreal gr = right - margin;
                const qreal gt = top + margin;
                const qreal gb = bottom - margin;
                if (gr <= gl || gb <= gt)
                    break;
                const int spacing = 40; // ~5mm at A4 scale
                const QColor minor =
                    L < 130 ? QColor(255, 255, 255, 60) : QColor(0xA8, 0xBB, 0xD8);
                const QColor major =
                    L < 130 ? QColor(255, 255, 255, 96) : QColor(0x7E, 0x9C, 0xC6);
                int ix = 0;
                for (qreal x = gl; x <= gr + 0.5; x += spacing, ++ix) {
                    const bool isMajor = (ix % 5) == 0;
                    painter->setPen(QPen(isMajor ? major : minor,
                                       isMajor ? 1.6 : 1.2));
                    painter->drawLine(QPointF(x, gt), QPointF(x, gb));
                }
                int iy = 0;
                for (qreal y = gt; y <= gb + 0.5; y += spacing, ++iy) {
                    const bool isMajor = (iy % 5) == 0;
                    painter->setPen(QPen(isMajor ? major : minor,
                                       isMajor ? 1.6 : 1.2));
                    painter->drawLine(QPointF(gl, y), QPointF(gr, y));
                }
                // Quiet margin hairline — NoBrush, sonst übermalt die Füllung
                // das Gitter (Brush war zuletzt das Papier).
                painter->setPen(QPen(L < 130 ? QColor(255, 255, 255, 28)
                                             : QColor(0xCB, 0xD5, 0xE1, 120),
                                    1.0));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(QRectF(QPointF(gl, gt), QPointF(gr, gb)));
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
        // Gleiche Sprache wie das Pages-Bar-Sheet: weisse Papier-Karte,
        // NoteChrome-Accent. Waehrend des Pulls: Fortschrittsring um das
        // Plus + Balken an der Unterkante, damit der Widerstand lesbar ist.
        const QRectF r = boundingRect().adjusted(1.0, 1.0, -1.0, -1.0);
        painter->setRenderHint(QPainter::Antialiasing);
        const qreal pull = m_pullProgress;
        const QColor accent(91, 157, 255); // NoteChrome::accent
        const QColor muted(100, 116, 139);
        const qreal rad = 14.0;

        // 1. Karte
        QPen cardPen(QColor(0xE3, 0xE3, 0xE3), 1.0);
        cardPen.setCosmetic(true);
        painter->setPen(cardPen);
        painter->setBrush(QColor(0xFF, 0xFF, 0xFF));
        painter->drawRoundedRect(r, rad, rad);
        if (pull > 0.0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(239, 246, 255, int(255 * pull)));
            painter->drawRoundedRect(r, rad, rad);
        }

        // 2. Gestrichelter CTA-Rahmen innen (wie der Sheet-Button)
        const QRectF cta = r.adjusted(22.0, 22.0, -22.0, -34.0);
        QPen ctaPen(pull > 0.0 ? accent : QColor(91, 157, 255, 170), 1.5,
                    pull >= 1.0 ? Qt::SolidLine : Qt::DashLine);
        ctaPen.setCosmetic(true);
        painter->setPen(ctaPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(cta, 10.0, 10.0);

        // 3. Fortschrittsring + Plus
        const qreal ringR = 26.0;
        const QPointF c(cta.center().x(), cta.top() + cta.height() * 0.40);
        const QRectF ring(c.x() - ringR, c.y() - ringR, 2 * ringR, 2 * ringR);
        painter->setPen(QPen(QColor(91, 157, 255, 50), 5.0));
        painter->drawEllipse(ring);
        if (pull > 0.0) {
            QPen arcPen(accent, 5.0, Qt::SolidLine, Qt::RoundCap);
            painter->setPen(arcPen);
            painter->drawArc(ring, 90 * 16, -int(360 * 16 * pull));
        }
        QPen plusPen(pull > 0.0 ? accent : muted, 3.5, Qt::SolidLine,
                     Qt::RoundCap);
        painter->setPen(plusPen);
        const qreal pl = 10.0;
        painter->drawLine(QPointF(c.x() - pl, c.y()), QPointF(c.x() + pl, c.y()));
        painter->drawLine(QPointF(c.x(), c.y() - pl), QPointF(c.x(), c.y() + pl));

        // 4. Beschriftung — feste Groesse (skaliert mit dem Zoom, nicht mit
        //    der Kartenhoehe).
        const QString caption =
            pull <= 0.0   ? QStringLiteral("Neue Seite")
            : pull < 1.0  ? QStringLiteral("Weiter ziehen f\u00FCr neue Seite")
                          : QStringLiteral("Neue Seite wird angelegt");
        QFont capFont = painter->font();
        capFont.setPixelSize(17);
        capFont.setWeight(QFont::DemiBold);
        painter->setFont(capFont);
        painter->setPen(pull > 0.0 ? accent : muted);
        const QRectF capRect(cta.left(), c.y() + ringR + 8.0, cta.width(), 28.0);
        painter->drawText(capRect, Qt::AlignHCenter | Qt::AlignTop, caption);

        // 5. Fortschrittsbalken an der Unterkante
        const QRectF track(r.left() + 22.0, r.bottom() - 20.0,
                           r.width() - 44.0, 6.0);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(91, 157, 255, pull > 0.0 ? 45 : 0));
        painter->drawRoundedRect(track, 3.0, 3.0);
        if (pull > 0.0) {
            painter->setBrush(accent);
            painter->drawRoundedRect(
                QRectF(track.left(), track.top(), track.width() * pull,
                       track.height()),
                3.0, 3.0);
        }
    }

private:
    qreal m_w;
    qreal m_h;
    double m_pullProgress{0.0};
};
