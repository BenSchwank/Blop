#include "mainwindow.h"
#include "settingsdialog.h"
#include "canvasview.h"
#include "UIStyles.h"
#include "moderntoolbar.h"

#include <QApplication>
#include <QStyle>
#include <QLabel>
#include <QStandardPaths>
#include <QDir>
#include <QHeaderView>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsBlurEffect>
#include <QMenu>
#include <QWidgetAction>
#include <QSlider>
#include <QMessageBox>
#include <QMouseEvent>
#include <QGridLayout>
#include <QButtonGroup>
#include <QTimer>
#include <QPropertyAnimation>
#include <QComboBox>
#include <utility>

// --- SidebarItemDelegate ---
SidebarItemDelegate::SidebarItemDelegate(MainWindow *parent)
    : QStyledItemDelegate(parent), m_window(parent) {}

QSize SidebarItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    return QSize(option.rect.width(), 36);
}

void SidebarItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    if (option.state & QStyle::State_Selected) {
        QColor accent = m_window->currentAccentColor();
        accent.setAlpha(40);
        painter->fillRect(option.rect, accent);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, QColor(255, 255, 255, 10));
    }

    int depth = 0;
    QModelIndex p = index.parent();
    while(p.isValid()) { depth++; p = p.parent(); }

    int indentStep = 3;
    int baseIndent = 5;
    int currentIndent = baseIndent + (depth * indentStep);

    const QFileSystemModel* model = qobject_cast<const QFileSystemModel*>(index.model());
    bool isDir = model ? model->isDir(index) : false;

    int arrowSize = 10;
    QRect arrowRect(option.rect.left() + currentIndent, option.rect.center().y() - arrowSize/2, arrowSize, arrowSize);

    if (isDir) {
        const QTreeView* tree = qobject_cast<const QTreeView*>(option.widget);
        bool isExpanded = tree ? tree->isExpanded(index) : false;

        painter->setBrush(QColor(200, 200, 200));
        painter->setPen(Qt::NoPen);

        QPainterPath path;
        if (isExpanded) {
            path.moveTo(arrowRect.left(), arrowRect.top() + 2);
            path.lineTo(arrowRect.right(), arrowRect.top() + 2);
            path.lineTo(arrowRect.center().x(), arrowRect.bottom() - 2);
        } else {
            path.moveTo(arrowRect.left() + 2, arrowRect.top());
            path.lineTo(arrowRect.left() + 2, arrowRect.bottom());
            path.lineTo(arrowRect.right() - 2, arrowRect.center().y());
        }
        path.closeSubpath();
        painter->drawPath(path);
    }

    int iconSize = 18;
    int iconX = arrowRect.right() + 6;
    QRect iconRect(iconX, option.rect.center().y() - iconSize/2, iconSize, iconSize);

    QColor iconColor = (option.state & QStyle::State_Selected) ? m_window->currentAccentColor() : QColor(180, 180, 180);

    QIcon standardIcon = index.data(Qt::DecorationRole).value<QIcon>();
    if (!standardIcon.isNull()) {
        standardIcon.paint(painter, iconRect, Qt::AlignCenter, (option.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
    } else {
        painter->setPen(QPen(iconColor, 1.5));
        painter->setBrush(Qt::NoBrush);
        if (isDir) {
            painter->drawRoundedRect(iconRect.adjusted(1,3,-1,-2), 2, 2);
        } else {
            painter->drawRoundedRect(iconRect.adjusted(3,1,-3,-1), 2, 2);
        }
    }

    QRect textRect = option.rect;
    textRect.setLeft(iconRect.right() + 8);
    textRect.setRight(option.rect.right() - 5);

    painter->setPen((option.state & QStyle::State_Selected) ? Qt::white : QColor(220, 220, 220));
    painter->setFont(m_window->font());

    QString text = index.data(Qt::DisplayRole).toString();
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));

    painter->restore();
}

bool SidebarItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);

        int depth = 0; QModelIndex p = index.parent(); while(p.isValid()) { depth++; p = p.parent(); }
        int indentStep = 3;
        int baseIndent = 5;
        int currentIndent = baseIndent + (depth * indentStep);

        QRect expandRect(option.rect.left(), option.rect.top(), currentIndent + 20, option.rect.height());

        QTreeView* tree = qobject_cast<QTreeView*>(const_cast<QWidget*>(option.widget));
        QFileSystemModel* fsModel = qobject_cast<QFileSystemModel*>(model);

        if (tree && fsModel && fsModel->isDir(index)) {
            if (expandRect.contains(me->pos())) {
                if (tree->isExpanded(index)) tree->collapse(index);
                else tree->expand(index);
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}


// --- ModernItemDelegate ---
ModernItemDelegate::ModernItemDelegate(MainWindow *parent)
    : QStyledItemDelegate(parent), m_window(parent) {}

QSize ModernItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(m_window->isTouchMode() ? 70 : 48);
    if (const FreeGridView *view = qobject_cast<const FreeGridView*>(option.widget)) {
        size.setWidth(view->itemSize().width());
    }
    return size;
}

void ModernItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect;
    rect.adjust(2, 2, -2, -2);

    QColor bgColor = QColor(0x1E2230);
    if (option.state & QStyle::State_Selected) bgColor = m_window->currentAccentColor().darker(150);
    else if (option.state & QStyle::State_MouseOver) bgColor = QColor(0x2A2E3F);

    painter->setBrush(bgColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(rect, 8, 8);

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (icon.isNull()) icon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);

    int iconSize = m_window->isTouchMode() ? 36 : 28;
    QRect iconRect(rect.left() + 10, rect.center().y() - iconSize/2, iconSize, iconSize);

    painter->setBrush(QColor(255, 255, 255, 10));
    painter->drawEllipse(iconRect.adjusted(-4,-4,4,4));
    icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);

    QRect textRect = rect;
    textRect.setLeft(iconRect.right() + 20);
    textRect.setRight(rect.right() - 40);

    painter->setPen(Qt::white);
    QFont f = painter->font();
    f.setBold(true);
    f.setPointSize(m_window->isTouchMode() ? 12 : 10);
    painter->setFont(f);

    QString text = index.data(Qt::DisplayRole).toString();
    QString elidedText = painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedText);

    QIcon menuIcon = m_window->createModernIcon("more_vert", Qt::gray);
    QRect menuRect(rect.right() - 35, rect.center().y() - 12, 24, 24);
    menuIcon.paint(painter, menuRect, Qt::AlignCenter);

    painter->restore();
}

