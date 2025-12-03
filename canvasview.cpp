#include "canvasview.h"
#include "mainwindow.h"
#include "UIStyles.h"
#include <QScrollBar>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGestureEvent>
#include <utility>
#include <cmath>
#include <QFile>
#include <QDataStream>

SelectionMenu::SelectionMenu(QWidget* parent) : QWidget(parent) {
    setStyleSheet(
        "QWidget { background-color: #252526; border-radius: 8px; border: 1px solid #444; }"
        "QPushButton { background: transparent; border: none; color: white; font-weight: bold; padding: 5px 10px; }"
        "QPushButton:hover { background-color: #3E3E42; border-radius: 4px; }"
        );
    setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout *layout = new QHBoxLayout(this); layout->setContentsMargins(5, 5, 5, 5); layout->setSpacing(5);
    QPushButton *btnCopy = new QPushButton("Kopieren", this); connect(btnCopy, &QPushButton::clicked, this, &SelectionMenu::copyRequested); layout->addWidget(btnCopy);
    QPushButton *btnDup = new QPushButton("Duplizieren", this); connect(btnDup, &QPushButton::clicked, this, &SelectionMenu::duplicateRequested); layout->addWidget(btnDup);
    QPushButton *btnDel = new QPushButton("Löschen", this); btnDel->setStyleSheet("color: #FF5555;"); connect(btnDel, &QPushButton::clicked, this, &SelectionMenu::deleteRequested); layout->addWidget(btnDel);
    adjustSize(); hide();
}

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
    , m_currentItem(nullptr)
    , m_lassoItem(nullptr)
    , m_isDrawing(false)
    , m_isInfinite(true)
    , m_penOnlyMode(true)
    , m_pageColor(UIStyles::PageBackground)
    , m_currentTool(ToolType::Pen)
    , m_penColor(Qt::black)
    , m_penWidth(3)
    , m_isPanning(false)
    , m_pullDistance(0.0f)
{
    m_scene = new QGraphicsScene(this); setScene(m_scene); m_a4Rect = QRectF(0, 0, 794 * 1.5, 1123 * 1.5);

    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    grabGesture(Qt::PinchGesture);

    setPageFormat(true);
    setMouseTracking(true);
    setDragMode(QGraphicsView::NoDrag);

    m_selectionMenu = new SelectionMenu(this);
    connect(m_selectionMenu, &SelectionMenu::deleteRequested, this, &CanvasView::deleteSelection);
    connect(m_selectionMenu, &SelectionMenu::duplicateRequested, this, &CanvasView::duplicateSelection);
}

void CanvasView::setPageColor(const QColor &color) {
    bool isNowDark = (color.value() < 100);
    QList<QGraphicsItem*> items = m_scene->items();
    for (auto *item : std::as_const(items)) {
        if (item->type() == QGraphicsPathItem::Type) {
            QGraphicsPathItem *pathItem = static_cast<QGraphicsPathItem*>(item);
            QColor c = pathItem->pen().color();
            if (isNowDark && (c == Qt::black || c == QColor(0,0,0))) {
                QPen p = pathItem->pen(); p.setColor(Qt::white); pathItem->setPen(p);
            } else if (!isNowDark && (c == Qt::white || c == QColor(255,255,255))) {
                QPen p = pathItem->pen(); p.setColor(Qt::black); pathItem->setPen(p);
            }
        }
    }
    if (isNowDark && (m_penColor == Qt::black)) m_penColor = Qt::white;
    else if (!isNowDark && (m_penColor == Qt::white)) m_penColor = Qt::black;
    m_pageColor = color;
    viewport()->update();
    emit contentModified();
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect) {
    if (m_isInfinite) {
        painter->fillRect(rect, m_pageColor);
    } else {
        painter->fillRect(rect, UIStyles::SceneBackground);
        QRectF pageRect = m_a4Rect.intersected(rect);
        if (!pageRect.isEmpty()) {
            painter->fillRect(pageRect, m_pageColor);
        }
    }
}

