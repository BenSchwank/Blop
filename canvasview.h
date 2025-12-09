#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QUndoStack>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPinchGesture>
#include <QGestureEvent>
#include <QMap>
#include <QPixmap>
#include <QTimer>
#include "InputPredictor.h"

enum class PageStyle {
    Blank,
    Lined,
    Squared,
    Dotted
};

class SelectionMenu : public QWidget {
    Q_OBJECT
public:
    explicit SelectionMenu(QWidget* parent = nullptr);
signals:
    void deleteRequested();
    void duplicateRequested();
    void copyRequested();
};

// --- Die Hybrid-Seite (Raster + Vektor Logik) ---
class CachedPageItem : public QGraphicsRectItem {
public:
    CachedPageItem(const QRectF& rect, int pageIndex, QGraphicsItem* parent = nullptr);

    void setPageStyle(PageStyle style, const QColor& color);
    void bakeStroke(const QPainterPath& path, const QPen& pen);
    void eraseStroke(const QPointF& pos, double radius);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QPixmap getContentPixmap() const { return m_contentLayer; }
    void setContentPixmap(const QPixmap& px) { m_contentLayer = px; update(); }

private:
    int m_pageIndex;
    PageStyle m_style;
    QColor m_color;
    QPixmap m_contentLayer;
    QPixmap m_gridLayer;
    void updateGrid();
};

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class ToolType { Pen, Eraser, Lasso, Select };

    explicit CanvasView(QWidget *parent = nullptr);

    void setPageFormat(bool isInfinite);
    bool isInfinite() const { return m_isInfinite; }

    void setPenMode(bool enabled) { m_penOnlyMode = enabled; }
    void setPageColor(const QColor &color);
    QColor pageColor() const { return m_pageColor; }

    void setPageStyle(PageStyle style, bool applyToAll);
    PageStyle currentPageStyle() const;

    void setTool(ToolType tool);
    void setPenColor(const QColor &color);
    void setPenWidth(int width);

    // --- HIER WAREN DIE FEHLENDEN DEKLARATIONEN ---
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    void deleteSelection();
    void duplicateSelection();
    void copySelection();
    void clearSelection();
    // ----------------------------------------------

    bool saveToFile();
    bool loadFromFile();
    void setFilePath(const QString &path) { m_filePath = path; }

signals:
    void contentModified();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool viewportEvent(QEvent *event) override;

    void drawBackground(QPainter* painter, const QRectF& rect) override;
    // --- AUCH DIESE HAT GEFEHLT ---
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    QGraphicsScene *m_scene;

    QGraphicsPathItem *m_currentStrokeItem;
    QPainterPath m_currentStrokePath;

    InputPredictor m_predictor;
    QGraphicsPathItem* m_phantomItem;
    void drawPhantomInk(const QPointF& realEndPos);

    bool m_isDrawing;
    bool m_isInfinite;
    bool m_penOnlyMode;

    QColor m_pageColor;
    QRectF m_a4Rect;

    QList<CachedPageItem*> m_pages;
    QVector<PageStyle> m_pageStyles;

    ToolType m_currentTool;
    QColor m_penColor;
    int m_penWidth;

    QString m_filePath;

    // Listen für Undo (Aktuell Dummys wegen Raster-Wechsel)
    QList<QGraphicsItem*> m_undoList;
    QList<QGraphicsItem*> m_redoList;
    QGraphicsPathItem *m_lassoItem;
    SelectionMenu *m_selectionMenu;

    bool m_isPanning;
    QPoint m_lastPanPos;
    float m_pullDistance;
    void addNewPage();
    void drawPullIndicator(QPainter* painter);

    CachedPageItem* getPageAt(const QPointF& pos);
    int getCurrentPageIndex() const;

    // --- UND DIESE HELPER WAREN PRIVAT UND FEHLTEN ---
    void applyEraser(const QPointF &pos);
    void finishLasso();
    void updateSelectionMenuPosition();
    // -------------------------------------------------

    bool gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
};

#endif // CANVASVIEW_H