bool ModernItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QRect rect = option.rect;
        int clickArea = 50;
        QRect menuRect(rect.right() - clickArea, rect.top(), clickArea, rect.height());
        if (menuRect.contains(me->pos())) {
            m_window->showContextMenu(QCursor::pos(), index);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// --- ModernButton ---
ModernButton::ModernButton(QWidget *parent) : QToolButton(parent), m_scale(1.0f), m_accentColor(Qt::white) {
    m_anim = new QPropertyAnimation(this, "scale", this); m_anim->setDuration(150); m_anim->setEasingCurve(QEasingCurve::OutQuad);
    setCursor(Qt::PointingHandCursor); setStyleSheet("border: none; background: transparent;");
    setIconSize(QSize(24, 24));
}
void ModernButton::setScale(float s) { m_scale = s; update(); }
void ModernButton::setAccentColor(QColor c) { m_accentColor = c; update(); }
void ModernButton::enterEvent(QEnterEvent *event) { m_anim->stop(); m_anim->setEndValue(1.2f); m_anim->start(); QToolButton::enterEvent(event); }
void ModernButton::leaveEvent(QEvent *event) { m_anim->stop(); m_anim->setEndValue(1.0f); m_anim->start(); QToolButton::leaveEvent(event); }
void ModernButton::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::SmoothPixmapTransform);
    QIcon ic = icon(); if (ic.isNull()) return; int w = width(); int h = height();
    int iconS = iconSize().width();
    p.translate(w / 2, h / 2); p.scale(m_scale, m_scale); p.translate(-iconS / 2, -iconS / 2);
    QIcon::Mode mode = isChecked() ? QIcon::Active : QIcon::Normal; ic.paint(&p, 0, 0, iconS, iconS, Qt::AlignCenter, mode);
}

// --- MainWindow ---
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_renameOverlay(nullptr) {
    // FRAMELESS WINDOW SETZEN
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    m_currentAccentColor = QColor(0x5E5CE6);
    m_penColor = Qt::black;
    m_penWidth = 3;
    m_activeToolType = CanvasView::ToolType::Pen;
    m_penOnlyMode = true;
    m_touchMode = false;
    m_lblActiveNote = nullptr;
    m_isSidebarOpen = true;

    m_currentItemSize = 180;
    m_currentSpacing = 20;

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(1500);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::performAutoSave);

    createDefaultFolder();
    setupUi();
    applyTheme();
    applyUiScaling();

    updateSidebarState();
    updateOverviewBackButton();
}
MainWindow::~MainWindow() {}

// NEU: Title Bar Setup
void MainWindow::setupTitleBar() {
    m_titleBarWidget = new QWidget(this);
    m_titleBarWidget->setFixedHeight(30);
    m_titleBarWidget->setStyleSheet("background-color: #0F111A; border-bottom: 1px solid #1F2335;");

    QHBoxLayout* layout = new QHBoxLayout(m_titleBarWidget);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(0);

    // Title Icon/Text
    QLabel* lblTitle = new QLabel("Blop", m_titleBarWidget);
    lblTitle->setStyleSheet("color: #888; font-weight: bold;");
    layout->addWidget(lblTitle);
    layout->addStretch();

    // Buttons Style
    QString btnStyle = "QPushButton { background: transparent; border: none; color: #BBB; font-family: 'Segoe MDL2 Assets'; font-size: 12px; } "
                       "QPushButton:hover { background: #252526; color: white; }";
    QString closeStyle = "QPushButton { background: transparent; border: none; color: #BBB; font-family: 'Segoe MDL2 Assets'; font-size: 12px; } "
                         "QPushButton:hover { background: #E81123; color: white; }";

    // Minimize
    m_btnWinMin = new QPushButton("─", m_titleBarWidget);
    m_btnWinMin->setFixedSize(45, 30);
    m_btnWinMin->setStyleSheet(btnStyle);
    connect(m_btnWinMin, &QPushButton::clicked, this, &MainWindow::onWinMinimize);
    layout->addWidget(m_btnWinMin);

    // Maximize
    m_btnWinMax = new QPushButton("☐", m_titleBarWidget);
    m_btnWinMax->setFixedSize(45, 30);
    m_btnWinMax->setStyleSheet(btnStyle);
    connect(m_btnWinMax, &QPushButton::clicked, this, &MainWindow::onWinMaximize);
    layout->addWidget(m_btnWinMax);

    // Close
    m_btnWinClose = new QPushButton("✕", m_titleBarWidget);
    m_btnWinClose->setFixedSize(45, 30);
    m_btnWinClose->setStyleSheet(closeStyle);
    connect(m_btnWinClose, &QPushButton::clicked, this, &MainWindow::onWinClose);
    layout->addWidget(m_btnWinClose);
}

void MainWindow::onWinMinimize() {
    showMinimized();
}

void MainWindow::onWinClose() {
    close();
}

void MainWindow::onWinMaximize() {
    if (isMaximized()) showNormal();
    else showMaximized();
}

