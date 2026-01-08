#include "moderntoolbar.h"
#include "UIStyles.h"
#include "tools/ToolManager.h"
#include "tools/ToolUIBridge.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <cmath>
#include <QWheelEvent>
#include <QColorDialog>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QRegion>
#include <QGraphicsDropShadowEffect>

// =============================================================================
// Helper Button Implementation
// =============================================================================
ToolbarBtn::ToolbarBtn(const QString& iconName, QWidget* parent) : QWidget(parent), m_iconName(iconName) {
    setBtnSize(40);
    setCursor(Qt::PointingHandCursor);
}
void ToolbarBtn::setBtnSize(int s) { if(m_size != s) { m_size = s; setFixedSize(s, s); update(); } }
void ToolbarBtn::setIcon(const QString& name) { m_iconName = name; update(); }
void ToolbarBtn::setActive(bool active) { m_active = active; update(); }
void ToolbarBtn::mousePressEvent(QMouseEvent* e) { if(e->button()==Qt::LeftButton) emit clicked(); }
void ToolbarBtn::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void ToolbarBtn::leaveEvent(QEvent*) { m_hover = false; update(); }
void ToolbarBtn::animateSelect() {
    QPropertyAnimation* anim = new QPropertyAnimation(this, "animScale");
    anim->setDuration(300); anim->setKeyValueAt(0, 1.0); anim->setKeyValueAt(0.5, 1.3); anim->setKeyValueAt(1, 1.0);
    anim->setEasingCurve(QEasingCurve::OutBack); anim->start(QAbstractAnimation::DeleteWhenStopped);
}
void ToolbarBtn::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    int r = m_size / 2 - 4;
    if (m_active) {
        p.setBrush(QColor(0x5E5CE6));
        p.setPen(Qt::NoPen);
        p.drawEllipse(rect().center(), r, r);
    }
    else if (m_hover) {
        p.setBrush(QColor(255,255,255,30));
        p.setPen(Qt::NoPen);
        p.drawEllipse(rect().center(), r, r);
    }
    p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    int w = width(); int h = height(); p.translate(w/2, h/2);
    double scale = (double)w / 110.0; scale *= m_animScale; p.scale(scale, scale); p.translate(-32, -32);

    if(m_iconName=="pen") { QPainterPath pa; pa.moveTo(12,52); pa.lineTo(22,52); pa.lineTo(50,24); pa.lineTo(40,14); pa.lineTo(12,42); pa.closeSubpath(); p.drawPath(pa); }
    else if(m_iconName=="pencil") { QPainterPath pa; pa.moveTo(14,50); pa.lineTo(24,50); pa.lineTo(52,22); pa.lineTo(42,12); pa.lineTo(14,40); pa.closeSubpath(); p.drawPath(pa); p.drawLine(16,48, 22,42); p.drawLine(18,50, 24,44); }
    else if(m_iconName=="highlighter") { p.setPen(Qt::NoPen); p.setBrush(Qt::white); p.save(); p.translate(32, 32); p.rotate(-45); p.drawRoundedRect(-6, -24, 12, 36, 2, 2); p.setOpacity(0.5); p.drawRect(-6, -30, 12, 6); p.restore(); }
    else if(m_iconName=="eraser") { p.drawRoundedRect(15,15,34,34,5,5); p.drawLine(25,25,39,39); p.drawLine(39,25,25,39); }
    else if(m_iconName=="lasso") { p.drawEllipse(15,15,34,34); p.drawLine(45,45,55,55); }
    else if(m_iconName=="ruler") { p.save(); p.translate(32, 32); p.rotate(-45); p.drawRect(-10, -25, 20, 50); for(int i=-20; i<=20; i+=10) p.drawLine(-10, i, -5, i); p.restore(); }
    else if(m_iconName=="shape") { p.drawRect(12, 32, 20, 20); p.drawEllipse(32, 12, 20, 20); }
    else if(m_iconName=="stickynote") { QPainterPath pa; pa.moveTo(15, 15); pa.lineTo(40, 15); pa.lineTo(50, 25); pa.lineTo(50, 50); pa.lineTo(15, 50); pa.closeSubpath(); p.drawPath(pa); p.drawLine(40, 15, 40, 25); p.drawLine(40, 25, 50, 25); }
    else if(m_iconName=="text") { p.drawLine(20, 15, 44, 15); p.drawLine(32, 15, 32, 49); p.drawLine(28, 49, 36, 49); }
    else if(m_iconName=="image") { p.drawRect(10, 15, 44, 34); p.drawEllipse(16, 20, 8, 8); p.drawLine(10, 40, 20, 30); p.drawLine(20, 30, 30, 40); p.drawLine(30, 40, 40, 25); p.drawLine(40, 25, 54, 45); }
    else if(m_iconName=="hand") { p.drawRoundedRect(20, 25, 24, 24, 4, 4); p.drawLine(20, 25, 20, 15); p.drawLine(28, 25, 28, 12); }
    else if(m_iconName=="undo") { QPainterPath pa; pa.moveTo(40,20); pa.arcTo(20,20,30,30,90,180); p.drawPath(pa); p.drawLine(20,35,10,25); p.drawLine(20,15,10,25); }
    else if(m_iconName=="redo") { QPainterPath pa; pa.moveTo(24,20); pa.arcTo(14,20,30,30,90,-180); p.drawPath(pa); p.drawLine(44,35,54,25); p.drawLine(44,15,54,25); }
}

