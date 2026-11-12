#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QPointF>
#include <QPainterPath>
#include <QColor>
#include <QImage>

struct Stroke {
    QVector<QPointF> points;
    /// Per-point stylus pressure (parallel to points); empty = uniform width.
    QVector<qreal> pressures;
    QPainterPath path;
    qreal width{2.0};
    QColor color{Qt::black};
    bool isEraser{false};
    bool isHighlighter{false};
    int pageIndex{0};
    /// Pixel-erased remnants are filled outlines (no pen): `path` is the
    /// filled shape in page-local coordinates, `points` stay empty.
    bool filled{false};
    /// Shape-tool strokes: `path` serializes as elements (like `filled`),
    /// keeps pen style and an optional fill under the outline.
    bool shape{false};
    QColor fillColor;      // invalid = outline only
    int penStyle{1};       // Qt::PenStyle (1 = SolidLine)
};

struct GraphFunction {
    QString expression;
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
    bool showTangent{false};
    double tangentX{0.0};
    bool isDerivativeCurve{false};
    QString sourceExpression;
    QColor rootMarkerColor{QColor(225, 88, 90)};
    QColor extremaMarkerColor{QColor(70, 170, 102)};
    /// Free parameter slider (e.g. a in a*x^2); empty = none.
    QString paramName;
    double paramValue{1.0};
    double paramMin{-5.0};
    double paramMax{5.0};
};

struct GraphObject {
    QRectF rect{QRectF(0, 0, 280, 180)};
    QVector<GraphFunction> functions;
    int selectedFunction{-1};
    double xMin{-10.0};
    double xMax{10.0};
    double yMin{-10.0};
    double yMax{10.0};
    /// 0 = auto (pixel-based), 1 = fixed step, 2 = target interval count
    int xTickMode{0};
    int yTickMode{0};
    double xTickStep{1.0};
    double yTickStep{1.0};
    int xTickCount{8};
    int yTickCount{8};
    /// Clean-paper / Kurvendiskussion view flags (Pass B).
    bool showPlotGrid{true};
    bool showAnalysisLabels{true};
    bool showLegend{true};
    bool showValueTable{false};
    double tableX0{-5.0};
    double tableX1{5.0};
    double tableDx{1.0};
};

struct StickyNoteObject {
    QPointF pos{0, 0}; // page-local top-left
    qreal width{168.0};
    qreal height{148.0};
    QString text;
    QColor color{QColor(255, 236, 120)};
    int fontPointSize{14};
};

struct TextObject {
    QPointF pos{0, 0}; // page-local top-left
    qreal width{300.0};
    QString text;
    QString html;          // rich text; empty on older notes
    QColor color{Qt::black};
    QString fontFamily;   // empty = default application font
    int fontPointSize{14};
    bool freeMove{false}; // off the invisible layout grid
};

/// Raster image placed on a page (ImageTool). Position/size are page-local.
struct ImageObject {
    QPointF pos{0, 0};
    qreal width{0.0};   // 0 = natural image width
    qreal height{0.0};  // 0 = natural image height
    qreal opacity{1.0};
    qreal rotation{0.0};
    QImage image;
};

struct NotePage {
    QString title;        // Seitenname
    QVector<Stroke> strokes;
    QVector<GraphObject> graphs;
    QVector<StickyNoteObject> stickies;
    QVector<TextObject> texts;
    QVector<ImageObject> images;
    QImage backgroundImage; // PDF-Import: Hintergrundbild (leer = nicht gesetzt)
    /// PageBackgroundType als int (0 Blank … 4 Legal); Standard = Grid (2)
    int backgroundType{2};
    /// Papierfarbe (Linien/Karo passen sich automatisch an)
    QColor paperColor{Qt::white};
    /// Content rotation within the A4 frame (0 / 90 / 180 / 270).
    int rotationDegrees{0};
    /// Drawboard-style page bookmark (left-rail bookmarks list).
    bool bookmarked{false};
};

/// Persistent ruler overlay state (scene coords, rotation in degrees).
/// `placed` = a RulerItem existed when the note was saved; the ruler is
/// restored hidden and only shown while the ruler tool is active.
struct RulerState {
    bool placed{false};
    QPointF pos;
    qreal rotation{0.0};
    bool compassMode{false};
    qreal width{0.0};   // 0 = default from config
    qreal height{0.0};  // 0 = default from config
};

struct Note {
    QString id;
    QString title;
    QStringList tags;
    QVector<NotePage> pages;
    RulerState ruler;

    void ensurePage(int index) {
        if (index >= pages.size()) {
            pages.resize(index + 1);
            // Automatische Benennung: "Seite 1", "Seite 2" usw.
            pages[index].title = QString("Seite %1").arg(index + 1);
        }
    }

    QList<int> bookmarkedPageIndices() const {
        QList<int> out;
        for (int i = 0; i < pages.size(); ++i) {
            if (pages[i].bookmarked)
                out.append(i);
        }
        return out;
    }
};