// NEU: Maus Events für Fensterbewegung
void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_titleBarWidget && m_titleBarWidget->geometry().contains(event->pos())) {
            m_windowDragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        if (m_titleBarWidget && m_titleBarWidget->geometry().contains(mapFromGlobal(QCursor::pos()))) {
            if (!isMaximized()) {
                move(event->globalPosition().toPoint() - m_windowDragPos);
                event->accept();
                return;
            }
        }
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::onContentModified() {
    m_autoSaveTimer->start();
}

void MainWindow::performAutoSave() {
    CanvasView* cv = getCurrentCanvas();
    if (cv) {
        cv->saveToFile();
    }
}

CanvasView* MainWindow::getCurrentCanvas() {
    QWidget* current = m_editorTabs->currentWidget();
    if (current) {
        return qobject_cast<CanvasView*>(current);
    }
    return nullptr;
}

void MainWindow::onToggleFloatingTools() {
    // Empty slot to satisfy connection
}

void MainWindow::applyTheme() {
    QString c = m_currentAccentColor.name(); QString c_light = m_currentAccentColor.lighter(130).name();

    if (m_fileListView) {
        m_fileListView->setAccentColor(m_currentAccentColor);
    }

    QString style = QString(
                        "QMainWindow { background-color: #0F111A; } "
                        "QListView { background: transparent; border: none; outline: 0; } "
                        "QTabWidget::pane { border: 0; background: #0F111A; } "
                        "QTabBar::tab { background: #161925; color: #888; padding: 10px 25px; border-top-left-radius: 12px; border-top-right-radius: 12px; } "
                        "QTabBar::tab:selected { background: %1; color: white; } "
                        "QMenu { background-color: #252526; border: 1px solid #444; border-radius: 10px; padding: 10px; color: #E0E0E0; } "
                        "QMenu::item { padding: 8px 20px; border-radius: 5px; } "
                        "QMenu::item:selected { background-color: %2; color: white; }"
                        ).arg(c, c_light);

    this->setStyleSheet(style);

    if(m_sidebarContainer) m_sidebarContainer->setStyleSheet("background-color: #161925; border-right: 1px solid #1F2335;");
    if(m_sidebarHeader) m_sidebarHeader->setStyleSheet("background-color: #161925;");
    if(m_sidebarStrip) m_sidebarStrip->setStyleSheet("background-color: #161925; border-right: 1px solid #1F2335;");

    if(m_folderTree) {
        m_folderTree->setStyleSheet("QTreeView { background-color: transparent; border: none; outline: 0; } QTreeView::item { border: none; } QTreeView::branch { background: transparent; }");
    }

    if(m_rightSidebar) m_rightSidebar->setStyleSheet("background-color: #161925; border-left: 1px solid #1F2335;");

    if(m_titleBarWidget) m_titleBarWidget->setStyleSheet("background-color: #0F111A; border-bottom: 1px solid #1F2335;");

    QString fabStyle = QString("QPushButton { background-color: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); color: white; border-radius: 28px; font-size: 32px; font-weight: 300; border: none; }").arg(c, c_light);
    if(m_fabNote) m_fabNote->setStyleSheet(fabStyle);

    if(m_fabFolder) {
        QString s = fabStyle;
        s.replace("28px","20px"); s.replace("32px","24px");
        m_fabFolder->setStyleSheet(s);
    }
}

void MainWindow::updateTheme(QColor accentColor) {
    m_currentAccentColor = accentColor;
    applyTheme();
}

void MainWindow::onItemSizeChanged(int value) {
    m_currentItemSize = value;
    updateGrid();
}

void MainWindow::onGridSpacingChanged(int value) {
    m_currentSpacing = value;
    updateGrid();
}

void MainWindow::updateGrid() {
    if (m_fileListView) {
        int h = m_touchMode ? 80 : 60;
        if (m_currentItemSize > 200) h += 20;

        QSize itemS(m_currentItemSize, h);
        m_fileListView->setItemSize(itemS);

        int gridW = itemS.width() + m_currentSpacing;
        int gridH = itemS.height() + m_currentSpacing;

        m_fileListView->setGridSize(QSize(gridW, gridH));
    }
}

void MainWindow::updateUiMode(bool touchMode) {
    m_touchMode = touchMode;
    applyUiScaling();
    if (m_rightSidebar && m_rightSidebar->isVisible()) {
        m_btnUiDesktop->setChecked(!m_touchMode);
        m_btnUiTouch->setChecked(m_touchMode);
    }
}

void MainWindow::applyUiScaling() {
    int iconS = m_touchMode ? 32 : 24;
    int tbH = m_touchMode ? 80 : 60;

    const auto buttons = this->findChildren<ModernButton*>();
    for (ModernButton* btn : std::as_const(buttons)) {
        btn->setIconSize(QSize(iconS, iconS));
    }

    if (m_sidebarHeader) m_sidebarHeader->setFixedHeight(60);

    updateGrid();
    if (m_fileListView) m_fileListView->viewport()->update();

    int fabS = m_touchMode ? 70 : 56;
    if (m_fabNote) {
        m_fabNote->setFixedSize(fabS, fabS);
        QString fabStyle = m_fabNote->styleSheet();
        if(m_touchMode) fabStyle.replace("32px", "40px");
        else fabStyle.replace("40px", "32px");
        m_fabNote->setStyleSheet(fabStyle);
    }

    QEvent resizeEv(QEvent::Resize);
    QApplication::sendEvent(this, &resizeEv);
}

QIcon MainWindow::createModernIcon(const QString &name, const QColor &color) {
    int size = 64; QPixmap pixmap(size, size); pixmap.fill(Qt::transparent); QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);

    if (name == "more_vert") { p.setBrush(color); p.setPen(Qt::NoPen); p.drawEllipse(28, 14, 8, 8); p.drawEllipse(28, 28, 8, 8); p.drawEllipse(28, 42, 8, 8); }
    else if (name == "arrow_left") { QPainterPath path; path.moveTo(42, 12); path.lineTo(20, 32); path.lineTo(42, 52); p.drawPath(path); }
    else if (name == "arrow_right") { QPainterPath path; path.moveTo(20, 12); path.lineTo(42, 32); path.lineTo(20, 52); p.drawPath(path); }
    if(name=="menu") { p.drawLine(12,20,52,20); p.drawLine(12,32,52,32); p.drawLine(12,44,52,44); }
    if(name=="save") { p.drawRoundedRect(14,14,36,36,6,6); p.drawLine(22,14,22,26); p.drawLine(42,14,42,26); p.drawLine(22,26,42,26); }
    if(name=="undo") { QPainterPath pa; pa.moveTo(40,20); pa.arcTo(20,20,30,30,90,180); p.drawPath(pa); p.drawLine(20,35,10,25); p.drawLine(20,15,10,25); }
    if(name=="redo") { QPainterPath pa; pa.moveTo(24,20); pa.arcTo(14,20,30,30,90,-180); p.drawPath(pa); p.drawLine(44,35,54,25); p.drawLine(44,15,54,25); }
    if(name=="pen") { QPainterPath pa; pa.moveTo(12,52); pa.lineTo(22,52); pa.lineTo(50,24); pa.lineTo(40,14); pa.lineTo(12,42); pa.closeSubpath(); p.drawPath(pa); }
    if(name=="eraser") { p.drawRoundedRect(15,15,34,34,5,5); p.drawLine(25,25,39,39); p.drawLine(39,25,25,39); }
    if(name=="lasso") { p.drawEllipse(15,15,34,34); p.drawLine(45,45,55,55); }
    if(name=="format_inf") { p.drawEllipse(10,22,22,20); p.drawEllipse(32,22,22,20); }
    if(name=="format_a4") { p.drawRect(20,12,24,40); p.drawLine(26,20,38,20); p.drawLine(26,30,38,30); }
    if(name=="settings") { p.drawEllipse(20,20,24,24); p.drawLine(32,10,32,20); p.drawLine(32,44,32,54); p.drawLine(10,32,20,32); p.drawLine(44,32,54,32); }
    if(name=="add") { p.drawLine(32, 14, 32, 50); p.drawLine(14, 32, 50, 32); }
    if(name=="cursor") {
        QPainterPath pa; pa.moveTo(20,20); pa.lineTo(30,50); pa.lineTo(38,38); pa.lineTo(50,50);
        p.setPen(QPen(color, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(pa);
    }
    if(name=="close") { p.drawLine(20,20,44,44); p.drawLine(44,20,20,44); }

    return QIcon(pixmap);
}

void MainWindow::createDefaultFolder() {
#ifdef Q_OS_ANDROID
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    m_rootPath = basePath + "/BlopNotizen";
    QDir dir(m_rootPath); if (!dir.exists()) dir.mkpath(".");
}

void MainWindow::setupUi() {
    // NEU: Setup TitleBar
    setupTitleBar();

    // Central Container für alles unter der Titlebar
    m_centralContainer = new QWidget(this);
    setCentralWidget(m_centralContainer);

    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralContainer);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    mainLayout->addWidget(m_titleBarWidget);
    m_mainSplitter = new QSplitter(Qt::Horizontal, m_centralContainer);
    mainLayout->addWidget(m_mainSplitter);

    m_fileModel = new QFileSystemModel(this); m_fileModel->setRootPath(m_rootPath); m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot); m_fileModel->setReadOnly(false);

    setupSidebar();

    m_rightStack = new QStackedWidget(this);
    m_overviewContainer = new QWidget(this); m_overviewContainer->installEventFilter(this);
    QVBoxLayout *overviewLayout = new QVBoxLayout(m_overviewContainer); overviewLayout->setContentsMargins(30, 30, 30, 30);

    QHBoxLayout *topBar = new QHBoxLayout();

    btnBackOverview = new ModernButton(this);
    btnBackOverview->setIcon(createModernIcon("arrow_left", Qt::white));
    btnBackOverview->setToolTip("Zurück");
    btnBackOverview->hide();
    connect(btnBackOverview, &QAbstractButton::clicked, this, &MainWindow::onNavigateUp);

    topBar->addWidget(btnBackOverview);

    btnSettings = new ModernButton(this); btnSettings->setIcon(createModernIcon("settings", Qt::white));
    connect(btnSettings, &QAbstractButton::clicked, this, &MainWindow::onOpenSettings);
    topBar->addWidget(btnSettings);

    btnNewNote = new ModernButton(this);
    btnNewNote->setIcon(createModernIcon("add", Qt::white));
    btnNewNote->setToolTip("Neue Notiz erstellen");
    connect(btnNewNote, &QAbstractButton::clicked, this, &MainWindow::onNewPage);
    topBar->addWidget(btnNewNote);

    topBar->addStretch();
    overviewLayout->addLayout(topBar);

    m_fileListView = new FreeGridView(this);
    m_fileListView->setModel(m_fileModel);
    m_fileListView->setRootIndex(m_fileModel->index(m_rootPath));

    updateGrid();
    m_fileListView->setSpacing(20);
    m_fileListView->setFrameShape(QFrame::NoFrame);
    m_fileListView->setItemDelegate(new ModernItemDelegate(this));
    connect(m_fileListView, &QListView::doubleClicked, this, &MainWindow::onFileDoubleClicked);
    m_fileListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileListView, &QWidget::customContextMenuRequested, [this](const QPoint &pos){
        QModelIndex index = m_fileListView->indexAt(pos);
        if (index.isValid()) showContextMenu(m_fileListView->mapToGlobal(pos), index);
    });
    overviewLayout->addWidget(m_fileListView);

    m_fabNote = new QPushButton("+", m_overviewContainer); m_fabNote->setCursor(Qt::PointingHandCursor); m_fabNote->setFixedSize(56, 56);
    QGraphicsDropShadowEffect* shadow1 = new QGraphicsDropShadowEffect(this); shadow1->setBlurRadius(30); shadow1->setOffset(0, 8); shadow1->setColor(QColor(0,0,0,100)); m_fabNote->setGraphicsEffect(shadow1);
    connect(m_fabNote, &QPushButton::clicked, this, &MainWindow::onNewPage);

    m_editorContainer = new QWidget(this);
    m_editorContainer->installEventFilter(this);

    QHBoxLayout *editorMainLayout = new QHBoxLayout(m_editorContainer);
    editorMainLayout->setContentsMargins(0,0,0,0);
    editorMainLayout->setSpacing(0);

    m_editorCenterWidget = new QWidget(m_editorContainer);
    QVBoxLayout *centerLayout = new QVBoxLayout(m_editorCenterWidget);
    centerLayout->setContentsMargins(0,0,0,0);
    centerLayout->setSpacing(0);

    QHBoxLayout *editorHeader = new QHBoxLayout();

    btnEditorMenu = new ModernButton(this);
    btnEditorMenu->setIcon(createModernIcon("menu", Qt::white));
    connect(btnEditorMenu, &QAbstractButton::clicked, this, &MainWindow::onToggleSidebar);
    editorHeader->addWidget(btnEditorMenu);

    ModernButton *backBtn = new ModernButton(this); backBtn->setIcon(createModernIcon("arrow_left", Qt::white));
    connect(backBtn, &QAbstractButton::clicked, this, &MainWindow::onBackToOverview);
    editorHeader->addWidget(backBtn);
    editorHeader->addStretch();

    btnEditorSettings = new ModernButton(this);
    btnEditorSettings->setIcon(createModernIcon("settings", Qt::white));
    connect(btnEditorSettings, &QAbstractButton::clicked, this, &MainWindow::onToggleRightSidebar);
    editorHeader->addWidget(btnEditorSettings);

    centerLayout->addLayout(editorHeader);

    m_editorTabs = new QTabWidget(this); m_editorTabs->setDocumentMode(true); m_editorTabs->setTabsClosable(true);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, [this](int index){ m_editorTabs->removeTab(index); if (m_editorTabs->count() == 0) onBackToOverview(); });
    connect(m_editorTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    centerLayout->addWidget(m_editorTabs);

    // --- ModernToolbar Integration ---
    ModernToolbar* overlayToolbar = new ModernToolbar(m_editorCenterWidget);
    overlayToolbar->move(20, 80);
    overlayToolbar->hide();

    m_floatingTools = overlayToolbar;

    // Connects
    connect(overlayToolbar, &ModernToolbar::toolChanged, [this](ToolMode m){
        setActiveTool((CanvasView::ToolType)m);
    });
    connect(overlayToolbar, &ModernToolbar::undoRequested, this, &MainWindow::onUndo);
    connect(overlayToolbar, &ModernToolbar::redoRequested, this, &MainWindow::onRedo);

    // Neue Settings Connects
    connect(overlayToolbar, &ModernToolbar::penConfigChanged, [this](QColor c, int w){
        m_penColor = c;
        m_penWidth = w;
        CanvasView* cv = getCurrentCanvas();
        if(cv) { cv->setPenColor(c); cv->setPenWidth(w); }
    });

    // Scale Sync
    connect(overlayToolbar, &ModernToolbar::scaleChanged, [this](qreal newScale){
        if(m_sliderToolbarScale) {
            m_sliderToolbarScale->blockSignals(true);
            m_sliderToolbarScale->setValue(newScale * 100);
            m_sliderToolbarScale->blockSignals(false);
        }
    });

    editorMainLayout->addWidget(m_editorCenterWidget);

    setupRightSidebar();
    editorMainLayout->addWidget(m_rightSidebar);

    m_rightStack->addWidget(m_overviewContainer); m_rightStack->addWidget(m_editorContainer);
    m_mainSplitter->addWidget(m_rightStack); m_mainSplitter->setStretchFactor(0, 0); m_mainSplitter->setStretchFactor(1, 1); m_mainSplitter->setCollapsible(0, true); m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: transparent; }");

    setWindowTitle("Blop");
}

