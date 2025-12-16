#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QUndoStack>
#include <QMenu>
#include <QPushButton>
#include <QBoxLayout>
#include <QObject>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QVector>
#include <QPointF>

class SelectionMenu : public QWidget {
    Q_OBJECT
public:
    explicit SelectionMenu(QWidget* parent = nullptr);
signals:
    void deleteRequested();
    void duplicateRequested();
    void copyRequested();
};

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class ToolType { Pen, Eraser, Select, Lasso, Highlighter, Text };
    enum class PageStyle { Blank, Lined, Squared, Dotted };

    explicit CanvasView(QWidget *parent = nullptr);

    void setTool(ToolType tool);
    void setPenColor(const QColor &color);
    void setPenWidth(int width);
    void setPenMode(bool penOnly) { m_penOnlyMode = penOnly; }

    void setPageFormat(bool isInfinite);
    void setPageColor(const QColor &color);
    void setPageStyle(PageStyle style);
    void setGridSize(int size);

    bool isInfinite() const { return m_isInfinite; }
    QColor pageColor() const { return m_pageColor; }
    PageStyle pageStyle() const { return m_pageStyle; }
    int gridSize() const { return m_gridSize; }

    void setFilePath(const QString &path) { m_filePath = path; }
    bool saveToFile();
    bool loadFromFile();

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

signals:
    void contentModified();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    // Performance Rendering
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void drawItems(QPainter *painter, int numItems, QGraphicsItem *items[], const QStyleOptionGraphicsItem options[]) override;

    bool event(QEvent *event) override;
    void gestureEvent(QGestureEvent *event);
    void pinchTriggered(class QPinchGesture *gesture);

private slots:
    void deleteSelection();
    void duplicateSelection();
    void copySelection();

private:
    void addNewPage();
    void finishLasso();
    void applyEraser(const QPointF &pos);
    void updateSelectionMenuPosition();
    void clearSelection();
    void updateBackgroundTile();
    void drawPullIndicator(QPainter* painter);
    void updateBakeState();

    // Pfad-Vereinfachung (Douglas-Peucker)
    QPainterPath simplifyPath(const QVector<QPointF> &points, float epsilon);
    void rdp(const QVector<QPointF> &points, float epsilon, int start, int end, QVector<bool> &keep);

    QGraphicsScene *m_scene;
    ToolType m_currentTool;

    // Drawing State
    bool m_isDrawing;
    QPainterPath m_currentPath;
    // NEU: Speichert Rohdaten für die Optimierung
    QVector<QPointF> m_currentPoints;

    QGraphicsPathItem *m_currentItem;
    QGraphicsPathItem *m_lassoItem;

    // Configuration
    bool m_isInfinite;
    bool m_penOnlyMode;
    QColor m_pageColor;
    PageStyle m_pageStyle;
    int m_gridSize;

    QColor m_penColor;
    int m_penWidth;

    // Navigation
    bool m_isPanning;
    QPoint m_lastPanPos;
    qreal m_pullDistance;

    // Data
    QString m_filePath;
    QRectF m_a4Rect;
    QPixmap m_bgTile;

    // Undo/Redo
    QList<QGraphicsItem*> m_undoList;
    QList<QGraphicsItem*> m_redoList;

    SelectionMenu *m_selectionMenu;
    bool m_bakeStrokes;
};

#endif // CANVASVIEW_H
