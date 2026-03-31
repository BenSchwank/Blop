#pragma once
#include <array>
#include <QColor>
#include <QString>

enum class EraserMode { Pixel, Object };
enum class LassoMode { Freehand, Rectangle };
enum class HighlighterTip { Round, Chisel };
enum class ShapeType { Rectangle, Circle, Triangle, Arrow, Star, Graph };

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

    // 7. RULER (NEU)
    RulerUnit rulerUnit = RulerUnit::Pixel;
    bool rulerSnap = true;      // Einrasten aktiv?
    bool compassMode = false;   // Rundes Lineal
    bool infiniteRuler = false; // Über die ganze Seite

    // 8. Schnellauswahl-Farben (persistente Slots für Tool-Settings)
    std::array<QColor, 5> quickColors{
        QColor(Qt::black), QColor(Qt::white), QColor(Qt::red),
        QColor(Qt::blue), QColor(0, 200, 0)};
};
