#include "canvasview.h"
#include "mainwindow.h"
#include "UIStyles.h"
#include "tools/ToolManager.h"  // <--- NEU
#include "tools/AbstractTool.h" // <--- NEU
#include <QScrollBar>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGestureEvent>
#include <utility>
#include <cmath>
#include <QFile>
#include <QDataStream>
#include <QInputDevice>
#include <QtMath>

// ... (SelectionMenu Code bleibt gleich) ...
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
    , m_pageStyle(PageStyle::Squared)
    , m_gridSize(40)
    , m_currentTool(ToolType::Pen)
    , m_penColor(Qt::black)
    , m_penWidth(3)
    , m_isPanning(false)
    , m_pullDistance(0.0f)
{
    m_scene = new QGraphicsScene(this); setScene(m_scene);
    m_a4Rect = QRectF(0, 0, 794 * 1.5f, 1123 * 1.5f); // PAGE_WIDTH/HEIGHT

    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform, false);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    grabGesture(Qt::PinchGesture);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
    setPageFormat(true);
    setMouseTracking(true);
    setDragMode(QGraphicsView::NoDrag);
    updateBackgroundTile();

    connect(this, &CanvasView::contentModified, this, &CanvasView::updateSceneRect);

    // NEU: Update bei Tool-Wechsel (z.B. für Overlay)
    connect(&ToolManager::instance(), &ToolManager::toolChanged, this, [this](AbstractTool*){
        viewport()->update();
    });

    m_selectionMenu = new SelectionMenu(this);
    connect(m_selectionMenu, &SelectionMenu::deleteRequested, this, &CanvasView::deleteSelection);
    connect(m_selectionMenu, &SelectionMenu::duplicateRequested, this, &CanvasView::duplicateSelection);
}

// ... (setPageFormat, updateSceneRect, setPageColor, setPageStyle, setGridSize, updateBackgroundTile, drawBackground bleiben gleich) ...
// Hier zur Abkürzung nur die geänderten Teile:

void CanvasView::setPageFormat(bool isInfinite) {
    m_isInfinite = isInfinite;
    if (m_isInfinite) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        updateSceneRect();
    }
    else {
        m_scene->setSceneRect(m_a4Rect);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
    m_scene->update();
    viewport()->update();
}

void CanvasView::updateSceneRect() {
    if (!m_isInfinite) return;
    QRectF contentRect = m_scene->itemsBoundingRect();
    qreal margin = 2000.0;
    if (contentRect.isNull()) contentRect = QRectF(0, 0, width(), height());
    contentRect.adjust(-margin, -margin, margin, margin);
    QRectF viewRect = mapToScene(viewport()->rect()).boundingRect();
    contentRect = contentRect.united(viewRect);
    m_scene->setSceneRect(contentRect);
}

