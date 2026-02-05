#include "canvasview.h"
#include "mainwindow.h"
#include "UIStyles.h"
#include "tools/ToolManager.h"
#include "tools/AbstractTool.h"
#include "tools/RulerTool.h" // NEU: RulerTool Header
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
#include <QClipboard>
#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QPainter>
#include <QCursor>
#include <QBitmap>
#include <QStyleOptionGraphicsItem>
#include <QVector2D>

static constexpr float PAGE_WIDTH = 794 * 1.5f;
static constexpr float PAGE_HEIGHT = 1123 * 1.5f;
static constexpr float PAGE_GAP = 60.0f;
static constexpr float TOTAL_PAGE_HEIGHT = PAGE_HEIGHT + PAGE_GAP;

// =============================================================================
// HELPER: BAKING TRANSFORM
// =============================================================================
static void bakeTransform(QGraphicsItem* item) {
    if (!item) return;
    QTransform localToParent = item->sceneTransform();
    if (item->parentItem()) {
        localToParent = localToParent * item->parentItem()->sceneTransform().inverted();
    }
    item->setTransform(localToParent);
    item->setPos(0, 0);
    item->setRotation(0);
    item->setScale(1.0);
}

// =============================================================================
// HELPER: ROTATED CURSOR GENERATOR
// =============================================================================
static QCursor getRotatedCursor(Qt::CursorShape shape, qreal angle) {
    qreal baseAngle = 0;
    bool isDoubleArrow = false;

    if (shape == Qt::SizeVerCursor) { baseAngle = 90; isDoubleArrow = true; }
    else if (shape == Qt::SizeHorCursor) { baseAngle = 0; isDoubleArrow = true; }
    else if (shape == Qt::SizeBDiagCursor) { baseAngle = 45; isDoubleArrow = true; }
    else if (shape == Qt::SizeFDiagCursor) { baseAngle = 135; isDoubleArrow = true; }

    if (!isDoubleArrow) return QCursor(shape);

    qreal totalAngle = baseAngle + angle;

    int size = 32;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    p.translate(size/2, size/2);
    p.rotate(totalAngle);

    QPen whitePen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap);
    QPen blackPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap);

    auto drawArrow = [&](QPainter& p) {
        p.drawLine(-9, 0, 9, 0);
        p.drawLine(-9, 0, -5, -4); p.drawLine(-9, 0, -5, 4);
        p.drawLine(9, 0, 5, -4); p.drawLine(9, 0, 5, 4);
    };

    p.setPen(whitePen); drawArrow(p);
    p.setPen(blackPen); drawArrow(p);

    return QCursor(pix, size/2, size/2);
}

// =============================================================================
// SELECTION MENU
// =============================================================================
class SelectionMenu : public QWidget {
    Q_OBJECT
public:
    explicit SelectionMenu(QWidget* parent = nullptr) : QWidget(parent) {
        setStyleSheet(
            "QWidget { background-color: #252526; border-radius: 8px; border: 1px solid #444; }"
            "QPushButton { background: transparent; border: none; color: white; font-weight: bold; padding: 5px 8px; font-size: 14px; }"
            "QPushButton:hover { background-color: #3E3E42; border-radius: 4px; }"
            );
        setAttribute(Qt::WA_StyledBackground);
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(5, 5, 5, 5); layout->setSpacing(2);

        auto addBtn = [&](QString text, auto signal) {
            QPushButton *btn = new QPushButton(text, this);
            connect(btn, &QPushButton::clicked, this, signal);
            layout->addWidget(btn);
        };

        addBtn("🔄", &SelectionMenu::transformRequested);
        addBtn("✂️", &SelectionMenu::cutRequested);
        addBtn("📋", &SelectionMenu::copyRequested);
        addBtn("🎨", &SelectionMenu::colorRequested);
        addBtn("📐", &SelectionMenu::cropRequested);
        addBtn("📸", &SelectionMenu::screenshotRequested);

        QFrame* line = new QFrame; line->setFrameShape(QFrame::VLine); line->setStyleSheet("color: #555;"); layout->addWidget(line);
        QPushButton *btnDel = new QPushButton("🗑️", this); btnDel->setStyleSheet("color: #FF5555;");
        connect(btnDel, &QPushButton::clicked, this, &SelectionMenu::deleteRequested); layout->addWidget(btnDel);
        adjustSize(); hide();
    }
signals:
    void deleteRequested(); void duplicateRequested(); void copyRequested(); void cutRequested();
    void colorRequested(); void screenshotRequested(); void cropRequested();
    void transformRequested();
};

// =============================================================================
// CROP MENU
// =============================================================================
class CropMenu : public QWidget {
    Q_OBJECT
public:
    explicit CropMenu(QWidget* parent = nullptr) : QWidget(parent) {
        setStyleSheet(
            "QWidget { background-color: #1E1E1E; border-radius: 8px; border: 1px solid #5E5CE6; }"
            "QPushButton { background: #333; border: none; color: #DDD; border-radius: 4px; padding: 6px 12px; font-size: 12px; }"
            "QPushButton:checked { background-color: #5E5CE6; color: white; }"
            "QPushButton:hover { background-color: #444; }"
            "QPushButton#ApplyBtn { background-color: #2E7D32; color: white; }"
            "QPushButton#CancelBtn { background-color: #C62828; color: white; }"
            );
        setAttribute(Qt::WA_StyledBackground);
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8); layout->setSpacing(8);

        btnRect = new QPushButton("Rechteck", this); btnRect->setCheckable(true);
        btnFree = new QPushButton("Freihand", this); btnFree->setCheckable(true);

        QButtonGroup* grp = new QButtonGroup(this);
        grp->addButton(btnRect); grp->addButton(btnFree);

