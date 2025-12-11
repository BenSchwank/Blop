#include "moderntoolbar.h"
#include "UIStyles.h"
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

// =============================================================================
// Helper Button Implementation
// =============================================================================
ToolbarBtn::ToolbarBtn(const QString& iconName, QWidget* parent) : QWidget(parent), m_iconName(iconName) {
    setBtnSize(40);
    setCursor(Qt::PointingHandCursor);
}

void ToolbarBtn::setBtnSize(int s) {
    if(m_size != s) { m_size = s; setFixedSize(s, s); update(); }
}

void ToolbarBtn::setIcon(const QString& name) { m_iconName = name; update(); }
void ToolbarBtn::setActive(bool active) { m_active = active; update(); }

void ToolbarBtn::mousePressEvent(QMouseEvent* e) {
    // Normalerweise fängt ModernToolbar die Events im Radial Mode ab.
    // Falls Style=Normal ist, greift dies hier:
    if(e->button()==Qt::LeftButton) emit clicked();
}

void ToolbarBtn::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void ToolbarBtn::leaveEvent(QEvent*) { m_hover = false; update(); }

void ToolbarBtn::animateSelect() {
    QPropertyAnimation* anim = new QPropertyAnimation(this, "pulseScale");
    anim->setDuration(300);
    anim->setKeyValueAt(0, 1.0f);
    anim->setKeyValueAt(0.5, 1.3f);
    anim->setKeyValueAt(1, 1.0f);
    anim->setEasingCurve(QEasingCurve::OutBack);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToolbarBtn::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int r = m_size / 5;
    if (m_active) {
        p.setBrush(QColor(0x5E5CE6));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(4,4,-4,-4), r, r);
    } else if (m_hover) {
        p.setBrush(QColor(255,255,255,30));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(4,4,-4,-4), r, r);
    }

    p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    int w = width();
    int h = height();
    p.translate(w/2, h/2);

    double scale = (double)w / 110.0;
    scale *= m_pulseScale;
    p.scale(scale, scale);
    p.translate(-32, -32);

    if(m_iconName=="pen") { QPainterPath pa; pa.moveTo(12,52); pa.lineTo(22,52); pa.lineTo(50,24); pa.lineTo(40,14); pa.lineTo(12,42); pa.closeSubpath(); p.drawPath(pa); }
    else if(m_iconName=="eraser") { p.drawRoundedRect(15,15,34,34,5,5); p.drawLine(25,25,39,39); p.drawLine(39,25,25,39); }
    else if(m_iconName=="lasso") { p.drawEllipse(15,15,34,34); p.drawLine(45,45,55,55); }
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
    btnEraser = new ToolbarBtn("eraser", this);
    btnLasso = new ToolbarBtn("lasso", this);
    btnUndo = new ToolbarBtn("undo", this);
    btnRedo = new ToolbarBtn("redo", this);

    m_buttons = {btnUndo, btnEraser, btnPen, btnLasso, btnRedo};
    m_customColors = {Qt::black, Qt::white, Qt::red, Qt::blue, Qt::green, Qt::yellow};

    // --- KLICK LOGIK ---
    auto handleToolClick = [this](ToolMode m) {
        if(mode_ == m) {
            // Wenn das aktive Tool erneut geklickt wird: Einstellungen öffnen/schließen
            if (m_settingsState == SettingsState::Closed) {
                m_settingsState = SettingsState::Main;
                // Bei "Normal" (Leiste) Pop-up, bei "Radial" (Kreis) Ring öffnen
                if (m_style == Normal) showVerticalPopup();
                else update();
            } else {
                m_settingsState = SettingsState::Closed;
                update();
            }
        } else {
            // Neues Tool aktivieren
            setToolMode(m);
            emit toolChanged(m);
        }
    };

    connect(btnPen, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Pen); });
    connect(btnEraser, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Eraser); });
    connect(btnLasso, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Lasso); });
    connect(btnUndo, &ToolbarBtn::clicked, this, &ModernToolbar::undoRequested);
    connect(btnRedo, &ToolbarBtn::clicked, this, &ModernToolbar::redoRequested);

    setStyle(Normal);
    setToolMode(ToolMode::Pen);
}

void ModernToolbar::showEvent(QShowEvent*) {
    updateLayout();
    if (m_style == Radial && !m_isPreview) constrainToParent();
}

bool ModernToolbar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize && !m_isPreview) {
        constrainToParent();
    }
    return QWidget::eventFilter(watched, event);
}

