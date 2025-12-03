#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QTabletEvent>
#include <functional> // Wichtig
#include "Note.h"
#include "ToolMode.h"

class MultiPageNoteView : public QGraphicsView {
    Q_OBJECT
public:
    explicit MultiPageNoteView(QWidget* parent=nullptr);

    void setNote(Note* note);
    Note* note() const { return note_; }

    void setToolMode(ToolMode m) { mode_ = m; }
    void setPenColor(const QColor& c) { penColor_ = c; }
    void setPenWidth(qreal w) { penWidth_ = w; }

    bool exportPageToPng(int pageIndex, const QString &path);
    bool exportPageToPdf(int pageIndex, const QString &path);

    std::function<void(Note*)> onSaveRequested;

protected:
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void tabletEvent(QTabletEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    bool viewportEvent(QEvent* e) override;

private:
    QGraphicsScene scene_;
    Note* note_{nullptr}; // <--- Hier definiert
    ToolMode mode_{ToolMode::Pen};
    qreal zoom_{1.0};
    bool drawing_{false};
    int currentPage_{0};

    QColor penColor_{Qt::black};
    qreal penWidth_{2.0};
    QVector<QGraphicsRectItem*> pageItems_;

    Stroke currentStroke_;
    QGraphicsPathItem* currentPathItem_{nullptr};

    void layoutPages();
    void ensureOverscrollPage();
    int pageAt(const QPointF& scenePos) const;
    QRectF pageRect(int idx) const;
};
