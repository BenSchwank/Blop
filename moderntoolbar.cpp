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
#include <QDebug>

// --- Helper Button ---
ToolbarBtn::ToolbarBtn(const QString& name, QWidget* parent) : QWidget(parent), m_iconName(name) {
    setBtnSize(40);
    setCursor(Qt::PointingHandCursor);
}
void ToolbarBtn::setBtnSize(int s) {
    m_size = s;
    setFixedSize(s, s);
    update();
}
void ToolbarBtn::setIcon(const QString& name) { m_iconName = name; update(); }
void ToolbarBtn::setActive(bool active) { m_active = active; update(); }
void ToolbarBtn::mousePressEvent(QMouseEvent* e) { if(e->button()==Qt::LeftButton) emit clicked(); }
void ToolbarBtn::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void ToolbarBtn::leaveEvent(QEvent*) { m_hover = false; update(); }
void ToolbarBtn::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    int r = 8;
    if (m_active) { p.setBrush(QColor(0x5E5CE6)); p.setPen(Qt::NoPen); p.drawRoundedRect(rect().adjusted(2,2,-2,-2), r, r); }
    else if (m_hover) { p.setBrush(QColor(255,255,255,30)); p.setPen(Qt::NoPen); p.drawRoundedRect(rect().adjusted(2,2,-2,-2), r, r); }

    p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);

    int w = width(); int h = height();
    p.translate(w/2, h/2);
    double scale = (double)w / 90.0;
    p.scale(scale, scale);
    p.translate(-32, -32);

    if(m_iconName=="pen") { QPainterPath pa; pa.moveTo(12,52); pa.lineTo(22,52); pa.lineTo(50,24); pa.lineTo(40,14); pa.lineTo(12,42); pa.closeSubpath(); p.drawPath(pa); }
    else if(m_iconName=="eraser") { p.drawRoundedRect(15,15,34,34,5,5); p.drawLine(25,25,39,39); p.drawLine(39,25,25,39); }
    else if(m_iconName=="lasso") { p.drawEllipse(15,15,34,34); p.drawLine(45,45,55,55); }
    else if(m_iconName=="undo") { QPainterPath pa; pa.moveTo(40,20); pa.arcTo(20,20,30,30,90,180); p.drawPath(pa); p.drawLine(20,35,10,25); p.drawLine(20,15,10,25); }
    else if(m_iconName=="redo") { QPainterPath pa; pa.moveTo(24,20); pa.arcTo(14,20,30,30,90,-180); p.drawPath(pa); p.drawLine(44,35,54,25); p.drawLine(44,15,54,25); }
}

// --- ModernToolbar ---

ModernToolbar::ModernToolbar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint);

    if (parent) parent->installEventFilter(this);

    btnPen = new ToolbarBtn("pen", this);
    btnEraser = new ToolbarBtn("eraser", this);
    btnLasso = new ToolbarBtn("lasso", this);
    btnUndo = new ToolbarBtn("undo", this);
    btnRedo = new ToolbarBtn("redo", this);

    m_buttons = {btnUndo, btnEraser, btnPen, btnLasso, btnRedo};
    m_customColors = {Qt::black, Qt::white, Qt::red, Qt::blue, Qt::green, Qt::yellow};

    auto handleToolClick = [this](ToolMode m) {
        if(mode_ == m) {
            if (m_settingsState == SettingsState::Closed) {
                m_settingsState = SettingsState::Main;
                if (m_style == Vertical) showVerticalPopup();
                else update();
            } else {
                m_settingsState = SettingsState::Closed;
                update();
            }
        } else {
            setToolMode(m);
            emit toolChanged(m);
        }
    };

    connect(btnPen, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Pen); });
    connect(btnEraser, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Eraser); });
    connect(btnLasso, &ToolbarBtn::clicked, [=](){ handleToolClick(ToolMode::Lasso); });
    connect(btnUndo, &ToolbarBtn::clicked, this, &ModernToolbar::undoRequested);
    connect(btnRedo, &ToolbarBtn::clicked, this, &ModernToolbar::redoRequested);

    setStyle(Vertical);
    setToolMode(ToolMode::Pen);
}

bool ModernToolbar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        constrainToParent();
    }
    return QWidget::eventFilter(watched, event);
}

void ModernToolbar::setTopBound(int top) {
    m_topBound = top;
    constrainToParent(); // Sofort anwenden falls wir drüber sind
}

void ModernToolbar::setScale(qreal s) {
    if (s < 0.5) s = 0.5;
    if (s > 2.0) s = 2.0;
    m_scale = s;
    int btnSize = 40 * m_scale;
    for(auto* b : m_buttons) b->setBtnSize(btnSize);
    setStyle(m_style);
    constrainToParent();
}

