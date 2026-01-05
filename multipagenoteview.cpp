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
static constexpr int PageSpacing = 60; // Deutlicher Abstand zwischen Seiten

MultiPageNoteView::MultiPageNoteView(QWidget *parent) : QGraphicsView(parent) {
    setScene(&scene_);
    setBackgroundBrush(UIStyles::SceneBackground);

    // WICHTIG: Gesten auf dem Viewport registrieren
    viewport()->grabGesture(Qt::PinchGesture);

    setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // WICHTIG: Kein automatisches Dragging, da es Gesten stören kann.
    // Wir implementieren Panning selbst.
    setDragMode(QGraphicsView::NoDrag);

    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(false);

    // Touch Events explizit akzeptieren, damit Gesten funktionieren
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
}

void MultiPageNoteView::setNote(Note *note) {
    note_ = note;
    scene_.clear();
    // Items wurden gelöscht, Pointer müssen auch aus der Liste raus
    pageItems_.clear();

    layoutPages();

    if (!note_) return;

    bool wasBlocked = scene_.blockSignals(true);

    for (int i = 0; i < note_->pages.size(); ++i) {
        for (const auto &s : note_->pages[i].strokes) {
            auto item = new QGraphicsPathItem(s.path);

            if (s.isHighlighter) item->setZValue(0.5);
            else item->setZValue(1.0);

            QPen pen;
            if (s.isEraser) {
                pen = QPen(UIStyles::PageBackground, s.width);
            } else if (s.isHighlighter) {
                QColor c = s.color;
                c.setAlpha(80);
                pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            } else {
                pen = QPen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            }

            item->setPen(pen);
            scene_.addItem(item);
            item->setPos(pageRect(i).topLeft());
        }
    }
    scene_.blockSignals(wasBlocked);
}

void MultiPageNoteView::layoutPages() {
    // Alte Seiten sicher entfernen
    for (auto* item : pageItems_) {
        // Prüfen, ob Item noch in Scene ist (könnte durch clear() schon weg sein)
        if (item->scene() == &scene_) {
            scene_.removeItem(item);
        }
        delete item;
    }
    pageItems_.clear();

    if (!note_) return;
    int n = std::max(1, (int)note_->pages.size());
    qreal y = 0;

    for (int i = 0; i < n; ++i) {
        auto pageItem = new PageItem(0, 0, A4W, A4H);
        scene_.addItem(pageItem);
        pageItem->setPos(0, y);
        pageItems_.push_back(pageItem);

        y += A4H + PageSpacing; // Lücke zwischen Seiten
    }

    // SceneRect anpassen, damit man scrollen kann
    scene_.setSceneRect(QRectF(-100, -100, A4W + 200, y + 200));
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
    // Wenn wir fast unten sind, neue Seite anfügen
    if (max > 0 && max - vbar->value() < 80) {
        note_->ensurePage(note_->pages.size());
        layoutPages();
        if (onSaveRequested)
            onSaveRequested(note_);
    }
}

void MultiPageNoteView::resizeEvent(QResizeEvent *) { ensureOverscrollPage(); }

void MultiPageNoteView::wheelEvent(QWheelEvent *e) {
    if (e->modifiers() & Qt::ControlModifier) {
        // Zoom per Mausrad + STRG
        qreal delta = e->angleDelta().y() / 120.0;
        qreal factor = (delta > 0) ? 1.1 : 0.9;
        scale(factor, factor);
        zoom_ *= factor;
        e->accept();
    } else {
        QGraphicsView::wheelEvent(e);
        ensureOverscrollPage();
    }
}

bool MultiPageNoteView::viewportEvent(QEvent *ev) {
    // Standard-Verarbeitung (wichtig für TouchBegin/Update/End)
    return QGraphicsView::viewportEvent(ev);
}

// --- GESTEN & TOUCH LOGIK ---

bool MultiPageNoteView::event(QEvent *event) {
    if (event->type() == QEvent::Gesture) {
        gestureEvent(static_cast<QGestureEvent*>(event));
        return true;
    }
    return QGraphicsView::event(event);
}

void MultiPageNoteView::cancelDrawing() {
    if (drawing_) {
        drawing_ = false;
        // Entferne den aktuellen, unfertigen Strich aus der Szene
        if (currentPathItem_) {
            scene_.removeItem(currentPathItem_);
            delete currentPathItem_;
            currentPathItem_ = nullptr;
        }
    }
}

void MultiPageNoteView::gestureEvent(QGestureEvent *event) {
    if (QGesture *pinch = event->gesture(Qt::PinchGesture)) {
        pinchTriggered(static_cast<QPinchGesture *>(pinch));
    }
}

