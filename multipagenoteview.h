#pragma once
#include <QPointer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QTabletEvent>
#include <QPinchGesture>
#include <QShowEvent>
#include <QGestureEvent>
#include <QUndoStack>
#include <functional>
#include "Note.h"
#include "ToolMode.h"
#include "PageItem.h"

class QFrame;
#include "TransformOverlay.h"

class NoteSelectionMenu;
class GraphCanvasItem;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class GraphLegendDock;
class GraphQuickActionPopup;
class GraphFormulaEntryBar;
class GraphTangentXPopup;
class StrokeAddUndoCommand;
class AbstractTool;
class GraphFormulaZone;

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
    int pageCount() const;
    QString pageTitle(int pageIndex) const;
    void renamePage(int pageIndex, const QString &title);
    void duplicatePages(const QList<int> &pageIndices);
    void deletePages(const QList<int> &pageIndices);
    void applyLayoutToPages(const QList<int> &pageIndices, int backgroundType,
                            const QColor &paperColor);

    std::function<void(Note*)> onSaveRequested;

    // PDF Import: renders each PDF page as a note page background image
    bool importPdfPages(const QString &pdfPath);

    /// Dialog wie „Neue Seite von Vorlage“ für die aktuell sichtbare Seite (Farbe + Muster)
    void openPageLayoutForVisiblePage();

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


public:
#ifdef Q_OS_ANDROID
    /// Request a one-shot auto-fit-to-width on the next show / next event-loop
    /// cycle. Call this from the editor when a fresh note has been wired up.
    /// No-op if the user has already manually zoomed.
    void requestAutoFit();
#endif

protected:
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent *event) override;
    void wheelEvent(QWheelEvent*) override;

#ifdef Q_OS_ANDROID
    /// Apply a fit-to-width transform so the A4 page exactly fits the current
    /// viewport horizontally. No-op if the user has manually zoomed.
    void autoFitPageToViewportWidth();