void MainWindow::setupSidebar() {
    m_sidebarStrip = new QWidget(this);
    m_sidebarStrip->setFixedWidth(60);
    m_sidebarStrip->hide();

    QVBoxLayout* stripLayout = new QVBoxLayout(m_sidebarStrip);
    stripLayout->setContentsMargins(0, 10, 0, 0);
    stripLayout->setSpacing(20);

    btnStripMenu = new ModernButton(m_sidebarStrip);
    btnStripMenu->setIcon(createModernIcon("menu", Qt::white));
    btnStripMenu->setFixedSize(40, 40);
    connect(btnStripMenu, &QAbstractButton::clicked, this, &MainWindow::onToggleSidebar);

    stripLayout->addWidget(btnStripMenu, 0, Qt::AlignHCenter);
    stripLayout->addStretch();

    m_mainSplitter->addWidget(m_sidebarStrip);

    m_sidebarContainer = new QWidget(this);
    m_sidebarContainer->setFixedWidth(250);
    QVBoxLayout *layout = new QVBoxLayout(m_sidebarContainer);
    layout->setContentsMargins(0,0,0,0);

    m_sidebarHeader = new QWidget(m_sidebarContainer);
    m_sidebarHeader->setFixedHeight(60);
    QHBoxLayout *hLayout = new QHBoxLayout(m_sidebarHeader);
    hLayout->setContentsMargins(15, 0, 15, 0);

    QLabel *lblWorkspace = new QLabel("Meine Dateien", m_sidebarHeader);
    lblWorkspace->setStyleSheet("color: #E0E0E0; font-weight: bold; font-size: 14px;");
    hLayout->addWidget(lblWorkspace);
    hLayout->addStretch();

    m_closeSidebarBtn = new QPushButton("«", m_sidebarHeader);
    m_closeSidebarBtn->setFixedSize(24,24);
    m_closeSidebarBtn->setCursor(Qt::PointingHandCursor);
    m_closeSidebarBtn->setStyleSheet("QPushButton { background: transparent; color: #888; border: none; font-size: 16px; } QPushButton:hover { color: white; background: #333; border-radius: 4px; }");
    connect(m_closeSidebarBtn, &QPushButton::clicked, this, &MainWindow::onToggleSidebar);
    hLayout->addWidget(m_closeSidebarBtn);

    layout->addWidget(m_sidebarHeader);

    // Tree View Setup
    m_folderTree = new QTreeView(m_sidebarContainer);
    m_folderTree->setModel(m_fileModel);
    m_folderTree->setRootIndex(m_fileModel->index(m_rootPath));

    m_folderTree->setRootIsDecorated(false);
    m_folderTree->setHeaderHidden(true);

    m_folderTree->setColumnHidden(1, true);
    m_folderTree->setColumnHidden(2, true);
    m_folderTree->setColumnHidden(3, true);

    m_folderTree->setMouseTracking(true);
    m_folderTree->viewport()->setAttribute(Qt::WA_Hover);
    m_folderTree->setIndentation(0);
    m_folderTree->setUniformRowHeights(true);
    m_folderTree->setItemDelegate(new SidebarItemDelegate(this));

    m_folderTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_folderTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_folderTree, &QWidget::customContextMenuRequested, this, &MainWindow::onSidebarContextMenu);

    connect(m_folderTree, &QTreeView::clicked, this, &MainWindow::onFolderSelected);
    connect(m_folderTree, &QTreeView::doubleClicked, this, &MainWindow::onFileDoubleClicked);

    m_folderTree->setDragEnabled(true);
    m_folderTree->setAcceptDrops(true);
    m_folderTree->setDropIndicatorShown(true);
    m_folderTree->setDragDropMode(QAbstractItemView::DragDrop);

    layout->addWidget(m_folderTree);

    m_fabFolder = new QPushButton("+", m_sidebarContainer);
    m_fabFolder->setCursor(Qt::PointingHandCursor);
    m_fabFolder->setFixedSize(40, 40);
    QGraphicsDropShadowEffect* shadowFolder = new QGraphicsDropShadowEffect(this);
    shadowFolder->setBlurRadius(20); shadowFolder->setOffset(0, 4); shadowFolder->setColor(QColor(0,0,0,80));
    m_fabFolder->setGraphicsEffect(shadowFolder);
    connect(m_fabFolder, &QPushButton::clicked, this, &MainWindow::onCreateFolder);

    m_mainSplitter->addWidget(m_sidebarContainer);
}

