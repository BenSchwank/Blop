#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QTabletEvent>
#include <QElapsedTimer> // NEU
#include <functional>
#include "Note.h"
#include "ToolMode.h"
#include "KineticPredictor.h" // NEU

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
    Note* note_{nullptr};
    ToolMode mode_{ToolMode::Pen};
    qreal zoom_{1.0};
    bool drawing_{false};
    int currentPage_{0};

    QColor penColor_{Qt::black};
    qreal penWidth_{2.0};
    QVector<QGraphicsRectItem*> pageItems_;

    Stroke currentStroke_;
    QGraphicsPathItem* currentPathItem_{nullptr};

    // --- NEU FÜR KINETIC ENGINE ---
    KineticPredictor m_predictor;
    QElapsedTimer m_timer;
    QGraphicsPathItem* m_phantomPathItem_{nullptr}; // Zeigt die Vorhersage (Phantom Ink)
    // ------------------------------

    void layoutPages();
    void ensureOverscrollPage();
    int pageAt(const QPointF& scenePos) const;
    QRectF pageRect(int idx) const;
};
