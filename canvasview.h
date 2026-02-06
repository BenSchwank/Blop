#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsItemGroup>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QUndoStack>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPinchGesture>

// Definition of background styles
enum class PageStyle { Blank, Lined, Squared, Dotted };

// Vorwärtsdeklarationen
class SelectionMenu;
class CropMenu;
class CropResizer;
class TransformOverlay;
class ToolManager; // WICHTIG

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    enum class ToolType { Pen, Eraser, Lasso, Select, Highlighter, Text, Ruler, Image, Shape };

    // Neuer Modus: Transform
    enum class InteractionMode { None, Crop, Transform };

    explicit CanvasView(QWidget *parent = nullptr);
    ~CanvasView();

    // --- WICHTIG: Manager Integration ---
    void setToolManager(ToolManager *manager);
    ToolManager* toolManager() const { return m_toolManager; }

    void setPageFormat(bool isInfinite);
    bool isInfinite() const { return m_isInfinite; }
    void setPenMode(bool enabled) { m_penOnlyMode = enabled; }
    void setPageColor(const QColor &color); // Nutzt jetzt QColor direkt
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

    // Transform Flow
    void startTransformSession();
    void applyTransform();

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
    void resizeEvent(QResizeEvent *event) override;

private:
    QGraphicsScene *m_scene;
    ToolManager *m_toolManager; // Pointer zum Manager

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

    QUndoStack *m_undoStack; // Nutze QUndoStack statt manueller Listen für Konsistenz

    // Selection & Menus
    QGraphicsPathItem *m_lassoItem;
    SelectionMenu *m_selectionMenu;

    // Crop
    CropMenu *m_cropMenu;
    CropResizer *m_cropResizer;

    // Transform
    TransformOverlay *m_transformOverlay;
    QGraphicsItemGroup *m_transformGroup;

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