void MultiPageNoteView::pinchTriggered(QPinchGesture *gesture) {
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();

    // Sobald ein Pinch beginnt (oder läuft), brechen wir Zeichnen/Panning ab
    if (gesture->state() == Qt::GestureStarted || gesture->state() == Qt::GestureUpdated) {
        if (drawing_) cancelDrawing();
        if (m_isPanning) {
            m_isPanning = false;
            setCursor(Qt::ArrowCursor);
        }
    }

    // 1. Zoom (Scale)
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        qreal factor = gesture->scaleFactor();
        qreal currentScale = transform().m11();

        // Limits
        if ((currentScale < 0.25 && factor < 1.0) || (currentScale > 4.0 && factor > 1.0)) {
            // Limit erreicht
        } else {
            scale(factor, factor);
            zoom_ *= factor;
        }
    }

    // 2. Pan (Verschieben) mit 2 Fingern
    if (changeFlags & QPinchGesture::CenterPointChanged) {
        QPointF delta = gesture->centerPoint() - gesture->lastCenterPoint();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    }

    if (gesture->state() == Qt::GestureFinished) {
        ensureOverscrollPage();
    }
}

void MultiPageNoteView::mousePressEvent(QMouseEvent *e) {
    const QInputDevice* dev = e->device();
    bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
    if (!isTouch && e->source() == Qt::MouseEventSynthesizedBySystem) isTouch = true;

    // --- FALL 1: Touch-Eingabe ---
    if (isTouch) {
        if (m_penOnlyMode) {
            // Nur-Stift-Modus an: Finger darf NICHT zeichnen -> Panning (Scrollen)
            m_isPanning = true;
            m_lastPanPos = e->pos();
            setCursor(Qt::ClosedHandCursor);
            e->accept();
            return;
        } else {
            // Touch & Stift an: Finger darf zeichnen.
        }
    }

    // --- Zeichnen initialisieren ---
    if (e->button() == Qt::LeftButton) {
        // TabletEvent synthetisieren für einheitliche Zeichenlogik
        QTabletEvent fake(QEvent::TabletPress,
                          QPointingDevice::primaryPointingDevice(),
                          e->position(),
                          e->globalPosition(),
                          1.0, 0, 0, 0, 0, 0,
                          e->modifiers(),
                          e->button(),
                          e->buttons());
        tabletEvent(&fake);

        // Wir akzeptieren das Event, damit wir MouseMoves bekommen.
        // Sollte eine Geste (Pinch) daraus werden, bricht pinchTriggered das Zeichnen ab.
        e->accept();
    } else {
        QGraphicsView::mousePressEvent(e);
    }
}

void MultiPageNoteView::mouseMoveEvent(QMouseEvent *e) {
    const QInputDevice* dev = e->device();
    bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
    if (!isTouch && e->source() == Qt::MouseEventSynthesizedBySystem) isTouch = true;

    // Panning (Verschieben mit 1 Finger im Pen-Only-Modus)
    if (m_isPanning) {
        QPoint delta = e->pos() - m_lastPanPos;
        m_lastPanPos = e->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        ensureOverscrollPage();
        e->accept();
        return;
    }

    // Zeichnen fortsetzen
    if (drawing_ && (e->buttons() & Qt::LeftButton)) {
        QTabletEvent fake(QEvent::TabletMove,
                          QPointingDevice::primaryPointingDevice(),
                          e->position(),
                          e->globalPosition(),
                          1.0, 0, 0, 0, 0, 0,
                          e->modifiers(),
                          e->button(),
                          e->buttons());
        tabletEvent(&fake);
        e->accept();
    } else {
        QGraphicsView::mouseMoveEvent(e);
    }
}

void MultiPageNoteView::mouseReleaseEvent(QMouseEvent *e) {
    if (m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        e->accept();
        return;
    }

    if (drawing_) {
        QTabletEvent fake(QEvent::TabletRelease,
                          QPointingDevice::primaryPointingDevice(),
                          e->position(),
                          e->globalPosition(),
                          1.0, 0, 0, 0, 0, 0,
                          e->modifiers(),
                          e->button(),
                          e->buttons());
        tabletEvent(&fake);
        e->accept();
    } else {
        QGraphicsView::mouseReleaseEvent(e);
    }
}