void ModernToolbar::setTopBound(int top) { m_topBound = top; constrainToParent(); }

ToolbarBtn* ModernToolbar::getButtonForMode(ToolMode m) {
    if (m == ToolMode::Pen) return btnPen;
    if (m == ToolMode::Eraser) return btnEraser;
    if (m == ToolMode::Lasso) return btnLasso;
    return btnPen;
}

void ModernToolbar::setScale(qreal s) {
    if (s < 0.5) s = 0.5; if (s > 2.0) s = 2.0;
    m_scale = s;
    if (m_style == Radial) {
        int btnSize = 40 * m_scale;
        for(auto* b : m_buttons) b->setBtnSize(btnSize);
        int size = 420 * m_scale;
        setFixedSize(size, size);
        updateLayout();
        update();
        if(!m_isPreview) constrainToParent();
    }
}

void ModernToolbar::constrainToParent() {
    if (!parentWidget() || m_isPreview) return;
    QRect pRect = parentWidget()->rect();
    int maxX = pRect.width() - width();
    int maxY = pRect.height() - height();
    int newX = x();
    int newY = y();

    if (newY < m_topBound) newY = m_topBound;
    if (newY > maxY) newY = maxY;

    if (m_style == Radial && m_radialType == HalfEdge) {
        snapToEdge();
        move(x(), newY);
    } else {
        if (newX < 0) newX = 0;
        if (newX > qMax(0, maxX)) newX = qMax(0, maxX);
        move(newX, newY);
    }
}

void ModernToolbar::reorderButtons() {
    ToolbarBtn* targetBtn = getButtonForMode(mode_);
    int targetIdx = m_buttons.indexOf(targetBtn);
    int centerIdx = 2; // Index 2 ist die Mitte
    if (targetIdx != -1 && targetIdx != centerIdx) {
        std::swap(m_buttons[targetIdx], m_buttons[centerIdx]);
    }
    updateLayout(false);
}

void ModernToolbar::setToolMode(ToolMode mode) {
    // Bei FullCircle tauschen wir Buttons, damit das aktive in der Mitte ist
    if (m_style == Radial && m_radialType == FullCircle && mode_ != mode) {
        ToolbarBtn* targetBtn = getButtonForMode(mode);
        int targetIdx = m_buttons.indexOf(targetBtn);
        int centerIdx = 2;
        if (targetIdx != -1 && targetIdx != centerIdx) {
            std::swap(m_buttons[targetIdx], m_buttons[centerIdx]);
            updateLayout(true);
        }
    }

    mode_ = mode;
    m_settingsState = SettingsState::Closed; // Reset settings beim Wechsel

    btnPen->setActive(mode == ToolMode::Pen);
    btnEraser->setActive(mode == ToolMode::Eraser);
    btnLasso->setActive(mode == ToolMode::Lasso);

    if (mode == ToolMode::Pen) btnPen->animateSelect();
    else if (mode == ToolMode::Eraser) btnEraser->animateSelect();
    else if (mode == ToolMode::Lasso) btnLasso->animateSelect();

    update();
}

void ModernToolbar::setStyle(Style style) {
    m_style = style;
    m_settingsState = SettingsState::Closed;

    // Im Radial-Modus ignorieren die Buttons die Maus -> Scrollen möglich
    // WICHTIG: Wir lassen dies 'true', aber fangen Klicks jetzt manuell in mousePressEvent ab!
    bool buttonsIgnoreMouse = (style == Radial);
    for(auto* b : m_buttons) {
        b->setAttribute(Qt::WA_TransparentForMouseEvents, buttonsIgnoreMouse);
    }

    if (m_style == Normal) {
        setMinimumSize(50, 50); setMaximumSize(1200, 1200);
        if (m_orientation == Vertical) resize(55, 310); else resize(310, 55);
    } else {
        int size = 460 * m_scale; setFixedSize(size, size); reorderButtons();
    }

    for(auto b : m_buttons) b->show();

    if (style == Radial && m_radialType == HalfEdge) snapToEdge();
    else constrainToParent();

    updateLayout();
    update();
}

void ModernToolbar::setRadialType(RadialType type) {
    m_radialType = type;
    m_scrollAngle = 0.0;

    if (m_style == Radial) {
        if (m_radialType == HalfEdge) snapToEdge();
        else {
            if (!m_isPreview && parentWidget()) {
                int maxX = qMax(0, parentWidget()->width() - width());
                move(qBound(0, x(), maxX), y());
            }
        }
        updateLayout();
        update();
    }
}