        connect(btnRect, &QPushButton::clicked, this, &CropMenu::rectRequested);
        connect(btnFree, &QPushButton::clicked, this, &CropMenu::freehandRequested);

        layout->addWidget(btnRect);
        layout->addWidget(btnFree);

        QFrame* line = new QFrame; line->setFrameShape(QFrame::VLine); line->setStyleSheet("color: #555;"); layout->addWidget(line);

        QPushButton* btnApply = new QPushButton("✔", this); btnApply->setObjectName("ApplyBtn");
        QPushButton* btnCancel = new QPushButton("✕", this); btnCancel->setObjectName("CancelBtn");

        connect(btnApply, &QPushButton::clicked, this, &CropMenu::applyRequested);
        connect(btnCancel, &QPushButton::clicked, this, &CropMenu::cancelRequested);

        layout->addWidget(btnApply);
        layout->addWidget(btnCancel);
        adjustSize(); hide();
    }
    void setMode(bool rect) { if(rect) btnRect->setChecked(true); else btnFree->setChecked(true); }

signals:
    void rectRequested(); void freehandRequested(); void applyRequested(); void cancelRequested();
private:
    QPushButton *btnRect, *btnFree;
};

// =============================================================================
// CROP RESIZER
// =============================================================================
class CropResizer : public QGraphicsObject {
public:
    enum Handle { None, TopLeft, Top, TopRight, Left, Right, BottomLeft, Bottom, BottomRight, Center };
    CropResizer(QRectF rect) : m_rect(rect) {
        setFlags(ItemIsSelectable | ItemIsMovable);
        setAcceptHoverEvents(true);
    }
    QRectF boundingRect() const override { return m_rect.adjusted(-20, -20, 20, 20); }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        painter->setPen(QPen(QColor(Qt::white), 2, Qt::DashLine));
        painter->setBrush(QColor(255, 255, 255, 30));
        painter->drawRect(m_rect);
        painter->setPen(QPen(QColor(Qt::black), 1)); painter->setBrush(Qt::white);

        qreal r = 6.0;
        QList<QPointF> handles = {m_rect.topLeft(), m_rect.topRight(), m_rect.bottomLeft(), m_rect.bottomRight(),
                                  QPointF(m_rect.center().x(), m_rect.top()), QPointF(m_rect.center().x(), m_rect.bottom()),
                                  QPointF(m_rect.left(), m_rect.center().y()), QPointF(m_rect.right(), m_rect.center().y())};
        for(auto p : handles) painter->drawEllipse(p, r, r);
    }
    QRectF currentRect() const { return mapRectToScene(m_rect); }
protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override {
        Handle h = handleAt(event->pos());
        if (h == TopLeft || h == BottomRight) setCursor(Qt::SizeFDiagCursor);
        else if (h == TopRight || h == BottomLeft) setCursor(Qt::SizeBDiagCursor);
        else if (h == Top || h == Bottom) setCursor(Qt::SizeVerCursor);
        else if (h == Left || h == Right) setCursor(Qt::SizeHorCursor);
        else setCursor(Qt::SizeAllCursor);
        QGraphicsObject::hoverMoveEvent(event);
    }
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        m_dragHandle = handleAt(event->pos());
        m_startPos = event->pos(); m_startRect = m_rect;
        if (m_dragHandle == None) m_dragHandle = Center;
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
        if (m_dragHandle == Center) { moveBy(event->pos().x() - m_startPos.x(), event->pos().y() - m_startPos.y()); return; }
        QPointF pos = event->pos();
        qreal l = m_startRect.left(), r = m_startRect.right(), t = m_startRect.top(), b = m_startRect.bottom();
        if (m_dragHandle==TopLeft || m_dragHandle==Left || m_dragHandle==BottomLeft) l = pos.x();
        if (m_dragHandle==TopRight || m_dragHandle==Right || m_dragHandle==BottomRight) r = pos.x();
        if (m_dragHandle==TopLeft || m_dragHandle==Top || m_dragHandle==TopRight) t = pos.y();
        if (m_dragHandle==BottomLeft || m_dragHandle==Bottom || m_dragHandle==BottomRight) b = pos.y();
        m_rect = QRectF(QPointF(l, t), QPointF(r, b)).normalized();
        prepareGeometryChange(); update();
    }
private:
    Handle handleAt(QPointF pos) {
        double r = 20.0;
        if (QVector2D(pos - m_rect.topLeft()).length() < r) return TopLeft;
        if (QVector2D(pos - m_rect.topRight()).length() < r) return TopRight;
        if (QVector2D(pos - m_rect.bottomLeft()).length() < r) return BottomLeft;
        if (QVector2D(pos - m_rect.bottomRight()).length() < r) return BottomRight;
        if (QVector2D(pos - QPointF(m_rect.center().x(), m_rect.top())).length() < r) return Top;
        if (QVector2D(pos - QPointF(m_rect.center().x(), m_rect.bottom())).length() < r) return Bottom;
        if (QVector2D(pos - QPointF(m_rect.left(), m_rect.center().y())).length() < r) return Left;
        if (QVector2D(pos - QPointF(m_rect.right(), m_rect.center().y())).length() < r) return Right;
        return None;
    }
    QRectF m_rect; QRectF m_startRect; QPointF m_startPos; Handle m_dragHandle;
};

// =============================================================================
// TRANSFORM OVERLAY (FINAL FIX: SCENE SPACE ROTATION)
// =============================================================================
class TransformOverlay : public QGraphicsObject {
public:
    enum Handle { None, TopLeft, Top, TopRight, Left, Right, BottomLeft, Bottom, BottomRight, Rotate, Center };

    TransformOverlay(QGraphicsItem* targetItem) : m_target(targetItem) {
        setFlags(ItemIsSelectable | ItemIsMovable);
        setAcceptHoverEvents(true);
        sync();
    }

