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
#include <QInputDevice>
#include <QPixmap>
#include <QBrush>
#include <QStyleOptionGraphicsItem>
#include <QBuffer>

// --- KONSTANTEN ---
static const float PAGE_W = 794.0f * 1.5f;
static const float PAGE_H = 1123.0f * 1.5f;
static const float PAGE_GAP = 50.0f;

// =========================================================
//  CachedPageItem
// =========================================================

CachedPageItem::CachedPageItem(const QRectF& rect, int pageIndex, QGraphicsItem* parent)
    : QGraphicsRectItem(rect, parent), m_pageIndex(pageIndex)
{
    // Bildspeicher für Tinte und Gitter initialisieren
    m_contentLayer = QPixmap(rect.size().toSize());
    m_contentLayer.fill(Qt::transparent);

    m_gridLayer = QPixmap(rect.size().toSize());

    setAcceptHoverEvents(false);
}

void CachedPageItem::setPageStyle(PageStyle style, const QColor& color) {
    m_style = style;
    m_color = color;
    updateGrid();
    update();
}

void CachedPageItem::updateGrid() {
    m_gridLayer.fill(m_color);

    QPainter p(&m_gridLayer);

    bool isDark = (m_color.value() < 128);
    QColor patternColor = isDark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 30);
    int spacing = 40;

    if (m_style == PageStyle::Squared) {
        p.setPen(patternColor);
        for(int x=0; x<m_gridLayer.width(); x+=spacing) p.drawLine(x, 0, x, m_gridLayer.height());
        for(int y=0; y<m_gridLayer.height(); y+=spacing) p.drawLine(0, y, m_gridLayer.width(), y);
    }
    else if (m_style == PageStyle::Lined) {
        p.setPen(patternColor);
        for(int y=60; y<m_gridLayer.height(); y+=spacing) p.drawLine(0, y, m_gridLayer.width(), y);
        QPen margin(isDark ? QColor(255,100,100,50) : QColor(255,0,0,30));
        p.setPen(margin);
        p.drawLine(80, 0, 80, m_gridLayer.height());
    }
    else if (m_style == PageStyle::Dotted) {
        p.setPen(Qt::NoPen); p.setBrush(patternColor);
        for(int x=spacing/2; x<m_gridLayer.width(); x+=spacing)
            for(int y=spacing/2; y<m_gridLayer.height(); y+=spacing)
                p.drawEllipse(QPoint(x,y), 2, 2);
    }
}

void CachedPageItem::bakeStroke(const QPainterPath& path, const QPen& pen) {
    QPainter p(&m_contentLayer);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QTransform trans;
    trans.translate(-pos().x(), -pos().y());

    p.setTransform(trans);
    p.setPen(pen);
    p.drawPath(path);

    update();
}

void CachedPageItem::eraseStroke(const QPointF& scenePos, double radius) {
    QPainter p(&m_contentLayer);
    p.setRenderHint(QPainter::Antialiasing);

    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);

    QPointF localPos = mapFromScene(scenePos);
    p.drawEllipse(localPos, radius, radius);

    update();
}

void CachedPageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(option); Q_UNUSED(widget);
    painter->drawPixmap(0, 0, m_gridLayer);
    painter->drawPixmap(0, 0, m_contentLayer);
    painter->setPen(QPen(QColor(0,0,0,30), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect());
}


// =========================================================
//  CanvasView
// =========================================================

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
    , m_currentStrokeItem(nullptr)
    , m_phantomItem(nullptr)
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
    m_scene = new QGraphicsScene(this); setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);

    // Performance: BoundingRect Update ist bei Bitmaps oft am stabilsten
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setOptimizationFlags(QGraphicsView::DontSavePainterState);

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
    viewport()->grabGesture(Qt::PinchGesture);

    setDragMode(QGraphicsView::NoDrag);

    m_pageStyles.append(PageStyle::Blank);
    addNewPage();

    m_phantomItem = new QGraphicsPathItem();
    m_phantomItem->setZValue(10000);
    m_phantomItem->setOpacity(0.5);
    m_scene->addItem(m_phantomItem);

    m_selectionMenu = new SelectionMenu(this);
}

void CanvasView::addNewPage() {
    int newIdx = m_pages.size();
    qreal yPos = 0;
    if (newIdx > 0) {
        yPos = m_pages.last()->y() + PAGE_H + PAGE_GAP;
    }

    QRectF pageRect(0, 0, PAGE_W, PAGE_H);
    CachedPageItem* page = new CachedPageItem(pageRect, newIdx);
    page->setPos(0, yPos);

    PageStyle s = PageStyle::Blank;
    if (newIdx < m_pageStyles.size()) s = m_pageStyles[newIdx];
    else if (!m_pageStyles.isEmpty()) s = m_pageStyles.last();

    if (newIdx >= m_pageStyles.size()) m_pageStyles.append(s);

    page->setPageStyle(s, m_pageColor);

    m_scene->addItem(page);
    m_pages.append(page);

    m_a4Rect = m_scene->itemsBoundingRect();
    m_a4Rect.setHeight(m_a4Rect.height() + 500);
    setSceneRect(m_a4Rect);
}

