#pragma once
#include <QGraphicsObject>
#include <QPainter>
#include <QtMath>
#include "../ToolSettings.h"
#include "../UIStyles.h"
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsSceneMouseEvent>
#include <QVector2D>
#include <QElapsedTimer>

class RulerItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 100 };
    int type() const override { return Type; }

    RulerItem(QGraphicsItem* parent = nullptr) : QGraphicsObject(parent) {
        setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::LeftButton);
        // Let the view translate touch to mouse events for consistent drag/resize.
        setAcceptTouchEvents(false);
        m_width = 1200; // Längeres Standard-Lineal
        m_height = 80;
        setTransformOriginPoint(m_width/2, m_height/2); // Drehung um die Mitte
        m_tapTimer.start();
    }

    QRectF boundingRect() const override {
        // Keep both shapes fully inside the repaint area.
        if (m_config.compassMode) {
            const qreal r = qMin(m_width, m_height) * 0.42;
            const QPointF c = transformOriginPoint();
            return QRectF(c.x() - r - 28, c.y() - r - 28, (r + 28) * 2, (r + 28) * 2);
        }
        return QRectF(-20, -20, m_width + 40, m_height + 40);
    }

    void setConfig(const ToolConfig& c) {
        // Preserve current center point while changing ruler dimensions.
        const QPointF oldCenterScene = mapToScene(transformOriginPoint());
        prepareGeometryChange();
        m_config = c;
        recalcSizeFromConfig();
        const QPointF newCenterScene = mapToScene(transformOriginPoint());
        setPos(pos() + (oldCenterScene - newCenterScene));
        update();
    }

    // Berechnet den Einrast-Punkt (Snapping)
    QPointF snapPoint(QPointF scenePos, QPointF strokeStart = QPointF()) const {
        if (m_config.compassMode) {
            // Kompass-Modus: Einrasten auf den Kreisradius relativ zum Lineal-Ursprung
            QPointF c = mapToScene(transformOriginPoint());
            if (strokeStart.isNull()) return scenePos; // On press, do not snap

            QLineF radiusLine(c, strokeStart);
            double r = radiusLine.length();

            QLineF currentLine(c, scenePos);
            currentLine.setLength(r);
            return currentLine.p2();
        } else {
            // Standard Lineal: Einrasten auf obere oder untere Kante
            QPointF localPos = sceneTransform().inverted().map(scenePos);
            qreal yTop = 0;
            qreal yBottom = m_height;

            if (!strokeStart.isNull()) {
                // Lock onto the edge where we started
                QPointF localStart = sceneTransform().inverted().map(strokeStart);
                if (std::abs(localStart.y() - yTop) < std::abs(localStart.y() - yBottom)) {
                    localPos.setY(yTop);
                } else {
                    localPos.setY(yBottom);
                }
            } else {
                // Initial snap
                if (std::abs(localPos.y() - yTop) < std::abs(localPos.y() - yBottom)) {
                    localPos.setY(yTop);
                } else {
                    localPos.setY(yBottom);
                }
            }

            return sceneTransform().map(localPos);
        }
    }

    // (getSnapPath was removed as it is no longer needed since points snap individually)

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        painter->setRenderHint(QPainter::Antialiasing);

        if (m_config.compassMode) {
            paintCircularRuler(painter);
            return;
        }

        // 1. Hintergrund (Halbtransparent dunkel)
        QColor bg(40, 40, 40, 220);
        painter->setBrush(bg);
        painter->setPen(QPen(QColor(80, 80, 80), 1));
        painter->drawRoundedRect(0, 0, m_width, m_height, 4, 4);

        // 2. Skalierung berechnen
        double pxPerUnit = 100.0; // Default Pixel
        int subdivisions = 10;
        QString unitLabel = "px";

        if (m_config.rulerUnit == RulerUnit::Centimeter) {
            pxPerUnit = 37.795; // ~96 DPI
            subdivisions = 10;
            unitLabel = "cm";
        } else if (m_config.rulerUnit == RulerUnit::Inch) {
            pxPerUnit = 96.0;
            subdivisions = 8;
            unitLabel = "in";
        }

        // 3. Markierungen zeichnen (Oben und Unten)
        painter->setPen(QColor(220, 220, 220));
        drawTicks(painter, 0, pxPerUnit, subdivisions, true);        // Oben
        drawTicks(painter, m_height, pxPerUnit, subdivisions, false); // Unten

        // 4. Info-Overlay (Winkel) in der Mitte
        paintOverlayInfo(painter);

    }

    void wheelEvent(QGraphicsSceneWheelEvent* event) override {
        double currentAngle = rotation();
        // Optionale Modifikation durch Shift-Taste
        double step = (event->modifiers() & Qt::ShiftModifier) ? 15.0 : 1.0;
        
        if (event->delta() > 0) {
            currentAngle -= step;
        } else {
            currentAngle += step;
        }
        
        // Winkel normalisieren (0-360)
        while (currentAngle >= 360) currentAngle -= 360;
        while (currentAngle < 0) currentAngle += 360;
        
        setRotation(currentAngle);
        event->accept();
    }

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override {
        Q_UNUSED(event);
        // UX: double tap/click arms a single fluid drag gesture.
        m_dragReady = true;
        update();
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) {
            QGraphicsObject::mousePressEvent(event);
            return;
        }

        const QPointF local = sceneTransform().inverted().map(event->scenePos());
        if (!m_config.compassMode) {
            const qreal handlePad = 44.0; // easier touch hitbox on tablets
            const bool onLeft = (local.x() <= handlePad && local.y() >= -20 && local.y() <= m_height + 20);
            const bool onRight = (local.x() >= m_width - handlePad && local.y() >= -20 && local.y() <= m_height + 20);
            if (onLeft || onRight) {
                m_resizingLeft = onLeft;
                m_resizingRight = onRight;
                m_dragReady = false;
                // Freeze opposite anchor in scene coordinates during resizing.
                m_anchorScene = onLeft
                    ? mapToScene(QPointF(m_width, m_height / 2))
                    : mapToScene(QPointF(0, m_height / 2));
                event->accept();
                return;
            }
        }

        // Double-tap to move: either native double-click or touch fallback.
        const qint64 elapsedMs = m_tapTimer.isValid() ? m_tapTimer.elapsed() : 100000;
        const bool nearTap = QLineF(event->scenePos(), m_lastTapScenePos).length() <= 48.0;
        if (elapsedMs <= 420 && nearTap) {
            m_dragReady = true;
        }
        m_lastTapScenePos = event->scenePos();
        m_tapTimer.restart();

        if (m_dragReady) {
            m_draggingBody = true;
            m_dragOffsetScene = event->scenePos() - pos();
            event->accept();
            return;
        }

        QGraphicsObject::mousePressEvent(event);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (m_draggingBody) {
            setPos(event->scenePos() - m_dragOffsetScene);
            event->accept();
            return;
        }

        if (m_resizingLeft || m_resizingRight) {
            resizeFromHandle(event->scenePos());
            event->accept();
            return;
        }

        QGraphicsObject::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        m_draggingBody = false;
        m_resizingLeft = false;
        m_resizingRight = false;

        // drag-ready is intentionally one-shot to avoid accidental drags later
        m_dragReady = false;
        update();
        QGraphicsObject::mouseReleaseEvent(event);
    }