    QGraphicsItem* target() const { return m_target; }

    QRectF boundingRect() const override {
        // Großzügiges Bounding Rect
        return m_target->boundingRect().adjusted(-5000, -5000, 5000, 5000);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        QTransform devTx = painter->deviceTransform();
        qreal sx = std::sqrt(devTx.m11()*devTx.m11() + devTx.m12()*devTx.m12());
        qreal sy = std::sqrt(devTx.m21()*devTx.m21() + devTx.m22()*devTx.m22());
        qreal angle = std::atan2(devTx.m12(), devTx.m11()) * 180.0 / M_PI;

        if (sx < 0.001) sx = 1.0; if (sy < 0.001) sy = 1.0;

        QRectF rect = m_target->boundingRect();

        // 1. Rahmen (Lokal)
        painter->save();
        QPen framePen(QColor(0x5E5CE6), 0, Qt::SolidLine);
        framePen.setCosmetic(true);
        framePen.setWidthF(2.0);
        painter->setPen(framePen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect);
        painter->restore();

        // 2. Griffe (Anti-Scale)
        qreal r = 7.0;

        auto drawHandle = [&](QPointF localPos, bool isBar) {
            painter->save();
            painter->translate(localPos);
            painter->rotate(-angle);
            painter->scale(1.0/sx, 1.0/sy);
            painter->setPen(QPen(QColor(Qt::black), 1));
            painter->setBrush(QColor(Qt::white));
            if (isBar) painter->drawRoundedRect(QRectF(-r, -r, 2*r, 2*r), r, r);
            else painter->drawEllipse(QPointF(0,0), r, r);
            painter->restore();
        };

        drawHandle(rect.topLeft(), false);
        drawHandle(rect.topRight(), false);
        drawHandle(rect.bottomLeft(), false);
        drawHandle(rect.bottomRight(), false);
        drawHandle(QPointF(rect.center().x(), rect.top()), true);
        drawHandle(QPointF(rect.center().x(), rect.bottom()), true);
        drawHandle(QPointF(rect.left(), rect.center().y()), true);
        drawHandle(QPointF(rect.right(), rect.center().y()), true);

        // 3. Rotation Handle (Lokal berechnet für Rotation)
        qreal localDist = 60.0 / sy;
        QPointF topCenterLocal(rect.center().x(), rect.top());
        QPointF rotHandleLocal(topCenterLocal.x(), topCenterLocal.y() - localDist);

        painter->save();
        QPen dashPen(QColor(0x5E5CE6), 1, Qt::DashLine);
        dashPen.setCosmetic(true);
        painter->setPen(dashPen);
        painter->drawLine(topCenterLocal, rotHandleLocal);
        painter->restore();

        painter->save();
        painter->translate(rotHandleLocal);
        painter->scale(1.0/sx, 1.0/sy);

        qreal rotR = 14.0;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x5E5CE6));
        painter->drawEllipse(QPointF(0,0), rotR, rotR);
        painter->setPen(QPen(QColor(Qt::white), 2.0));
        painter->setBrush(Qt::NoBrush);
        qreal iconR = rotR * 0.6;
        painter->drawArc(QRectF(-iconR, -iconR, iconR*2, iconR*2), 0, 270*16);
        painter->restore();
    }

    void sync() {
        prepareGeometryChange();
        setPos(m_target->pos());
        setRotation(m_target->rotation());
        setScale(m_target->scale());
        setTransform(m_target->transform());
        setTransformOriginPoint(m_target->transformOriginPoint());
        update();
    }

    Handle handleAt(QPointF scenePos) {
        if (!scene() || scene()->views().isEmpty()) return None;
        QGraphicsView* view = scene()->views().first();

        QTransform viewTx = view->viewportTransform();
        QTransform itemToView = sceneTransform() * viewTx;

        qreal sx = std::sqrt(itemToView.m11()*itemToView.m11() + itemToView.m12()*itemToView.m12());
        qreal sy = std::sqrt(itemToView.m21()*itemToView.m21() + itemToView.m22()*itemToView.m22());
        if (sy < 0.001) sy = 1.0;

        QPointF mouseP(view->mapFromScene(scenePos)); // Screen Space Mouse
        QRectF r = m_target->boundingRect();

        double scaleThreshold = 25.0;
        double rotateThreshold = 40.0;

        // 1. Rotation Handle (Exakte Übereinstimmung mit Paint)
        qreal localDist = 60.0 / sy;
        QPointF topCenterLocal(r.center().x(), r.top());
        QPointF rotHandleLocal(topCenterLocal.x(), topCenterLocal.y() - localDist);
        QPointF rotHandleView = itemToView.map(rotHandleLocal); // Nach Screen Space mappen

        if (QVector2D(mouseP - rotHandleView).length() < rotateThreshold) return Rotate;

        // 2. Scale Handles
        auto check = [&](QPointF localP) {
            QPointF handleView = itemToView.map(localP);
            return QVector2D(mouseP - handleView).length() < scaleThreshold;
        };

        if (check(r.topLeft())) return TopLeft;
        if (check(r.topRight())) return TopRight;
        if (check(r.bottomLeft())) return BottomLeft;
        if (check(r.bottomRight())) return BottomRight;
        if (check(QPointF(r.center().x(), r.top()))) return Top;
        if (check(QPointF(r.center().x(), r.bottom()))) return Bottom;
        if (check(QPointF(r.left(), r.center().y()))) return Left;
        if (check(QPointF(r.right(), r.center().y()))) return Right;

        // 3. Center
        QPointF localMouse = sceneTransform().inverted().map(scenePos);
        if (r.contains(localMouse)) return Center;

        return None;
    }

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override {
        Handle h = handleAt(event->scenePos());
        QTransform t = m_target->transform();
        qreal rot = std::atan2(t.m12(), t.m11()) * 180.0 / M_PI;

        if (h == Rotate) setCursor(Qt::PointingHandCursor);
        else if (h == Center) setCursor(Qt::SizeAllCursor);
        else if (h == None) setCursor(Qt::ArrowCursor);
        else {
            Qt::CursorShape shape = Qt::ArrowCursor;
            if (h == Top || h == Bottom) shape = Qt::SizeVerCursor;
            else if (h == Left || h == Right) shape = Qt::SizeHorCursor;
            else if (h == TopLeft || h == BottomRight) shape = Qt::SizeFDiagCursor;
            else if (h == TopRight || h == BottomLeft) shape = Qt::SizeBDiagCursor;
            setCursor(getRotatedCursor(shape, rot));
        }
        QGraphicsObject::hoverMoveEvent(event);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        m_dragHandle = handleAt(event->scenePos());
        if (m_dragHandle != None && m_dragHandle != Center) {
            bakeTransform(m_target);
            sync();
        }
        m_startScenePos = event->scenePos();
        m_initialSceneTransform = m_target->sceneTransform();
        m_initialTransform = m_target->transform();
        m_initialBoundingRect = m_target->boundingRect();
        if (m_dragHandle == None) m_dragHandle = Center;
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
        if (m_dragHandle == Center) {
            QPointF delta = event->scenePos() - m_startScenePos;
            m_target->moveBy(delta.x(), delta.y());
            m_startScenePos = event->scenePos();
            sync(); return;
        }

        if (m_dragHandle == Rotate) {
            QPointF sceneCenter = m_initialSceneTransform.map(m_initialBoundingRect.center());
            QLineF startLine(sceneCenter, m_startScenePos);
            QLineF currentLine(sceneCenter, event->scenePos());
            qreal angleDiff = startLine.angleTo(currentLine);

            // --- 90° SNAPPING (NEU) ---
            // 1. Initial Rotation aus der Matrix extrahieren
            qreal initialRot = std::atan2(m_initialSceneTransform.m12(), m_initialSceneTransform.m11()) * 180.0 / M_PI;

            // 2. Ziel-Rotation berechnen
            qreal targetRot = initialRot - angleDiff;

            // 3. Auf 90° runden
            qreal quotient = targetRot / 90.0;
            qreal nearest90 = std::round(quotient) * 90.0;

            // 4. Wenn nah genug (5° Toleranz), einrasten
            if (std::abs(targetRot - nearest90) < 5.0) {
                targetRot = nearest90;
                // angleDiff anpassen, damit wir exakt auf targetRot landen
                angleDiff = initialRot - targetRot;
            }
            // --- ENDE SNAPPING ---

            QTransform rotMatrix;
            rotMatrix.translate(sceneCenter.x(), sceneCenter.y());
            rotMatrix.rotate(-angleDiff);
            rotMatrix.translate(-sceneCenter.x(), -sceneCenter.y());

            QTransform newSceneTransform = m_initialSceneTransform * rotMatrix;
            if (m_target->parentItem()) {
                newSceneTransform = newSceneTransform * m_target->parentItem()->sceneTransform().inverted();
            }
            m_target->setTransform(newSceneTransform);

            sync(); return;
        }

        QPointF localPos = m_initialSceneTransform.inverted().map(event->scenePos());
        QRectF r = m_initialBoundingRect;
        QPointF pivot; qreal dsx = 1.0; qreal dsy = 1.0;

        if (m_dragHandle == Left || m_dragHandle == TopLeft || m_dragHandle == BottomLeft) {
            pivot.setX(r.right());
            qreal oldWidth = r.width(); qreal newWidth = r.right() - localPos.x();
            if (oldWidth > 0.1) dsx = newWidth / oldWidth;
        }
        else if (m_dragHandle == Right || m_dragHandle == TopRight || m_dragHandle == BottomRight) {
            pivot.setX(r.left());
            qreal oldWidth = r.width(); qreal newWidth = localPos.x() - r.left();
            if (oldWidth > 0.1) dsx = newWidth / oldWidth;
        }
        else { pivot.setX(r.center().x()); }

        if (m_dragHandle == Top || m_dragHandle == TopLeft || m_dragHandle == TopRight) {
            pivot.setY(r.bottom());
            qreal oldHeight = r.height(); qreal newHeight = r.bottom() - localPos.y();
            if (oldHeight > 0.1) dsy = newHeight / oldHeight;
        }
        else if (m_dragHandle == Bottom || m_dragHandle == BottomLeft || m_dragHandle == BottomRight) {
            pivot.setY(r.top());
            qreal oldHeight = r.height(); qreal newHeight = localPos.y() - r.top();
            if (oldHeight > 0.1) dsy = newHeight / oldHeight;
        }
        else { pivot.setY(r.center().y()); }

        if (dsx < 0.01) dsx = 0.01; if (dsy < 0.01) dsy = 0.01;

        QTransform scaleMatrix;
        scaleMatrix.translate(pivot.x(), pivot.y());
        scaleMatrix.scale(dsx, dsy);
        scaleMatrix.translate(-pivot.x(), -pivot.y());

        m_target->setTransform(scaleMatrix * m_initialTransform);
        sync();
    }