void ModernToolbar::snapToEdge() {
    if (!parentWidget() || m_isPreview) return;
    int parentW = parentWidget()->width();
    int mid = parentW / 2;
    int targetX = 0;

    if (x() + width()/2 < mid) { targetX = 0; m_isDockedLeft = true; }
    else { targetX = parentW - width(); m_isDockedLeft = false; }

    QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(300);
    anim->setStartValue(pos());
    anim->setEndValue(QPoint(targetX, y()));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this](){ updateLayout(); update(); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ModernToolbar::wheelEvent(QWheelEvent *e) {
    if (m_style == Radial && m_radialType == HalfEdge) {
        double delta = e->angleDelta().y() / 8.0;
        m_scrollAngle += delta;
        if (m_scrollAngle > 140) m_scrollAngle = 140;
        if (m_scrollAngle < -140) m_scrollAngle = -140;
        updateLayout();
        e->accept();
    } else {
        QWidget::wheelEvent(e);
    }
}

void ModernToolbar::checkOrientation(const QPoint& globalPos) {
    Q_UNUSED(globalPos);
    if (!parentWidget() || m_style != Normal) return;

    QRect myRect = geometry();
    int parentW = parentWidget()->width();
    int parentH = parentWidget()->height();

    int distLeft = myRect.left();
    int distRight = parentW - myRect.right();
    int distTop = myRect.top();
    int distBottom = parentH - myRect.bottom();

    int threshold = 80;
    bool horizontal = false;

    if (distTop < threshold || distBottom < threshold) {
        horizontal = true;
    } else if (distLeft < threshold || distRight < threshold) {
        horizontal = false;
    } else {
        return;
    }

    setOrientation(horizontal ? Horizontal : Vertical, true);
}

void ModernToolbar::setOrientation(Orientation o, bool animate) {
    if (m_orientation == o) return;
    m_orientation = o;

    QSize currentS = size();
    QSize targetS(currentS.height(), currentS.width());

    if (animate) {
        QPropertyAnimation* anim = new QPropertyAnimation(this, "size");
        anim->setDuration(250);
        anim->setStartValue(currentS);
        anim->setEndValue(targetS);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::valueChanged, this, [this](const QVariant&){ updateLayout(); });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        resize(targetS);
        updateLayout();
    }
}

int ModernToolbar::calculateMinLength() {
    int btnS = 40 * m_scale; if (!m_buttons.isEmpty()) btnS = m_buttons[0]->width();
    int dragH = 50; int handleH = 30; int minGap = 5; int numButtons = m_buttons.size();
    return dragH + (numButtons * btnS) + ((numButtons+1) * minGap) + handleH;
}