void CanvasView::setPageFormat(bool isInfinite) {
    m_isInfinite = isInfinite;
}

void CanvasView::setPageColor(const QColor &color) {
    m_pageColor = color;
    for(auto* p : m_pages) {
        p->setPageStyle(m_pageStyles[0], color);
    }
}

void CanvasView::setPageStyle(PageStyle style, bool applyToAll) {
    if (applyToAll) {
        for(int i=0; i<m_pageStyles.size(); ++i) m_pageStyles[i] = style;
        for(auto* p : m_pages) p->setPageStyle(style, m_pageColor);
    } else {
        int idx = getCurrentPageIndex();
        if (idx >= 0 && idx < m_pages.size()) {
            m_pageStyles[idx] = style;
            m_pages[idx]->setPageStyle(style, m_pageColor);
        }
    }
}

// --- FEHLER BEHOBEN: currentPageStyle implementiert ---
PageStyle CanvasView::currentPageStyle() const {
    int idx = getCurrentPageIndex();
    if (idx >= 0 && idx < m_pageStyles.size()) return m_pageStyles[idx];
    return PageStyle::Blank;
}

int CanvasView::getCurrentPageIndex() const {
    QPointF center = mapToScene(viewport()->rect().center());
    for(int i=0; i<m_pages.size(); ++i) {
        if (m_pages[i]->sceneBoundingRect().contains(center)) return i;
    }
    return 0;
}