void MainWindow::onSidebarContextMenu(const QPoint &pos) {
    QModelIndex index = m_folderTree->indexAt(pos);
    if (!index.isValid()) return;

    QMenu menu(this);
    menu.addAction("Umbenennen", [this, index]() { startRename(index); });
    menu.addAction("Kopieren", [this, index]() { performCopy(index); });
    QAction* delAct = menu.addAction("Löschen");
    connect(delAct, &QAction::triggered, [this, index]() {
        QString path = m_fileModel->filePath(index);
        QMessageBox::StandardButton res = QMessageBox::question(this, "Löschen", "Wirklich löschen?");
        if (res == QMessageBox::Yes) {
            if (m_fileModel->isDir(index)) {
                QDir(path).removeRecursively();
            } else {
                QFile::remove(path);
            }
        }
    });
    menu.exec(m_folderTree->mapToGlobal(pos));
}

void MainWindow::performCopy(const QModelIndex &index) {
    QString srcPath = m_fileModel->filePath(index);
    QFileInfo info(srcPath);
    QString newName = info.baseName() + " Kopie";
    if (!info.suffix().isEmpty()) newName += "." + info.suffix();

    QString dstPath = info.dir().filePath(newName);

    int i = 1;
    while (QFile::exists(dstPath)) {
        newName = info.baseName() + QString(" Kopie %1").arg(i++);
        if (!info.suffix().isEmpty()) newName += "." + info.suffix();
        dstPath = info.dir().filePath(newName);
    }

    if (info.isDir()) {
        copyRecursive(srcPath, dstPath);
    } else {
        QFile::copy(srcPath, dstPath);
    }
}

bool MainWindow::copyRecursive(const QString &src, const QString &dst) {
    QDir dir(src);
    if (!dir.exists()) return false;
    QDir().mkpath(dst);
    for (const QString &entry : dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString srcItem = src + "/" + entry;
        QString dstItem = dst + "/" + entry;
        if (QFileInfo(srcItem).isDir()) {
            if (!copyRecursive(srcItem, dstItem)) return false;
        } else {
            if (!QFile::copy(srcItem, dstItem)) return false;
        }
    }
    return true;
}

void MainWindow::onNavigateUp() {
    QModelIndex current = m_fileListView->rootIndex();
    if (current.isValid()) {
        QModelIndex parent = current.parent();
        m_fileListView->setRootIndex(parent);
        updateOverviewBackButton();
    }
}

void MainWindow::updateOverviewBackButton() {
    QModelIndex current = m_fileListView->rootIndex();
    QModelIndex root = m_fileModel->index(m_rootPath);

    bool canGoUp = (current != root && current.isValid());
    btnBackOverview->setVisible(canGoUp);
}

