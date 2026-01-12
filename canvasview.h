#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsItemGroup> // WICHTIG für Transformationen
#include <QMouseEvent>
#include <QWheelEvent>
#include <QUndoStack>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPinchGesture>

// Definition of background styles
enum class PageStyle { Blank, Lined, Squared, Dotted };

class SelectionMenu;
class CropMenu;
class CropResizer;
class TransformOverlay; // Neue Klasse

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class ToolType { Pen, Eraser, Lasso, Select, Highlighter, Text };

    // Neuer Modus: Transform
    enum class InteractionMode { None, Crop, Transform };

    explicit CanvasView(QWidget *parent = nullptr);

    void setPageFormat(bool isInfinite);
    bool isInfinite() const { return m_isInfinite; }
    void setPenMode(bool enabled) { m_penOnlyMode = enabled; }
    void setPageColor(const QColor &color);
    QColor pageColor() const { return m_pageColor; }
    void setPageStyle(PageStyle style);
    PageStyle pageStyle() const { return m_pageStyle; }
    void setGridSize(int size);
    int gridSize() const { return m_gridSize; }
    void setTool(ToolType tool);
    void setPenColor(const QColor &color);
    void setPenWidth(int width);

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // Actions
    void deleteSelection();
    void duplicateSelection();
    void copySelection();
    void cutSelection();
    void changeSelectionColor();
    void screenshotSelection();

    // Crop Flow
    void startCropSession();
    void setCropModeRect();
    void setCropModeFreehand();
    void applyCrop();
    void cancelCrop();

    // Transform Flow (NEU)
    void startTransformSession();
    void applyTransform(); // Beendet Session

    void setFilePath(const QString &path) { m_filePath = path; }
    QString filePath() const { return m_filePath; }
    bool saveToFile();
    bool loadFromFile();

public slots:
    void updateSceneRect();

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
    PageStyle m_pageStyle;
    int m_gridSize;
    QPixmap m_bgTile;
    QRectF m_a4Rect;
    ToolType m_currentTool;
    QColor m_penColor;
    int m_penWidth;
    QString m_filePath;
    QList<QGraphicsItem*> m_undoList;
    QList<QGraphicsItem*> m_redoList;

    // Selection & Menus
    QGraphicsPathItem *m_lassoItem;
    SelectionMenu *m_selectionMenu;

    // Crop
    CropMenu *m_cropMenu;
    CropResizer *m_cropResizer;

    // Transform (NEU)
    TransformOverlay *m_transformOverlay;
    QGraphicsItemGroup *m_transformGroup; // Temporäre Gruppe für ausgewählte Items

    InteractionMode m_interactionMode{InteractionMode::None};
    int m_cropSubMode{0}; // 0=None, 1=Rect, 2=Freehand

    bool m_isPanning;
    QPoint m_lastPanPos;
    float m_pullDistance;

    void addNewPage();
    void drawPullIndicator(QPainter* painter);
    void updateBackgroundTile();
    void applyEraser(const QPointF &pos);
    void finishLasso();
    void updateSelectionMenuPosition();
    void updateCropMenuPosition();
    void clearSelection();
    void gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
};

#endif // CANVASVIEW_H
