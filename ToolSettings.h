#pragma once
#include <QColor>
#include <QString>

enum class EraserMode { Pixel, Object };
enum class LassoMode { Freehand, Rectangle };
enum class HighlighterTip { Round, Chisel };
enum class ShapeType { Rectangle, Circle, Triangle, Arrow, Star, Graph };
enum class RulerUnit { Centimeter, Inch, Pixel };

struct ToolConfig {
    // --- BASIS EINSTELLUNGEN ---
    int penWidth = 3;
    QColor penColor = Qt::black;
    double opacity = 1.0;          // 0.0 - 1.0

    // 1. PEN / WRITING
    int smoothing = 0;             // 0 - 100 (Stabilization)
    bool pressureSensitivity = true;

    // 2. PENCIL
    int hardness = 50;
    bool tiltShading = true;
    QString texture = "Fein";

    // 3. HIGHLIGHTER
    bool smartLine = false;
    bool drawBehind = true;
    HighlighterTip tipType = HighlighterTip::Chisel;

    // 4. ERASER
    EraserMode eraserMode = EraserMode::Pixel;
    bool eraserKeepInk = false;

    // 5. LASSO
    LassoMode lassoMode = LassoMode::Freehand;
    bool aspectLock = true;

    // 6. IMAGE, SHAPES, ETC. (Platzhalter für weitere Tools)
    double imageOpacity = 1.0;
    bool showGrid = true;
};