void CanvasView::drawForeground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawForeground(painter, rect);
    if (!m_isInfinite) {
        painter->save();
        painter->setPen(QPen(QColor(0,0,0, 40), 2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(m_a4Rect);
        if (m_pullDistance > 10.0f) drawPullIndicator(painter);
        painter->restore();
    }
}

void CanvasView::drawPullIndicator(QPainter* painter) {
    painter->resetTransform();
    int w = viewport()->width(); int h = viewport()->height();
    float maxPull = 250.0f; float progress = qMin(m_pullDistance / maxPull, 1.0f);
    int size = 60; int yPos = h - size - 30; int xPos = (w - size) / 2;
    QRect circleRect(xPos, yPos, size, size);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(QColor(0, 0, 0, 150)); painter->setPen(Qt::NoPen); painter->drawEllipse(circleRect);
    if (progress > 0.05f) {
        QPen arcPen(QColor(0x5E5CE6)); arcPen.setWidth(4); arcPen.setCapStyle(Qt::RoundCap);
        painter->setPen(arcPen); painter->setBrush(Qt::NoBrush);
        int spanAngle = -progress * 360 * 16;
        painter->drawArc(circleRect.adjusted(8, 8, -8, -8), 90 * 16, spanAngle);
    }
    if (progress > 0.2f) {
        painter->setPen(QPen(Qt::white, 3));
        int center = size / 2; int pSize = (size / 4) * progress;
        QPoint c = circleRect.center();
        painter->drawLine(c.x() - pSize, c.y(), c.x() + pSize, c.y());
        painter->drawLine(c.x(), c.y() - pSize, c.x(), c.y() + pSize);
    }
}

void CanvasView::addNewPage() {
    float a4Height = 1123 * 1.5;
    m_a4Rect.setHeight(m_a4Rect.height() + a4Height);
    setSceneRect(m_a4Rect);
    viewport()->update();
}

bool CanvasView::saveToFile() {
    if (m_filePath.isEmpty()) return false;
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&file);
    out << (quint32)0xB10B0001;
    QList<QGraphicsItem*> items = m_scene->items(Qt::AscendingOrder);
    int count = 0;
    for(auto* item : std::as_const(items)) { if(item->type() == QGraphicsPathItem::Type && item != m_lassoItem) count++; }
    out << count;
    for (auto* item : std::as_const(items)) {
        if (item->type() == QGraphicsPathItem::Type && item != m_lassoItem) {
            QGraphicsPathItem* pathItem = static_cast<QGraphicsPathItem*>(item);
            out << pathItem->pos() << pathItem->pen().color() << (int)pathItem->pen().width() << pathItem->path();
        }
    }
    return true;
}

bool CanvasView::loadFromFile() {
    if (m_filePath.isEmpty()) return false;
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QDataStream in(&file);
    quint32 magic; in >> magic; if (magic != 0xB10B0001) return false;
    m_scene->clear(); m_undoList.clear(); m_redoList.clear();
    int count; in >> count;
    for (int i = 0; i < count; ++i) {
        QPointF pos; QColor color; int width; QPainterPath path;
        in >> pos >> color >> width >> path;
        QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QGraphicsPathItem* item = m_scene->addPath(path, pen);
        item->setPos(pos);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setFlag(QGraphicsItem::ItemIsMovable);
    }
    return true;
}

void CanvasView::setTool(ToolType tool) {
    m_currentTool = tool;
    if (tool == ToolType::Select) {
        setCursor(Qt::ArrowCursor); setDragMode(QGraphicsView::RubberBandDrag);
    } else {
        clearSelection(); setDragMode(QGraphicsView::NoDrag);
        if (tool == ToolType::Pen) setCursor(Qt::CrossCursor);
        else if (tool == ToolType::Eraser) setCursor(Qt::ForbiddenCursor);
        else if (tool == ToolType::Lasso) setCursor(Qt::CrossCursor);
    }
}

void CanvasView::setPenColor(const QColor &color) { m_penColor = color; }
void CanvasView::setPenWidth(int width) { m_penWidth = width; }

void CanvasView::undo() { if (m_undoList.isEmpty()) return; QGraphicsItem* lastItem = m_undoList.takeLast(); if (m_scene->items().contains(lastItem)) { m_scene->removeItem(lastItem); m_redoList.append(lastItem); emit contentModified(); } }
void CanvasView::redo() { if (m_redoList.isEmpty()) return; QGraphicsItem* item = m_redoList.takeLast(); m_scene->addItem(item); m_undoList.append(item); emit contentModified(); }
bool CanvasView::canUndo() const { return !m_undoList.isEmpty(); }
bool CanvasView::canRedo() const { return !m_redoList.isEmpty(); }

