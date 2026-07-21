#pragma once
#include <array>
#include <QColor>
#include <QString>
#include <QVector>

enum class EraserMode { Pixel, Object };
enum class LassoMode { Freehand, Rectangle };
enum class HighlighterTip { Round, Chisel };
enum class ShapeType { Rectangle, Circle, Triangle, Arrow, Star, Graph };

/// Modus des Formen-Werkzeugs (Aufzieh-Rechteck als Bounds).
enum class ShapeToolKind {
  Rectangle = 0,
  Circle = 1,   ///< inscribed circle (equal rx/ry)
  Axes2D = 2,
  SineGraph = 3,
  CoordinateGraph = 4,
  Line = 5,
  Arrow = 6,
  Ellipse = 7 ///< free ellipse from drag bounds
};

struct GraphFunctionSpec {
    QString expression{"sin(x)"};
    QColor color{QColor(94, 92, 230)};
    bool visible{true};
    bool showDerivative{false};
    bool showRoots{false};
    bool showExtrema{false};
};

// NEU: Lineal Einheiten
enum class RulerUnit { Pixel, Centimeter, Inch };

struct ToolConfig {
    // --- BASIS EINSTELLUNGEN ---
    int penWidth = 3;
    QColor penColor = Qt::black;
    double opacity = 1.0;

    // 1. PEN
    int smoothing = 0;
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

    // 6. IMAGE, SHAPES
    double imageOpacity = 1.0;
    bool showGrid = true;
    /// Shape fill (transparent = stroke only).
    QColor fillColor{Qt::transparent};

    // 7. RULER (NEU)
    RulerUnit rulerUnit = RulerUnit::Pixel;
    bool rulerSnap = true;      // Einrasten aktiv?
    bool compassMode = false;   // Rundes Lineal
    bool infiniteRuler = false; // Über die ganze Seite

    // 8. Schnellauswahl-Farben (persistente Slots für Tool-Settings)
    std::array<QColor, 5> quickColors{
        QColor(Qt::black), QColor(Qt::white), QColor(Qt::red),
        QColor(Qt::blue), QColor(0, 200, 0)};

    // 9. Halten → Form (AbstractStrokeTool: Stift, Bleistift, Marker, Radierer)
    int holdShapeSensitivity = 50; ///< 0 = streng, 100 = nachgiebig
    bool holdEnableCircle = true;
    bool holdEnableTriangle = true;
    int holdStillDelayMs = 360; ///< 200–900 ms für Halte-Erkennung

    // 10. Text / Sticky
    QString fontFamily; ///< Empty = system default; e.g. "Serif", "Mono"
    QColor stickyBgColor{QColor(255, 236, 120)}; ///< Sticky note card fill

    // 11. Formen-Werkzeug (ShapeTool)
    ShapeToolKind shapeToolKind = ShapeToolKind::Rectangle;
    /// Sinus im Formen-Werkzeug: y = cy − (h/2)·(a·sin(b·2π·t + c) + d), t∈[0,1] links→rechts
    double shapeMathA = 0.45;  ///< Amplitudenfaktor (bezogen auf halbe Höhe)
    double shapeMathB = 1.0;   ///< Anzahl Perioden über die Breite
    double shapeMathC = 0.0;   ///< Phasenverschiebung (Radiant)
    double shapeMathD = 0.0;   ///< Vertikaler Offset (in Einheiten der halben Höhe)
    /// true: Sinus mit fester räumlicher Frequenz/Amplitude in Szenenkoordinaten (siehe ShapeTool)
    bool shapeSineFixedParams = false;
    int shapeAxisTicks = 5;          ///< Ticks je Halbachse (2–12)
    QVector<GraphFunctionSpec> graphFunctions{GraphFunctionSpec{}};
    int graphSelectedFunction = 0;
    double graphXMin = -10.0;
    double graphXMax = 10.0;
    double graphYMin = -10.0;
    double graphYMax = 10.0;
};