void ModernToolbar::constrainToParent() {
    if (!parentWidget()) return;
    QRect pRect = parentWidget()->rect();
    int maxX = pRect.width() - width();
    int maxY = pRect.height() - height();
    int newX = x();
    int newY = y();

    // Y Limit mit m_topBound
    if (newY < m_topBound) newY = m_topBound;
    if (newY > maxY) newY = maxY;

    if (m_style == Radial && m_radialType == HalfEdge) {
        move(x(), newY);
        snapToEdge();
    } else {
        if (newX < 0) newX = 0;
        if (newX > maxX) newX = maxX;
        move(newX, newY);
    }
}

void ModernToolbar::setToolMode(ToolMode mode) {
    mode_ = mode;
    m_settingsState = SettingsState::Closed;
    btnPen->setActive(mode == ToolMode::Pen);
    btnEraser->setActive(mode == ToolMode::Eraser);
    btnLasso->setActive(mode == ToolMode::Lasso);
    update();
}

void ModernToolbar::setStyle(Style style) {
    m_style = style;
    m_settingsState = SettingsState::Closed;
    if (m_style == Vertical) {
        // Höhe = Buttons (260) + Handle (30)
        setFixedSize(60 * m_scale, 290 * m_scale);
    } else {
        setFixedSize(460 * m_scale, 460 * m_scale);
    }
    for(auto b : m_buttons) b->show();
    updateLayout();
    update();
}

void ModernToolbar::setRadialType(RadialType type) {
    m_radialType = type;
    m_scrollAngle = 0.0;
    if (m_style == Radial) {
        if (m_radialType == HalfEdge) snapToEdge();
        else move(qBound(0, x(), parentWidget()->width()-width()), y());
        updateLayout();
        update();
    }
}

void ModernToolbar::snapToEdge() {
    if (!parentWidget()) return;
    int parentW = parentWidget()->width();
    int mid = parentW / 2;

    if (x() + width()/2 < mid) {
        move(-width()/2, y());
        m_isDockedLeft = true;
    } else {
        move(parentW - width()/2, y());
        m_isDockedLeft = false;
    }
    updateLayout();
}

void ModernToolbar::wheelEvent(QWheelEvent *e) {
    if (m_style == Radial && m_radialType == HalfEdge) {
        double delta = e->angleDelta().y() / 8.0;
        m_scrollAngle += delta;
        if (m_scrollAngle > 150) m_scrollAngle = 150;
        if (m_scrollAngle < -150) m_scrollAngle = -150;
        updateLayout();
        e->accept();
    } else {
        QWidget::wheelEvent(e);
    }
}