// --- FEHLER BEHOBEN: wheelEvent implementiert ---
void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double angle = event->angleDelta().y();
        double factor = std::pow(1.0015, angle);
        double currentScale = transform().m11();
        if ((currentScale < 0.1 && factor < 1) || (currentScale > 10 && factor > 1)) return;
        scale(factor, factor);
        event->accept();
    } else {
        // Scrollen und Pull-to-Add-Page
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
    const QInputDevice* dev = event->device();
    bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
    if (!isTouch && event->source() == Qt::MouseEventSynthesizedBySystem) isTouch = true;

    if (m_penOnlyMode && isTouch) {
        m_isPanning = true; m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor); event->accept(); return;
    }
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true; m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor); event->accept(); return;
    }

    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        CachedPageItem* targetPage = getPageAt(scenePos);
        if (!targetPage && !m_isInfinite) return;

        if (m_currentTool == ToolType::Pen) {
            m_isDrawing = true;
            m_predictor.reset();
            m_predictor.addPoint(scenePos);

            m_currentStrokePath = QPainterPath();
            m_currentStrokePath.moveTo(scenePos);

            m_currentStrokeItem = m_scene->addPath(m_currentStrokePath, QPen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            m_currentStrokeItem->setZValue(5000);

            m_phantomItem->setPath(QPainterPath());
            event->accept();
            return;
        }
        else if (m_currentTool == ToolType::Eraser) {
            m_isDrawing = true;
            applyEraser(scenePos);
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());

        if (verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 10) {
            addNewPage();
        }
        return;
    }

    QPointF scenePos = mapToScene(event->pos());

    if (m_isDrawing) {
        if (m_currentTool == ToolType::Pen && m_currentStrokeItem) {
            m_currentStrokePath.lineTo(scenePos);
            m_currentStrokeItem->setPath(m_currentStrokePath);
            m_predictor.addPoint(scenePos);
            drawPhantomInk(scenePos);
        }
        else if (m_currentTool == ToolType::Eraser) {
            applyEraser(scenePos);
        }
    }
    event->accept();
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event) {
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);

    if (event->button() == Qt::LeftButton && m_isDrawing) {
        if (m_currentTool == ToolType::Pen && m_currentStrokeItem) {
            QRectF strokeRect = m_currentStrokePath.boundingRect();
            for(auto* page : m_pages) {
                if (page->sceneBoundingRect().intersects(strokeRect)) {
                    page->bakeStroke(m_currentStrokePath, m_currentStrokeItem->pen());
                }
            }
            m_scene->removeItem(m_currentStrokeItem);
            delete m_currentStrokeItem;
            m_currentStrokeItem = nullptr;

            m_phantomItem->setPath(QPainterPath());
            emit contentModified();
        }
        m_isDrawing = false;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::drawPhantomInk(const QPointF& realEndPos) {
    QPointF predictedPos = m_predictor.predict();
    qreal dist = QLineF(realEndPos, predictedPos).length();
    if (dist > 50.0 || dist < 1.0) { m_phantomItem->setPath(QPainterPath()); return; }

    QPainterPath p; p.moveTo(realEndPos);
    p.quadTo((realEndPos + predictedPos)/2.0, predictedPos);

    QPen pen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QColor c = m_penColor; c.setAlpha(120); pen.setColor(c);
    m_phantomItem->setPen(pen);
    m_phantomItem->setPath(p);
}

void CanvasView::applyEraser(const QPointF &pos) {
    CachedPageItem* page = getPageAt(pos);
    if (page) {
        page->eraseStroke(pos, 20.0);
    }
}

CachedPageItem* CanvasView::getPageAt(const QPointF& pos) {
    for(auto* p : m_pages) {
        if (p->contains(p->mapFromScene(pos))) return p;
    }
    return nullptr;
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect) {
    painter->fillRect(rect, UIStyles::SceneBackground);
}

void CanvasView::drawForeground(QPainter *painter, const QRectF &rect) {
    QGraphicsView::drawForeground(painter, rect);
    if (!m_isInfinite && m_pullDistance > 10.0f) {
        painter->save();
        drawPullIndicator(painter);
        painter->restore();
    }
}

// --- FEHLER BEHOBEN: drawPullIndicator implementiert ---
void CanvasView::drawPullIndicator(QPainter* painter) {
    painter->resetTransform();
    int w = viewport()->width(); int h = viewport()->height();
    float maxPull = 250.0f; float progress = qMin(m_pullDistance / maxPull, 1.0f);
    int size = 60; int yPos = h - size - 30; int xPos = (w - size) / 2;
    QRect circleRect(xPos, yPos, size, size);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(QColor(0, 0, 0, 150));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(circleRect);

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

bool CanvasView::saveToFile() {
    if (m_filePath.isEmpty()) return false;
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&file);
    out << (quint32)0xB10B0003;
    out << (int)m_pages.size();
    for(auto* p : m_pages) {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        p->getContentPixmap().save(&buffer, "PNG");
        out << ba;
    }
    return true;
}

bool CanvasView::loadFromFile() {
    if (m_filePath.isEmpty()) return false;
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QDataStream in(&file);
    quint32 magic; in >> magic;

    if (magic == 0xB10B0003) {
        m_pages.clear(); m_scene->clear();
        m_phantomItem = new QGraphicsPathItem();
        m_phantomItem->setZValue(10000);
        m_phantomItem->setOpacity(0.5);
        m_scene->addItem(m_phantomItem);

        int count; in >> count;
        for(int i=0; i<count; ++i) {
            QByteArray ba; in >> ba;
            QPixmap pix; pix.loadFromData(ba, "PNG");
            QRectF r(0, i * (PAGE_H+PAGE_GAP), PAGE_W, PAGE_H);
            CachedPageItem* item = new CachedPageItem(r, i);
            item->setContentPixmap(pix);
            item->setPageStyle(PageStyle::Blank, m_pageColor);
            m_scene->addItem(item);
            m_pages.append(item);
        }
        m_a4Rect = m_scene->itemsBoundingRect();
        setSceneRect(m_a4Rect);
        return true;
    }
    return false;
}

bool CanvasView::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::Gesture) return gestureEvent((QGestureEvent*)event);
    return QGraphicsView::viewportEvent(event);
}
bool CanvasView::gestureEvent(QGestureEvent* e) {
    if (QGesture *p = e->gesture(Qt::PinchGesture)) {
        pinchTriggered((QPinchGesture*)p);
    }
    return true;
}
void CanvasView::pinchTriggered(QPinchGesture *gesture) {
    if(gesture->changeFlags() & QPinchGesture::ScaleFactorChanged) {
        qreal factor = gesture->scaleFactor();
        scale(factor, factor);
    }
}

// --- DUMMY IMPLS ---
void CanvasView::setTool(ToolType t) { m_currentTool = t; }
void CanvasView::setPenColor(const QColor &c) { m_penColor = c; }
void CanvasView::setPenWidth(int w) { m_penWidth = w; }
void CanvasView::undo() {}
void CanvasView::redo() {}
bool CanvasView::canUndo() const { return false; }
bool CanvasView::canRedo() const { return false; }
void CanvasView::deleteSelection() {}
void CanvasView::duplicateSelection() {}
void CanvasView::copySelection() {}
void CanvasView::updateSelectionMenuPosition() {}
void CanvasView::finishLasso() {}
void CanvasView::clearSelection() {}
