#pragma once
#include "AbstractStrokeTool.h"

// --- PEN TOOL (Standard Füller) ---
class PenTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;

    ToolMode mode() const override { return ToolMode::Pen; }
    QString name() const override { return "Füller"; }
    QString iconName() const override { return "pen"; }

protected:
    QPen createPen() const override {
        QPen pen(m_config.penColor, m_config.penWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        return pen;
    }
    qreal getZValue() const override { return 10; } // Liegt über dem Marker
};

// --- PENCIL TOOL (Bleistift) ---
class PencilTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;

    ToolMode mode() const override { return ToolMode::Pencil; }
    QString name() const override { return "Bleistift"; }
    QString iconName() const override { return "pencil"; }

protected:
    QPen createPen() const override {
        QPen pen(m_config.penColor, m_config.penWidth);
        // Trick: DotLine simuliert die körnige Textur eines Bleistifts
        pen.setStyle(Qt::DotLine);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        return pen;
    }
    qreal getZValue() const override { return 15; }
};

// --- HIGHLIGHTER TOOL (Textmarker) ---
class HighlighterTool : public AbstractStrokeTool {
    Q_OBJECT
public:
    using AbstractStrokeTool::AbstractStrokeTool;

    ToolMode mode() const override { return ToolMode::Highlighter; }
    QString name() const override { return "Marker"; }
    QString iconName() const override { return "highlighter"; }

protected:
    QPen createPen() const override {
        QColor c = m_config.penColor;
        c.setAlpha(80); // Macht die Farbe transparent (ca 30% Deckkraft)

        // Marker ist immer etwas dicker
        int width = std::max(10, m_config.penWidth * 3);

        QPen pen(c, width);
        pen.setCapStyle(Qt::FlatCap); // Marker hat flache Enden
        pen.setJoinStyle(Qt::MiterJoin);
        return pen;
    }

    qreal getZValue() const override { return -10; } // Zeichnet HINTER dem Text/Stift
};