void CanvasView::setPageColor(const QColor &color) {
    bool isNowDark = (color.value() < 100);
    QList<QGraphicsItem*> items = m_scene->items();
    for (auto *item : std::as_const(items)) {
        if (item->type() == QGraphicsPathItem::Type) {
            QGraphicsPathItem *pathItem = static_cast<QGraphicsPathItem*>(item);
            if (pathItem->zValue() == 0.1) continue;
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
    updateBackgroundTile();
    viewport()->update();
    emit contentModified();
}

void CanvasView::setPageStyle(PageStyle style) {
    if (m_pageStyle != style) {
        m_pageStyle = style;
        updateBackgroundTile();
        viewport()->update();
    }
}

void CanvasView::setGridSize(int size) {
    if (m_gridSize != size && size > 5) {
        m_gridSize = size;
        updateBackgroundTile();
        viewport()->update();
    }
}

void CanvasView::updateBackgroundTile() {
    int baseSize = m_gridSize;
    if (baseSize < 1) baseSize = 40;
    int multiplier = 512 / baseSize;
    if (multiplier < 1) multiplier = 1;
    int texSize = baseSize * multiplier;
    m_bgTile = QPixmap(texSize, texSize);
    m_bgTile.fill(Qt::transparent);
    if (m_pageStyle == PageStyle::Blank) return;
    QPainter p(&m_bgTile);
    p.setRenderHint(QPainter::Antialiasing, false);
    QColor lineColor = (m_pageColor.value() < 128) ? QColor(255,255,255, 30) : QColor(0,0,0, 20);
    p.setPen(QPen(lineColor, 1));
    if (m_pageStyle == PageStyle::Squared) {
        for (int i = 1; i <= multiplier; ++i) {
            int pos = i * baseSize - 1;
            p.drawLine(0, pos, texSize, pos);
            p.drawLine(pos, 0, pos, texSize);
        }
    } else if (m_pageStyle == PageStyle::Lined) {
        for (int i = 1; i <= multiplier; ++i) {
            int pos = i * baseSize - 1;
            p.drawLine(0, pos, texSize, pos);
        }
    } else if (m_pageStyle == PageStyle::Dotted) {
        p.setPen(Qt::NoPen);
        p.setBrush(lineColor);
        for (int x = 1; x <= multiplier; ++x) {
            for (int y = 1; y <= multiplier; ++y) {
                p.drawRect(x * baseSize - 2, y * baseSize - 2, 2, 2);
            }
        }
    }
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect) {
    if (!m_isInfinite) painter->fillRect(rect, UIStyles::SceneBackground);
    else painter->fillRect(rect, m_pageColor);

    if (m_isInfinite && !m_bgTile.isNull() && m_pageStyle != PageStyle::Blank) {
        double zoomLevel = transform().m11();
        if (m_gridSize * zoomLevel < 8.0) return;
        painter->save();
        qreal w = m_bgTile.width(); qreal h = m_bgTile.height();
        qreal ox = std::fmod(rect.left(), w); if (ox < 0) ox += w;
        qreal oy = std::fmod(rect.top(), h); if (oy < 0) oy += h;
        painter->drawTiledPixmap(rect, m_bgTile, QPointF(ox, oy));
        painter->restore();
    } else if (!m_isInfinite) {
        // (Page Mode code...)
        painter->save();
        float PAGE_HEIGHT = 1123 * 1.5f; float PAGE_GAP = 60.0f; float TOTAL_PAGE_HEIGHT = PAGE_HEIGHT + PAGE_GAP;
        int startPage = std::max(0, static_cast<int>(rect.top() / TOTAL_PAGE_HEIGHT));
        int endPage = static_cast<int>(rect.bottom() / TOTAL_PAGE_HEIGHT);
        int maxPage = static_cast<int>(m_a4Rect.height() / TOTAL_PAGE_HEIGHT);
        if (endPage > maxPage) endPage = maxPage;
        for (int i = startPage; i <= endPage; ++i) {
            qreal y = i * TOTAL_PAGE_HEIGHT;
            QRectF pageRect(0, y, 794 * 1.5f, PAGE_HEIGHT);
            painter->fillRect(pageRect, m_pageColor);
            painter->setBrushOrigin(pageRect.topLeft());
            painter->drawTiledPixmap(pageRect, m_bgTile);
        }
        painter->restore();
    }
}

void CanvasView::drawForeground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawForeground(painter, rect);
    if (m_pullDistance > 1.0f) drawPullIndicator(painter);

    // --- NEU: OVERLAY FÜR TOOLS ---
    AbstractTool* tool = ToolManager::instance().activeTool();
    if (tool) {
        painter->save();
        tool->drawOverlay(painter, rect);
        painter->restore();
    }

    if (!m_isInfinite) {
        // (Page Borders Code...)
        painter->save();
        painter->setPen(QPen(QColor(0,0,0, 60), 1, Qt::SolidLine)); painter->setBrush(Qt::NoBrush);
        float PAGE_HEIGHT = 1123 * 1.5f; float PAGE_GAP = 60.0f; float TOTAL_PAGE_HEIGHT = PAGE_HEIGHT + PAGE_GAP;
        int startPage = std::max(0, static_cast<int>(rect.top() / TOTAL_PAGE_HEIGHT));
        int endPage = static_cast<int>(rect.bottom() / TOTAL_PAGE_HEIGHT);
        int maxPage = static_cast<int>(m_a4Rect.height() / TOTAL_PAGE_HEIGHT);
        if (endPage > maxPage) endPage = maxPage;
        for (int i = startPage; i <= endPage; ++i) {
            QRectF pageRect(0, i * TOTAL_PAGE_HEIGHT, 794 * 1.5f, PAGE_HEIGHT);
            painter->drawRect(pageRect);
        }
        painter->restore();
    }
}

// ... (drawPullIndicator, addNewPage, saveToFile, loadFromFile bleiben gleich) ...
// Hier wieder die Abkürzung zu den relevanten Teilen:

void CanvasView::drawPullIndicator(QPainter* painter) {
    // (Original Code beibehalten)
    painter->save();
    painter->resetTransform();
    painter->setClipping(false);
    int w = viewport()->width(); int h = viewport()->height();
    float maxPull = 250.0f; float progress = qMin(m_pullDistance / maxPull, 1.0f);
    int size = 60; int yPos = h - size - 150 - (progress * 20); int xPos = (w - size) / 2;
    QRect circleRect(xPos, yPos, size, size);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(QColor(40, 40, 40, 200)); painter->setPen(Qt::NoPen); painter->drawEllipse(circleRect);
    if (progress > 0.05f) {
        QPen arcPen(QColor(0x5E5CE6)); arcPen.setWidth(4); arcPen.setCapStyle(Qt::RoundCap);
        painter->setPen(arcPen); painter->setBrush(Qt::NoBrush);
        painter->drawArc(circleRect.adjusted(8, 8, -8, -8), 90 * 16, -progress * 360 * 16);
    }
    if (progress > 0.15f) {
        painter->setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap));
        int center = size / 2; float growFactor = (progress - 0.15f) / 0.85f; int pSize = 12 * growFactor;
        QPoint c = circleRect.center();
        painter->drawLine(c.x(), c.y() - pSize, c.x(), c.y() + pSize);
        painter->drawLine(c.x() - pSize, c.y(), c.x() + pSize, c.y());
    }
    painter->restore();
}