// =============================================================================
// ModernToolbar Implementation
// =============================================================================

ModernToolbar::ModernToolbar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);
    setMouseTracking(true);
    if (parent) parent->installEventFilter(this);

    btnPen = new ToolbarBtn("pen", this);
    btnPencil = new ToolbarBtn("pencil", this);
    btnHighlighter = new ToolbarBtn("highlighter", this);
    btnEraser = new ToolbarBtn("eraser", this);
    btnLasso = new ToolbarBtn("lasso", this);
    btnRuler = new ToolbarBtn("ruler", this);
    btnShape = new ToolbarBtn("shape", this);
    btnStickyNote = new ToolbarBtn("stickynote", this);
    btnText = new ToolbarBtn("text", this);
    btnImage = new ToolbarBtn("image", this);
    btnHand = new ToolbarBtn("hand", this);
    btnUndo = new ToolbarBtn("undo", this);
    btnRedo = new ToolbarBtn("redo", this);

    m_buttons = {
        btnUndo,
        btnPen, btnPencil, btnHighlighter, btnEraser,
        btnLasso, btnRuler, btnShape, btnStickyNote,
        btnText, btnImage, btnHand,
        btnRedo
    };

    auto handleToolClick = [this](ToolMode m) {
        // 1. POSITIONIERUNG
        ToolbarBtn* btn = getButtonForMode(m);
        if (btn) {
            QPoint pos = btn->mapToGlobal(QPoint(btn->width() + 15, 0));
            ToolUIBridge::instance().setOverlayPosition(pos);
        }

        // 2. LOGIK
        ToolManager::instance().selectTool(m);

        // 3. UI
        if(mode_ != m) {
            setToolMode(m);
        }
    };

    connect(btnPen, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Pen); });
    connect(btnPencil, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Pencil); });
    connect(btnHighlighter, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Highlighter); });
    connect(btnEraser, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Eraser); });
    connect(btnLasso, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Lasso); });
    connect(btnRuler, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Ruler); });
    connect(btnShape, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Shape); });
    connect(btnStickyNote, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::StickyNote); });
    connect(btnText, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Text); });
    connect(btnImage, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Image); });
    connect(btnHand, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Hand); });

    connect(btnUndo, &ToolbarBtn::clicked, this, &ModernToolbar::undoRequested);
    connect(btnRedo, &ToolbarBtn::clicked, this, &ModernToolbar::redoRequested);

    setStyle(Normal);

    // Initialisierung
    setToolMode(ToolMode::Pen);
    ToolManager::instance().selectTool(ToolMode::Pen);
}

