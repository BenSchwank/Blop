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
};

struct GraphFunction {
    QString expression;
    QColor color{QColor(94, 92, 230)};
    bool visible{true};
    bool showDerivative{false};
    bool showRoots{false};
    bool showExtrema{false};
    bool showTangent{false};
    double tangentX{0.0};
    bool isDerivativeCurve{false};
    QString sourceExpression;
    QColor rootMarkerColor{QColor(225, 88, 90)};
    QColor extremaMarkerColor{QColor(70, 170, 102)};
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
};

struct StickyNoteObject {
    QPointF pos{0, 0}; // page-local top-left
    qreal width{168.0};
    qreal height{148.0};
    QString text;
    QColor color{QColor(255, 236, 120)};
    int fontPointSize{14};
};

/// Freehand / geometric shape path (not CoordinateGraph — that stays in graphs).
struct ShapeObject {
    QPointF pos{0, 0}; // page-local
    QPainterPath path;
    qreal penWidth{2.0};
    QColor penColor{Qt::black};
    QColor fillColor{Qt::transparent};
    int kind{0}; // ShapeToolKind
};

struct TextObject {
    QPointF pos{0, 0};
    QString text;
    QString fontFamily;
    int fontPointSize{16};
    QColor color{Qt::black};
    int align{0}; // 0 left, 1 center, 2 right
};

struct ImageObject {
    QPointF pos{0, 0};
    QByteArray png;
    qreal opacity{1.0};
    qreal scale{1.0};
};

struct NotePage {
    QString title;        // Seitenname
    QVector<Stroke> strokes;
    QVector<GraphObject> graphs;
    QVector<StickyNoteObject> stickies;
    QVector<ShapeObject> shapes;
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

struct Note {
    QString id;
    QString title;
    QStringList tags;
    QVector<NotePage> pages;

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