private:
    QGraphicsItem* m_target;
    QPointF m_startScenePos;
    Handle m_dragHandle;
    QTransform m_initialSceneTransform;
    QTransform m_initialTransform;
    QRectF m_initialBoundingRect;
};

// =============================================================================
// CANVAS VIEW IMPLEMENTATION
// =============================================================================

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
    , m_cropResizer(nullptr)
    , m_transformOverlay(nullptr)
    , m_transformGroup(nullptr)
{
    m_scene = new QGraphicsScene(this); setScene(m_scene);
    m_a4Rect = QRectF(0, 0, PAGE_WIDTH, PAGE_HEIGHT);

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

    // VERBESSERTE TOOL LOGIK
    connect(&ToolManager::instance(), &ToolManager::toolChanged, this, [this](AbstractTool* tool){
        if(tool && tool->mode() == ToolMode::Ruler) {
            RulerTool::ensureRulerExists(m_scene, ToolManager::instance().config());
        }
        viewport()->update();
    });

    // --- MENUS ---
    m_selectionMenu = new SelectionMenu(this);
    connect(m_selectionMenu, &SelectionMenu::deleteRequested, this, &CanvasView::deleteSelection);
    connect(m_selectionMenu, &SelectionMenu::duplicateRequested, this, &CanvasView::duplicateSelection);
    connect(m_selectionMenu, &SelectionMenu::copyRequested, this, &CanvasView::copySelection);
    connect(m_selectionMenu, &SelectionMenu::cutRequested, this, &CanvasView::cutSelection);
    connect(m_selectionMenu, &SelectionMenu::colorRequested, this, &CanvasView::changeSelectionColor);
    connect(m_selectionMenu, &SelectionMenu::screenshotRequested, this, &CanvasView::screenshotSelection);
    connect(m_selectionMenu, &SelectionMenu::cropRequested, this, &CanvasView::startCropSession);
    connect(m_selectionMenu, &SelectionMenu::transformRequested, this, &CanvasView::startTransformSession);

    m_cropMenu = new CropMenu(this);
    connect(m_cropMenu, &CropMenu::rectRequested, this, &CanvasView::setCropModeRect);
    connect(m_cropMenu, &CropMenu::freehandRequested, this, &CanvasView::setCropModeFreehand);
    connect(m_cropMenu, &CropMenu::applyRequested, this, &CanvasView::applyCrop);
    connect(m_cropMenu, &CropMenu::cancelRequested, this, &CanvasView::cancelCrop);
}

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
    if (contentRect.isNull()) { contentRect = QRectF(0, 0, width(), height()); }
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
    }
    else if (m_pageStyle == PageStyle::Lined) {
        for (int i = 1; i <= multiplier; ++i) {
            int pos = i * baseSize - 1;
            p.drawLine(0, pos, texSize, pos);
        }
    }
    else if (m_pageStyle == PageStyle::Dotted) {
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
    if (!m_isInfinite) { painter->fillRect(rect, UIStyles::SceneBackground); }
    else { painter->fillRect(rect, m_pageColor); }

    if (m_isInfinite && !m_bgTile.isNull() && m_pageStyle != PageStyle::Blank) {
        double zoomLevel = transform().m11();
        if (m_gridSize * zoomLevel < 8.0) return;
        painter->save();
        qreal w = m_bgTile.width(); qreal h = m_bgTile.height();
        qreal ox = std::fmod(rect.left(), w); if (ox < 0) ox += w;
        qreal oy = std::fmod(rect.top(), h); if (oy < 0) oy += h;
        painter->drawTiledPixmap(rect, m_bgTile, QPointF(ox, oy));
        painter->restore();
    }
    else if (!m_isInfinite) {
        painter->save();
        int startPage = std::max(0, static_cast<int>(rect.top() / TOTAL_PAGE_HEIGHT));
        int endPage = static_cast<int>(rect.bottom() / TOTAL_PAGE_HEIGHT);
        int maxPage = static_cast<int>(m_a4Rect.height() / TOTAL_PAGE_HEIGHT);
        if (endPage > maxPage) endPage = maxPage;
        for (int i = startPage; i <= endPage; ++i) {
            qreal y = i * TOTAL_PAGE_HEIGHT;
            QRectF pageRect(0, y, PAGE_WIDTH, PAGE_HEIGHT);
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
    AbstractTool* tool = ToolManager::instance().activeTool();
    if (tool) {
        painter->save();
        tool->drawOverlay(painter, rect);
        painter->restore();
    }
    if (!m_isInfinite) {
        painter->save();
        painter->setPen(QPen(QColor(0,0,0, 60), 1, Qt::SolidLine));
        painter->setBrush(Qt::NoBrush);
        int startPage = std::max(0, static_cast<int>(rect.top() / TOTAL_PAGE_HEIGHT));
        int endPage = static_cast<int>(rect.bottom() / TOTAL_PAGE_HEIGHT);
        int maxPage = static_cast<int>(m_a4Rect.height() / TOTAL_PAGE_HEIGHT);
        if (endPage > maxPage) endPage = maxPage;
        for (int i = startPage; i <= endPage; ++i) {
            QRectF pageRect(0, i * TOTAL_PAGE_HEIGHT, PAGE_WIDTH, PAGE_HEIGHT);
            painter->drawRect(pageRect);
        }
        painter->restore();
    }
}

void CanvasView::drawPullIndicator(QPainter* painter) {
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
    m_a4Rect.setHeight(m_a4Rect.height() + PAGE_HEIGHT + PAGE_GAP);
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
        if (item->type() == QGraphicsPathItem::Type && item != m_lassoItem && item != m_cropResizer && item != m_transformOverlay) {
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
    if (magic == 0xB10B0001) { m_isInfinite = true; } else if (magic == 0xB10B0002) { in >> m_isInfinite; } else { return false; }
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
        item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
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

    // LINEAL INTEGRATION (FALLS ToolType::Ruler EXISTIERT ODER VIA TOOLMANAGER GESTEUERT WIRD)
    // Wenn ToolManager bereits auf Ruler ist, sicherstellen, dass es existiert.
    if (ToolManager::instance().activeToolMode() == ToolMode::Ruler) {
        RulerTool::ensureRulerExists(m_scene, ToolManager::instance().config());
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
    QGraphicsItem* item = m_redoList.takeLast();
    m_scene->addItem(item);
    m_undoList.append(item);
    emit contentModified();
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
            newItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
            newItem->setZValue(pathItem->zValue());
            newItem->setSelected(true); m_undoList.append(newItem);
        }
    }
    updateSelectionMenuPosition(); emit contentModified();
}

void CanvasView::copySelection() {
    const QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.isEmpty()) return;
    QRectF selRect; for(auto* i : selected) selRect = selRect.united(i->sceneBoundingRect());
    if (!selRect.isEmpty()) {
        QImage img(selRect.size().toSize(), QImage::Format_ARGB32); img.fill(Qt::transparent);
        QPainter p(&img); m_scene->render(&p, QRectF(0,0,img.width(),img.height()), selRect);
        QGuiApplication::clipboard()->setImage(img);
    }
    m_selectionMenu->hide();
}

void CanvasView::cutSelection() { copySelection(); deleteSelection(); }

void CanvasView::changeSelectionColor() {
    QColor c = QColorDialog::getColor(Qt::black, this, "Farbe ändern");
    if (c.isValid()) {
        for (auto* item : m_scene->selectedItems()) {
            if (auto* pi = dynamic_cast<QGraphicsPathItem*>(item)) {
                QPen p = pi->pen(); p.setColor(c); pi->setPen(p);
            }
        }
        m_scene->update();
    }
    m_selectionMenu->hide();
}

void CanvasView::screenshotSelection() {
    const QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.isEmpty()) return;
    QRectF selRect; for(auto* i : selected) selRect = selRect.united(i->sceneBoundingRect());
    if (!selRect.isEmpty()) {
        QImage img(selRect.size().toSize(), QImage::Format_ARGB32); img.fill(Qt::transparent);
        QPainter p(&img); m_scene->render(&p, QRectF(0,0,img.width(),img.height()), selRect);
        QString path = QFileDialog::getSaveFileName(this, "Save", "sel.png", "*.png");
        if(!path.isEmpty()) img.save(path);
    }
    m_selectionMenu->hide();
}

