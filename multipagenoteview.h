#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QTabletEvent>
#include <QPinchGesture>
#include <QGestureEvent>
#include <functional>
#include "Note.h"
#include "ToolMode.h"
#include "PageItem.h"

class MultiPageNoteView : public QGraphicsView {
    Q_OBJECT
public:
    explicit MultiPageNoteView(QWidget* parent=nullptr);

    void setNote(Note* note);
    Note* note() const { return note_; }

    void setToolMode(ToolMode m) { mode_ = m; }
    void setPenColor(const QColor& c) { penColor_ = c; }
    void setPenWidth(qreal w) { penWidth_ = w; }

    void setPenOnlyMode(bool enabled) { m_penOnlyMode = enabled; }
    bool penOnlyMode() const { return m_penOnlyMode; }

    bool exportPageToPng(int pageIndex, const QString &path);
    bool exportPageToPdf(int pageIndex, const QString &path);

    // NEU: Thumbnail Generierung für PageManager
    QPixmap generateThumbnail(int pageIndex, QSize size);

    // NEU: Seite verschieben
    void movePage(int fromIndex, int toIndex);

    // NEU: Scrollt zu einer bestimmten Seite
    void scrollToPage(int pageIndex);

    std::function<void(Note*)> onSaveRequested;

protected:
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void tabletEvent(QTabletEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    bool viewportEvent(QEvent* e) override;
    bool event(QEvent *event) override;

private:
    QGraphicsScene scene_;
    Note* note_{nullptr};
    ToolMode mode_{ToolMode::Pen};
    qreal zoom_{1.0};
    bool drawing_{false};
    int currentPage_{0};

    bool m_penOnlyMode{true};
    bool m_isPanning{false};
    QPoint m_lastPanPos;

    QColor penColor_{Qt::black};
    qreal penWidth_{2.0};

    QVector<PageItem*> pageItems_;
    Stroke currentStroke_;
    QGraphicsPathItem* currentPathItem_{nullptr};

    void layoutPages();
    void ensureOverscrollPage();
    int pageAt(const QPointF& scenePos) const;
    QRectF pageRect(int idx) const;

    void gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
};