void MultiPageNoteView::tabletEvent(QTabletEvent *e) {
    if (!note_ || mode_ == ToolMode::Lasso) {
        e->ignore();
        return;
    }

    QPointF scenePos = mapToScene(e->position().toPoint());
    int p = pageAt(scenePos);

    // Außerhalb der Seiten (und noch nicht am Zeichnen)? Ignorieren.
    if (p < 0 && !drawing_) {
        e->ignore();
        return;
    }
    // Falls wir zeichnen aber p < 0 ist, bleiben wir auf der letzten bekannten Seite
    if (p < 0) p = currentPage_;

    QPointF local = scenePos - pageRect(p).topLeft();

    if (e->type() == QEvent::TabletPress) {
        drawing_ = true;
        currentPage_ = p;
        currentStroke_ = Stroke{};
        currentStroke_.pageIndex = p;

        currentStroke_.isEraser = (mode_ == ToolMode::Eraser);
        currentStroke_.isHighlighter = (mode_ == ToolMode::Highlighter);

        if (currentStroke_.isEraser) {
            currentStroke_.width = 20.0;
            currentStroke_.color = UIStyles::PageBackground;
        } else if (currentStroke_.isHighlighter) {
            currentStroke_.width = 24.0;
            QColor c = penColor_;
            c.setAlpha(80);
            currentStroke_.color = c;
        } else {
            currentStroke_.width = std::max<qreal>(2.0, e->pressure() * 4.0);
            currentStroke_.color = penColor_;
        }

        currentStroke_.points.push_back(local);
        currentStroke_.path = QPainterPath(local);
        currentPathItem_ = new QGraphicsPathItem();

        QPen pen(currentStroke_.color, currentStroke_.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        currentPathItem_->setPen(pen);

        if (currentStroke_.isHighlighter) currentPathItem_->setZValue(0.5);
        else currentPathItem_->setZValue(1.0);

        scene_.addItem(currentPathItem_);
        currentPathItem_->setPos(pageRect(p).topLeft());
        e->accept();
    } else if (e->type() == QEvent::TabletMove && drawing_) {
        if (currentPathItem_) {
            currentStroke_.points.push_back(local);
            currentStroke_.path.lineTo(local);
            currentPathItem_->setPath(currentStroke_.path);
        }
        e->accept();
    } else if (e->type() == QEvent::TabletRelease && drawing_) {
        drawing_ = false;
        if (note_ && currentPathItem_) {
            note_->pages[currentPage_].strokes.push_back(currentStroke_);
            if (onSaveRequested)
                onSaveRequested(note_);
        }
        currentPathItem_ = nullptr;
        e->accept();
    } else {
        e->ignore();
    }
}

// --- Page Management Implementation (NEU) ---

QPixmap MultiPageNoteView::generateThumbnail(int pageIndex, QSize targetSize) {
    if (!note_ || pageIndex < 0 || pageIndex >= note_->pages.size()) {
        QPixmap empty(100, 141);
        empty.fill(Qt::white);
        return empty;
    }

    // Wir erstellen ein Bild der Seite in voller Auflösung und skalieren dann
    QImage img(A4W, A4H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white); // Oder UIStyles::PageBackground
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    // Inhalt der Seite zeichnen (Strokes)
    for (const auto &s : note_->pages[pageIndex].strokes) {
        QColor c = s.color;
        if (s.isHighlighter) c.setAlpha(80);
        else if (!s.isEraser) c.setAlpha(255);

        QPen pen;
        if (s.isEraser) {
            pen = QPen(Qt::white, s.width);
        } else {
            pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        }
        p.setPen(pen);
        p.drawPath(s.path);
    }
    p.end();

    QPixmap px = QPixmap::fromImage(img);
    if (!targetSize.isEmpty()) {
        return px.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return px;
}

void MultiPageNoteView::movePage(int fromIndex, int toIndex) {
    if (!note_) return;
    int maxIdx = note_->pages.size() - 1;
    if (fromIndex < 0 || fromIndex > maxIdx || toIndex < 0 || toIndex > maxIdx || fromIndex == toIndex) return;

    note_->pages.move(fromIndex, toIndex);

    // Komplettes Neuladen der Ansicht, um Reihenfolge zu aktualisieren
    setNote(note_);
    if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::deletePage(int index) {
    if (!note_ || index < 0 || index >= note_->pages.size()) return;

    note_->pages.removeAt(index);
    // Wenn alles gelöscht, leere Seite sicherstellen
    if (note_->pages.isEmpty()) note_->ensurePage(0);

    setNote(note_);
    if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::duplicatePage(int index) {
    if (!note_ || index < 0 || index >= note_->pages.size()) return;

    NotePage copy = note_->pages[index]; // Deep Copy der Struktur
    note_->pages.insert(index + 1, copy);

    setNote(note_);
    if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::scrollToPage(int index) {
    if (!note_ || index < 0 || index >= note_->pages.size()) return;

    QRectF r = pageRect(index);
    // Scrollen
    verticalScrollBar()->setValue(r.top());
}

// --- Export ---

bool MultiPageNoteView::exportPageToPng(int pageIndex, const QString &path) {
    QImage img(A4W, A4H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    if (note_ && pageIndex >= 0 && pageIndex < note_->pages.size()) {
        for (const auto &s : note_->pages[pageIndex].strokes) {
            QColor c = s.color;
            if (s.isHighlighter) c.setAlpha(80);
            else if (!s.isEraser) c.setAlpha(255);

            QPen pen;
            if (s.isEraser) {
                pen = QPen(Qt::white, s.width);
            } else {
                pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            }
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
    p.fillRect(QRectF(0, 0, A4W, A4H), Qt::white);

    if (note_ && pageIndex >= 0 && pageIndex < note_->pages.size()) {
        for (const auto &s : note_->pages[pageIndex].strokes) {
            QColor c = s.color;
            if (s.isHighlighter) c.setAlpha(80);

            QPen pen;
            if (s.isEraser) {
                pen = QPen(Qt::white, s.width);
            } else {
                pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            }
            p.setPen(pen);
            p.drawPath(s.path);
        }
    }
    p.end();
    return true;
}
