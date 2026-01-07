#pragma once
#include "AbstractStrokeTool.h"

// FÜLLER
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
    qreal getZValue() const override { return 10; }
};

// BLEISTIFT
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
        pen.setStyle(Qt::DotLine); // Das macht den Bleistift-Effekt
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        return pen;
    }
    qreal getZValue() const override { return 15; }
};

// MARKER
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
        c.setAlpha(80);
        int width = std::max(10, m_config.penWidth * 3);
        QPen pen(c, width);
        pen.setCapStyle(Qt::FlatCap);
        pen.setJoinStyle(Qt::MiterJoin);
        return pen;
    }
    qreal getZValue() const override { return -10; }
};