#endif
    void tabletEvent(QTabletEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

    // WICHTIG: Gesten kommen hier an
    bool viewportEvent(QEvent* e) override;

    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    QGraphicsScene scene_;
    Note* note_{nullptr};
    ToolMode mode_{ToolMode::Pen};
    qreal zoom_{1.0};
    bool drawing_{false};
    int currentPage_{0};
#ifdef Q_OS_ANDROID
    /// True once the user has pinched/wheel-zoomed the canvas. Suppresses the
    /// auto-fit-to-width behaviour that otherwise re-fits the page on resize.
    bool m_userTouchedZoom{false};
    /// True until the very first auto-fit has been applied on Android, so we
    /// don't override an already-restored zoom from a saved session.
    bool m_pendingInitialFit{true};
    /// Reentrancy lock for autoFitPageToViewportWidth() so a scrollbar toggle
    /// inside setTransform() cannot recursively trigger another auto-fit pass.
    bool m_isAutoFitting{false};
#endif

    NoteSelectionMenu* m_selectionMenu{nullptr};

    TransformOverlay* m_transformOverlay{nullptr};
    QGraphicsItemGroup* m_transformGroup{nullptr};

    bool m_penOnlyMode{false};

    // Flags für Panning & Zoom
    bool m_isPanning{false};
    bool m_isZooming{false};
    QPoint m_lastPanPos;

    /// Kumuliertes „Überziehen“ am unteren Rand (Rad / Pan), bis das Blob-Overlay erscheint
    float m_pullDistance{0.0f};
    QWidget* m_bottomSheet{nullptr};
    QFrame* m_pagesBarCard{nullptr};
    SkeletonPageItem* m_pagesBarAnchorStrip{nullptr};

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
    /// Scene rect must cover mapToScene(viewport) or letterbox margins show wrong color until scroll.
    void ensureSceneRectCoversViewport();
    QGraphicsPathItem *createStrokeGraphicsItem(const Stroke &s);
    void pushStrokeUndoCommand(int pageIdx, Stroke stroke);
    // void ensureOverscrollPage(); // Entfernt/Ersetzt durch Pull-Logik
    int pageAt(const QPointF& scenePos) const;
    QRectF pageRect(int idx) const;

    void addNewPage();
    void addNewPageWithLayout(int backgroundType, const QColor &paperColor);
    void applyLayoutToPage(int pageIndex, int backgroundType, const QColor &paperColor);
    int visiblePageIndex() const;
    void showBottomSheetFromPull();
    void hideBottomSheet();
    void updateBottomSheetGeometry();
    void syncPagesBarVisibility();
    /// Unterkante der letzten Seite in Szenenkoordinaten (für Panel-Position).
    qreal lastPageBottomSceneY() const;
    /// Sichtbarkeit: nur wenn die Skeleton-Leiste (unter der letzten Seite) im Viewport liegt.
    bool isSkeletonStripIntersectingViewport() const;
    void pickAndAddImagePage();
    void pickAndImportPdf();
    void showBottomMoreMenu();
    void openTemplatePageDialog();

    void gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
    void pushUndoSnapshot(const QVector<NotePage>& beforeState);

    /// After a stroke tool finishes (mouse or tablet), move StrokeItems from the scene into the note model.
    void commitPendingStrokeItemsToNote(AbstractTool* tool);
    void syncGraphItemsToNote();
    void refreshGraphPanelForSelection();
    void syncGraphLegendLayout();
    void repositionGraphEntryBar();
    void ensureGraphLegendFadeSetup();
    void ensureGraphEntryBarFadeSetup();
    void presentGraphLegendAnimated();
    void hideGraphLegendQuick();
    void hideGraphQuickPopup();
    void presentGraphEntryBarAnimated();
    void hideGraphEntryBarQuick();
    void closeGraphEntryBar();
    void abandonGraphEntrySession();
    void openGraphEntryBarForGraph(GraphCanvasItem* gi, bool fromPlus = false);
    void openGraphFormulaZone(GraphCanvasItem* gi);

    void syncGraphPlusLayout(GraphCanvasItem* gi);
    void bindGraphChrome(GraphCanvasItem* gi);
    /// After scene_.clear(): drop dangling graph pointers, hide overlays (no graph item access).
    void resetGraphChromeAfterSceneClear();
    /// After model sync: refresh bound data and layout only if chrome is already visible.
    void updateGraphChromeIfVisible();

    GraphLegendDock* m_graphLegendDock{nullptr};
    GraphQuickActionPopup* m_graphQuickPopup{nullptr};
    GraphFormulaEntryBar* m_graphEntryBar{nullptr};
    GraphTangentXPopup* m_tangentXPopup{nullptr};
    GraphCanvasItem* m_selectedGraphItem{nullptr};
    GraphCanvasItem* m_graphPanelTargetGraph{nullptr};
    bool m_graphPanelExplicitOpen{false};
    /// Last plot-background tap in scene coords (quick action popup anchor).
    QPointF m_graphQuickAnchorScene;
    bool m_graphQuickPopupWanted{false};
    bool m_syncingGraphs{false};
    int m_livePreviewIndex{-1};
    bool m_graphEntryBarOpen{false};
    GraphCanvasItem* m_graphEntryTargetGraph{nullptr};
    /// While set, graph "+" interaction bypasses tools and touch pan (see CanvasView equivalent).
    GraphCanvasItem* m_graphPlusBypassItem{nullptr};
    GraphCanvasItem* m_graphPlotBypassItem{nullptr};
    /// Pending stylus/tablet tap on graph chrome (press without mouse bypass).
    QPointer<GraphCanvasItem> m_graphTabletPendingItem;
    QPointF m_graphTabletPressScene;
    qint64 m_graphTabletPressMs{0};

    QGraphicsOpacityEffect* m_graphLegendOpacityFx{nullptr};
    QPropertyAnimation* m_graphLegendFadeAnim{nullptr};
    QGraphicsOpacityEffect* m_graphEntryBarOpacityFx{nullptr};
    QPropertyAnimation* m_graphEntryBarFadeAnim{nullptr};

    /// Currently active inline formula input zone (created by "+" tap).
    QPointer<GraphFormulaZone> m_activeFormulaZone;
};