void ModernToolbar::updateLayout() {
    int btnS = 40 * m_scale;

    if (m_style == Vertical) {
        int x = (width() - btnS) / 2;
        // Mehr Platz oben für Drag (20px -> 30px)
        int y = 30 * m_scale;
        int gap = 48 * m_scale;

        btnUndo->move(x, y); y += gap;
        btnRedo->move(x, y); y += gap;
        y += 5 * m_scale;
        btnPen->move(x, y); y += gap;
        btnEraser->move(x, y); y += gap;
        btnLasso->move(x, y);
        for(auto b : m_buttons) b->show();

    } else {
        int cx = width() / 2;
        int cy = height() / 2;

        if (m_radialType == HalfEdge) {
            int r = 70 * m_scale;
            double baseAngle = m_isDockedLeft ? 0.0 : 180.0;
            double spacing = 35.0;
            for (int i = 0; i < m_buttons.size(); ++i) {
                int relativeIdx = i - 2;
                double angleDeg = baseAngle + (relativeIdx * spacing) + m_scrollAngle;
                double angleRad = angleDeg * 3.14159 / 180.0;
                int localCx = m_isDockedLeft ? 0 : width();
                int bx = localCx + r * std::cos(angleRad) - btnS/2;
                int by = cy + r * std::sin(angleRad) - btnS/2;
                m_buttons[i]->move(bx, by);
                bool visible = false;
                if (m_isDockedLeft) visible = std::cos(angleRad) > 0.05;
                else visible = std::cos(angleRad) < -0.05;
                m_buttons[i]->setVisible(visible);
            }
        } else {
            int r = 65 * m_scale;
            btnPen->move(cx - btnS/2, cy - btnS/2);
            btnEraser->move(cx - btnS/2, cy - r - btnS/2);
            btnLasso->move(cx + r - btnS/2, cy - btnS/2);
            btnUndo->move(cx - btnS/2, cy + r - btnS/2);
            btnRedo->move(cx - r - btnS/2, cy - btnS/2);
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

    if (m_style == Vertical) {
        // Main Body (lässt unten Platz für Handle)
        int handleH = 30 * m_scale;
        QRect mainRect(0, 0, width(), height() - handleH);

        p.setBrush(QColor(37,37,38,240));
        p.setPen(QPen(QColor(80,80,80), 1));
        p.drawRoundedRect(mainRect.adjusted(1,1,-1,-1), 15*m_scale, 15*m_scale);

        // Drag Area oben (Vergrößert)
        p.setBrush(QColor(150,150,150)); p.setPen(Qt::NoPen);
        p.drawEllipse(width()/2 - 3, (int)(15*m_scale), 6, 6);

        // Resize Handle unten rechts (Außerhalb des Main Body, aber im Widget Rect)
        // Zeichne einen kleinen Griff-Bereich
        int ry = height();
        int rx = width();

        QPainterPath handlePath;
        handlePath.moveTo(rx, ry);
        handlePath.lineTo(rx - 20*m_scale, ry);
        handlePath.lineTo(rx, ry - 20*m_scale);
        handlePath.closeSubpath();

        p.setBrush(QColor(80,80,80, 200));
        p.drawPath(handlePath);

        // Linien für Grip
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(rx - 5*m_scale, ry - 5*m_scale, rx - 10*m_scale, ry - 10*m_scale);

    } else {
        // Radial
        p.setBrush(QColor(37,37,38,230));
        p.setPen(QPen(QColor(0x5E5CE6), 2));

        int rMain = 95 * m_scale;
        int rRing1 = 135 * m_scale;
        int rRing2 = 175 * m_scale;

        int paintCx = cx;
        double startAngle = 0; double spanAngle = 360;

        if (m_radialType == HalfEdge) {
            paintCx = m_isDockedLeft ? 0 : width();
            startAngle = m_isDockedLeft ? -90 : 90;
            spanAngle = 180;
            QRect box(paintCx - rMain, cy - rMain, rMain*2, rMain*2);
            p.drawPie(box, startAngle * 16, spanAngle * 16);
        } else {
            p.drawEllipse(QPoint(cx, cy), rMain, rMain);
            p.setBrush(QColor(30,30,30,255)); p.setPen(Qt::NoPen);
            int hole = 45 * m_scale;
            p.drawEllipse(QPoint(cx, cy), hole, hole);
        }

        if (m_settingsState != SettingsState::Closed) {
            paintRadialRing1(p, paintCx, cy, rMain, rRing1, startAngle, spanAngle);
        }

        if (m_settingsState == SettingsState::ColorSelect || m_settingsState == SettingsState::SizeSelect ||
            m_settingsState == SettingsState::EraserMode || m_settingsState == SettingsState::LassoMode) {
            paintRadialRing2(p, paintCx, cy, rRing1, rRing2, startAngle, spanAngle);
        }
    }
}

void ModernToolbar::paintRadialRing1(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle) {
    p.setPen(Qt::NoPen);
    QColor col1 = (m_settingsState == SettingsState::ColorSelect || m_settingsState == SettingsState::EraserMode) ? QColor(0x5E5CE6) : QColor(60,60,60);
    QColor col2 = (m_settingsState == SettingsState::SizeSelect || m_settingsState == SettingsState::LassoMode) ? QColor(0x5E5CE6) : QColor(60,60,60);
    double midAngle = startAngle + spanAngle / 2.0;

    QPainterPath path1;
    path1.arcMoveTo(cx - rOut, cy - rOut, rOut*2, rOut*2, startAngle);
    path1.arcTo(cx - rOut, cy - rOut, rOut*2, rOut*2, startAngle, spanAngle/2);
    path1.arcTo(cx - rIn, cy - rIn, rIn*2, rIn*2, midAngle, -spanAngle/2);
    path1.closeSubpath();
    p.setBrush(col1); p.drawPath(path1);

    double a1 = startAngle + spanAngle/4.0;
    double rad1 = -a1 * 3.14159 / 180.0;
    double rMid = (rIn + rOut) / 2.0;
    int sx1 = cx + rMid * std::cos(rad1); int sy1 = cy + rMid * std::sin(rad1);
    p.setBrush(Qt::white);
    p.drawEllipse(QPoint(sx1, sy1), 4, 4); p.drawEllipse(QPoint(sx1-6, sy1+4), 3, 3); p.drawEllipse(QPoint(sx1+6, sy1+4), 3, 3);

    QPainterPath path2;
    path2.arcMoveTo(cx - rOut, cy - rOut, rOut*2, rOut*2, midAngle);
    path2.arcTo(cx - rOut, cy - rOut, rOut*2, rOut*2, midAngle, spanAngle/2);
    path2.arcTo(cx - rIn, cy - rIn, rIn*2, rIn*2, startAngle + spanAngle, -spanAngle/2);
    path2.closeSubpath();
    p.setBrush(col2); p.drawPath(path2);

    double a2 = midAngle + spanAngle/4.0;
    double rad2 = -a2 * 3.14159 / 180.0;
    int sx2 = cx + rMid * std::cos(rad2); int sy2 = cy + rMid * std::sin(rad2);
    p.setBrush(Qt::white); p.drawEllipse(QPoint(sx2, sy2), 6, 6);
}

void ModernToolbar::paintRadialRing2(QPainter& p, int cx, int cy, int rIn, int rOut, double startAngle, double spanAngle) {
    p.setBrush(QColor(40,40,40)); p.setPen(Qt::NoPen);
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
            p.setPen(QPen(Qt::gray, 1)); p.setBrush(c);
            int size = 10 * m_scale;
            if (m_config.penColor == c) { p.setPen(QPen(Qt::white, 2)); size = 13 * m_scale; }
            p.drawEllipse(QPoint(px, py), size, size);
        }
    }
    else if (m_settingsState == SettingsState::SizeSelect) {
        p.setBrush(Qt::white); p.setPen(Qt::NoPen);
        int count = 5; double step = spanAngle / (count + 1);
        double rCircle = (rIn + rOut) / 2.0;
        for(int i=1; i<=count; ++i) {
            double a = startAngle + i * step; double rad = -a * 3.14159 / 180.0;
            int px = cx + rCircle * std::cos(rad); int py = cy + rCircle * std::sin(rad);
            int size = (i * 2 + 2) * m_scale; p.drawEllipse(QPoint(px, py), size, size);
        }
    }
}