void ModernToolbar::updateLayout(bool animate) {
    if (m_style == Normal) {
        int w = width(); int h = height();
        int len = (m_orientation == Vertical) ? h : w;
        int breadth = qMin(w, h);
        int btnS = breadth - 14;
        if (btnS > 48) btnS = 48; if (btnS < 20) btnS = 20;

        for(auto* b : m_buttons) b->setBtnSize(btnS);

        int dragSize = 50; int handleSize = 30;
        int available = len - dragSize - handleSize;
        int content = 5 * btnS;
        int gap = 8;
        if (available > content) { gap = (available - content) / 6; if (gap > 25) gap = 25; }
        if (gap < 5) gap = 5;

        int currentPos = dragSize;
        auto place = [&](ToolbarBtn* b) {
            int bx, by;
            if (m_orientation == Vertical) { bx = (w - btnS) / 2; by = currentPos; currentPos += btnS + gap; }
            else { bx = currentPos; by = (h - btnS) / 2; currentPos += btnS + gap; }

            if(animate) {
                QPropertyAnimation* anim = new QPropertyAnimation(b, "pos");
                anim->setDuration(200); anim->setEndValue(QPoint(bx, by));
                anim->setEasingCurve(QEasingCurve::OutQuad);
                anim->start(QAbstractAnimation::DeleteWhenStopped);
            } else {
                b->move(bx, by);
            }
        };
        place(btnUndo); place(btnRedo); currentPos += 5; place(btnPen); place(btnEraser); place(btnLasso);
        for(auto b : m_buttons) b->show();
    } else {
        int cx = width() / 2; int cy = height() / 2; int btnS = 40 * m_scale;

        if (m_radialType == HalfEdge) {
            int paintCx = m_isDockedLeft ? 0 : width();
            int r = 70 * m_scale;
            double baseAngle = m_isDockedLeft ? 0.0 : 180.0;
            double spacing = 35.0;

            for (int i = 0; i < m_buttons.size(); ++i) {
                int relativeIdx = i - 2;
                double angleDeg = baseAngle + (relativeIdx * spacing) + m_scrollAngle;
                double angleRad = angleDeg * 3.14159 / 180.0;
                int bx = paintCx + r * std::cos(angleRad) - btnS/2;
                int by = cy + r * std::sin(angleRad) - btnS/2;
                m_buttons[i]->move(bx, by);

                bool visible = false;
                if (m_isDockedLeft) visible = std::cos(angleRad) > 0.05;
                else visible = std::cos(angleRad) < -0.05;
                m_buttons[i]->setVisible(visible);
            }
        } else {
            // Full Circle Layout (Kreuz-Anordnung)
            int r = 65 * m_scale;
            int centerBtnX = (width() - btnS) / 2;
            int centerBtnY = (height() - btnS) / 2;

            auto moveBtn = [&](ToolbarBtn* b, int bx, int by) {
                if(animate) {
                    QPropertyAnimation* anim = new QPropertyAnimation(b, "pos");
                    anim->setDuration(200);
                    anim->setEndValue(QPoint(bx, by));
                    anim->setEasingCurve(QEasingCurve::OutBack);
                    anim->start(QAbstractAnimation::DeleteWhenStopped);
                } else { b->move(bx, by); }
            };

            // Mitte (Aktives Tool), Oben, Rechts, Unten, Links
            moveBtn(m_buttons[2], centerBtnX, centerBtnY);
            moveBtn(m_buttons[1], centerBtnX, centerBtnY - r);
            moveBtn(m_buttons[3], centerBtnX + r, centerBtnY);
            moveBtn(m_buttons[0], centerBtnX, centerBtnY + r);
            moveBtn(m_buttons[4], centerBtnX - r, centerBtnY);

            for(auto b : m_buttons) b->show();
        }
    }
}

void ModernToolbar::resizeEvent(QResizeEvent *) { updateLayout(); }

void ModernToolbar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int cx = width() / 2;
    int cy = height() / 2;

    if (m_style == Normal) {
        int w = width(); int h = height();
        p.setBrush(QColor(37,37,38,240)); p.setPen(QPen(QColor(80,80,80), 1)); p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 15, 15);
        p.setBrush(QColor(150,150,150)); p.setPen(Qt::NoPen);
        if (m_orientation == Vertical) {
            p.drawRoundedRect(w/2 - 12, 25, 24, 4, 2, 2); int knobSize = 25; p.setBrush(QColor(50,50,50, 255)); p.setPen(QPen(QColor(80,80,80), 1)); p.drawEllipse(w - knobSize + 5, h - knobSize + 5, knobSize, knobSize); p.setPen(QPen(Qt::white, 2)); int kx = w - knobSize + 5; int ky = h - knobSize + 5; p.drawLine(kx+8, ky+8, kx+16, ky+16);
        } else {
            p.drawRoundedRect(25, h/2 - 12, 4, 24, 2, 2); int knobSize = 25; p.setBrush(QColor(50,50,50, 255)); p.setPen(QPen(QColor(80,80,80), 1)); p.drawEllipse(w - knobSize + 5, h - knobSize + 5, knobSize, knobSize); p.setPen(QPen(Qt::white, 2)); int kx = w - knobSize + 5; int ky = h - knobSize + 5; p.drawLine(kx+8, ky+8, kx+16, ky+16);
        }
    } else {
        // RADIAL STYLE
        p.setBrush(QColor(37,37,38,230));
        p.setPen(QPen(QColor(0x5E5CE6), 2));

        int rMain = 95 * m_scale;
        int rRing1 = 135 * m_scale;
        int rRing2 = 175 * m_scale;

        double startAngle = 0;
        double spanAngle = 360;
        int paintCx = cx;

        if (m_radialType == HalfEdge) {
            paintCx = m_isDockedLeft ? 0 : width();
            startAngle = m_isDockedLeft ? -90 : 90;
            spanAngle = 180;
            QRect box(paintCx - rMain, cy - rMain, rMain*2, rMain*2);
            p.drawPie(box, startAngle * 16, spanAngle * 16);
        } else {
            // Full Circle: Zeichnet den inneren Hintergrundkreis
            p.drawEllipse(QPoint(cx, cy), rMain, rMain);
            p.setBrush(QColor(30,30,30,255));
            p.setPen(Qt::NoPen);
            int hole = 45 * m_scale;
            p.drawEllipse(QPoint(cx, cy), hole, hole);
        }

        // Wenn Settings offen sind, Ringe zeichnen
        if (m_settingsState != SettingsState::Closed) {
            int pCx = (m_radialType == HalfEdge) ? paintCx : cx;
            double sA = (m_radialType == HalfEdge) ? startAngle : 0;
            double spA = (m_radialType == HalfEdge) ? 180 : 360;

            // Ring 1 (Auswahl: Farbe vs Größe)
            paintRadialRing1(p, pCx, cy, rMain, rRing1, sA, spA);

            // Ring 2 (Detaileinstellungen)
            if (m_settingsState == SettingsState::ColorSelect ||
                m_settingsState == SettingsState::SizeSelect ||
                m_settingsState == SettingsState::EraserMode ||
                m_settingsState == SettingsState::LassoMode)
            {
                paintRadialRing2(p, pCx, cy, rRing1, rRing2, sA, spA);
            }
        }
    }
}