ToolbarBtn* ModernToolbar::getButtonForMode(ToolMode m) {
    switch(m) {
    case ToolMode::Pen: return btnPen;
    case ToolMode::Pencil: return btnPencil;
    case ToolMode::Highlighter: return btnHighlighter;
    case ToolMode::Eraser: return btnEraser;
    case ToolMode::Lasso: return btnLasso;
    case ToolMode::Ruler: return btnRuler;
    case ToolMode::Shape: return btnShape;
    case ToolMode::StickyNote: return btnStickyNote;
    case ToolMode::Text: return btnText;
    case ToolMode::Image: return btnImage;
    case ToolMode::Hand: return btnHand;
    default: return btnPen;
    }
}

void ModernToolbar::showEvent(QShowEvent*) { updateLayout(); if (m_style == Radial && !m_isPreview) constrainToParent(); updateHitbox(); }
bool ModernToolbar::eventFilter(QObject* watched, QEvent* event) { if (watched == parentWidget() && event->type() == QEvent::Resize && !m_isPreview) constrainToParent(); return QWidget::eventFilter(watched, event); }
void ModernToolbar::setTopBound(int top) { m_topBound = top; constrainToParent(); }
void ModernToolbar::setScale(double s) { if (s < 0.5) s = 0.5; if (s > 2.0) s = 2.0; m_scale = s; if (m_style == Radial) { int btnSize = 40 * m_scale; for(auto* b : m_buttons) b->setBtnSize(btnSize); int size = 460 * m_scale; setFixedSize(size, size); updateLayout(); updateHitbox(); update(); if(!m_isPreview) constrainToParent(); } }
void ModernToolbar::constrainToParent() { if (!parentWidget() || m_isPreview) return; QRect pRect = parentWidget()->rect(); int padding = 0; if (m_style == Radial) { int rVisual = 175 * m_scale; int rWidget = width() / 2; padding = rWidget - rVisual; } int maxX = pRect.width() - width() + padding; int maxY = pRect.height() - height() + padding; int minX = -padding; int newX = x(); int newY = y(); int effectiveTop = (m_style == Radial) ? -padding : m_topBound; if (newY < effectiveTop) newY = effectiveTop; if (newY > maxY) newY = maxY; if (m_style == Radial && m_radialType == HalfEdge) { snapToEdge(); move(x(), newY); } else { if (newX < minX) newX = minX; if (newX > maxX) newX = maxX; move(newX, newY); } }
void ModernToolbar::reorderButtons() { ToolbarBtn* targetBtn = getButtonForMode(mode_); int targetIdx = m_buttons.indexOf(targetBtn); if (targetIdx > 0 && targetIdx < m_buttons.size() - 1) { m_buttons.move(targetIdx, 0); } updateLayout(false); }
void ModernToolbar::setToolMode(ToolMode mode) { if (m_style == Radial && m_radialType == FullCircle && mode_ != mode) { ToolbarBtn* targetBtn = getButtonForMode(mode); if (targetBtn) { int targetIdx = m_buttons.indexOf(targetBtn); if (targetIdx != -1) { m_buttons.move(targetIdx, 0); updateLayout(true); } } } mode_ = mode; for(auto* b : m_buttons) b->setActive(false); ToolbarBtn* activeBtn = getButtonForMode(mode); if(activeBtn) { activeBtn->setActive(true); activeBtn->animateSelect(); } update(); updateHitbox(); }
void ModernToolbar::setStyle(Style style) { m_style = style; m_cachedMask = QRegion(); bool buttonsIgnoreMouse = (style == Radial); for(auto* b : m_buttons) { b->setAttribute(Qt::WA_TransparentForMouseEvents, buttonsIgnoreMouse); } if (m_style == Normal) { setMinimumSize(50, 50); setMaximumSize(1200, 1200); if (m_orientation == Vertical) resize(55, calculateMinLength()); else resize(calculateMinLength(), 55); } else { int size = 460 * m_scale; setFixedSize(size, size); reorderButtons(); } for(auto b : m_buttons) b->show(); if (style == Radial && m_radialType == HalfEdge) snapToEdge(); else constrainToParent(); updateLayout(); updateHitbox(); update(); }
void ModernToolbar::setRadialType(RadialType type) { if (m_radialType == type) return; m_radialType = type; m_scrollAngle = 0.0; if (m_style == Radial) { if (m_radialType == HalfEdge && !m_isDragging) snapToEdge(); else { if (!m_isPreview && parentWidget() && !m_isDragging) { constrainToParent(); } } updateLayout(true); updateHitbox(); update(); } }
void ModernToolbar::updateHitbox() { if (m_isDragging) return; if (m_style != Radial) { clearMask(); m_cachedMask = QRegion(); return; } int r = 95 * m_scale; int maskR = r + 4; QRegion newMask; if (m_radialType == FullCircle) { int cx = width() / 2; int cy = height() / 2; newMask = QRegion(cx - maskR, cy - maskR, 2 * maskR, 2 * maskR, QRegion::Ellipse); } else { int cy = height() / 2; int cx = m_isDockedLeft ? 0 : width(); QRegion circleReg(cx - maskR, cy - maskR, 2 * maskR, 2 * maskR, QRegion::Ellipse); QRegion widgetReg(rect()); newMask = circleReg.intersected(widgetReg); } if (newMask == m_cachedMask) return; setMask(newMask); m_cachedMask = newMask; }
void ModernToolbar::snapToEdge() { if (!parentWidget() || m_isPreview) return; int parentW = parentWidget()->width(); int mid = parentW / 2; int targetX = 0; if (x() + width()/2 < mid) { targetX = 0; m_isDockedLeft = true; } else { targetX = parentW - width(); m_isDockedLeft = false; } QPropertyAnimation* anim = new QPropertyAnimation(this, "pos"); anim->setDuration(300); anim->setStartValue(pos()); anim->setEndValue(QPoint(targetX, y())); anim->setEasingCurve(QEasingCurve::OutCubic); connect(anim, &QPropertyAnimation::finished, this, [this](){ updateLayout(); updateHitbox(); update(); }); anim->start(QAbstractAnimation::DeleteWhenStopped); }
void ModernToolbar::wheelEvent(QWheelEvent *e) { if (m_style == Radial && m_radialType == HalfEdge) { double delta = e->angleDelta().y() / 8.0; m_scrollAngle += delta; double maxScroll = (m_buttons.size() * 30.0); if (m_scrollAngle > maxScroll) m_scrollAngle = maxScroll; if (m_scrollAngle < -maxScroll) m_scrollAngle = -maxScroll; updateLayout(); e->accept(); } else { QWidget::wheelEvent(e); } }
void ModernToolbar::checkOrientation(const QPoint& globalPos) { Q_UNUSED(globalPos); if (!parentWidget() || m_style != Normal) return; QRect myRect = geometry(); int parentW = parentWidget()->width(); int parentH = parentWidget()->height(); int distLeft = myRect.left(); int distRight = parentW - myRect.right(); int distTop = myRect.top(); int distBottom = parentH - myRect.bottom(); int threshold = 80; bool horizontal = false; if (distTop < threshold || distBottom < threshold) horizontal = true; else if (distLeft < threshold || distRight < threshold) horizontal = false; else return; setOrientation(horizontal ? Horizontal : Vertical, true); }
void ModernToolbar::setOrientation(Orientation o, bool animate) { if (m_orientation == o) return; m_orientation = o; m_cachedMask = QRegion(); QSize currentS = size(); QSize targetS(currentS.height(), currentS.width()); if (animate) { QPropertyAnimation* anim = new QPropertyAnimation(this, "size"); anim->setDuration(250); anim->setStartValue(currentS); anim->setEndValue(targetS); anim->setEasingCurve(QEasingCurve::OutCubic); connect(anim, &QPropertyAnimation::valueChanged, this, [this](const QVariant&){ updateLayout(); }); anim->start(QAbstractAnimation::DeleteWhenStopped); } else { resize(targetS); updateLayout(); } }
int ModernToolbar::calculateMinLength() { int btnS = 40 * m_scale; if (!m_buttons.isEmpty()) btnS = m_buttons[0]->width(); int dragH = 50; int handleH = 30; int minGap = 5; int numButtons = m_buttons.size(); return dragH + (numButtons * btnS) + ((numButtons+1) * minGap) + handleH; }
void ModernToolbar::updateLayout(bool animate) { if (m_isDragging) animate = false; if (m_style == Normal) { int w = width(); int h = height(); int len = (m_orientation == Vertical) ? h : w; int breadth = qMin(w, h); int btnS = breadth - 14; if (btnS > 48) btnS = 48; if (btnS < 20) btnS = 20; for(auto* b : m_buttons) b->setBtnSize(btnS); int dragSize = 50; int handleSize = 30; int available = len - dragSize - handleSize; int content = m_buttons.size() * btnS; int gap = 8; if (available > content) { gap = (available - content) / (m_buttons.size() + 1); if (gap > 25) gap = 25; } if (gap < 2) gap = 2; int currentPos = dragSize; auto place = [&](ToolbarBtn* b) { int bx, by; if (m_orientation == Vertical) { bx = (w - btnS) / 2; by = currentPos; currentPos += btnS + gap; } else { bx = currentPos; by = (h - btnS) / 2; currentPos += btnS + gap; } if(animate) { QPropertyAnimation* anim = new QPropertyAnimation(b, "pos"); anim->setDuration(200); anim->setEndValue(QPoint(bx, by)); anim->setEasingCurve(QEasingCurve::OutQuad); anim->start(QAbstractAnimation::DeleteWhenStopped); } else { b->move(bx, by); } }; for(auto* b : m_buttons) { place(b); } for(auto b : m_buttons) b->show(); } else { int cx = width() / 2; int cy = height() / 2; int btnS = 40 * m_scale; if (m_radialType == HalfEdge) { int paintCx = m_isDockedLeft ? 0 : width(); int r = 70 * m_scale; double baseAngle = m_isDockedLeft ? 0.0 : 180.0; double spacing = 35.0; for (int i = 0; i < m_buttons.size(); ++i) { int relativeIdx = i - 2; double angleDeg = baseAngle + (relativeIdx * spacing) + m_scrollAngle; double angleRad = angleDeg * 3.14159 / 180.0; int bx = paintCx + r * std::cos(angleRad) - btnS/2; int by = cy + r * std::sin(angleRad) - btnS/2; if(animate) { QPropertyAnimation* anim = new QPropertyAnimation(m_buttons[i], "pos"); anim->setDuration(250); anim->setEndValue(QPoint(bx, by)); anim->setEasingCurve(QEasingCurve::OutBack); anim->start(QAbstractAnimation::DeleteWhenStopped); } else { m_buttons[i]->move(bx, by); } bool visible = false; if (m_isDockedLeft) visible = std::cos(angleRad) > 0.05; else visible = std::cos(angleRad) < -0.05; m_buttons[i]->setVisible(visible); } } else { int r = 65 * m_scale; int centerBtnX = (width() - btnS) / 2; int centerBtnY = (height() - btnS) / 2; auto moveBtn = [&](ToolbarBtn* b, int bx, int by) { if(animate) { QPropertyAnimation* anim = new QPropertyAnimation(b, "pos"); anim->setDuration(250); anim->setEndValue(QPoint(bx, by)); anim->setEasingCurve(QEasingCurve::OutBack); anim->start(QAbstractAnimation::DeleteWhenStopped); } else { b->move(bx, by); } }; moveBtn(m_buttons[0], centerBtnX, centerBtnY); int countRing = m_buttons.size() - 1; if (countRing < 1) countRing = 1; double angleStep = 360.0 / countRing; double startAngle = -90.0; for(int i=1; i<m_buttons.size(); ++i) { double angle = startAngle + (i-1) * angleStep; double rad = angle * 3.14159 / 180.0; int bx = cx + r * std::cos(rad) - btnS/2; int by = cy + r * std::sin(rad) - btnS/2; moveBtn(m_buttons[i], bx, by); } for(auto b : m_buttons) b->show(); } } }
void ModernToolbar::resizeEvent(QResizeEvent *) { updateLayout(); updateHitbox(); }