// === CROP SYSTEM (MIT BILDERN) ===

void CanvasView::startCropSession() {
    m_selectionMenu->hide();
    m_interactionMode = InteractionMode::Crop; // Mode setzen
    updateCropMenuPosition();
    m_cropMenu->show();
    setCropModeRect();
}

void CanvasView::setCropModeRect() {
    m_cropSubMode = 1; // Rect
    m_cropMenu->setMode(true);

    QRectF totalRect;
    for(auto* i : m_scene->selectedItems()) totalRect = totalRect.united(i->sceneBoundingRect());

    if (m_cropResizer) { m_scene->removeItem(m_cropResizer); delete m_cropResizer; }
    m_cropResizer = new CropResizer(totalRect.isEmpty() ? QRectF(0,0,200,200) : totalRect);
    m_cropResizer->setZValue(99999);
    m_scene->addItem(m_cropResizer);

    m_lassoItem = nullptr;
}

void CanvasView::setCropModeFreehand() {
    m_cropSubMode = 2; // Freehand
    m_cropMenu->setMode(false);
    if (m_cropResizer) { m_scene->removeItem(m_cropResizer); delete m_cropResizer; m_cropResizer = nullptr; }
}

void CanvasView::applyCrop() {
    QPainterPath clipPath;

    if (m_cropSubMode == 1 && m_cropResizer) {
        clipPath.addRect(m_cropResizer->currentRect());
        m_scene->removeItem(m_cropResizer); delete m_cropResizer; m_cropResizer = nullptr;
    }
    else if (m_cropSubMode == 2 && m_lassoItem) {
        clipPath = m_lassoItem->path();
        m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr;
    }

    if (clipPath.isEmpty()) {
        m_cropMenu->hide(); m_interactionMode = InteractionMode::None; return;
    }

    for (auto* item : m_scene->selectedItems()) {
        if (auto* pathItem = dynamic_cast<QGraphicsPathItem*>(item)) {
            QPainterPath localClip = item->mapFromScene(clipPath);
            localClip.setFillRule(Qt::WindingFill);
            QPainterPath result = pathItem->path().intersected(localClip);
            pathItem->setPath(result);
        }
        else if (auto* pixmapItem = dynamic_cast<QGraphicsPixmapItem*>(item)) {
            QPainterPath localClip = item->mapFromScene(clipPath);
            QPixmap original = pixmapItem->pixmap();
            if (original.isNull()) continue;
            QPixmap result(original.size()); result.fill(Qt::transparent);
            QPainter p(&result); p.setRenderHint(QPainter::Antialiasing);
            p.setClipPath(localClip);
            p.drawPixmap(0, 0, original); p.end();
            pixmapItem->setPixmap(result);
        }
    }

    m_cropMenu->hide();
    m_interactionMode = InteractionMode::None;
    emit contentModified();
}