void ModernToolbar::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        // 1. Check Resize Handle (Unten Rechts)
        if (m_style == Vertical) {
            int handleH = 30 * m_scale;
            if (e->pos().y() > height() - handleH) {
                m_isResizing = true;
                dragStartPos_ = e->pos();
                return;
            }
        }

        int cx = width()/2; int cy = height()/2;
        if (m_style == Radial && m_radialType == HalfEdge) { cx = m_isDockedLeft ? 0 : width(); }
        int dx = e->pos().x() - cx; int dy = e->pos().y() - cy;
        double dist = std::sqrt(dx*dx + dy*dy);

        int rMain = 95 * m_scale;
        int r1_out = 135 * m_scale;
        int r2_out = 175 * m_scale;
        int dragR = 45 * m_scale;

        if (dist < dragR && m_style == Radial) { m_isDragging = true; dragStartPos_ = e->pos(); return; }

        // Vertikal Drag oben
        if (m_style == Vertical && e->pos().y() < 60*m_scale) { m_isDragging = true; dragStartPos_ = e->pos(); return; }

        if (m_style == Radial && m_settingsState != SettingsState::Closed) {
            if (dist > rMain && dist < r1_out) { handleRadialSettingsClick(e->pos(), cx, cy, rMain, r1_out); return; }
            if (dist > r1_out && dist < r2_out && m_settingsState != SettingsState::Main) { handleRadialSettingsClick(e->pos(), cx, cy, r1_out, r2_out); return; }
        }
    }
}

