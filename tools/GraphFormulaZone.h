#pragma once

#include <QGraphicsObject>
#include <QPainterPath>
#include <QTimer>
#include <QVector>

class GraphCanvasItem;
class QNetworkAccessManager;
class QNetworkReply;

/// A single formula input "line" positioned to the right of a GraphCanvasItem.
///
/// Visual layout (in parent-local coords of GraphCanvasItem):
///   ┌───────────────────────┐
///   │  · · · · · · · · · ·  │  ← dotted writing baseline
///   │  y = x²               │  ← recognized expression (small text)
///   │            (🗙)       │  ← round clear button
///   └───────────────────────┘
///
/// The zone does NOT capture mouse events itself.  Instead, MultiPageNoteView
/// checks every committed stroke against active zones and calls addStroke().
/// This keeps the normal pen/eraser flow 100 % intact.
class GraphFormulaZone : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = QGraphicsItem::UserType + 230 };
    int type() const override { return Type; }

    /// slotIndex = vertical position index (0 = first slot, 1 = second, …).
    explicit GraphFormulaZone(int slotIndex, GraphCanvasItem *parentGraph);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

    // ── Stroke management ────────────────────────────────────────────
    /// Add a page-local stroke that overlaps this zone.  Returns true if
    /// the stroke was accepted (i.e. its bounding rect actually overlaps).
    bool addStroke(const QPainterPath &path, const QColor &color, qreal width);

    /// Remove a stroke (e.g. on eraser action).  Triggers re-recognition.
    void removeStroke(const QPainterPath &path);

    /// Erase specific parts of strokes using the eraser's bounding path.
    void eraseStrokesIntersecting(const QPainterPath &eraserPath, qreal eraserWidth);

    /// Clear all strokes and reset state.
    void clearZone();

    /// True when the zone has strokes (active writing).
    bool hasStrokes() const { return !m_strokes.isEmpty(); }

    /// Scene bounding rect of the "catch area" where strokes are captured.
    QRectF catchAreaSceneRect() const;

    // ── Recognition results ──────────────────────────────────────────
    /// Returns the most recently recognized expression, or empty.
    QString recognizedExpression() const { return m_recognizedExpr; }

    /// True while auto-commit countdown is running (preview curve should be dashed).
    bool isInPreview() const { return m_previewActive; }

    int slotIndex() const { return m_slotIndex; }

    /// True if the small round clear button at `scenePos` was hit.
    bool hitClearButton(const QPointF &scenePos) const;
    
    /// True if the checkmark commit button at `scenePos` was hit.
    bool hitCommitButton(const QPointF &scenePos) const;

signals:
    /// Emitted when a new expression was recognized (preview curve).
    void expressionRecognized(const QString &expr);

    /// Emitted when the user explicitly clicks the checkmark button.
    void commitRequested(const QString &expr);

    /// Emitted when zone is cleared by the user.
    void zoneCleared();

private slots:
    void onRecognizeTimer();

private:
    void scheduleRecognition();
    void cancelTimers();
    QImage renderStrokesToImage() const;
    void startBackendRecognition(const QImage &img);
    QString recognizeLocalCandidates() const;

    // ── Data ─────────────────────────────────────────────────────────
    int m_slotIndex{0};
    GraphCanvasItem *m_parentGraph{nullptr};

    struct InkStroke {
        QPainterPath path;
        QColor       color;
        qreal        width{2.0};
    };
    QVector<InkStroke> m_strokes;

    QString m_recognizedExpr;
    bool    m_previewActive{false};

    QTimer  m_recognizeTimer;    // 600 ms after last stroke → run OCR

    QNetworkAccessManager *m_nam{nullptr};
    QNetworkReply *m_pendingReply{nullptr};

    // ── Layout constants & state ─────────────────────────────────────
    qreal m_currentWidth{180.0};
    static constexpr qreal kSlotHeight   = 44.0;
    static constexpr qreal kSlotWidth    = 180.0;
    static constexpr qreal kMarginRight  = 14.0;   // gap between graph edge and zone
    static constexpr qreal kClearBtnSize = 18.0;
    static constexpr qreal kCommitBtnSize = 22.0;
    static constexpr qreal kBaselineY    = 28.0;    // y within slot for the solid line
};