void CanvasView::cancelCrop() {
    if (m_cropResizer) { m_scene->removeItem(m_cropResizer); delete m_cropResizer; m_cropResizer = nullptr; }
    if (m_lassoItem) { m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; }
    m_cropMenu->hide();
    m_interactionMode = InteractionMode::None;
}

// === TRANSFORM SYSTEM (FIXED - NO GROUP IF SINGLE, CENTER PIVOT FIX) ===

void CanvasView::startTransformSession() {
    QList<QGraphicsItem*> items = m_scene->selectedItems();
    if (items.isEmpty()) return;

    m_selectionMenu->hide();
    m_interactionMode = InteractionMode::Transform;

    if (items.count() == 1) {
        QGraphicsItem* singleItem = items.first();
        m_transformGroup = nullptr;

        // Verschiebe TransformOriginPoint in die Mitte, ohne das Item visuell zu verschieben
        QPointF oldCenter = singleItem->sceneBoundingRect().center();
        singleItem->setTransformOriginPoint(singleItem->boundingRect().center());
        QPointF newCenter = singleItem->sceneBoundingRect().center();
        singleItem->moveBy(oldCenter.x() - newCenter.x(), oldCenter.y() - newCenter.y());

        // BAKE: Damit wir sauber skalieren können
        bakeTransform(singleItem);

        m_transformOverlay = new TransformOverlay(singleItem);
    }
    else {
        // MEHRERE ITEMS: Gruppe nutzen
        m_transformGroup = m_scene->createItemGroup(items);
        QRectF grpRect = m_transformGroup->boundingRect();
        m_transformGroup->setTransformOriginPoint(grpRect.center());
        m_transformOverlay = new TransformOverlay(m_transformGroup);
    }

    m_transformOverlay->setZValue(99999);
    m_scene->addItem(m_transformOverlay);
}