void MainWindow::setupRightSidebar() {
    m_rightSidebar = new QWidget(this);
    m_rightSidebar->setFixedWidth(250);
    m_rightSidebar->hide();

    QVBoxLayout *layout = new QVBoxLayout(m_rightSidebar);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    QHBoxLayout *header = new QHBoxLayout;
    QLabel *title = new QLabel("Einstellungen");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: white;");
    header->addWidget(title);
    header->addStretch();

    ModernButton *closeBtn = new ModernButton(this);
    closeBtn->setIcon(createModernIcon("close", Qt::white));
    closeBtn->setFixedSize(30,30);
    connect(closeBtn, &QAbstractButton::clicked, this, &MainWindow::onToggleRightSidebar);
    header->addWidget(closeBtn);

    layout->addLayout(header);

    m_lblActiveNote = new QLabel("Keine Notiz", m_rightSidebar);
    m_lblActiveNote->setStyleSheet("color: #5E5CE6; font-size: 14px; font-weight: bold; margin-bottom: 5px;");
    m_lblActiveNote->setWordWrap(true);
    m_lblActiveNote->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_lblActiveNote);

    layout->addWidget(new QLabel("Seitenformat:", m_rightSidebar));
    m_btnFormatInfinite = new QPushButton("Unendlich");
    m_btnFormatInfinite->setCheckable(true);
    m_btnFormatInfinite->setChecked(true);
    m_btnFormatInfinite->setCursor(Qt::PointingHandCursor);
    m_btnFormatInfinite->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnFormatInfinite, &QPushButton::clicked, [this](){ setCanvasFormat(true); });

    m_btnFormatA4 = new QPushButton("DIN A4");
    m_btnFormatA4->setCheckable(true);
    m_btnFormatA4->setCursor(Qt::PointingHandCursor);
    m_btnFormatA4->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnFormatA4, &QPushButton::clicked, [this](){ setCanvasFormat(false); });

    QButtonGroup *grp = new QButtonGroup(this);
    grp->addButton(m_btnFormatInfinite);
    grp->addButton(m_btnFormatA4);
    grp->setExclusive(true);

    layout->addWidget(m_btnFormatInfinite);
    layout->addWidget(m_btnFormatA4);

    layout->addWidget(new QLabel("Seitenfarbe:", m_rightSidebar));
    m_btnColorWhite = new QPushButton("Hell");
    m_btnColorWhite->setCheckable(true);
    m_btnColorWhite->setChecked(true);
    m_btnColorWhite->setCursor(Qt::PointingHandCursor);
    m_btnColorWhite->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnColorWhite, &QPushButton::clicked, [this](){ setPageColor(false); });

    m_btnColorDark = new QPushButton("Dunkel");
    m_btnColorDark->setCheckable(true);
    m_btnColorDark->setCursor(Qt::PointingHandCursor);
    m_btnColorDark->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnColorDark, &QPushButton::clicked, [this](){ setPageColor(true); });

    QButtonGroup *grpColor = new QButtonGroup(this);
    grpColor->addButton(m_btnColorWhite);
    grpColor->addButton(m_btnColorDark);
    grpColor->setExclusive(true);

    layout->addWidget(m_btnColorWhite);
    layout->addWidget(m_btnColorDark);

    layout->addWidget(new QLabel("Eingabemodus:", m_rightSidebar));
    m_btnInputPen = new QPushButton("Nur Stift\n(1 Finger scrollt)");
    m_btnInputPen->setCheckable(true);
    m_btnInputPen->setChecked(true);
    m_btnInputPen->setCursor(Qt::PointingHandCursor);
    m_btnInputPen->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; text-align: left; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnInputPen, &QPushButton::clicked, [this](){ updateInputMode(true); });

    m_btnInputTouch = new QPushButton("Touch & Stift\n(2 Finger scrollen)");
    m_btnInputTouch->setCheckable(true);
    m_btnInputTouch->setCursor(Qt::PointingHandCursor);
    m_btnInputTouch->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; text-align: left; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnInputTouch, &QPushButton::clicked, [this](){ updateInputMode(false); });

    QButtonGroup *grpInput = new QButtonGroup(this);
    grpInput->addButton(m_btnInputPen);
    grpInput->addButton(m_btnInputTouch);
    grpInput->setExclusive(true);

    layout->addWidget(m_btnInputPen);
    layout->addWidget(m_btnInputTouch);

    layout->addWidget(new QLabel("Werkzeugleiste:", m_rightSidebar));
    m_comboToolbarStyle = new QComboBox();
    m_comboToolbarStyle->addItems({"Vertikal", "Radial (Voll)", "Radial (Halb)"});
    m_comboToolbarStyle->setStyleSheet("QComboBox { background: #333; color: white; border: 1px solid #444; padding: 5px; border-radius: 5px; } QComboBox::drop-down { border: 0px; } QComboBox QAbstractItemView { background: #333; color: white; selection-background-color: #5E5CE6; }");
    m_comboToolbarStyle->setCursor(Qt::PointingHandCursor);

    // FIX: Expliziter Cast für überladenes Signal
    connect(m_comboToolbarStyle, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int index){
        ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools);
        if (tb) {
            if (index == 0) tb->setStyle(ModernToolbar::Normal);
            else if (index == 1) {
                tb->setStyle(ModernToolbar::Radial);
                tb->setRadialType(ModernToolbar::FullCircle);
            } else {
                tb->setStyle(ModernToolbar::Radial);
                tb->setRadialType(ModernToolbar::HalfEdge);
            }
        }
    });

    layout->addWidget(m_comboToolbarStyle);

    // Toolbar Size Slider
    layout->addWidget(new QLabel("Größe Werkzeugleiste:", m_rightSidebar));
    m_sliderToolbarScale = new QSlider(Qt::Horizontal);
    m_sliderToolbarScale->setRange(50, 150);
    m_sliderToolbarScale->setValue(100);
    m_sliderToolbarScale->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");

    connect(m_sliderToolbarScale, &QSlider::valueChanged, [this](int val){
        ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools);
        if(tb) tb->setScale(val / 100.0);
    });
    layout->addWidget(m_sliderToolbarScale);

    layout->addWidget(new QLabel("Oberfläche (Größe):", m_rightSidebar));
    m_btnUiDesktop = new QPushButton("Desktop (Kompakt)");
    m_btnUiDesktop->setCheckable(true);
    m_btnUiDesktop->setChecked(!m_touchMode);
    m_btnUiDesktop->setCursor(Qt::PointingHandCursor);
    m_btnUiDesktop->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnUiDesktop, &QPushButton::clicked, [this](){ updateUiMode(false); });

    m_btnUiTouch = new QPushButton("Touch (Groß)");
    m_btnUiTouch->setCheckable(true);
    m_btnUiTouch->setChecked(m_touchMode);
    m_btnUiTouch->setCursor(Qt::PointingHandCursor);
    m_btnUiTouch->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnUiTouch, &QPushButton::clicked, [this](){ updateUiMode(true); });

    QButtonGroup *grpUi = new QButtonGroup(this);
    grpUi->addButton(m_btnUiDesktop);
    grpUi->addButton(m_btnUiTouch);
    grpUi->setExclusive(true);

    layout->addWidget(m_btnUiDesktop);
    layout->addWidget(m_btnUiTouch);

    layout->addStretch();
}

