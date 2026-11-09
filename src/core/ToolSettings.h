#pragma once
#include <array>
#include <QColor>
#include <QString>
#include <QVector>

enum class EraserMode { Pixel, Object };
enum class LassoMode { Freehand, Rectangle };
enum class HighlighterTip { Round, Chisel };
enum class ShapeType { Rectangle, Circle, Triangle, Arrow, Star, Graph };
/// Pen Studio stroke style (independent of ToolMode Pen vs Pencil).
enum class PenStrokeStyle {
  Fountain = 0,   ///< pressure-sensitive
  Fineliner = 1,  ///< fixed width
  Pencil = 2,     ///< textured (routes to Pencil feel)
  Calligraphy = 3,
  Dashed = 4,
  Dotted = 5
};

/// Modus des Formen-Werkzeugs (Aufzieh-Rechteck als Bounds).
enum class ShapeToolKind {
  Rectangle = 0,
  Circle = 1,   ///< inscribed circle (equal rx/ry)
  Axes2D = 2,
  SineGraph = 3,
  CoordinateGraph = 4,
  Line = 5,
  Arrow = 6,
  Ellipse = 7, ///< free ellipse from drag bounds
  DashedLine = 8, ///< construction / dashed helper line
  DoubleArrow = 9,
  ElbowConnector = 10,
  RoundedRect = 11,
  Triangle = 12,
  Polyline = 13,
  Brace = 14,
  AxesPositive = 15, ///< 1st quadrant only
  ComplexPlane = 16, ///< Re/Im labels on Axes2D
  UnitCircle = 17,
  StampResistor = 18,
  StampGnd = 19,
  Axes3D = 20,
  StampCapacitor = 21,
  StampInductor = 22,
  StampVoltageSrc = 23,
  StampCurrentSrc = 24,
  StampNode = 25,
  FlowDecision = 26,
  FlowProcess = 27,
  FlowIO = 28
};

struct GraphFunctionSpec {
    QString expression{"sin(x)"};
    QColor color{QColor(94, 92, 230)};
    bool visible{true};
    bool showDerivative{false};
    bool showSecondDerivative{false};
    bool showRoots{false};
    bool showExtrema{false};
    bool showInflection{false};
    bool showIntersections{false};
    bool showIntegral{false};
    double integralA{0.0};
    double integralB{1.0};
    QString paramName;
    double paramValue{1.0};
    double paramMin{-5.0};
    double paramMax{5.0};
};

// NEU: Lineal Einheiten
enum class RulerUnit { Pixel, Centimeter, Inch };

enum class DimensionKind { Length = 0, Angle3Point = 1, Area = 2 };

struct ToolConfig {
    // --- BASIS EINSTELLUNGEN ---
    int penWidth = 3;
    QColor penColor = Qt::black;
    double opacity = 1.0;

    // 1. PEN
    int smoothing = 0;
    bool pressureSensitivity = true;
    PenStrokeStyle penStrokeStyle = PenStrokeStyle::Fountain;
    /// Paper scale for measure tools: one grid cell (5mm) equals this many units.
    double measureUnitsPerCell = 1.0;

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
    bool compassMode = false;   // Geodreieck / Protractor
    bool infiniteRuler = false; // Über die ganze Seite
    DimensionKind dimensionKind = DimensionKind::Length;

    // 8. Schnellauswahl-Farben (persistente Slots für Tool-Settings)
    // Pro-Ink palette (Studio Canvas) — eye-friendly, not neon RGB.
    std::array<QColor, 5> quickColors{
        QColor(0x1E, 0x29, 0x3B), QColor(0xF8, 0xFA, 0xFC),
        QColor(0x25, 0x63, 0xEB), QColor(0xE1, 0x1D, 0x48),
        QColor(0x10, 0xB9, 0x81)};

    // 9. Halten → Form (AbstractStrokeTool: Stift, Bleistift, Marker, Radierer)
    int holdShapeSensitivity = 50; ///< 0 = streng, 100 = nachgiebig
    bool holdEnableCircle = true;
    bool holdEnableTriangle = true;
    int holdStillDelayMs = 350; ///< 200–900 ms für Halte-Erkennung
    bool holdEnableLine = true; ///< Tinte zu gerader Linie

    // 10. Text / Sticky
    QString fontFamily; ///< Empty = system default; e.g. "Serif", "Mono"
    QColor stickyBgColor{QColor(255, 236, 120)}; ///< Sticky note card fill
    /// 0=left, 1=center, 2=right — applied to new TextTool items.
    int textAlign{0};
    bool textBold{false};
    bool textItalic{false};
    bool textUnderline{false};
    /// 0 = none, 1 = bullets, 2 = numbers. Applied to the next text frame.
    int textListKind{0};

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