void ModernToolbar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int cx = width() / 2;
    int cy = height() / 2;

    if (m_style == Normal) {
        int w = width();
        int h = height();
        p.setBrush(QColor(37,37,38,245));
        p.setPen(QPen(QColor(60,60,60), 1));

        int radius = (m_orientation == Vertical) ? w / 2 : h / 2;
        p.drawRoundedRect(rect().adjusted(1,1,-1,-1), radius, radius);

        p.setBrush(QColor(150,150,150));
        p.setPen(Qt::NoPen);

        if (m_orientation == Vertical) {
            p.drawRoundedRect(w/2 - 12, 18, 24, 4, 2, 2);
        } else {
            p.drawRoundedRect(18, h/2 - 12, 4, 24, 2, 2);
        }
    }
    else {
        p.setBrush(QColor(37,37,38,230));
        p.setPen(QPen(QColor(0x5E5CE6), 2));
        int rMain = 95 * m_scale;

        // Kein äußerer Ring mehr, da QML Overlay genutzt wird

        if (m_radialType == HalfEdge) {
            int paintCx = m_isDockedLeft ? 0 : width();
            QRect box(paintCx - rMain, cy - rMain, rMain*2, rMain*2);
            p.drawPie(box, (m_isDockedLeft ? -90 : 90) * 16, 180 * 16);
        } else {
            p.drawEllipse(QPoint(cx, cy), rMain, rMain);
            p.setBrush(QColor(30,30,30,255)); p.setPen(Qt::NoPen);

            // FIX: Expliziter Cast zu int, um Compiler-Mehrdeutigkeit zu lösen
            int hole = static_cast<int>(45 * m_scale);
            p.drawEllipse(QPoint(cx, cy), hole, hole);
        }
    }
}

