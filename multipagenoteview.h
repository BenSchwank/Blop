#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QTabletEvent>
#include <QPinchGesture>
#include <QGestureEvent>
#include <QUndoStack>
#include <functional>
#include "Note.h"
#include "ToolMode.h"
#include "PageItem.h"
#include "TransformOverlay.h"

class NoteSelectionMenu;
class StrokeAddUndoCommand;

class MultiPageNoteView : public QGraphicsView {
    Q_OBJECT
    friend class StrokeAddUndoCommand;
public:
    explicit MultiPageNoteView(QWidget* parent=nullptr);

    void setNote(Note* note);
    Note* note() const { return note_; }

    void setToolMode(ToolMode m) { mode_ = m; }
    void toggleRuler(bool active);

    void setPenColor(const QColor& c) { penColor_ = c; }
    void setPenWidth(qreal w) { penWidth_ = w; }
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    void setPenOnlyMode(bool enabled) { m_penOnlyMode = enabled; }
    bool penOnlyMode() const { return m_penOnlyMode; }

    // Methoden für Export / Thumbnails
    bool exportPageToPng(int pageIndex, const QString &path);
    bool exportPageToPdf(int pageIndex, const QString &path);

    // Methoden für PageManager
    QPixmap generateThumbnail(int pageIndex, const QSize& size);
    void movePage(int fromIndex, int toIndex);
    void duplicatePage(int pageIndex);
    void deletePage(int pageIndex);
    void scrollToPage(int pageIndex);

    std::function<void(Note*)> onSaveRequested;

    // PDF Import: renders each PDF page as a note page background image
    bool importPdfPages(const QString &pdfPath);

public slots:
    void onSelectionChanged();
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void duplicateSelection();
    void changeSelectionColor();
    void startTransformSession();
    void applyTransform();
    void screenshotSelection();


protected:
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void tabletEvent(QTabletEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

    // WICHTIG: Gesten kommen hier an
    bool viewportEvent(QEvent* e) override;

    // NEU: Zum Zeichnen des Pull-Indikators
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    QGraphicsScene scene_;
    Note* note_{nullptr};
    ToolMode mode_{ToolMode::Pen};
    qreal zoom_{1.0};
    bool drawing_{false};
    int currentPage_{0};

    NoteSelectionMenu* m_selectionMenu{nullptr};

    TransformOverlay* m_transformOverlay{nullptr};
    QGraphicsItemGroup* m_transformGroup{nullptr};

    bool m_penOnlyMode{false};

    // Flags für Panning & Zoom
    bool m_isPanning{false};
    bool m_isZooming{false};
    QPoint m_lastPanPos;

    // NEU: Pull-to-Add Variablen
    float m_pullDistance{0.0f};

    QColor penColor_{Qt::black};
    qreal penWidth_{2.0};

    QVector<PageItem*> pageItems_;

    Stroke currentStroke_;
    QGraphicsPathItem* currentPathItem_{nullptr};
    QVector<NotePage> m_strokeSnapshot;
    bool m_hasStrokeSnapshot{false};
    QVector<QVector<NotePage>> m_undoHistory;
    QVector<QVector<NotePage>> m_redoHistory;
    static constexpr int MaxUndoSteps = 40;

    QUndoStack *m_undoStack{nullptr};

    void layoutPages();
    QGraphicsPathItem *createStrokeGraphicsItem(const Stroke &s);
    void pushStrokeUndoCommand(int pageIdx, Stroke stroke);
    // void ensureOverscrollPage(); // Entfernt/Ersetzt durch Pull-Logik
    int pageAt(const QPointF& scenePos) const;
    QRectF pageRect(int idx) const;

    // NEU: Helper
    void addNewPage();
    void drawPullIndicator(QPainter* painter);

    void gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
    void pushUndoSnapshot(const QVector<NotePage>& beforeState);
};