void ModernToolbar::paintRadialRing1(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle) {
    p.setPen(Qt::NoPen);

    // Farben definieren: Je nach Modus hervorheben
    QColor col1 = (m_settingsState == SettingsState::ColorSelect || m_settingsState == SettingsState::EraserMode) ? QColor(0x5E5CE6) : QColor(60,60,60);
    QColor col2 = (m_settingsState == SettingsState::SizeSelect || m_settingsState == SettingsState::LassoMode) ? QColor(0x5E5CE6) : QColor(60,60,60);

    double midAngle = startAngle + spanAngle / 2.0;

    // Segment 1 (z.B. Oben bei FullCircle, Unten bei HalfEdge Links) -> Farbe
    QPainterPath path1;
    path1.arcMoveTo(cx - rOut, cy - rOut, rOut*2, rOut*2, startAngle);
    path1.arcTo(cx - rOut, cy - rOut, rOut*2, rOut*2, startAngle, spanAngle/2);
    path1.arcTo(cx - rIn, cy - rIn, rIn*2, rIn*2, midAngle, -spanAngle/2);
    path1.closeSubpath();
    p.setBrush(col1);
    p.drawPath(path1);

    // Icon (3 Punkte für Farbe)
    double a1 = startAngle + spanAngle/4.0;
    double rad1 = -a1 * 3.14159 / 180.0;
    double rMid = (rIn + rOut) / 2.0;
    int sx1 = cx + rMid * std::cos(rad1);
    int sy1 = cy + rMid * std::sin(rad1);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(sx1, sy1), 4, 4);
    p.drawEllipse(QPointF(sx1-6, sy1+4), 3, 3);
    p.drawEllipse(QPointF(sx1+6, sy1+4), 3, 3);

    // Segment 2 (z.B. Unten bei FullCircle) -> Größe
    QPainterPath path2;
    path2.arcMoveTo(cx - rOut, cy - rOut, rOut*2, rOut*2, midAngle);
    path2.arcTo(cx - rOut, cy - rOut, rOut*2, rOut*2, midAngle, spanAngle/2);
    path2.arcTo(cx - rIn, cy - rIn, rIn*2, rIn*2, startAngle + spanAngle, -spanAngle/2);
    path2.closeSubpath();
    p.setBrush(col2);
    p.drawPath(path2);

    // Icon (1 großer Punkt für Größe)
    double a2 = midAngle + spanAngle/4.0;
    double rad2 = -a2 * 3.14159 / 180.0;
    int sx2 = cx + rMid * std::cos(rad2);
    int sy2 = cy + rMid * std::sin(rad2);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(sx2, sy2), 6, 6);
}

