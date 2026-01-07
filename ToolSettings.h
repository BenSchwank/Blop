#pragma once
#include <QColor>
#include <QString>

// Enums für spezifische Tool-Modi
enum class EraserMode { Pixel, Object };
enum class LassoMode { Freehand, Rectangle };
enum class HighlighterTip { Round, Chisel };
enum class ShapeType { Rectangle, Circle, Triangle, Arrow, Star, Graph };
enum class RulerUnit { Centimeter, Inch, Pixel };
enum class TextAlignment { Left, Center, Right };

struct ToolConfig {
    // --- BASIS EINSTELLUNGEN ---
    int penWidth = 5;
    QColor penColor = Qt::black;
    double opacity = 1.0;

    // 1. PEN
    int smoothing = 10;
    bool pressureSensitivity = true;

    // 2. PENCIL
    int hardness = 50; // 0-100
    bool tiltShading = true;
    QString texture = "Fein"; // "Fein", "Grob", "Kohle"

    // 3. HIGHLIGHTER
    bool smartLine = false;
    bool drawBehind = true;
    HighlighterTip tipType = HighlighterTip::Chisel;

    // 4. ERASER
    EraserMode eraserMode = EraserMode::Pixel;
    bool eraserKeepInk = false; // "Nur Textmarker löschen"

    // 5. LASSO
    LassoMode lassoMode = LassoMode::Freehand;
    bool aspectLock = true;

    // 6. IMAGE
    double imageOpacity = 1.0;
    int imageFilter = 0; // 0=Orig, 1=BW, 2=Contrast
    int borderWidth = 0;
    QColor borderColor = Qt::white; // <--- Rahmenfarbe
    bool imageInForeground = true;

    // 7. RULER
    RulerUnit rulerUnit = RulerUnit::Centimeter;
    bool rulerSnap = true;
    bool compassMode = false;

    // 8. SHAPES
    ShapeType shapeType = ShapeType::Rectangle;
    int shapeFill = 0; // 0=None, 1=Solid, 2=Transparent
    bool showGrid = true;
    bool showXAxis = true; // <--- NEU
    bool showYAxis = true; // <--- NEU

    // 9. STICKY NOTES
    QColor paperColor = QColor(0xFF, 0xEB, 0x3B); // Default Gelb
    bool collapsed = false;

    // 10. TEXT
    QString fontFamily = "Arial";
    bool bold = false;
    bool italic = false;
    bool underline = false;
    int fontSize = 12;
    int alignment = 0; // 0=L, 1=C, 2=R
    bool fillTextBackground = false;
    QColor textBackgroundColor = Qt::yellow; // <--- NEU
};
