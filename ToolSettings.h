#pragma once
#include <QColor>

enum class EraserMode {
    Standard, // Pixel exact
    Stroke    // Delete whole stroke
};

enum class LassoMode {
    Freehand,
    Rectangular
};

// Config container
struct ToolConfig {
    QColor penColor{Qt::black};
    int penWidth{3};
    EraserMode eraserMode{EraserMode::Standard};
    LassoMode lassoMode{LassoMode::Freehand};
};