void ModernToolbar::paintRadialRing2(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle) {
    p.setBrush(QColor(40,40,40));
    p.setPen(Qt::NoPen);

    // Hintergrund Ring 2
    QPainterPath bg;
    bg.arcMoveTo(cx - rOut, cy - rOut, rOut*2, rOut*2, startAngle);
    bg.arcTo(cx - rOut, cy - rOut, rOut*2, rOut*2, startAngle, spanAngle);
    bg.arcTo(cx - rIn, cy - rIn, rIn*2, rIn*2, startAngle+spanAngle, -spanAngle);
    bg.closeSubpath();
    p.drawPath(bg);

    if (m_settingsState == SettingsState::ColorSelect) {
        int count = m_customColors.size();
        double step = spanAngle / (count + 1);
        double rCircle = (rIn + rOut) / 2.0;

        for (int i = 0; i < count; ++i) {
            QColor c = m_customColors[i];
            double a = startAngle + (i + 1) * step;
            double rad = -a * 3.14159 / 180.0;
            int px = cx + rCircle * std::cos(rad);
            int py = cy + rCircle * std::sin(rad);

            p.setPen(QPen(Qt::gray, 1));
            p.setBrush(c);
            int size = 10 * m_scale;
            if (m_config.penColor == c) {
                p.setPen(QPen(Qt::white, 2));
                size = 13 * m_scale;
            }
            p.drawEllipse(QPointF(px, py), size, size);
        }
    } else if (m_settingsState == SettingsState::SizeSelect) {
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        int count = 5;
        double step = spanAngle / (count + 1);
        double rCircle = (rIn + rOut) / 2.0;

        for(int i=1; i<=count; ++i) {
            double a = startAngle + i * step;
            double rad = -a * 3.14159 / 180.0;
            int px = cx + rCircle * std::cos(rad);
            int py = cy + rCircle * std::sin(rad);
            int size = (i * 2 + 2) * m_scale;
            p.drawEllipse(QPointF(px, py), size, size);
        }
    }
}

ToolbarBtn* ModernToolbar::getRadialButtonAt(const QPoint& pos) {
    for(auto* b : m_buttons) {
        if(b->isVisible() && b->geometry().contains(pos)) {
            return b;
        }
    }
    return nullptr;
}

void ModernToolbar::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        // 1. ZUERST prüfen, ob wir einen Button treffen.
        // Das ist wichtig im Radial-Modus, da die Buttons Maus-transparent sind.
        if (m_style == Radial) {
            if (ToolbarBtn* b = getRadialButtonAt(e->pos())) {
                m_pressedButton = b;
                return; // Kein Drag starten, Button hat Vorrang!
            }
        }

        // 2. Radial Logik (Settings Ringe / Scrollen)
        if (m_style == Radial) {
            int cx = (m_radialType == HalfEdge) ? (m_isDockedLeft ? 0 : width()) : width()/2;
            int cy = height()/2;
            int dx = e->pos().x() - cx;
            int dy = e->pos().y() - cy;
            double dist = std::sqrt(dx*dx + dy*dy);

            // Scroll Erkennung (nur HalfEdge)
            int rMain = 95 * m_scale;
            if (dist > 50 * m_scale && dist < rMain && m_radialType == HalfEdge) {
                m_isScrolling = true;
                m_hasScrolled = false;
                m_dragStartAngle = std::atan2(-dy, dx) * 180.0 / 3.14159;
                m_scrollStartAngleVal = m_scrollAngle;
                return;
            }

            // Klick auf Ringe
            if (m_settingsState != SettingsState::Closed) {
                int r1_out = 135 * m_scale;
                int r2_out = 175 * m_scale;

                // Ring 1 (Kategorie)
                if (dist > rMain && dist < r1_out) {
                    handleRadialSettingsClick(e->pos(), cx, cy, rMain, r1_out);
                    return;
                }

                // Ring 2 (Werte)
                if (dist > r1_out && dist < r2_out && m_settingsState != SettingsState::Main) {
                    handleRadialSettingsClick(e->pos(), cx, cy, r1_out, r2_out);
                    return;
                }
            }
        }

        // 3. Resizing (Normal Style)
        if (m_isResizing) return;
        if (m_style == Normal && !m_isPreview) {
            int handleSize = 30; if (e->pos().x() > width() - handleSize && e->pos().y() > height() - handleSize) { m_isResizing = true; dragStartPos_ = e->pos(); resizeStartPos_ = e->pos(); startSize_ = size(); return; }
        }

        // 4. Dragging (Verschieben)
        int dragR = 45 * m_scale;
        int cx = width()/2;
        int cy = height()/2;
        if (m_style == Radial && m_radialType == HalfEdge) cx = m_isDockedLeft ? 0 : width();
        int dx = e->pos().x() - cx;
        int dy = e->pos().y() - cy;
        double dist = std::sqrt(dx*dx + dy*dy);

        // Im Radial-Modus nur Draggen wenn in der Mitte (aber kein Button getroffen wurde, s.o.)
        if (!m_isPreview && m_style == Radial && dist < dragR) { m_isDragging = true; dragStartPos_ = e->pos(); return; }
        if (!m_isPreview && m_style == Normal) {
            if (m_orientation == Vertical && e->pos().y() < 50) { m_isDragging = true; dragStartPos_ = e->pos(); return; }
            if (m_orientation == Horizontal && e->pos().x() < 50) { m_isDragging = true; dragStartPos_ = e->pos(); return; }
        }
    }
}