void CanvasView::addNewPage() {
    m_a4Rect.setHeight(m_a4Rect.height() + (1123 * 1.5f) + 60.0f);
    setSceneRect(m_a4Rect);
    viewport()->update();
}

bool CanvasView::saveToFile() {
    if (m_filePath.isEmpty()) return false;
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&file);
    out << (quint32)0xB10B0002;
    out << m_isInfinite;
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
    quint32 magic; in >> magic;
    if (magic == 0xB10B0001) m_isInfinite = true;
    else if (magic == 0xB10B0002) in >> m_isInfinite;
    else return false;
    setPageFormat(m_isInfinite);
    m_scene->clear(); m_undoList.clear(); m_redoList.clear();
    int count; in >> count;
    bool wasBlocked = m_scene->blockSignals(true);
    for (int i = 0; i < count; ++i) {
        QPointF pos; QColor color; int width; QPainterPath path;
        in >> pos >> color >> width >> path;
        QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QGraphicsPathItem* item = m_scene->addPath(path, pen);
        item->setPos(pos);
        if (color.alpha() < 255) item->setZValue(0.1); else item->setZValue(1.0);
        item->setFlag(QGraphicsItem::ItemIsSelectable); item->setFlag(QGraphicsItem::ItemIsMovable);
    }
    m_scene->blockSignals(wasBlocked);
    if (m_isInfinite) updateSceneRect();
    return true;
}

void CanvasView::setTool(ToolType tool) {
    m_currentTool = tool;
    if (tool == ToolType::Select) {
        setCursor(Qt::ArrowCursor); setDragMode(QGraphicsView::RubberBandDrag);
    } else if (tool == ToolType::Text) {
        setCursor(Qt::IBeamCursor); setDragMode(QGraphicsView::NoDrag);
    } else {
        clearSelection(); setDragMode(QGraphicsView::NoDrag);
        if (tool == ToolType::Pen) setCursor(Qt::CrossCursor);
        else if (tool == ToolType::Highlighter) setCursor(Qt::CrossCursor);
        else if (tool == ToolType::Eraser) setCursor(Qt::ForbiddenCursor);
        else if (tool == ToolType::Lasso) setCursor(Qt::CrossCursor);
    }
}