ToolbarBtn* ModernToolbar::getRadialButtonAt(const QPoint& pos) { for(auto* b : m_buttons) { if(b->isVisible() && b->geometry().contains(pos)) { return b; } } return nullptr; }
void ModernToolbar::mousePressEvent(QMouseEvent *e) { if (e->button() == Qt::LeftButton) { m_pressedButton = nullptr; m_isScrolling = false; m_hasScrolled = false; m_isDragging = false; if (m_style == Radial) { m_pressedButton = getRadialButtonAt(e->pos()); } int dragR = 45 * m_scale; int cx = width()/2; int cy = height()/2; if (m_style == Radial && m_radialType == HalfEdge) cx = m_isDockedLeft ? 0 : width(); int dx = e->pos().x() - cx; int dy = e->pos().y() - cy; double dist = std::sqrt(dx*dx + dy*dy); if (m_style == Radial && m_radialType == HalfEdge) { if (dist > dragR) { m_dragStartAngle = std::atan2(-dy, dx) * 180.0 / 3.14159; m_scrollStartAngleVal = m_scrollAngle; m_isScrolling = true; } } if (m_isResizing) return; if (m_style == Normal && !m_isPreview) { int handleSize = 30; if (e->pos().x() > width() - handleSize && e->pos().y() > height() - handleSize) { m_isResizing = true; dragStartPos_ = e->pos(); resizeStartPos_ = e->pos(); startSize_ = size(); return; } } if (!m_isPreview && !m_pressedButton) { bool canDrag = false; if (m_style == Radial) { if (dist < dragR) canDrag = true; } else { if (m_orientation == Vertical && e->pos().y() < 50) canDrag = true; if (m_orientation == Horizontal && e->pos().x() < 50) canDrag = true; } if (canDrag) { m_isDragging = true; dragStartPos_ = e->pos(); m_isScrolling = false; m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0,0)); clearMask(); return; } } } }
void ModernToolbar::mouseMoveEvent(QMouseEvent *e) { if (m_style == Radial && m_isScrolling && m_radialType == HalfEdge && !m_pressedButton && !m_isDragging) { int cx = m_isDockedLeft ? 0 : width(); int cy = height()/2; int dx = e->pos().x() - cx; int dy = e->pos().y() - cy; double currentAngle = std::atan2(-dy, dx) * 180.0 / 3.14159; double delta = currentAngle - m_dragStartAngle; if (delta > 180) delta -= 360; if (delta < -180) delta += 360; if (std::abs(delta) > 1.0) m_hasScrolled = true; m_scrollAngle = m_scrollStartAngleVal - delta; double maxScroll = (m_buttons.size() * 30.0); if (m_scrollAngle > maxScroll) m_scrollAngle = maxScroll; if (m_scrollAngle < -maxScroll) m_scrollAngle = -maxScroll; updateLayout(); return; } if (m_isDragging) { if (!parentWidget()) return; QPoint newPosGlobal = e->globalPosition().toPoint() - m_dragOffset; QPoint newTopLeft = parentWidget()->mapFromGlobal(newPosGlobal); QPoint globalMousePosInParent = parentWidget()->mapFromGlobal(e->globalPosition().toPoint()); int parentW = parentWidget()->width(); int parentH = parentWidget()->height(); int effectivePadding = 0; if (m_style == Radial) { int rVisual = 175 * m_scale; int rWidget = width() / 2; effectivePadding = rWidget - rVisual; } if (m_style == Radial) { int snapDist = 60 * m_scale; int tearDist = 120 * m_scale; if (m_radialType == FullCircle) { if (globalMousePosInParent.x() < snapDist) { m_isDockedLeft = true; setRadialType(HalfEdge); move(0, y()); m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0,0)); return; } else if (globalMousePosInParent.x() > (parentW - snapDist)) { m_isDockedLeft = false; setRadialType(HalfEdge); move(parentW - width(), y()); m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0,0)); return; } } else if (m_radialType == HalfEdge) { bool pullAway = false; if (m_isDockedLeft && globalMousePosInParent.x() > tearDist) pullAway = true; else if (!m_isDockedLeft && globalMousePosInParent.x() < (parentW - tearDist)) pullAway = true; if (pullAway) { setRadialType(FullCircle); clearMask(); int newX = globalMousePosInParent.x() - (width() / 2); move(newX, y()); m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0,0)); return; } else { int fixedX = m_isDockedLeft ? 0 : (parentW - width()); int minSlidingY = -effectivePadding; int maxSlidingY = parentH - height() + effectivePadding; move(fixedX, qBound(minSlidingY, newTopLeft.y(), maxSlidingY)); return; } } } int minX = -effectivePadding; int maxX = parentW - width() + effectivePadding; int maxY = parentH - height() + effectivePadding; int effectiveTop = (m_style == Radial) ? -effectivePadding : m_topBound; move(qBound(minX, newTopLeft.x(), maxX), qBound(effectiveTop, newTopLeft.y(), maxY)); return; } if (m_isResizing && m_style == Normal) { QPoint delta = e->pos() - resizeStartPos_; int newW = startSize_.width() + delta.x(); int newH = startSize_.height() + delta.y(); int minL = calculateMinLength(); if (m_orientation == Vertical) { if (newH < minL) newH = minL; if (newW < 50) newW = 50; if(newW > 100) newW = 100; } else { if (newW < minL) newW = minL; if (newH < 50) newH = 50; if(newH > 100) newH = 100; } resize(newW, newH); constrainToParent(); return; } if (m_style == Normal && !m_isPreview) { if (e->pos().x() > width() - 30 && e->pos().y() > height() - 30) setCursor(Qt::SizeFDiagCursor); else setCursor(Qt::ArrowCursor); } }
void ModernToolbar::mouseReleaseEvent(QMouseEvent *e) { if (m_pressedButton) { if (m_style == Radial && m_radialType == HalfEdge && m_hasScrolled) { } else { m_pressedButton->triggerClick(); } m_pressedButton = nullptr; } if (m_isDragging) { m_cachedMask = QRegion(); updateHitbox(); if (m_style == Normal) { checkOrientation(e->globalPosition().toPoint()); } } m_isDragging = false; m_isResizing = false; m_isScrolling = false; m_hasScrolled = false; if (m_style == Radial && m_radialType == HalfEdge) { snapToEdge(); } else { constrainToParent(); } }
void ModernToolbar::leaveEvent(QEvent *) { setCursor(Qt::ArrowCursor); }