void ModernToolbar::handleRadialSettingsClick(const QPoint& pos, int cx, int cy, int, int) {
    double angle = std::atan2(-(pos.y() - cy), (pos.x() - cx)) * 180.0 / 3.14159;
    if (angle < 0) angle += 360.0; // Normalisierung auf 0-360

    int dx = pos.x() - cx;
    int dy = pos.y() - cy;
    double dist = std::sqrt(dx*dx + dy*dy);

    bool isRing2 = (dist > 135 * m_scale);

    double startA = 0;
    double spanA = 360;
    if (m_radialType == HalfEdge) {
        startA = m_isDockedLeft ? -90 : 90;
        spanA = 180;
    }

    double currentA = angle;
    if (m_radialType == HalfEdge && m_isDockedLeft && currentA > 270) currentA -= 360;

    double relA = currentA - startA;
    while(relA < 0) relA += 360;

    if (m_radialType == HalfEdge && (relA < 0 || relA > spanA)) return;

    // --- Logik für Ring 1 (Umschalten zwischen Kategorien) ---
    if (!isRing2) {
        double midLimit = spanA / 2.0;

        // FullCircle: 0-180 (Oben) -> Farbe, 180-360 (Unten) -> Größe
        if (relA < midLimit) {
            if (m_settingsState == SettingsState::ColorSelect) m_settingsState = SettingsState::SizeSelect;
            else m_settingsState = SettingsState::ColorSelect;
        } else {
            if (m_settingsState == SettingsState::SizeSelect) m_settingsState = SettingsState::Closed;
            else m_settingsState = SettingsState::SizeSelect;
        }
        update();
    }
    // --- Logik für Ring 2 (Wert setzen) ---
    else {
        if (m_settingsState == SettingsState::ColorSelect) {
            int count = m_customColors.size();
            double step = spanA / (count + 1);
            int idx = (int)(relA / step) - 1;

            if (idx >= 0 && idx < count) {
                QColor c = m_customColors[idx];
                if (m_config.penColor == c) {
                    QColor newC = QColorDialog::getColor(c, this, "Farbe ändern");
                    if (newC.isValid()) {
                        m_customColors[idx] = newC;
                        m_config.penColor = newC;
                        emit penConfigChanged(newC, m_config.penWidth);
                    }
                } else {
                    m_config.penColor = c;
                    emit penConfigChanged(c, m_config.penWidth);
                }
                update();
            }
        } else if (m_settingsState == SettingsState::SizeSelect) {
            int count = 5;
            double step = spanA / (count + 1);
            int idx = (int)(relA / step) - 1;

            if (idx >= 0 && idx < count) {
                m_config.penWidth = (idx + 1) * 3 + 2;
                emit penConfigChanged(m_config.penColor, m_config.penWidth);
                update();
            }
        }
    }
}