private:
    qreal m_width;
    qreal m_height;
    ToolConfig m_config;
    bool m_dragReady{false};
    bool m_draggingBody{false};
    bool m_resizingLeft{false};
    bool m_resizingRight{false};
    QPointF m_dragOffsetScene;
    QPointF m_anchorScene;
    QElapsedTimer m_tapTimer;
    QPointF m_lastTapScenePos;

    void resizeFromHandle(const QPointF& scenePos) {
        // Axis direction of the ruler in scene space.
        QPointF left = mapToScene(QPointF(0, m_height / 2));
        QPointF right = mapToScene(QPointF(m_width, m_height / 2));
        QVector2D axis(right - left);
        if (axis.length() < 0.001f) return;
        axis.normalize();

        qreal newW = m_width;
        const qreal minW = 300.0;
        const qreal maxW = 7000.0;

        if (m_resizingRight) {
            QVector2D v(scenePos - m_anchorScene);
            newW = qBound(minW, (qreal)QVector2D::dotProduct(v, axis), maxW);
            setWidthKeepingAnchor(newW, true /*keepLeftAnchor*/);
        } else if (m_resizingLeft) {
            QVector2D v(m_anchorScene - scenePos);
            newW = qBound(minW, (qreal)QVector2D::dotProduct(v, axis), maxW);
            setWidthKeepingAnchor(newW, false /*keepRightAnchor*/);
        }
    }

    void setWidthKeepingAnchor(qreal newW, bool keepLeftAnchor) {
        QPointF fixedAnchor = keepLeftAnchor
            ? mapToScene(QPointF(0, m_height / 2))
            : mapToScene(QPointF(m_width, m_height / 2));

        prepareGeometryChange();
        m_width = newW;
        setTransformOriginPoint(m_width / 2, m_height / 2);

        QPointF movedAnchor = keepLeftAnchor
            ? mapToScene(QPointF(0, m_height / 2))
            : mapToScene(QPointF(m_width, m_height / 2));
        setPos(pos() + (fixedAnchor - movedAnchor));
        update();
    }

    void recalcSizeFromConfig() {
        if (m_config.compassMode) {
            // Circle mode size remains stable and compact.
            m_width = 520;
            m_height = 520;
        } else if (m_config.infiniteRuler && scene()) {
            const qreal sw = scene()->sceneRect().width();
            // Span most of the page width, with sensible bounds.
            m_width = qBound<qreal>(1800.0, sw * 2.0, 6000.0);
            m_height = 80;
        } else {
            // Longer normal ruler than before.
            m_width = 1200;
            m_height = 80;
        }
        setTransformOriginPoint(m_width / 2, m_height / 2);
    }

    void paintCircularRuler(QPainter* p) {
        const QPointF c = transformOriginPoint();
        const qreal radius = qMin(m_width, m_height) * 0.42;

        // Outer ring
        p->setBrush(QColor(40, 40, 40, 220));
        p->setPen(QPen(QColor(90, 90, 90), 1.2));
        p->drawEllipse(c, radius, radius);

        // Inner cutout to visualize a ring ruler
        p->setBrush(QColor(18, 18, 26, 220));
        p->setPen(Qt::NoPen);
        p->drawEllipse(c, radius - 18, radius - 18);

        // Degree / unit ticks around the ring
        p->setBrush(Qt::NoBrush);
        for (int deg = 0; deg < 360; deg += 2) {
            const bool major = (deg % 30 == 0);
            const bool mid = (!major && deg % 10 == 0);
            const qreal a = qDegreesToRadians((qreal)deg - 90.0);
            const qreal len = major ? 14.0 : (mid ? 9.0 : 5.0);

            const QPointF p1(c.x() + (radius - len) * std::cos(a),
                             c.y() + (radius - len) * std::sin(a));
            const QPointF p2(c.x() + radius * std::cos(a),
                             c.y() + radius * std::sin(a));

            p->setPen(QPen(major ? QColor(235, 235, 235) : QColor(170, 170, 170),
                           major ? 1.8 : 1.0));
            p->drawLine(p1, p2);
        }

        // Center indicator (for snapping radius)
        p->setPen(QPen(QColor("#5E5CE6"), 2));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(c, 6, 6);
        p->drawLine(c.x() - 4, c.y(), c.x() + 4, c.y());
        p->drawLine(c.x(), c.y() - 4, c.x(), c.y() + 4);
    }

    void paintOverlayInfo(QPainter* p) {
        p->save();
        p->translate(m_width/2, m_height/2);

        // Text soll immer horizontal lesbar sein, also Gegenrotation
        p->rotate(-rotation());

        // Hintergrund Kapsel
        p->setBrush(QColor(0,0,0,180));
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(-35, -12, 70, 24, 12, 12);

        // Text (Winkel)
        p->setPen(Qt::white);
        p->setFont(QFont("Segoe UI", 9, QFont::Bold));

        int deg = (int)rotation() % 360;
        if (deg < 0) deg += 360;

        p->drawText(QRectF(-35, -12, 70, 24), Qt::AlignCenter, QString("%1°").arg(deg));
        p->restore();
    }

    void drawTicks(QPainter* p, qreal y, double pxPerUnit, int subs, bool isTop) {
        double centerX = m_width / 2.0;

        // Wie viele Einheiten passen links und rechts hin?
        int unitsCount = (int)(m_width / pxPerUnit / 2) + 1;

        for (int i = -unitsCount; i <= unitsCount; ++i) {
            double x = centerX + i * pxPerUnit;
            if (x < 0 || x > m_width) continue;

            // Hauptmarkierung (Groß)
            double hMain = 15.0;
            p->setPen(QPen(QColor(220, 220, 220), 1.5));
            p->drawLine(QPointF(x, isTop ? y : y-hMain), QPointF(x, isTop ? y+hMain : y));

            // Unterteilungen
            double subStep = pxPerUnit / subs;
            p->setPen(QPen(QColor(160, 160, 160), 1));

            for (int j = 1; j < subs; ++j) {
                double sx = x + j * subStep;
                if (sx > m_width) break; // Nicht über Rand zeichnen bei letztem Segment

                // Mittlere Unterteilung etwas größer
                double hSub = (j == subs/2) ? 10.0 : 6.0;
                p->drawLine(QPointF(sx, isTop ? y : y-hSub), QPointF(sx, isTop ? y+hSub : y));
            }
        }
    }
};
