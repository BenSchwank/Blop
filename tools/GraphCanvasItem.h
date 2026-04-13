#pragma once

#include "../Note.h"
#include <QElapsedTimer>
#include <QGraphicsObject>

class GraphCanvasItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum { Type = QGraphicsItem::UserType + 220 };
    int type() const override { return Type; }

    explicit GraphCanvasItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    GraphObject toData() const;
    void fromData(const GraphObject& d);
    /// Update only the function list + selectedFunction without touching position/size.
    void updateFunctions(const QVector<GraphFunction>& fns, int selectedFn);

    /// After external `fromData` (e.g. axis dialog), call this so MultiPageNoteView can sync the note.
    void notifyGraphChanged();

    int selectedFunction() const { return m_data.selectedFunction; }
    void setSelectedFunction(int idx);
    GraphObject& data() { return m_data; }
    const GraphObject& data() const { return m_data; }

    /// Szene-Rechteck um den aktiven +-Button (Slot = Anzahl fester Funktionen).
    QRectF plusButtonSceneRect() const;

    void setCommittedFunctionCountForPlusLayout(int count);
    void setPlusButtonSuppressed(bool suppressed);
    int committedFunctionCountForPlusLayout() const { return m_committedPlusCount; }

    /// True if scenePos hits the visible "+" (respects suppression when the entry bar is open).
    bool hitPlusButtonAtScene(const QPointF& scenePos) const;

    /// Same effect as the user tapping "+" (forwards to connected UI, e.g. formula bar).
    void requestPlusTap();

    /// True for taps on the inner plot (not on a curve hit-zone, not +, not resize handle).
    bool hitPlotBackgroundAtScene(const QPointF& scenePos) const;

    /// True if scenePos lies on this graph's interactive chrome (frame, plot, + column, resize area).
    bool hitGraphChromeAtScene(const QPointF& scenePos) const;

    /// Same tap outcome as mouse release at scenePos (for tablet when mouse bypass is not used).
    void deliverTapFromScene(const QPointF& scenePos, int holdElapsedMs = 0);

    void requestPlotBackgroundTap();

signals:
    void graphChanged();
    /// Position/size tweak while dragging (not full graphChanged — avoids heavy note sync each pixel).
    void graphGeometryTweaked();
    void functionTapped(int index);
    void functionLongPressed(int index);
    void plusTapped();
    /// User tapped near the x-axis (y=0) inside the plot; scene position for anchoring a popup.
    void xAxisTapped(double dataX, QPointF scenePos);
    void plotBackgroundTapped(QPointF scenePos);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    int nearestFunctionAtScenePos(const QPointF& scenePos, qreal* outDist = nullptr) const;
    QPointF mapToLocalPlot(double x, double y) const;
    QRectF plusButtonLocalRect() const;
    QRectF plusButtonHitLocalRect() const;
    QRectF plotAreaLocalRect() const;
    bool hitXAxisAtLocal(const QPointF& localPos, double* outDataX) const;

    QRectF m_rect{0, 0, 280, 180};
    GraphObject m_data;
    int m_committedPlusCount{0};
    bool m_plusSuppressed{false};
    bool m_resizing{false};
    QPointF m_resizeStartScene;
    QRectF m_resizeStartRect;
    QElapsedTimer m_holdTimer;
    int m_pressedFunction{-1};
    bool m_pressedPlus{false};

    void processTapRelease(const QPointF& scenePos, int holdElapsedMs);
};