void ModernToolbar::mouseMoveEvent(QMouseEvent *e) {
    // Falls Button gedrückt wird, kein Move erlauben
    if (m_pressedButton) return;

    if (m_style == Radial && m_isScrolling && m_radialType == HalfEdge) {
        int cx = m_isDockedLeft ? 0 : width();
        int cy = height()/2;
        int dx = e->pos().x() - cx;
        int dy = e->pos().y() - cy;
        double currentAngle = std::atan2(-dy, dx) * 180.0 / 3.14159;
        double delta = currentAngle - m_dragStartAngle;
        if (delta > 180) delta -= 360;
        if (delta < -180) delta += 360;

        m_scrollAngle = m_scrollStartAngleVal - delta;
        if (m_scrollAngle > 140) m_scrollAngle = 140;
        if (m_scrollAngle < -140) m_scrollAngle = -140;

        if (std::abs(delta) > 2.0) m_hasScrolled = true;
        updateLayout();
        return;
    }

    if (m_isResizing && m_style == Normal) {
        QPoint delta = e->pos() - resizeStartPos_; int newW = startSize_.width() + delta.x(); int newH = startSize_.height() + delta.y(); int minL = calculateMinLength();
        if (m_orientation == Vertical) { if (newH < minL) newH = minL; if (newW < 50) newW = 50; if(newW > 100) newW = 100; } else { if (newW < minL) newW = minL; if (newH < 50) newH = 50; if(newH > 100) newH = 100; }
        resize(newW, newH); constrainToParent(); return;
    }
    if (m_isDragging && (e->buttons() & Qt::LeftButton) && !m_isPreview) {
        if (!parentWidget()) return;

        QPoint newPos = mapToParent(e->pos()) - dragStartPos_;
        int maxY = parentWidget()->height() - height();
        int maxX = parentWidget()->width() - width();

        if (m_style == Radial && m_radialType == HalfEdge) {
            int limitedX = qBound(-width()/2, newPos.x(), parentWidget()->width() - width()/2);
            move(limitedX, qBound(0, newPos.y(), maxY));
            bool newIsLeft = (x() + width()/2 < parentWidget()->width() / 2);
            if (newIsLeft != m_isDockedLeft) { m_isDockedLeft = newIsLeft; updateLayout(); update(); }
        } else {
            move(qBound(0, newPos.x(), maxX), qBound(0, newPos.y(), maxY));
            if (m_style == Normal) checkOrientation(e->globalPosition().toPoint());
        }
    }
    if (m_style == Normal && !m_isPreview) { if (e->pos().x() > width() - 30 && e->pos().y() > height() - 30) setCursor(Qt::SizeFDiagCursor); else setCursor(Qt::ArrowCursor); }
}

void ModernToolbar::mouseReleaseEvent(QMouseEvent *e) {
    // 1. Button Release Logik (Entscheidend für Klicks!)
    if (m_pressedButton) {
        // Nur klicken, wenn man auch über dem Button loslässt
        if (getRadialButtonAt(e->pos()) == m_pressedButton) {
            m_pressedButton->triggerClick();
        }
        m_pressedButton = nullptr;
        return;
    }

    // 2. Scroll Logic
    if (m_style == Radial && m_isScrolling) {
        m_isScrolling = false;
        // Falls kaum gescrollt wurde, evtl. Klick (Fallback)
        if (!m_hasScrolled) {
            if (ToolbarBtn* b = getRadialButtonAt(e->pos())) {
                b->triggerClick();
            }
        }
        return;
    }

    m_isDragging = false;
    m_isResizing = false;
    if (m_style == Radial && m_radialType == HalfEdge) { snapToEdge(); } else { constrainToParent(); }
}

void ModernToolbar::leaveEvent(QEvent *) { setCursor(Qt::ArrowCursor); }

void ModernToolbar::showVerticalPopup() {
    QDialog* popup = new QDialog(this->window()); popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint); popup->setAttribute(Qt::WA_TranslucentBackground);
    popup->setStyleSheet("background-color: #2D2D30; border: 1px solid #555; border-radius: 8px; color: white;");
    QVBoxLayout* lay = new QVBoxLayout(popup); lay->setContentsMargins(10,10,10,10);
    if (mode_ == ToolMode::Pen) {
        lay->addWidget(new QLabel("Stift")); QSlider* sl = new QSlider(Qt::Horizontal); sl->setRange(1, 20); sl->setValue(m_config.penWidth);
        connect(sl, &QSlider::valueChanged, [this](int v){ m_config.penWidth = v; emit penConfigChanged(m_config.penColor, m_config.penWidth); }); lay->addWidget(sl);
        QHBoxLayout* colors = new QHBoxLayout; for(auto c : m_customColors) { QPushButton* b = new QPushButton; b->setFixedSize(24,24); b->setStyleSheet(QString("background-color: %1; border-radius: 12px; border: 1px solid #555;").arg(c.name())); connect(b, &QPushButton::clicked, [this, c, popup](){ m_config.penColor = c; emit penConfigChanged(m_config.penColor, m_config.penWidth); popup->close(); }); colors->addWidget(b); } lay->addLayout(colors);
    }
    QPoint globalPos = mapToGlobal(QPoint(width(), 0)); popup->move(globalPos.x() + 5, globalPos.y()); popup->show(); connect(popup, &QDialog::finished, [this](){ m_settingsState = SettingsState::Closed; update(); });
}
