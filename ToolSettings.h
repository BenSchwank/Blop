#pragma once
#include <QColor>

enum class EraserMode {
    Standard, // Pixelgenau
    Stroke    // Ganzen Strich löschen
};

enum class LassoMode {
    Freehand,
    Rectangular
};

// Container für alle aktuellen Einstellungen
struct ToolConfig {
    QColor penColor{Qt::black};
    int penWidth{3};
    EraserMode eraserMode{EraserMode::Standard};
    LassoMode lassoMode{LassoMode::Freehand};
};
