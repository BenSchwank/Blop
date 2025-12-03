#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QUndoStack>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPinchGesture>

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
    enum class ToolType {
        Pen,
        Eraser,
        Lasso,
        Select
    };

    explicit CanvasView(QWidget *parent = nullptr);

    void setPageFormat(bool isInfinite);
    bool isInfinite() const { return m_isInfinite; }

    void setPenMode(bool enabled) { m_penOnlyMode = enabled; }

    // NEU: Seite färben
    void setPageColor(const QColor &color);
    QColor pageColor() const { return m_pageColor; }

    void setTool(ToolType tool);
    void setPenColor(const QColor &color);
    void setPenWidth(int width);

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    void deleteSelection();
    void duplicateSelection();
    void copySelection();

    void setFilePath(const QString &path) { m_filePath = path; }
    QString filePath() const { return m_filePath; }
    bool saveToFile();
    bool loadFromFile();

signals:
    void contentModified();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    bool event(QEvent *event) override;

    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    QGraphicsScene *m_scene;
    QGraphicsPathItem *m_currentItem;
    QPainterPath m_currentPath;
    bool m_isDrawing;
    bool m_isInfinite;

    bool m_penOnlyMode;
    QColor m_pageColor;

    QRectF m_a4Rect;

    ToolType m_currentTool;
    QColor m_penColor;
    int m_penWidth;

    QString m_filePath;

    QList<QGraphicsItem*> m_undoList;
    QList<QGraphicsItem*> m_redoList;

    QGraphicsPathItem *m_lassoItem;
    SelectionMenu *m_selectionMenu;

    bool m_isPanning;
    QPoint m_lastPanPos;

    float m_pullDistance;
    void addNewPage();
    void drawPullIndicator(QPainter* painter);

    void applyEraser(const QPointF &pos);
    void finishLasso();
    void updateSelectionMenuPosition();
    void clearSelection();

    void gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
};

#endif // CANVASVIEW_H