void ModernToolbar::handleRadialSettingsClick(const QPoint& pos, int cx, int cy, int, int) {
    double angle = std::atan2(-(pos.y() - cy), (pos.x() - cx)) * 180.0 / 3.14159;
    if (angle < 0) angle += 360.0;
    int dx = pos.x() - cx; int dy = pos.y() - cy;
    double dist = std::sqrt(dx*dx + dy*dy);
    bool isRing2 = (dist > 135 * m_scale);

    if (!isRing2) {
        double startA = 0; if (m_radialType == HalfEdge) startA = m_isDockedLeft ? -90 : 90;
        double relA = angle - startA; while(relA < 0) relA += 360; while(relA >= 360) relA -= 360;
        double midLimit = (m_radialType == HalfEdge) ? 90 : 180;
        if (relA < midLimit) {
            if (m_settingsState == SettingsState::ColorSelect) m_settingsState = SettingsState::SizeSelect;
            else m_settingsState = SettingsState::ColorSelect;
        } else {
            if (m_settingsState == SettingsState::SizeSelect) m_settingsState = SettingsState::Closed;
            else m_settingsState = SettingsState::SizeSelect;
        }
        update();
    } else {
        double startA = 0; double spanA = 360;
        if (m_radialType == HalfEdge) { startA = m_isDockedLeft ? -90 : 90; spanA = 180; }
        double currentA = angle; if (m_radialType == HalfEdge && m_isDockedLeft && currentA > 270) currentA -= 360;
        double relA = currentA - startA; while(relA < 0) relA += 360;

        if (m_settingsState == SettingsState::ColorSelect) {
            int count = m_customColors.size(); double step = spanA / (count + 1);
            int idx = (int)(relA / step) - 1;
            if (idx >= 0 && idx < count) {
                QColor c = m_customColors[idx];
                if (m_config.penColor == c) {
                    QColor newC = QColorDialog::getColor(c, this, "Farbe ändern");
                    if (newC.isValid()) { m_customColors[idx] = newC; m_config.penColor = newC; emit penConfigChanged(newC, m_config.penWidth); }
                } else { m_config.penColor = c; emit penConfigChanged(c, m_config.penWidth); }
                update();
            }
        }
        else if (m_settingsState == SettingsState::SizeSelect) {
            int count = 5; double step = spanA / (count + 1); int idx = (int)(relA / step) - 1;
            if (idx >= 0 && idx < count) { m_config.penWidth = (idx + 1) * 3 + 2; emit penConfigChanged(m_config.penColor, m_config.penWidth); update(); }
        }
    }
}

void ModernToolbar::mouseMoveEvent(QMouseEvent *e) {
    if (m_isResizing && m_style == Vertical) {
        QPoint delta = e->pos() - dragStartPos_;
        qreal change = delta.y() * 0.01;
        setScale(m_scale + change);
        emit scaleChanged(m_scale * 100);
        dragStartPos_ = e->pos();
        return;
    }
    if (m_isDragging && (e->buttons() & Qt::LeftButton)) {
        if (!parentWidget()) return;
        QPoint newPos = mapToParent(e->pos()) - dragStartPos_;
        int maxY = parentWidget()->height() - height();
        if (m_style == Radial && m_radialType == HalfEdge) {
            int x = 0;
            if (newPos.x() + width()/2 < parentWidget()->width() / 2) x = -width()/2;
            else x = parentWidget()->width() - width()/2;
            move(x, qBound(0, newPos.y(), maxY));
            bool newIsLeft = (x < 0);
            if (newIsLeft != m_isDockedLeft) { m_isDockedLeft = newIsLeft; updateLayout(); update(); }
        } else {
            int maxX = parentWidget()->width() - width();
            move(qBound(0, newPos.x(), maxX), qBound(0, newPos.y(), maxY));
        }
    }
}

void ModernToolbar::mouseReleaseEvent(QMouseEvent *) {
    m_isDragging = false;
    m_isResizing = false;
}

void ModernToolbar::showVerticalPopup() {
    QDialog* popup = new QDialog(this->window());
    popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    popup->setAttribute(Qt::WA_TranslucentBackground);
    popup->setStyleSheet("background-color: #2D2D30; border: 1px solid #555; border-radius: 8px; color: white;");
    QVBoxLayout* lay = new QVBoxLayout(popup); lay->setContentsMargins(10,10,10,10);
    if (mode_ == ToolMode::Pen) {
        lay->addWidget(new QLabel("Stift-Einstellungen"));
        QSlider* sl = new QSlider(Qt::Horizontal); sl->setRange(1, 20); sl->setValue(m_config.penWidth);
        connect(sl, &QSlider::valueChanged, [this](int v){ m_config.penWidth = v; emit penConfigChanged(m_config.penColor, m_config.penWidth); });
        lay->addWidget(sl);
        QHBoxLayout* colors = new QHBoxLayout;
        for(auto c : m_customColors) {
            QPushButton* b = new QPushButton; b->setFixedSize(24,24);
            b->setStyleSheet(QString("background-color: %1; border-radius: 12px; border: 1px solid #555;").arg(c.name()));
            connect(b, &QPushButton::clicked, [this, c, popup](){ m_config.penColor = c; emit penConfigChanged(m_config.penColor, m_config.penWidth); popup->close(); });
            colors->addWidget(b);
        }
        lay->addLayout(colors);
    }
    QPoint globalPos = mapToGlobal(QPoint(width(), 0));
    popup->move(globalPos.x() + 5, globalPos.y());
    popup->show();
    connect(popup, &QDialog::finished, [this](){ m_settingsState = SettingsState::Closed; update(); });
}