void CanvasView::applyTransform() {
    if (m_transformOverlay) {
        m_scene->removeItem(m_transformOverlay);
        delete m_transformOverlay;
        m_transformOverlay = nullptr;
    }

    if (m_transformGroup) {
        m_scene->destroyItemGroup(m_transformGroup);
        m_transformGroup = nullptr;
    }

    m_interactionMode = InteractionMode::None;
    emit contentModified();
}

void CanvasView::updateSelectionMenuPosition() {
    if (m_scene->selectedItems().isEmpty()) { m_selectionMenu->hide(); return; }
    QRectF totalRect;
    for (auto* item : m_scene->selectedItems()) totalRect = totalRect.united(item->sceneBoundingRect());
    QPoint viewPos = mapFromScene(totalRect.topLeft());
    int x = viewPos.x() + (mapFromScene(totalRect.bottomRight()).x() - viewPos.x()) / 2 - m_selectionMenu->width() / 2;
    int y = viewPos.y() - m_selectionMenu->height() - 10;
    x = qMax(0, qMin(x, width() - m_selectionMenu->width()));
    y = qMax(0, y);
    m_selectionMenu->move(x, y);
    m_selectionMenu->show(); m_selectionMenu->raise();
}

void CanvasView::updateCropMenuPosition() {
    QRectF totalRect;
    for (auto* item : m_scene->selectedItems()) totalRect = totalRect.united(item->sceneBoundingRect());
    if (totalRect.isEmpty()) totalRect = mapToScene(viewport()->rect()).boundingRect();

    QPoint viewPos = mapFromScene(totalRect.topLeft());
    int x = viewPos.x() + (mapFromScene(totalRect.bottomRight()).x() - viewPos.x()) / 2 - m_cropMenu->width() / 2;
    int y = viewPos.y() - m_cropMenu->height() - 10;
    x = qMax(0, qMin(x, width() - m_cropMenu->width()));
    y = qMax(0, y);
    m_cropMenu->move(x, y);
}

