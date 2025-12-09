#include "multipagenoteview.h"
#include "UIStyles.h"
#include <QGraphicsRectItem>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <cmath>
#include <algorithm>
#include <QPointingDevice>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>

static constexpr int A4W = 793;
static constexpr int A4H = 1122;
static constexpr int PageSpacing = 40;

MultiPageNoteView::MultiPageNoteView(QWidget *parent) : QGraphicsView(parent) {
    setScene(&scene_);
    setBackgroundBrush(UIStyles::SceneBackground);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate); // Wichtig für flüssiges Zeichnen
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(false);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    // Timer starten für Zeitstempel
    m_timer.start();
}

void MultiPageNoteView::setNote(Note *note) {
    note_ = note;
    scene_.clear();
    pageItems_.clear();
    layoutPages();

    if (!note_) return;

    for (int i = 0; i < note_->pages.size(); ++i) {
        for (const auto &s : note_->pages[i].strokes) {
            auto item = new QGraphicsPathItem(s.path);
            item->setZValue(1.0);
            QPen pen;
            if (s.isEraser) {
                pen = QPen(UIStyles::PageBackground, s.width);
            } else {
                pen = QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            }
            item->setPen(pen);
            scene_.addItem(item);
            item->setPos(pageRect(i).topLeft());
        }
    }
}

void MultiPageNoteView::layoutPages() {
    if (!note_) return;
    int n = std::max(1, (int)note_->pages.size());
    qreal y = 0;
    for (int i = 0; i < n; ++i) {
        auto rectItem = new QGraphicsRectItem(0, 0, A4W, A4H);
        rectItem->setBrush(UIStyles::PageBackground);
        rectItem->setPen(QPen(QColor(200, 200, 200), 1));
        rectItem->setZValue(0.0);
        scene_.addItem(rectItem);
        rectItem->setPos(0, y);
        pageItems_.push_back(rectItem);
        y += A4H + PageSpacing;
    }
    scene_.setSceneRect(QRectF(-20, -20, A4W + 40, y + 40));
}

QRectF MultiPageNoteView::pageRect(int idx) const {
    qreal y = idx * (A4H + PageSpacing);
    return QRectF(0, y, A4W, A4H);
}

int MultiPageNoteView::pageAt(const QPointF &scenePos) const {
    for (int i = 0; i < pageItems_.size(); ++i) {
        if (pageItems_[i]->sceneBoundingRect().contains(scenePos))
            return i;
    }
    return -1;
}

void MultiPageNoteView::ensureOverscrollPage() {
    if (!note_) return;
    auto vbar = verticalScrollBar();
    int max = vbar->maximum();
    if (max - vbar->value() < 80) {
        note_->ensurePage(note_->pages.size());
        layoutPages();
        if (onSaveRequested) onSaveRequested(note_);
    }
}

void MultiPageNoteView::resizeEvent(QResizeEvent *) { ensureOverscrollPage(); }

void MultiPageNoteView::wheelEvent(QWheelEvent *e) {
    if (e->modifiers() & Qt::ControlModifier) {
        qreal delta = e->angleDelta().y() / 120.0;
        qreal factor = (delta > 0) ? 1.1 : 0.9;
        qreal newZoom = std::clamp(zoom_ * factor, 0.25, 4.0);
        scale(newZoom / zoom_, newZoom / zoom_);
        zoom_ = newZoom;
        e->accept();
    } else {
        QGraphicsView::wheelEvent(e);
        ensureOverscrollPage();
    }
}

bool MultiPageNoteView::viewportEvent(QEvent *ev) {
    if (ev->type() == QEvent::TouchBegin || ev->type() == QEvent::TouchUpdate || ev->type() == QEvent::TouchEnd) {
        return QGraphicsView::viewportEvent(ev);
    }
    return QGraphicsView::viewportEvent(ev);
}

