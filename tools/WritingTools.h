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

protected:
    QPen createPen() const override {
        QColor c = m_config.penColor;
        c.setAlphaF(m_config.opacity);
        qreal w = m_config.penWidth;
        if (m_config.penStrokeStyle == PenStrokeStyle::Fineliner)
            w = qMax(0.5, w); // fixed width — pressure ignored in stroke tool
        QPen pen(c, w);
        pen.setCapStyle(m_config.penStrokeStyle == PenStrokeStyle::Calligraphy
                            ? Qt::FlatCap
                            : Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        switch (m_config.penStrokeStyle) {
        case PenStrokeStyle::Dashed:
            pen.setStyle(Qt::DashLine);
            break;
        case PenStrokeStyle::Dotted:
            pen.setStyle(Qt::DotLine);
            break;
        default:
            pen.setStyle(Qt::SolidLine);
            break;
        }
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

    StrokeItem::StrokeStyle strokeStyle() const override {
        // Always Highlighter so StrokeItem paints with Multiply.
        return StrokeItem::Highlighter;
    }

protected:
    QPen createPen() const override {
        QColor c = m_config.penColor;
        // Clean-sheet: fixed readability under black ink.
        const qreal alpha = m_config.opacity > 0.01 ? m_config.opacity : 0.35;
        c.setAlphaF(qBound(0.05, alpha, 1.0));

        const int width = qMax(14, m_config.penWidth > 0 ? m_config.penWidth : 14);
        QPen pen(c, width);

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
        // Live stroke must sit ABOVE the page item while drawing — negative
        // z hides the preview under the page. On commit the recreated item
        // lives inside the page at z 0.5 (below ink, above paper).
        return 10.0;
    }
};