void CanvasView::setPenColor(const QColor &color) { m_penColor = color; }
void CanvasView::setPenWidth(int width) { m_penWidth = width; }

void CanvasView::undo() {
    if (m_undoList.isEmpty()) return;
    QGraphicsItem* lastItem = m_undoList.takeLast();
    if (m_scene->items().contains(lastItem)) { m_scene->removeItem(lastItem); m_redoList.append(lastItem); emit contentModified(); }
}
void CanvasView::redo() {
    if (m_redoList.isEmpty()) return;
    QGraphicsItem* item = m_redoList.takeLast(); m_scene->addItem(item); m_undoList.append(item); emit contentModified();
}
bool CanvasView::canUndo() const { return !m_undoList.isEmpty(); }
bool CanvasView::canRedo() const { return !m_redoList.isEmpty(); }

void CanvasView::deleteSelection() {
    const QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.isEmpty()) return;
    for (auto* item : std::as_const(selected)) m_scene->removeItem(item);
    m_selectionMenu->hide();
    emit contentModified();
}

void CanvasView::duplicateSelection() {
    const QList<QGraphicsItem*> selected = m_scene->selectedItems(); if (selected.isEmpty()) return;
    clearSelection();
    for (auto* item : std::as_const(selected)) {
        if (item->type() == QGraphicsPathItem::Type) {
            QGraphicsPathItem* pathItem = static_cast<QGraphicsPathItem*>(item);
            QGraphicsPathItem* newItem = m_scene->addPath(pathItem->path(), pathItem->pen());
            newItem->setPos(item->pos() + QPointF(20, 20));
            newItem->setFlag(QGraphicsItem::ItemIsSelectable); newItem->setFlag(QGraphicsItem::ItemIsMovable);
            newItem->setZValue(pathItem->zValue());
            newItem->setSelected(true); m_undoList.append(newItem);
        }
    }
    updateSelectionMenuPosition(); emit contentModified();
}

void CanvasView::copySelection() {}
void CanvasView::clearSelection() { m_scene->clearSelection(); if (m_lassoItem) { m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; } m_selectionMenu->hide(); }

void CanvasView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double angle = event->angleDelta().y();
        double factor = std::pow(1.0015, angle);
        double currentScale = transform().m11();
        if ((currentScale < 0.1 && factor < 1) || (currentScale > 10 && factor > 1)) return;
        scale(factor, factor);
        viewport()->update();
        event->accept();
    } else {
        if (!m_isInfinite) {
            QScrollBar *vb = verticalScrollBar();
            if (vb->value() >= vb->maximum() && event->angleDelta().y() < 0) {
                m_pullDistance += std::abs(event->angleDelta().y()) * 0.5;
                viewport()->update();
                if (m_pullDistance > 250.0f) { addNewPage(); m_pullDistance = 0; }
                event->accept(); return;
            } else { if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); } }
        }
        QGraphicsView::wheelEvent(event);
    }
}