// --------------------------------------------------------------------------------
// HIGH PERFORMANCE INKING LOGIK
// --------------------------------------------------------------------------------
void MultiPageNoteView::tabletEvent(QTabletEvent *e) {
    if (!note_ || mode_ == ToolMode::Lasso) {
        e->ignore();
        return;
    }

    QPointF scenePos = mapToScene(e->position().toPoint());
    int p = pageAt(scenePos);

    // Außerhalb der Seite abbrechen, außer wir zeichnen schon
    if (p < 0 && !drawing_) { e->ignore(); return; }
    if (p < 0) p = currentPage_; // Weiterzeichnen auch wenn man leicht rausmalt

    QPointF local = scenePos - pageRect(p).topLeft();

    if (e->type() == QEvent::TabletPress) {
        drawing_ = true;
        currentPage_ = p;

        // Predictor Reset
        m_predictor.reset();
        m_predictor.addPoint(local, m_timer.elapsed()); // Erster Punkt

        currentStroke_ = Stroke{};
        currentStroke_.pageIndex = p;
        currentStroke_.isEraser = (mode_ == ToolMode::Eraser);
        currentStroke_.width = (mode_ == ToolMode::Eraser ? 12.0 : std::max<qreal>(2.0, e->pressure() * 4.0));
        currentStroke_.color = penColor_;

        // Initialer Punkt (geglättet ist hier gleich roh)
        currentStroke_.points.push_back(local);
        currentStroke_.path = QPainterPath(local);

        // Haupt-Item erstellen
        currentPathItem_ = new QGraphicsPathItem();
        QPen pen = currentStroke_.isEraser
                       ? QPen(UIStyles::PageBackground, currentStroke_.width)
                       : QPen(currentStroke_.color, currentStroke_.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        currentPathItem_->setPen(pen);
        currentPathItem_->setZValue(1.0);
        scene_.addItem(currentPathItem_);
        currentPathItem_->setPos(pageRect(p).topLeft());

        // Phantom-Item (für die Vorhersage)
        m_phantomPathItem_ = new QGraphicsPathItem();
        QPen phantomPen = pen;
        // Optional: Phantom leicht transparenter machen oder dünner
        QColor pc = pen.color(); pc.setAlpha(150); phantomPen.setColor(pc);
        m_phantomPathItem_->setPen(phantomPen);
        m_phantomPathItem_->setZValue(1.1); // Über dem echten Strich
        scene_.addItem(m_phantomPathItem_);
        m_phantomPathItem_->setPos(pageRect(p).topLeft());

        e->accept();

    } else if (e->type() == QEvent::TabletMove && drawing_) {
        // 1. Physik Update
        m_predictor.addPoint(local, m_timer.elapsed());

        QPointF smoothed = m_predictor.getSmoothedPoint();
        QPointF predicted = m_predictor.getPredictedPoint();

        // 2. Daten speichern (nur geglättete, keine vorhergesagten!)
        currentStroke_.points.push_back(smoothed);
        currentStroke_.path.lineTo(smoothed);

        // 3. Echten Strich updaten (Sicherer Pfad)
        currentPathItem_->setPath(currentStroke_.path);

        // 4. Phantom Ink zeichnen (Brücke von smoothed -> predicted)
        // Wir nehmen den letzten Punkt des echten Pfads und ziehen eine Linie zum Predicted Point
        QPainterPath phantomPath;
        phantomPath.moveTo(smoothed);
        phantomPath.lineTo(predicted);
        m_phantomPathItem_->setPath(phantomPath);

        e->accept();

    } else if (e->type() == QEvent::TabletRelease && drawing_) {
        drawing_ = false;

        // Phantom Ink entfernen (die Zukunft ist jetzt Vergangenheit)
        if (m_phantomPathItem_) {
            scene_.removeItem(m_phantomPathItem_);
            delete m_phantomPathItem_;
            m_phantomPathItem_ = nullptr;
        }

        // Finalisieren: Der letzte geglättete Punkt ist schon drin.
        // Optional: Den allerletzten Raw-Punkt noch hinzufügen, damit der Strich den Stift einholt?
        // Bei guter Prediction meist nicht nötig, da Prediction auf 0 konvergiert wenn Speed 0.

        if (note_) {
            note_->pages[currentPage_].strokes.push_back(currentStroke_);
            if (onSaveRequested) onSaveRequested(note_);
        }
        currentPathItem_ = nullptr;
        e->accept();
    } else {
        e->ignore();
    }
}

void MultiPageNoteView::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        QTabletEvent fake(QEvent::TabletPress, QPointingDevice::primaryPointingDevice(), e->position(), e->globalPosition(), 1.0, 0, 0, 0, 0, 0, e->modifiers(), e->button(), e->buttons());
        tabletEvent(&fake);
    } else {
        QGraphicsView::mousePressEvent(e);
    }
}

void MultiPageNoteView::mouseMoveEvent(QMouseEvent *e) {
    if (e->buttons() & Qt::LeftButton && drawing_) {
        QTabletEvent fake(QEvent::TabletMove, QPointingDevice::primaryPointingDevice(), e->position(), e->globalPosition(), 1.0, 0, 0, 0, 0, 0, e->modifiers(), e->button(), e->buttons());
        tabletEvent(&fake);
    } else {
        QGraphicsView::mouseMoveEvent(e);
    }
}

void MultiPageNoteView::mouseReleaseEvent(QMouseEvent *e) {
    if (drawing_) {
        QTabletEvent fake(QEvent::TabletRelease, QPointingDevice::primaryPointingDevice(), e->position(), e->globalPosition(), 1.0, 0, 0, 0, 0, 0, e->modifiers(), e->button(), e->buttons());
        tabletEvent(&fake);
    } else {
        QGraphicsView::mouseReleaseEvent(e);
    }
}

bool MultiPageNoteView::exportPageToPng(int pageIndex, const QString &path) {
    // (Unverändert, siehe oben im Original-Snippet wenn nötig, Logik bleibt gleich)
    QImage img(A4W, A4H, QImage::Format_ARGB32_Premultiplied);
    img.fill(UIStyles::PageBackground);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing); // Wichtig für Export
    if (note_ && pageIndex >= 0 && pageIndex < note_->pages.size()) {
        for (const auto &s : note_->pages[pageIndex].strokes) {
            QPen pen(s.isEraser ? QPen(QBrush(UIStyles::PageBackground), s.width)
                                : QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setPen(pen);
            p.drawPath(s.path);
        }
    }
    return img.save(path);
}

bool MultiPageNoteView::exportPageToPdf(int pageIndex, const QString &path) {
    QPdfWriter pdf(path);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&pdf);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(QRectF(0, 0, A4W, A4H), UIStyles::PageBackground);
    if (note_ && pageIndex >= 0 && pageIndex < note_->pages.size()) {
        for (const auto &s : note_->pages[pageIndex].strokes) {
            QPen pen(s.isEraser ? QPen(QBrush(UIStyles::PageBackground), s.width)
                                : QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setPen(pen);
            p.drawPath(s.path);
        }
    }
    p.end();
    return true;
}

