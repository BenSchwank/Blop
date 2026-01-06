#pragma once
#include <QColor>
#include <QFont>

enum class EraserMode {
    Standard, // Pixel exact / Pfad-Teile
    Stroke    // Delete whole stroke
};

enum class LassoMode {
    Freehand,
    Rectangular
};

enum class ShapeType {
    Rectangle,
    Circle,
    Line,
    CoordinateSystem
};

// Config container
struct ToolConfig {
    // --- Pen / Pencil / Highlighter ---
    QColor penColor{Qt::black};
    int penWidth{3};
    bool pressureSensitivity{true}; // Neu: Druckempfindlichkeit an/aus

    // --- Eraser ---
    EraserMode eraserMode{EraserMode::Standard};
    int eraserWidth{20};

    // --- Lasso ---
    LassoMode lassoMode{LassoMode::Freehand};

    // --- Text ---
    QFont textFont{"Segoe UI", 12};
    QColor textColor{Qt::black};

    // --- Shapes ---
    ShapeType shapeType{ShapeType::Rectangle};
    bool fillShape{false};
    QColor fillColor{Qt::transparent};

    // --- Sticky Note ---
    QColor stickyNoteColor{0xFFF7D1}; // Klassisches Gelb
};