void CanvasView::deleteSelection() { const QList<QGraphicsItem*> selected = m_scene->selectedItems(); if (selected.isEmpty()) return; for (auto* item : std::as_const(selected)) m_scene->removeItem(item); m_selectionMenu->hide(); emit contentModified(); }

void CanvasView::duplicateSelection() {
    const QList<QGraphicsItem*> selected = m_scene->selectedItems(); if (selected.isEmpty()) return;
    clearSelection();
    for (auto* item : std::as_const(selected)) {
        if (item->type() == QGraphicsPathItem::Type) {
            QGraphicsPathItem* pathItem = static_cast<QGraphicsPathItem*>(item);
            QGraphicsPathItem* newItem = m_scene->addPath(pathItem->path(), pathItem->pen());
            newItem->setPos(item->pos() + QPointF(20, 20));
            newItem->setFlag(QGraphicsItem::ItemIsSelectable); newItem->setFlag(QGraphicsItem::ItemIsMovable);
            newItem->setSelected(true); m_undoList.append(newItem);
        }
    }
    updateSelectionMenuPosition(); emit contentModified();
}

void CanvasView::copySelection() {}
void CanvasView::clearSelection() { m_scene->clearSelection(); if (m_lassoItem) { m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; } m_selectionMenu->hide(); }

void CanvasView::setPageFormat(bool isInfinite) {
    m_isInfinite = isInfinite;
    if (m_isInfinite) { m_scene->setSceneRect(-50000, -50000, 100000, 100000); setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); }
    else { m_scene->setSceneRect(m_a4Rect); setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded); setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); }
    m_scene->update(); viewport()->update();
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        double angle = event->angleDelta().y();
        double factor = std::pow(1.0015, angle);
        double currentScale = transform().m11();
        if ((currentScale < 0.1 && factor < 1) || (currentScale > 10 && factor > 1)) return;
        scale(factor, factor);
        event->accept();
    } else {
        if (!m_isInfinite) {
            QScrollBar *vb = verticalScrollBar();
            if (vb->value() >= vb->maximum() && event->angleDelta().y() < 0) {
                m_pullDistance += std::abs(event->angleDelta().y()) * 0.5;
                viewport()->update();
                if (m_pullDistance > 250.0f) { addNewPage(); m_pullDistance = 0; }
                event->accept(); return;
            } else {
                if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); }
            }
        }
        QGraphicsView::wheelEvent(event);
    }
}

