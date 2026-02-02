#pragma once
#include <QGraphicsObject>
#include <QPainter>
#include <QtMath>
#include "../ToolSettings.h"
#include "../UIStyles.h"

class RulerItem : public QGraphicsObject {
    Q_OBJECT
public:
    // ID für qgraphicsitem_cast
    enum { Type = UserType + 100 };
    int type() const override { return Type; }

    RulerItem(QGraphicsItem* parent = nullptr) : QGraphicsObject(parent) {
        setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        m_width = 800;
        m_height = 100;
        setTransformOriginPoint(m_width/2, m_height/2);
    }

    QRectF boundingRect() const override {
        return QRectF(-20, -20, m_width + 40, m_height + 40);
    }

    void setConfig(const ToolConfig& c) {
        m_config = c;
        update();
    }

    QPointF snapPoint(QPointF scenePos) const {
        QPointF localPos = sceneTransform().inverted().map(scenePos);
        if (m_config.compassMode) {
            // Im Kompassmodus ist jeder Punkt valide (wir zeichnen Bögen),
            // aber wir geben die Position zurück, damit AbstractStrokeTool den Radius kennt.
            return sceneTransform().map(localPos);
        } else {
            // Auf Linie einrasten
            qreal yTop = 0;
            qreal yBottom = m_height;
            if (std::abs(localPos.y() - yTop) < std::abs(localPos.y() - yBottom)) localPos.setY(yTop);
            else localPos.setY(yBottom);
            return sceneTransform().map(localPos);
        }
    }

    QPainterPath getSnapPath(QPointF sceneStart, QPointF sceneEnd) const {
        QPainterPath p;
        if (m_config.compassMode) {
            QPointF center = mapToScene(transformOriginPoint());
            QLineF lineStart(center, sceneStart);
            QLineF lineEnd(center, sceneEnd);
            double radius = lineStart.length();
            QRectF rect(center.x() - radius, center.y() - radius, radius*2, radius*2);
            double as = lineStart.angle();
            double ae = lineEnd.angle();
            double sweep = ae - as;
            while (sweep > 180) sweep -= 360;
            while (sweep < -180) sweep += 360;
            p.arcMoveTo(rect, as);
            p.arcTo(rect, as, sweep);
        } else {
            p.moveTo(sceneStart);
            p.lineTo(sceneEnd);
        }
        return p;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        painter->setRenderHint(QPainter::Antialiasing);
        QColor bg(40, 40, 40, 210);
        painter->setBrush(bg);
        painter->setPen(QPen(QColor(80, 80, 80), 1));
        painter->drawRoundedRect(0, 0, m_width, m_height, 4, 4);

        double pxPerUnit = 100.0;
        int subs = 10;
        if (m_config.rulerUnit == RulerUnit::Centimeter) { pxPerUnit = 37.795; subs = 10; }
        else if (m_config.rulerUnit == RulerUnit::Inch) { pxPerUnit = 96.0; subs = 8; }

        painter->setPen(QColor(220, 220, 220));
        drawTicks(painter, 0, pxPerUnit, subs, true);
        drawTicks(painter, m_height, pxPerUnit, subs, false);
        paintOverlayInfo(painter);

        if (m_config.compassMode) {
            QPointF c = transformOriginPoint();
            painter->setPen(QPen(QColor("#5E5CE6"), 2));
            painter->drawEllipse(c, 5, 5);
        }
    }

private:
    qreal m_width;
    qreal m_height;
    ToolConfig m_config;

    void paintOverlayInfo(QPainter* p) {
        p->save();
        p->translate(m_width/2, m_height/2);
        p->rotate(-rotation());
        p->setBrush(QColor(0,0,0,180));
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(-40, -15, 80, 30, 15, 15);
        p->setPen(Qt::white);
        p->setFont(QFont("Arial", 10, QFont::Bold));
        int deg = (int)rotation() % 360;
        if (deg < 0) deg += 360;
        p->drawText(QRectF(-40, -15, 80, 30), Qt::AlignCenter, QString("%1°").arg(deg));
        p->restore();
    }

    void drawTicks(QPainter* p, qreal y, double px, int subs, bool top) {
        double off = m_width / 2.0;
        int cnt = (int)(m_width / px) + 2;
        for (int i = -cnt/2; i <= cnt/2; ++i) {
            double x = off + i * px;
            if (x < 0 || x > m_width) continue;
            double h = 20.0;
            p->setPen(QPen(QColor(220, 220, 220), 1.5));
            p->drawLine(QPointF(x, top ? y : y-h), QPointF(x, top ? y+h : y));

            double subStep = px / subs;
            p->setPen(QPen(QColor(180, 180, 180), 1));
            for (int j = 1; j < subs; ++j) {
                double sx = x + j * subStep;
                if (sx > m_width) break;
                double sh = (j == subs/2) ? 12.0 : 7.0;
                p->drawLine(QPointF(sx, top ? y : y-sh), QPointF(sx, top ? y+sh : y));
            }
        }
    }
};