void CanvasView::clearSelection() {
    if (m_interactionMode != InteractionMode::None) return; // Nicht aufheben während Transform/Crop
    m_scene->clearSelection();
    if (m_lassoItem) { m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; }
    m_selectionMenu->hide();
}

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

    // --- CHECK FOR HANDLE OR CENTER FIRST ---
    bool hitHandle = false;
    if (m_interactionMode == InteractionMode::Transform && m_transformOverlay) {
        TransformOverlay::Handle h = m_transformOverlay->handleAt(mapToScene(event->pos()));
        if (h != TransformOverlay::None) hitHandle = true;
    }

    if (m_penOnlyMode && isTouch && !hitHandle) { // Nur Pannen, wenn KEIN Handle getroffen wurde
        m_isPanning = true; m_lastPanPos = event->pos(); setCursor(Qt::ClosedHandCursor); event->accept(); return;
    }
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true; m_lastPanPos = event->pos(); setCursor(Qt::ClosedHandCursor); event->accept(); return;
    }

    // --- INTERACTION MODES ---
    if (m_interactionMode == InteractionMode::Crop) {
        if (m_cropSubMode == 2) { // Freehand
            m_isDrawing = true; m_currentPath = QPainterPath(); m_currentPath.moveTo(mapToScene(event->pos()));
            if (!m_lassoItem) {
                m_lassoItem = new QGraphicsPathItem();
                m_lassoItem->setPen(QPen(Qt::red, 2, Qt::DashLine)); m_lassoItem->setZValue(99999);
                m_scene->addItem(m_lassoItem);
            }
        }
        QGraphicsView::mousePressEvent(event); return;
    }

    if (m_interactionMode == InteractionMode::Transform) {
        if (m_transformOverlay && !m_transformOverlay->isUnderMouse()) {
            applyTransform();
        } else {
            QGraphicsView::mousePressEvent(event); return;
        }
    }

    AbstractTool* newTool = ToolManager::instance().activeTool();
    if (newTool && event->button() == Qt::LeftButton) {
        QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMousePress);
        QPointF scenePos = mapToScene(event->pos());
        scEvent.setScenePos(scenePos);
        scEvent.setButton(event->button());
        scEvent.setButtons(event->buttons());
        scEvent.setModifiers(event->modifiers());

        if(newTool->handleMousePress(&scEvent, m_scene)) {
            m_isDrawing = true;
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        if (m_selectionMenu->isVisible() && !itemAt(event->pos())) m_selectionMenu->hide();

        bool onPage = false;
        if (!m_isInfinite) {
            qreal currentY = 0;
            while(currentY < m_a4Rect.height()) {
                QRectF pageRect(0, currentY, PAGE_WIDTH, PAGE_HEIGHT);
                if (pageRect.contains(scenePos)) { onPage = true; break; }
                currentY += PAGE_HEIGHT + PAGE_GAP;
            }
            if (!onPage) return;
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
        event->accept(); return;
    }

    if (m_interactionMode == InteractionMode::Crop && m_cropSubMode == 2 && m_isDrawing && m_lassoItem) {
        m_currentPath.lineTo(mapToScene(event->pos())); m_lassoItem->setPath(m_currentPath); return;
    }

    AbstractTool* newTool = ToolManager::instance().activeTool();
    if (newTool && m_isDrawing && m_interactionMode == InteractionMode::None) {
        QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseMove);
        QPointF scenePos = mapToScene(event->pos());
        scEvent.setScenePos(scenePos);
        scEvent.setButtons(event->buttons());
        scEvent.setModifiers(event->modifiers());
        if (newTool->handleMouseMove(&scEvent, m_scene)) { event->accept(); return; }
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event) {
    const QInputDevice* dev = event->device();
    bool isTouch = (dev && dev->type() == QInputDevice::DeviceType::TouchScreen);
    if (!isTouch && event->source() == Qt::MouseEventSynthesizedBySystem) isTouch = true;

    if (m_penOnlyMode && isTouch) { m_isPanning = false; if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); } setTool(m_currentTool); event->accept(); return; }
    if (event->button() == Qt::MiddleButton) { m_isPanning = false; if (m_pullDistance > 0) { m_pullDistance = 0; viewport()->update(); } setTool(m_currentTool); event->accept(); return; }

    if (m_interactionMode == InteractionMode::Crop && m_cropSubMode == 2 && m_isDrawing) {
        m_isDrawing = false; m_currentPath.closeSubpath(); if(m_lassoItem) m_lassoItem->setPath(m_currentPath); return;
    }

    AbstractTool* newTool = ToolManager::instance().activeTool();
    if (newTool && m_isDrawing && event->button() == Qt::LeftButton && m_interactionMode == InteractionMode::None) {
        QGraphicsSceneMouseEvent scEvent(QEvent::GraphicsSceneMouseRelease);
        QPointF scenePos = mapToScene(event->pos());
        scEvent.setScenePos(scenePos);
        scEvent.setButton(event->button());

        if (newTool->handleMouseRelease(&scEvent, m_scene)) {
            m_isDrawing = false;
            emit contentModified();
            if (newTool->mode() == ToolMode::Lasso) { finishLasso(); }
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseReleaseEvent(event);
}

bool CanvasView::event(QEvent *event) {
    if (event->type() == QEvent::Gesture) { gestureEvent(static_cast<QGestureEvent*>(event)); return true; }
    return QGraphicsView::event(event);
}

void CanvasView::gestureEvent(QGestureEvent *event) {
    if (QGesture *pinch = event->gesture(Qt::PinchGesture)) pinchTriggered(static_cast<QPinchGesture *>(pinch));
}

void CanvasView::pinchTriggered(QPinchGesture *gesture) {
    // --- PINCH TO TRANSFORM ITEM ---
    if (m_interactionMode == InteractionMode::Transform && m_transformOverlay) {
        if (gesture->state() == Qt::GestureStarted) {
            bakeTransform(m_transformOverlay->target());
            m_transformOverlay->sync();
        }

        QGraphicsItem* item = m_transformOverlay->target();
        qreal scaleFactor = gesture->scaleFactor();
        qreal rotationAngle = gesture->rotationAngle() - gesture->lastRotationAngle();

        // 1. Translation: Move item by finger delta
        QPointF diff = mapToScene(gesture->centerPoint().toPoint()) - mapToScene(gesture->lastCenterPoint().toPoint());
        item->moveBy(diff.x(), diff.y());

        // 2. Rotate/Scale around the new finger center
        QPointF scenePivot = mapToScene(gesture->centerPoint().toPoint());
        QPointF localPivot = item->transform().inverted().map(scenePivot);

        QTransform delta;
        delta.translate(localPivot.x(), localPivot.y());
        delta.rotate(-rotationAngle);
        delta.scale(scaleFactor, scaleFactor);
        delta.translate(-localPivot.x(), -localPivot.y());

        item->setTransform(delta * item->transform());

        m_transformOverlay->sync();
        return;
    }

    if (m_isDrawing) { m_isDrawing = false; if (m_currentItem) { m_scene->removeItem(m_currentItem); delete m_currentItem; m_currentItem = nullptr; } if (m_lassoItem) { m_scene->removeItem(m_lassoItem); delete m_lassoItem; m_lassoItem = nullptr; } }
    if (m_isPanning) m_isPanning = false;
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        qreal factor = gesture->scaleFactor();
        double currentScale = transform().m11();
        if (!((currentScale < 0.1 && factor < 1) || (currentScale > 10 && factor > 1))) scale(factor, factor);
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
    if (!m_scene->selectedItems().isEmpty()) updateSelectionMenuPosition();
    else m_selectionMenu->hide();
}

void CanvasView::applyEraser(const QPointF &pos) {
    QRectF eraserRect(pos.x() - 10, pos.y() - 10, 20, 20); const QList<QGraphicsItem*> items = m_scene->items(eraserRect);
    bool erased = false;
    for (auto* item : std::as_const(items)) {
        if (item->type() == QGraphicsPathItem::Type) { m_scene->removeItem(item); m_undoList.removeOne(item); delete item; erased = true; }
        else if (item->type() == QGraphicsTextItem::Type) { m_scene->removeItem(item); m_undoList.removeOne(item); delete item; erased = true; }
    }
    if (erased) emit contentModified();
}

#include "canvasview.moc"
