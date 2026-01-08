#pragma once
#include "AbstractStrokeTool.h"
#include "StrokeItem.h"
#include <QBrush>
#include <QRandomGenerator>
#include <QMap>
#include <QPixmap>
#include <QPainter>

// === FIX: 'inline' statt 'static' verhindert den LNK1163 Fehler! ===
inline QPixmap getNoiseTexture(const QColor& baseColor, int densityValue) {
    QString key = QString("noise_%1_%2_%3").arg(baseColor.name()).arg(baseColor.alpha()).arg(densityValue);

    // Statischer Cache ist okay innerhalb einer Inline-Funktion
    static QMap<QString, QPixmap> cache;
    if(cache.contains(key)) return cache[key];

    int size = 64;
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    int threshold = densityValue;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (QRandomGenerator::global()->bounded(100) >= threshold) {
                QColor noiseCol = baseColor;
                int alphaVar = QRandomGenerator::global()->bounded(40);
                noiseCol.setAlpha(std::max(0, baseColor.alpha() - alphaVar));

                p.setPen(noiseCol);
                p.drawPoint(x, y);
            }
        }
    }

    QPixmap pm = QPixmap::fromImage(img);
    cache.insert(key, pm);
    return pm;
}

// 1. FÜLLER
class PenTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;
    ToolMode mode() const override { return ToolMode::Pen; }
    QString name() const override { return "Füller"; }
    QString iconName() const override { return "pen"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* e, QGraphicsScene* s) override {
        AbstractStrokeTool::handleMousePress(e, s);
        if(m_currentItem) {
            s->removeItem(m_currentItem); delete m_currentItem;
            StrokeItem* si = new StrokeItem(m_currentPath, createPen(), StrokeItem::Normal);
            si->setZValue(getZValue());
            s->addItem(si);
            m_currentItem = si;
        }
        return true;
    }

protected:
    QPen createPen() const override {
        QColor c = m_config.penColor;
        c.setAlphaF(m_config.opacity);
        QPen pen(c, m_config.penWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setStyle(Qt::SolidLine);
        return pen;
    }
    qreal getZValue() const override { return 10; }
};

// 2. BLEISTIFT
class PencilTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;
    ToolMode mode() const override { return ToolMode::Pencil; }
    QString name() const override { return "Bleistift"; }
    QString iconName() const override { return "pencil"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* e, QGraphicsScene* s) override {
        AbstractStrokeTool::handleMousePress(e, s);
        if(m_currentItem) {
            s->removeItem(m_currentItem); delete m_currentItem;
            StrokeItem* si = new StrokeItem(m_currentPath, createPen(), StrokeItem::Normal);
            si->setZValue(getZValue());
            s->addItem(si);
            m_currentItem = si;
        }
        return true;
    }

protected:
    QPen createPen() const override {
        QColor c = m_config.penColor;
        double hardnessFactor = (m_config.hardness / 100.0) * 0.5;
        double finalAlpha = m_config.opacity * (1.0 - hardnessFactor);
        c.setAlphaF(std::max(0.05, finalAlpha));

        QPen pen;
        pen.setWidth(m_config.penWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);

        int noiseDensity = 50;
        if (m_config.texture == "Fein") noiseDensity = 25;
        else if (m_config.texture == "Grob") noiseDensity = 65;
        else if (m_config.texture == "Kohle") { noiseDensity = 80; pen.setWidth(pen.width() * 1.2); }

        QPixmap noisePx = getNoiseTexture(c, noiseDensity);
        pen.setBrush(QBrush(noisePx));
        pen.setStyle(Qt::SolidLine);
        return pen;
    }
    qreal getZValue() const override { return 15; }
};

// 3. MARKER
class HighlighterTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;
    ToolMode mode() const override { return ToolMode::Highlighter; }
    QString name() const override { return "Marker"; }
    QString iconName() const override { return "highlighter"; }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (m_config.smartLine && m_currentItem && !m_pointsBuffer.isEmpty()) {
            QPointF start = m_pointsBuffer.first();
            QPointF current = event->scenePos();
            m_currentPath = QPainterPath();
            m_currentPath.moveTo(start);
            m_currentPath.lineTo(current);
            m_currentItem->setPath(m_currentPath);
            return true;
        }
        return AbstractStrokeTool::handleMouseMove(event, scene);
    }

    bool handleMousePress(QGraphicsSceneMouseEvent* e, QGraphicsScene* s) override {
        AbstractStrokeTool::handleMousePress(e, s);
        if(m_currentItem) {
            s->removeItem(m_currentItem); delete m_currentItem;

            // Draw Behind -> Nutzt "Multiply" Modus im StrokeItem
            StrokeItem::Type type = m_config.drawBehind ? StrokeItem::Highlighter : StrokeItem::Normal;

            StrokeItem* si = new StrokeItem(m_currentPath, createPen(), type);
            si->setZValue(getZValue());
            s->addItem(si);
            m_currentItem = si;
        }
        return true;
    }

protected:
    QPen createPen() const override {
        QColor c = m_config.penColor;
        c.setAlphaF(0.4 * m_config.opacity);

        int width = std::max(10, m_config.penWidth * 3);
        QPen pen(c, width);

        // Kanten-Logik (Chisel)
        if (m_config.tipType == HighlighterTip::Chisel) {
            pen.setCapStyle(Qt::FlatCap);
            pen.setJoinStyle(Qt::MiterJoin);
        } else {
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
        }
        pen.setStyle(Qt::SolidLine);
        return pen;
    }
    qreal getZValue() const override {
        return m_config.drawBehind ? -10 : 5;
    }
};