// ==========================================================
// MOUSE EVENTS: HIER IST DER FIX
// ==========================================================
void CanvasView::mousePressEvent(QMouseEvent *event) {
    // 1. Touch/Pan Logic
    const QInputDevice* dev = event->device();
    bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
    if (!isTouch && event->source() == Qt::MouseEventSynthesizedBySystem) isTouch = true;

    if (m_penOnlyMode && isTouch) {
        m_isPanning = true; m_lastPanPos = event->pos(); setCursor(Qt::ClosedHandCursor); event->accept(); return;
    }
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true; m_lastPanPos = event->pos(); setCursor(Qt::ClosedHandCursor); event->accept(); return;
    }

    // 2. --- NEUE TOOL ARCHITEKTUR ---
    // Hier fragen wir den ToolManager. Wenn der ein Tool hat, benutzen wir es.
    AbstractTool* newTool = ToolManager::instance().activeTool();
    if (newTool && event->button() == Qt::LeftButton) {
        QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMousePress);
        QPointF scenePos = mapToScene(event->pos());
        scEvent.setScenePos(scenePos);
        scEvent.setButton(event->button());
        scEvent.setButtons(event->buttons());
        scEvent.setModifiers(event->modifiers());

        // WICHTIG: Wir übergeben 'm_scene' an das Tool!
        if(newTool->handleMousePress(&scEvent, m_scene)) {
            m_isDrawing = true;
            event->accept();
            return; // Wir verlassen die Funktion hier, damit der alte Code nicht ausgeführt wird!
        }
    }

    // 3. --- ALTE LOGIK (FALLBACK) ---
    // Dieser Teil wird nur ausgeführt, wenn KEIN neues Tool aktiv ist.
    // Da wir aber oben registerAllTools() gemacht haben, sollte dieser Teil fast nie mehr erreicht werden,
    // außer für spezielle Fälle wie SelectionMenu Klicks.

    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        if (m_selectionMenu->isVisible() && !itemAt(event->pos())) m_selectionMenu->hide();

        // (Hier folgt dein alter Code für Seiten-Checks und alte Tools...)
        bool onPage = false;
        if (!m_isInfinite) {
            float PAGE_HEIGHT = 1123 * 1.5f; float PAGE_GAP = 60.0f;
            qreal currentY = 0;
            while(currentY < m_a4Rect.height()) {
                QRectF pageRect(0, currentY, 794 * 1.5f, PAGE_HEIGHT);
                if (pageRect.contains(scenePos)) { onPage = true; break; }
                currentY += PAGE_HEIGHT + PAGE_GAP;
            }
            if (!onPage) return;
        }

        if (m_currentTool == ToolType::Select) { QGraphicsView::mousePressEvent(event); return; }
        else if (m_currentTool == ToolType::Pen) {
            // Alte Pen Implementierung...
            m_isDrawing = true; m_currentPath = QPainterPath(); m_currentPath.moveTo(scenePos);
            m_currentItem = m_scene->addPath(m_currentPath, QPen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            m_currentItem->setFlag(QGraphicsItem::ItemIsSelectable); m_currentItem->setFlag(QGraphicsItem::ItemIsMovable);
            m_currentItem->setZValue(1.0);
            qDeleteAll(m_redoList); m_redoList.clear(); m_undoList.append(m_currentItem);
            event->accept(); return;
        }
        // ... (Rest der alten Tools)
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        if (!m_isInfinite) { /* Scroll Logic */ }
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept(); return;
    }

    // --- NEU: TOOL ---
    AbstractTool* newTool = ToolManager::instance().activeTool();
    if (newTool && m_isDrawing) {
        QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseMove);
        QPointF scenePos = mapToScene(event->pos());
        scEvent.setScenePos(scenePos);
        scEvent.setButtons(event->buttons());
        scEvent.setModifiers(event->modifiers());

        if (newTool->handleMouseMove(&scEvent, m_scene)) {
            event->accept();
            return;
        }
    }

    // --- ALT ---
    QPointF scenePos = mapToScene(event->pos());
    if (m_isDrawing) {
        if ((m_currentTool == ToolType::Pen || m_currentTool == ToolType::Highlighter) && m_currentItem) {
            m_currentPath.lineTo(scenePos); m_currentItem->setPath(m_currentPath);
        }
        else if (m_currentTool == ToolType::Eraser) { applyEraser(scenePos); }
        else if (m_currentTool == ToolType::Lasso && m_lassoItem) { m_currentPath.lineTo(scenePos); m_lassoItem->setPath(m_currentPath); }
        event->accept();
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event) {
    const QInputDevice* dev = event->device();
    bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
    if (!isTouch && event->source() == Qt::MouseEventSynthesizedBySystem) isTouch = true;

    if (m_penOnlyMode && isTouch) { m_isPanning = false; if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); } setTool(m_currentTool); event->accept(); return; }
    if (event->button() == Qt::MiddleButton) { m_isPanning = false; if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); } setTool(m_currentTool); event->accept(); return; }

    // --- NEU: TOOL ---
    AbstractTool* newTool = ToolManager::instance().activeTool();
    if (newTool && event->button() == Qt::LeftButton) {
        QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseRelease);
        QPointF scenePos = mapToScene(event->pos());
        scEvent.setScenePos(scenePos);
        scEvent.setButton(event->button());

        if (newTool->handleMouseRelease(&scEvent, m_scene)) {
            m_isDrawing = false;
            emit contentModified();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if (m_currentTool == ToolType::Lasso && m_isDrawing) finishLasso();
        if (!m_scene->selectedItems().isEmpty()) updateSelectionMenuPosition();
        if (m_isDrawing) emit contentModified();
        m_isDrawing = false; m_currentItem = nullptr;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

// ... (Rest der Datei: event, gestureEvent, pinchTriggered, finishLasso, updateSelectionMenuPosition, applyEraser - bleibt gleich) ...
bool CanvasView::event(QEvent *event) { if (event->type() == QEvent::Gesture) { gestureEvent(static_cast<QGestureEvent*>(event)); return true; } return QGraphicsView::event(event); }
void CanvasView::gestureEvent(QGestureEvent *event) { if (QGesture *pinch = event->gesture(Qt::PinchGesture)) { pinchTriggered(static_cast<QPinchGesture *>(pinch)); } }
void CanvasView::pinchTriggered(QPinchGesture *gesture) {
    if (m_isDrawing) { m_isDrawing = false; if (m_currentItem) { m_scene->removeItem(m_currentItem); delete m_currentItem; m_currentItem = nullptr; } if (m_lassoItem) { m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; } }
    if (m_isPanning) m_isPanning = false;
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) { qreal factor = gesture->scaleFactor(); double currentScale = transform().m11(); if (!((currentScale < 0.1 && factor < 1) || (currentScale > 10 && factor > 1))) scale(factor, factor); }
    if (gesture->state() == Qt::GestureUpdated) { QPoint delta = gesture->centerPoint().toPoint() - gesture->lastCenterPoint().toPoint(); if (!delta.isNull()) { horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x()); verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y()); } }
}
void CanvasView::finishLasso() { if (!m_lassoItem) return; m_currentPath.closeSubpath(); m_lassoItem->setPath(m_currentPath); m_scene->setSelectionArea(m_currentPath, QTransform()); m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; MainWindow* mw = qobject_cast<MainWindow*>(window()); if (mw) mw->switchToSelectTool(); if (!m_scene->selectedItems().isEmpty()) updateSelectionMenuPosition(); }
void CanvasView::updateSelectionMenuPosition() { if (m_scene->selectedItems().isEmpty()) { m_selectionMenu->hide(); return; } QRectF totalRect; const QList<QGraphicsItem*> items = m_scene->selectedItems(); for (auto* item : items) totalRect = totalRect.united(item->sceneBoundingRect()); QPoint viewPos = mapFromScene(totalRect.topLeft()); int x = viewPos.x() + (mapFromScene(totalRect.bottomRight()).x() - viewPos.x()) / 2 - m_selectionMenu->width() / 2; int y = viewPos.y() - m_selectionMenu->height() - 10; x = qMax(0, qMin(x, width() - m_selectionMenu->width())); y = qMax(0, y); m_selectionMenu->move(x, y); m_selectionMenu->show(); m_selectionMenu->raise(); }
void CanvasView::applyEraser(const QPointF &pos) { QRectF eraserRect(pos.x() - 10, pos.y() - 10, 20, 20); const QList<QGraphicsItem*> items = m_scene->items(eraserRect); bool erased = false; for (auto* item : std::as_const(items)) { if (item->type() == QGraphicsPathItem::Type) { m_scene->removeItem(item); m_undoList.removeOne(item); delete item; erased = true; } else if (item->type() == QGraphicsTextItem::Type) { m_scene->removeItem(item); m_undoList.removeOne(item); delete item; erased = true; } } if (erased) emit contentModified(); }