void CanvasView::mousePressEvent(QMouseEvent *event) {
    bool isTouch = (event->source() == Qt::MouseEventSynthesizedBySystem);

    if (m_penOnlyMode && isTouch) { m_isPanning = true; m_lastPanPos = event->pos(); setCursor(Qt::ClosedHandCursor); event->accept(); return; }
    if (event->button() == Qt::MiddleButton) { m_isPanning = true; m_lastPanPos = event->pos(); setCursor(Qt::ClosedHandCursor); event->accept(); return; }

    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        if (m_selectionMenu->isVisible() && !itemAt(event->pos())) m_selectionMenu->hide();
        if (!m_isInfinite && !m_a4Rect.contains(scenePos)) return;

        if (m_currentTool == ToolType::Select) { QGraphicsView::mousePressEvent(event); return; }
        else if (m_currentTool == ToolType::Pen) {
            m_isDrawing = true; m_currentPath = QPainterPath(); m_currentPath.moveTo(scenePos);
            m_currentItem = m_scene->addPath(m_currentPath, QPen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            m_currentItem->setFlag(QGraphicsItem::ItemIsSelectable); m_currentItem->setFlag(QGraphicsItem::ItemIsMovable);
            qDeleteAll(m_redoList); m_redoList.clear(); m_undoList.append(m_currentItem); event->accept(); return;
        }
        else if (m_currentTool == ToolType::Eraser) { m_isDrawing = true; applyEraser(scenePos); event->accept(); return; }
        else if (m_currentTool == ToolType::Lasso) { m_isDrawing = true; clearSelection(); m_currentPath = QPainterPath(); m_currentPath.moveTo(scenePos); m_lassoItem = m_scene->addPath(m_currentPath, QPen(QColor(0x5E5CE6), 2, Qt::DashLine)); event->accept(); return; }
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos; m_lastPanPos = event->pos();

        if (!m_isInfinite) {
            QScrollBar *vb = verticalScrollBar();
            if (vb->value() >= vb->maximum() && delta.y() < 0) {
                m_pullDistance += std::abs(delta.y()) * 0.5; viewport()->update();
                if (m_pullDistance > 250.0f) { addNewPage(); m_pullDistance = 0; }
                event->accept(); return;
            } else {
                if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); }
            }
        }
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept(); return;
    }

    QPointF scenePos = mapToScene(event->pos());
    if (m_isDrawing) {
        if (m_currentTool == ToolType::Pen && m_currentItem) { m_currentPath.lineTo(scenePos); m_currentItem->setPath(m_currentPath); }
        else if (m_currentTool == ToolType::Eraser) { applyEraser(scenePos); }
        else if (m_currentTool == ToolType::Lasso && m_lassoItem) { m_currentPath.lineTo(scenePos); m_lassoItem->setPath(m_currentPath); }
        event->accept();
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event) {
    bool isTouch = (event->source() == Qt::MouseEventSynthesizedBySystem);
    if (m_penOnlyMode && isTouch) {
        m_isPanning = false; if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); }
        setTool(m_currentTool); event->accept(); return;
    }
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false; if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); }
        setTool(m_currentTool); event->accept(); return;
    }
    if (event->button() == Qt::LeftButton) {
        if (m_currentTool == ToolType::Lasso && m_isDrawing) finishLasso();
        if (!m_scene->selectedItems().isEmpty()) updateSelectionMenuPosition();
        if (m_isDrawing) { emit contentModified(); }
        m_isDrawing = false; m_currentItem = nullptr;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

bool CanvasView::event(QEvent *event) {
    if (event->type() == QEvent::Gesture) { gestureEvent(static_cast<QGestureEvent*>(event)); return true; }
    return QGraphicsView::event(event);
}
void CanvasView::gestureEvent(QGestureEvent *event) { if (QGesture *pinch = event->gesture(Qt::PinchGesture)) { pinchTriggered(static_cast<QPinchGesture *>(pinch)); } }
void CanvasView::pinchTriggered(QPinchGesture *gesture) {
    if (m_isDrawing) {
        m_isDrawing = false;
        if (m_currentItem) { m_scene->removeItem(m_currentItem); delete m_currentItem; m_currentItem = nullptr; }
        if (m_lassoItem) { m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; }
    }
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        qreal factor = gesture->scaleFactor();
        double currentScale = transform().m11();
        if (!((currentScale < 0.1 && factor < 1) || (currentScale > 10 && factor > 1))) { scale(factor, factor); }
    }
    if (gesture->state() == Qt::GestureUpdated) {
        QPoint delta = gesture->centerPoint().toPoint() - gesture->lastCenterPoint().toPoint();
        if (!delta.isNull()) {
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        }
    }
}

void CanvasView::finishLasso() {
    if (!m_lassoItem) return;
    m_currentPath.closeSubpath(); m_lassoItem->setPath(m_currentPath);
    m_scene->setSelectionArea(m_currentPath, QTransform());
    m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr;
    MainWindow* mw = qobject_cast<MainWindow*>(window()); if (mw) mw->switchToSelectTool();
    if (!m_scene->selectedItems().isEmpty()) updateSelectionMenuPosition();
}

void CanvasView::updateSelectionMenuPosition() {
    if (m_scene->selectedItems().isEmpty()) { m_selectionMenu->hide(); return; }
    QRectF totalRect; const QList<QGraphicsItem*> items = m_scene->selectedItems(); for (auto* item : items) totalRect = totalRect.united(item->sceneBoundingRect());
    QPoint viewPos = mapFromScene(totalRect.topLeft());
    int x = viewPos.x() + (mapFromScene(totalRect.bottomRight()).x() - viewPos.x()) / 2 - m_selectionMenu->width() / 2; int y = viewPos.y() - m_selectionMenu->height() - 10;
    x = qMax(0, qMin(x, width() - m_selectionMenu->width())); y = qMax(0, y);
    m_selectionMenu->move(x, y); m_selectionMenu->show(); m_selectionMenu->raise();
}
void CanvasView::applyEraser(const QPointF &pos) {
    QRectF eraserRect(pos.x() - 10, pos.y() - 10, 20, 20); const QList<QGraphicsItem*> items = m_scene->items(eraserRect);
    bool erased = false;
    for (auto* item : std::as_const(items)) {
        if (item->type() == QGraphicsPathItem::Type) { m_scene->removeItem(item); m_undoList.removeOne(item); delete item; erased = true; }
    }
    if (erased) emit contentModified();
}