void MainWindow::animateSidebar(bool show) {
    int start = show ? 0 : 250;
    int end = show ? 250 : 0;

    m_isSidebarOpen = show;

    if (show) {
        m_sidebarContainer->setVisible(true);
        updateSidebarState();
    }

    QPropertyAnimation *anim = new QPropertyAnimation(m_sidebarContainer, "maximumWidth");
    anim->setDuration(250);
    anim->setStartValue(start);
    anim->setEndValue(end);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(anim, &QPropertyAnimation::finished, [this, show](){
        if (!show) {
            m_sidebarContainer->hide();
            updateSidebarState();
        }
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::onToggleSidebar() {
    bool isVisible = m_sidebarContainer->isVisible() && m_sidebarContainer->width() > 0;
    animateSidebar(!isVisible);
}

void MainWindow::updateSidebarState() {
    bool isEditor = (m_rightStack->currentWidget() == m_editorContainer);

    if (m_floatingTools) {
        m_floatingTools->setVisible(isEditor);
    }

    if (m_isSidebarOpen) {
        m_sidebarStrip->hide();
        btnEditorMenu->hide();
        return;
    }

    if (isEditor) {
        m_sidebarStrip->hide();
        btnEditorMenu->show();
    } else {
        m_sidebarStrip->show();
        btnEditorMenu->hide();
    }
}

void MainWindow::animateRightSidebar(bool show) {
    int start = show ? 0 : 250;
    int end = show ? 250 : 0;

    m_rightSidebar->setVisible(true);
    QPropertyAnimation *anim = new QPropertyAnimation(m_rightSidebar, "maximumWidth");
    anim->setDuration(250);
    anim->setStartValue(start);
    anim->setEndValue(end);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(anim, &QPropertyAnimation::finished, [this, show](){
        if (!show) m_rightSidebar->hide();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::onToggleRightSidebar() {
    bool isVisible = m_rightSidebar->isVisible() && m_rightSidebar->width() > 0;
    animateRightSidebar(!isVisible);

    if (!isVisible) {
        CanvasView *cv = getCurrentCanvas();
        if (cv) {
            bool inf = cv->isInfinite();
            m_btnFormatInfinite->setChecked(inf);
            m_btnFormatA4->setChecked(!inf);
            m_btnInputPen->setChecked(m_penOnlyMode);
            m_btnInputTouch->setChecked(!m_penOnlyMode);
            m_btnUiDesktop->setChecked(!m_touchMode);
            m_btnUiTouch->setChecked(m_touchMode);

            bool isDark = (cv->pageColor() == UIStyles::SceneBackground);
            m_btnColorDark->setChecked(isDark);
            m_btnColorWhite->setChecked(!isDark);
        }

        ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools);
        if (tb) {
            if (tb->currentStyle() == ModernToolbar::Normal) {
                m_comboToolbarStyle->setCurrentIndex(0);
            } else {
                if (tb->radialType() == ModernToolbar::FullCircle) m_comboToolbarStyle->setCurrentIndex(1);
                else m_comboToolbarStyle->setCurrentIndex(2);
            }

            if(m_sliderToolbarScale) m_sliderToolbarScale->setValue(tb->scale() * 100);
        }
    }
}

void MainWindow::onFileDoubleClicked(const QModelIndex &index) {
    if (m_fileModel->isDir(index)) {
        m_fileListView->setRootIndex(index);
        m_fileModel->setRootPath(m_fileModel->filePath(index));
        updateOverviewBackButton();
    } else {
        CanvasView *canvas = new CanvasView(this);
        canvas->setPenColor(m_penColor);
        canvas->setPenWidth(m_penWidth);
        canvas->setPenMode(m_penOnlyMode);

        canvas->setFilePath(m_fileModel->filePath(index));
        canvas->loadFromFile();

        connect(canvas, &CanvasView::contentModified, this, &MainWindow::onContentModified);

        m_editorTabs->addTab(canvas, index.data().toString());
        m_editorTabs->setCurrentWidget(canvas);
        m_rightStack->setCurrentWidget(m_editorContainer);

        setActiveTool(m_activeToolType);

        updateSidebarState();
    }
}

void MainWindow::onFolderSelected(const QModelIndex &index) {
    if (m_fileModel->isDir(index)) {
        m_fileListView->setRootIndex(index);
        m_fileModel->setRootPath(m_fileModel->filePath(index));
        updateOverviewBackButton();
    }
}

void MainWindow::onBackToOverview() {
    m_rightStack->setCurrentWidget(m_overviewContainer);
    updateSidebarState();
}

void MainWindow::showContextMenu(const QPoint &globalPos, const QModelIndex &index) {
    QMenu menu(this);
    menu.addAction("Öffnen", [this, index](){ onFileDoubleClicked(index); });
    menu.addAction("Umbenennen", [this, index](){ startRename(index); });
    menu.addAction("Löschen", [this, index](){ m_fileModel->remove(index); });
    menu.exec(globalPos);
}

void MainWindow::startRename(const QModelIndex &index) {
    m_indexToRename = index;
    showRenameOverlay(index.data().toString());
}

void MainWindow::showRenameOverlay(const QString &currentName) {
    if (!m_renameOverlay) {
        m_renameOverlay = new QWidget(this);
        m_renameOverlay->resize(size());
        m_renameOverlay->setStyleSheet("background-color: rgba(0,0,0,200);");
        m_renameInput = new QLineEdit(m_renameOverlay);
        m_renameInput->setFixedSize(300, 40);
        m_renameInput->setStyleSheet("QLineEdit { background: #333; color: white; border: 1px solid #555; font-size: 16px; padding: 5px; }");
        connect(m_renameInput, &QLineEdit::returnPressed, this, &MainWindow::finishRename);
    }
    m_renameInput->move(width()/2 - 150, height()/2 - 20);
    m_renameInput->setText(currentName);
    m_renameOverlay->show();
    m_renameInput->setFocus();
}

void MainWindow::finishRename() {
    if (m_renameOverlay && m_indexToRename.isValid()) {
        QString newName = m_renameInput->text();
        if (!newName.isEmpty()) { m_fileModel->setData(m_indexToRename, newName, Qt::EditRole); }
        m_renameOverlay->hide();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    int fabOffset = m_touchMode ? 100 : 80;

    if (m_fabNote && m_overviewContainer) {
        m_fabNote->move(m_overviewContainer->width() - fabOffset, m_overviewContainer->height() - fabOffset);
        m_fabNote->raise();
    }

    if (m_fabFolder && m_sidebarContainer) {
        int sidebarW = m_sidebarContainer->width();
        int sidebarH = m_sidebarContainer->height();
        int btnS = m_fabFolder->width();
        m_fabFolder->move(sidebarW - btnS - 20, sidebarH - btnS - 20);
        m_fabFolder->raise();
    }

    if (m_renameOverlay) {
        m_renameOverlay->resize(size());
        if (m_renameInput) m_renameInput->move(width()/2 - 150, height()/2 - 20);
    }

    // NEU: Toolbar Limit updaten
    if (m_editorTabs && m_floatingTools) {
        int top = 0;
        if (m_editorTabs->tabBar()) {
            QPoint tabsPos = m_editorTabs->mapTo(m_editorCenterWidget, QPoint(0,0));
            top = tabsPos.y() + m_editorTabs->tabBar()->height();
        }
        ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools);
        if (tb) tb->setTopBound(top);
    }
}

void MainWindow::onNewPage() {
    bool ok;
    QString text = QInputDialog::getText(this, "Neue Notiz", "Name:", QLineEdit::Normal, "Neue Notiz", &ok);
    if (ok && !text.isEmpty()) {
        QString path = m_fileModel->rootPath() + "/" + text + ".blop";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) { file.close(); }
    }
}

void MainWindow::onCreateFolder() {
    bool ok;
    QString text = QInputDialog::getText(this, "Neuer Ordner", "Name:", QLineEdit::Normal, "Neuer Ordner", &ok);
    if (ok && !text.isEmpty()) { m_fileModel->mkdir(m_fileModel->index(m_fileModel->rootPath()), text); }
}

void MainWindow::onOpenSettings() {
    QPixmap pixmap = this->grab();
    QWidget *overlay = new QWidget(this);
    overlay->setGeometry(this->rect());
    overlay->setAutoFillBackground(true);
    QGraphicsBlurEffect *blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(25);
    QLabel *blurBg = new QLabel(overlay);
    blurBg->setPixmap(pixmap);
    blurBg->setGraphicsEffect(blur);
    blurBg->resize(this->size());
    QWidget *dimmer = new QWidget(overlay);
    dimmer->setStyleSheet("background-color: rgba(0, 0, 0, 180);");
    dimmer->resize(this->size());
    overlay->show();

    SettingsDialog dlg(this);
    dlg.setTouchMode(m_touchMode);
    dlg.setGridValues(m_currentItemSize, m_currentSpacing);

    ModernToolbar* toolbar = qobject_cast<ModernToolbar*>(m_floatingTools);
    if(toolbar) {
        bool isRad = (toolbar->currentStyle() == ModernToolbar::Radial);
        bool isHalf = (toolbar->radialType() == ModernToolbar::HalfEdge);
        int scale = (int)(toolbar->scale() * 100);
        dlg.setToolbarConfig(isRad, isHalf, scale);
    }

    connect(&dlg, &SettingsDialog::accentColorChanged, this, &MainWindow::updateTheme);
    connect(&dlg, &SettingsDialog::uiModeChanged, this, &MainWindow::updateUiMode);
    connect(&dlg, &SettingsDialog::itemSizeChanged, this, &MainWindow::onItemSizeChanged);
    connect(&dlg, &SettingsDialog::gridSpacingChanged, this, &MainWindow::onGridSpacingChanged);

    connect(&dlg, &SettingsDialog::toolbarStyleChanged, [this, toolbar](bool radial){
        if(toolbar) toolbar->setStyle(radial ? ModernToolbar::Radial : ModernToolbar::Normal);
    });

    connect(&dlg, &SettingsDialog::toolbarScaleChanged, [toolbar](int percent){
        if(toolbar) toolbar->setScale(percent / 100.0);
    });

    dlg.exec();
    delete overlay;
}

void MainWindow::setCanvasFormat(bool infinite) {
    CanvasView *cv = getCurrentCanvas();
    if (cv) cv->setPageFormat(infinite);
}

void MainWindow::updateInputMode(bool penOnly) {
    m_penOnlyMode = penOnly;
    CanvasView* cv = getCurrentCanvas();
    if (cv) cv->setPenMode(penOnly);
}

void MainWindow::setPageColor(bool dark) {
    CanvasView *cv = getCurrentCanvas();
    if (cv) {
        cv->setPageColor(dark ? UIStyles::SceneBackground : UIStyles::PageBackground);
    }
}

//void MainWindow::setupToolbar() {}

void MainWindow::setActiveTool(CanvasView::ToolType tool) {
    m_activeToolType = tool;

    ModernToolbar* toolbar = qobject_cast<ModernToolbar*>(m_floatingTools);
    if(toolbar) {
        toolbar->setToolMode((ToolMode)tool);
    }

    CanvasView *cv = getCurrentCanvas();
    if (cv) {
        cv->setTool(tool);
    }
}

void MainWindow::switchToSelectTool() {
    onToolSelect();
}

void MainWindow::onToolSelect() {
    setActiveTool(CanvasView::ToolType::Select);
}

void MainWindow::onToolPen() {
    if (m_activeToolType == CanvasView::ToolType::Pen) {
        //  showPenSettingsMenu();
    } else {
        setActiveTool(CanvasView::ToolType::Pen);
    }
}

void MainWindow::onToolEraser() {
    setActiveTool(CanvasView::ToolType::Eraser);
}

void MainWindow::onToolLasso() {
    setActiveTool(CanvasView::ToolType::Lasso);
}

void MainWindow::onUndo() {
    CanvasView *cv = getCurrentCanvas();
    if (cv) cv->undo();
}

void MainWindow::onRedo() {
    CanvasView *cv = getCurrentCanvas();
    if (cv) cv->redo();
}

void MainWindow::onTabChanged(int index) {
    CanvasView *cv = getCurrentCanvas();
    if (cv) {
        cv->setPenMode(m_penOnlyMode);
    }
    setActiveTool(m_activeToolType);

    if (m_lblActiveNote) {
        QString text = "Keine Notiz";
        if (index >= 0) {
            text = m_editorTabs->tabText(index);
        }
        m_lblActiveNote->setText(text);
    }

    if (m_rightSidebar && m_rightSidebar->isVisible()) {
        if (cv) {
            bool inf = cv->isInfinite();
            m_btnFormatInfinite->setChecked(inf);
            m_btnFormatA4->setChecked(!inf);
            m_btnInputPen->setChecked(m_penOnlyMode);
            m_btnInputTouch->setChecked(!m_penOnlyMode);
            m_btnUiDesktop->setChecked(!m_touchMode);
            m_btnUiTouch->setChecked(m_touchMode);
            bool isDark = (cv->pageColor() == UIStyles::SceneBackground);
            m_btnColorDark->setChecked(isDark);
            m_btnColorWhite->setChecked(!isDark);
        }
    }

    updateSidebarState();
}

//void MainWindow::onToggleFormat() {}
