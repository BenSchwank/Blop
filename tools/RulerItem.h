#pragma once
#include <QGraphicsObject>
#include <QPainter>
#include <QtMath>
#include "../ToolSettings.h"
#include "../UIStyles.h"

class RulerItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = UserType + 100 };
    int type() const override { return Type; }

    RulerItem(QGraphicsItem* parent = nullptr) : QGraphicsObject(parent) {
        setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        m_width = 600; // Standardbreite
        m_height = 80;
        setTransformOriginPoint(m_width/2, m_height/2); // Drehung um die Mitte
    }

    QRectF boundingRect() const override {
        // Etwas größerer Bereich für Schatten und Griffe
        return QRectF(-20, -20, m_width + 40, m_height + 40);
    }

    void setConfig(const ToolConfig& c) {
        m_config = c;
        update();
    }

    // Berechnet den Einrast-Punkt (Snapping)
    QPointF snapPoint(QPointF scenePos) const {
        // Transformiere Szenen-Punkt in lokales System des Lineals
        QPointF localPos = sceneTransform().inverted().map(scenePos);

        if (m_config.compassMode) {
            // Kompass-Modus: Einrasten auf den Kreisradius relativ zum Lineal-Ursprung
            // Wir nutzen die X-Position als Radius-Definition?
            // Besser: Wir nutzen den Abstand vom Zentrum des Lineals als Radius.
            // Aber für einen Zirkel ist der Radius meist fixiert durch eine Einstellung oder eine zweite Hand.
            // Hier vereinfacht: Der User zeichnet entlang der Kante -> Radius = Breite/2 ?
            // Nein, intuitive Lösung: Projektion auf die Linie, dann Rotation.

            // Für diesen Code implementieren wir: Der Stift rastet auf der Linie ein,
            // aber die BEWEGUNG (in AbstractStrokeTool) erzeugt einen Kreisbogen.
            return sceneTransform().map(localPos);
        } else {
            // Standard Lineal: Einrasten auf obere oder untere Kante
            qreal yTop = 0;
            qreal yBottom = m_height;

            // Zu welcher Kante ist die Maus näher?
            if (std::abs(localPos.y() - yTop) < std::abs(localPos.y() - yBottom)) {
                localPos.setY(yTop);
            } else {
                localPos.setY(yBottom);
            }

            // Optional: Begrenzung auf die Länge des Lineals
            // if (localPos.x() < 0) localPos.setX(0);
            // if (localPos.x() > m_width) localPos.setX(m_width);

            return sceneTransform().map(localPos);
        }
    }

    // Liefert den Pfad für die Linie (Gerade oder Bogen)
    QPainterPath getSnapPath(QPointF sceneStart, QPointF sceneEnd) const {
        QPainterPath p;
        if (m_config.compassMode) {
            // Zirkel-Logik: Zeichne Bogen um den Mittelpunkt des Lineals
            QPointF center = mapToScene(transformOriginPoint());
            QLineF lineStart(center, sceneStart);
            QLineF lineEnd(center, sceneEnd);

            double radius = lineStart.length();
            QRectF rect(center.x() - radius, center.y() - radius, radius*2, radius*2);

            double angleStart = lineStart.angle();
            double angleEnd = lineEnd.angle();
            double sweepLength = angleEnd - angleStart;

            // Normalisierung der Winkel für kürzesten Weg
            while (sweepLength > 180) sweepLength -= 360;
            while (sweepLength < -180) sweepLength += 360;

            p.arcMoveTo(rect, angleStart);
            p.arcTo(rect, angleStart, sweepLength);
        } else {
            // Lineal-Logik: Einfache gerade Linie
            p.moveTo(sceneStart);
            p.lineTo(sceneEnd);
        }
        return p;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        painter->setRenderHint(QPainter::Antialiasing);

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

        // 5. Kompass-Zentrum Indikator
        if (m_config.compassMode) {
            QPointF c = transformOriginPoint();
            painter->setPen(QPen(QColor("#5E5CE6"), 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(c, 6, 6);
            painter->drawLine(c.x()-4, c.y(), c.x()+4, c.y());
            painter->drawLine(c.x(), c.y()-4, c.x(), c.y()+4);
        }
    }

private:
    qreal m_width;
    qreal m_height;
    ToolConfig m_config;

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
