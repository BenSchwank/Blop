#include "mainwindow.h"
#include "settingsdialog.h"
#include "canvasview.h"
#include "UIStyles.h"
#include "moderntoolbar.h"
#include "profileeditordialog.h"
#include "newnotedialog.h"

#include <QApplication>
#include <QStyle>
#include <QLabel>
#include <QStandardPaths>
#include <QDir>
#include <QHeaderView>
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
#include <QLineEdit>
#include <QDataStream>
#include <utility>
#include <algorithm>
// NEU: Netzwerk & Desktop Services
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QDateTime>
#include <QLocale>

// ============================================================================
// 1. DELEGATES & BUTTONS
// ============================================================================

// --- SidebarNavDelegate ---
SidebarNavDelegate::SidebarNavDelegate(MainWindow *parent)
    : QStyledItemDelegate(parent), m_window(parent) {}

QSize SidebarNavDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    bool isHeader = index.data(Qt::UserRole + 1).toBool();
    return QSize(option.rect.width(), isHeader ? 40 : 44);
}

void SidebarNavDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    bool isHeader = index.data(Qt::UserRole + 1).toBool();
    bool isExpandable = index.data(Qt::UserRole + 6).toBool();
    bool isExpanded = index.data(Qt::UserRole + 3).toBool();
    int depth = index.data(Qt::UserRole + 9).toInt();
    int indent = 15 * depth;

    QString text = index.data(Qt::DisplayRole).toString();

    if (isHeader) {
        painter->setPen(QColor(150, 150, 150));
        QFont f = m_window->font(); f.setBold(true); f.setPointSize(9); painter->setFont(f);
        int arrowX = option.rect.left() + 15; int arrowY = option.rect.center().y();
        QPainterPath arrow;
        bool collapsed = index.data(Qt::UserRole + 3).toBool();
        if (collapsed) { arrow.moveTo(arrowX, arrowY - 4); arrow.lineTo(arrowX + 5, arrowY); arrow.lineTo(arrowX, arrowY + 4); }
        else { arrow.moveTo(arrowX, arrowY - 2); arrow.lineTo(arrowX + 8, arrowY - 2); arrow.lineTo(arrowX + 4, arrowY + 3); }
        painter->setBrush(QColor(150, 150, 150)); painter->setPen(Qt::NoPen); painter->drawPath(arrow);
        QRect textRect = option.rect.adjusted(30, 0, -15, 0);
        painter->setPen(QColor(150, 150, 150)); painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
        QFontMetrics fm(f); int textW = fm.horizontalAdvance(text);
        int lineX = textRect.left() + textW + 10; int lineY = option.rect.center().y();
        painter->setPen(QColor(60, 60, 60)); painter->drawLine(lineX, lineY, option.rect.right() - 15, lineY);
    } else {
        QRect rect = option.rect.adjusted(8 + indent, 2, -8, -2);
        bool selected = (option.state & QStyle::State_Selected);
        bool hover = (option.state & QStyle::State_MouseOver);

        if (selected) { painter->setBrush(m_window->currentAccentColor().darker(120)); painter->setPen(Qt::NoPen); painter->drawRoundedRect(rect, 10, 10); }
        else if (hover) { painter->setBrush(QColor(255, 255, 255, 10)); painter->setPen(Qt::NoPen); painter->drawRoundedRect(rect, 10, 10); }

        int iconOffset = 10;
        if (isExpandable) {
            int arrowX = rect.left() + 6; int arrowY = rect.center().y();
            QPainterPath arrow;
            if (isExpanded) { arrow.moveTo(arrowX, arrowY - 2); arrow.lineTo(arrowX + 8, arrowY - 2); arrow.lineTo(arrowX + 4, arrowY + 3); }
            else { arrow.moveTo(arrowX, arrowY - 4); arrow.lineTo(arrowX + 5, arrowY); arrow.lineTo(arrowX, arrowY + 4); }
            painter->setBrush(selected ? Qt::white : QColor(180,180,180)); painter->setPen(Qt::NoPen); painter->drawPath(arrow);
            iconOffset = 18;
        }

        QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        int iconSize = 20;
        QRect iconRect(rect.left() + iconOffset, rect.center().y() - iconSize/2, iconSize, iconSize);
        QIcon::Mode mode = selected ? QIcon::Selected : QIcon::Normal;
        if (selected) { icon.paint(painter, iconRect, Qt::AlignCenter, mode, QIcon::On); }
        else { painter->setOpacity(0.7); icon.paint(painter, iconRect, Qt::AlignCenter, mode, QIcon::Off); painter->setOpacity(1.0); }

        QRect textRect = rect; textRect.setLeft(iconRect.right() + 12); textRect.setRight(rect.right() - 40);
        painter->setPen(selected ? Qt::white : QColor(200, 200, 200));
        QFont f = m_window->font(); f.setPointSize(10); if (selected) f.setBold(true); painter->setFont(f);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));

        QString count = index.data(Qt::UserRole + 2).toString();
        if (!count.isEmpty()) {
            int badgeW = 24; int badgeH = 18;
            QRect badgeRect(rect.right() - badgeW - 5, rect.center().y() - badgeH/2, badgeW, badgeH);
            painter->setPen(selected ? Qt::white : m_window->currentAccentColor());
            QFont bf = f; bf.setBold(true); bf.setPointSize(9); painter->setFont(bf);
            painter->drawText(badgeRect, Qt::AlignCenter, count);
        }
    }
    painter->restore();
}

// --- ModernItemDelegate ---
ModernItemDelegate::ModernItemDelegate(MainWindow *parent) : QStyledItemDelegate(parent), m_window(parent) {}
QSize ModernItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const { return QStyledItemDelegate::sizeHint(option, index); }
void ModernItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save(); painter->setRenderHint(QPainter::Antialiasing);
    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    QColor bgColor = QColor(0x1E2230);
    if (option.state & QStyle::State_Selected) bgColor = m_window->currentAccentColor().darker(150);
    else if (option.state & QStyle::State_MouseOver) bgColor = QColor(0x2A2E3F);
    painter->setBrush(bgColor); painter->setPen(Qt::NoPen); painter->drawRoundedRect(rect, 8, 8);
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (icon.isNull()) icon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    QString text = index.data(Qt::DisplayRole).toString();
    painter->setPen(Qt::white);
    bool isWideList = rect.width() > (rect.height() * 1.5);
    if (isWideList) {
        int iconDim = rect.height() - 16; if (iconDim < 16) iconDim = 16;
        QRect iconRect(rect.left() + 10, rect.center().y() - iconDim/2, iconDim, iconDim);
        icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
        QRect textRect = rect; textRect.setLeft(iconRect.right() + 15); textRect.setRight(rect.right() - 40);
        QFont f = painter->font(); f.setBold(true); f.setPointSize(10); painter->setFont(f);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    } else {
        int textH = 24; if (rect.height() < 60) textH = 0;
        int maxIconH = rect.height() - textH - 10; int maxIconW = rect.width() - 10;
        int iconDim = qMin(maxIconW, maxIconH); if (iconDim < 20) iconDim = 20;
        int contentHeight = iconDim + textH; int startY = rect.top() + (rect.height() - contentHeight) / 2;
        QRect iconRect(rect.center().x() - iconDim/2, startY, iconDim, iconDim);
        icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
        if (textH > 0) {
            QRect textRect(rect.left() + 2, iconRect.bottom() + 2, rect.width() - 4, textH);
            QFont f = painter->font(); f.setPointSize(9); painter->setFont(f);
            QTextOption opt; opt.setAlignment(Qt::AlignCenter); opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            painter->drawText(textRect, text, opt);
        }
    }
    QIcon menuIcon = m_window->createModernIcon("more_vert", Qt::gray);
    QRect menuRect(rect.right() - 24, rect.top() + 4, 20, 20); if(isWideList) menuRect.moveTop(rect.center().y() - 10);
    menuIcon.paint(painter, menuRect, Qt::AlignCenter);
    painter->restore();
}
bool ModernItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QRect rect = option.rect.adjusted(4,4,-4,-4);
        int clickArea = 40; QRect menuRect(rect.right() - clickArea, rect.top(), clickArea, rect.height());
        if (menuRect.contains(me->pos())) { m_window->showContextMenu(QCursor::pos(), index); return true; }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// --- ModernButton ---
// FIX: qreal
ModernButton::ModernButton(QWidget *parent) : QToolButton(parent), m_scale(1.0), m_accentColor(Qt::white) {
    m_anim = new QPropertyAnimation(this, "scale", this); m_anim->setDuration(150); m_anim->setEasingCurve(QEasingCurve::OutQuad);
    setCursor(Qt::PointingHandCursor); setStyleSheet("border: none; background: transparent;");
    setIconSize(QSize(24, 24));
}
// FIX: qreal
void ModernButton::setScale(qreal s) { m_scale = s; update(); }
void ModernButton::setAccentColor(QColor c) { m_accentColor = c; update(); }
void ModernButton::enterEvent(QEnterEvent *event) { m_anim->stop(); m_anim->setEndValue(1.2); m_anim->start(); QToolButton::enterEvent(event); }
void ModernButton::leaveEvent(QEvent *event) { m_anim->stop(); m_anim->setEndValue(1.0); m_anim->start(); QToolButton::leaveEvent(event); }
void ModernButton::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.setRenderHint(QPainter::SmoothPixmapTransform);
    QIcon ic = icon(); if (ic.isNull()) return; int w = width(); int h = height();
    int iconS = iconSize().width();
    p.translate(w / 2, h / 2); p.scale(m_scale, m_scale); p.translate(-iconS / 2, -iconS / 2);
    QIcon::Mode mode = isChecked() ? QIcon::Active : QIcon::Normal; ic.paint(&p, 0, 0, iconS, iconS, Qt::AlignCenter, mode);
}


// ============================================================================
// 2. MAINWINDOW IMPLEMENTIERUNG
// ============================================================================

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_renameOverlay(nullptr) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    m_profileManager = new UiProfileManager(this);
    connect(m_profileManager, &UiProfileManager::profileChanged, this, &MainWindow::applyProfile);

    m_currentAccentColor = QColor(0x5E5CE6);
    m_penColor = Qt::black;
    m_penWidth = 3;
    m_activeToolType = CanvasView::ToolType::Pen;
    m_penOnlyMode = true;
    m_lblActiveNote = nullptr;
    m_isSidebarOpen = true;

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(1500);
    m_autoSaveTimer->setSingleShot(true);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::performAutoSave);

    m_gridSpacingTimer = new QTimer(this);
    m_gridSpacingTimer->setInterval(100);
    m_gridSpacingTimer->setSingleShot(true);
    connect(m_gridSpacingTimer, &QTimer::timeout, this, &MainWindow::applyDelayedGridSpacing);

    createDefaultFolder();
    setupUi();

    applyProfile(m_profileManager->currentProfile());
    applyTheme();

    updateSidebarState();
    updateOverviewBackButton();

    // NEU: Update Check nach 2 Sekunden starten
    QTimer::singleShot(2000, this, &MainWindow::checkForUpdates);
}
MainWindow::~MainWindow() {}

// NEU: Die Funktion für den Update Check mit DEINER URL (Blop-releases)
void MainWindow::checkForUpdates() {
    if (!m_netManager) {
        m_netManager = new QNetworkAccessManager(this);
    }

    // URL angepasst auf dein öffentliches Repo "BenSchwank/Blop-releases"
    QUrl url("https://api.github.com/repos/BenSchwank/Blop-releases/releases/tags/nightly");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Blop-App");

    QNetworkReply *reply = m_netManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();

            // Release Zeit vom Server (z.B. "2023-10-27T10:00:00Z")
            QString publishedAt = obj["published_at"].toString();
            QDateTime serverDate = QDateTime::fromString(publishedAt, Qt::ISODate);

            // Eigene Build-Zeit ermitteln
            QString buildDateStr = QString("%1 %2").arg(__DATE__).arg(__TIME__);
            QLocale usLocale(QLocale::English, QLocale::UnitedStates);
            QDateTime localDate = usLocale.toDateTime(buildDateStr, "MMM d yyyy hh:mm:ss");
            if (!localDate.isValid()) {
                localDate = usLocale.toDateTime(buildDateStr, "MMM  d yyyy hh:mm:ss");
            }

            // Wenn Server neuer ist als lokaler Build -> Update verfügbar!
            if (serverDate.isValid() && localDate.isValid() && serverDate > localDate) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Update verfügbar");
                msgBox.setText("Eine neue Version von Blop ist verfügbar!");
                msgBox.setInformativeText("Möchtest du das Update jetzt herunterladen?");
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::Yes);

                msgBox.setStyleSheet("QMessageBox { background-color: #252526; color: white; } QLabel { color: white; } QPushButton { background-color: #5E5CE6; color: white; border: none; padding: 5px 15px; border-radius: 4px; } QPushButton:hover { background-color: #4b49c9; }");

                if (msgBox.exec() == QMessageBox::Yes) {
                    if (obj.contains("assets") && obj["assets"].isArray()) {
                        QJsonArray assets = obj["assets"].toArray();
                        if (!assets.isEmpty()) {
                            QString downloadUrl = assets.first().toObject()["browser_download_url"].toString();
                            QDesktopServices::openUrl(QUrl(downloadUrl));
                        }
                    }
                }
            }
        }
        reply->deleteLater();
    });
}

void MainWindow::setupTitleBar() {
    m_titleBarWidget = new QWidget(this);
    m_titleBarWidget->setFixedHeight(30);
    m_titleBarWidget->setStyleSheet("background-color: #0F111A; border-bottom: 1px solid #1F2335;");
    QHBoxLayout* layout = new QHBoxLayout(m_titleBarWidget); layout->setContentsMargins(10, 0, 0, 0); layout->setSpacing(0);
    QLabel* lblTitle = new QLabel("Blop", m_titleBarWidget); lblTitle->setStyleSheet("color: #888; font-weight: bold;"); layout->addWidget(lblTitle); layout->addStretch();
    QString btnStyle = "QPushButton { background: transparent; border: none; color: #BBB; font-family: 'Segoe MDL2 Assets'; font-size: 12px; } QPushButton:hover { background: #252526; color: white; }";
    QString closeStyle = "QPushButton { background: transparent; border: none; color: #BBB; font-family: 'Segoe MDL2 Assets'; font-size: 12px; } QPushButton:hover { background: #E81123; color: white; }";
    m_btnWinMin = new QPushButton("─", m_titleBarWidget); m_btnWinMin->setFixedSize(45, 30); m_btnWinMin->setStyleSheet(btnStyle); connect(m_btnWinMin, &QPushButton::clicked, this, &MainWindow::onWinMinimize); layout->addWidget(m_btnWinMin);
    m_btnWinMax = new QPushButton("☐", m_titleBarWidget); m_btnWinMax->setFixedSize(45, 30); m_btnWinMax->setStyleSheet(btnStyle); connect(m_btnWinMax, &QPushButton::clicked, this, &MainWindow::onWinMaximize); layout->addWidget(m_btnWinMax);
    m_btnWinClose = new QPushButton("✕", m_titleBarWidget); m_btnWinClose->setFixedSize(45, 30); m_btnWinClose->setStyleSheet(closeStyle); connect(m_btnWinClose, &QPushButton::clicked, this, &MainWindow::onWinClose); layout->addWidget(m_btnWinClose);
}
void MainWindow::onWinMinimize() { showMinimized(); }
void MainWindow::onWinClose() { close(); }
void MainWindow::onWinMaximize() { if (isMaximized()) showNormal(); else showMaximized(); }
void MainWindow::mousePressEvent(QMouseEvent *event) { if (event->button() == Qt::LeftButton && m_titleBarWidget && m_titleBarWidget->geometry().contains(event->pos())) { m_windowDragPos = event->globalPosition().toPoint() - frameGeometry().topLeft(); event->accept(); return; } QMainWindow::mousePressEvent(event); }
void MainWindow::mouseMoveEvent(QMouseEvent *event) { if (event->buttons() & Qt::LeftButton && m_titleBarWidget && m_titleBarWidget->geometry().contains(mapFromGlobal(QCursor::pos())) && !isMaximized()) { move(event->globalPosition().toPoint() - m_windowDragPos); event->accept(); return; } QMainWindow::mouseMoveEvent(event); }

void MainWindow::onContentModified() { m_autoSaveTimer->start(); }
void MainWindow::performAutoSave() {
    CanvasView* cv = getCurrentCanvas();
    if (cv) {
        cv->saveToFile();
        updateSidebarBadges();
    }
}
CanvasView* MainWindow::getCurrentCanvas() { QWidget* current = m_editorTabs->currentWidget(); if (current) return qobject_cast<CanvasView*>(current); return nullptr; }
void MainWindow::onToggleFloatingTools() {}

void MainWindow::applyTheme() {
    QString c = m_currentAccentColor.name(); QString c_light = m_currentAccentColor.lighter(130).name();
    if (m_fileListView) m_fileListView->setAccentColor(m_currentAccentColor);
    QString style = QString("QMainWindow { background-color: #0F111A; } QListView { background: transparent; border: none; outline: 0; } QTabWidget::pane { border: 0; background: #0F111A; } QTabBar::tab { background: #161925; color: #888; padding: 10px 25px; border-top-left-radius: 12px; border-top-right-radius: 12px; } QTabBar::tab:selected { background: %1; color: white; } QMenu { background-color: #252526; border: 1px solid #444; border-radius: 10px; padding: 10px; color: #E0E0E0; } QMenu::item { padding: 8px 20px; border-radius: 5px; } QMenu::item:selected { background-color: %2; color: white; }").arg(c, c_light);
    this->setStyleSheet(style);

    if(m_sidebarContainer) m_sidebarContainer->setStyleSheet("background-color: #161925; border-right: 1px solid #1F2335;");
    if(m_sidebarStrip) m_sidebarStrip->setStyleSheet("background-color: #161925; border-right: 1px solid #1F2335;");
    if(m_navSidebar) m_navSidebar->setStyleSheet("QListWidget { background-color: transparent; border: none; outline: 0; margin-left: 5px; margin-right: 5px; } QListWidget::item { border: none; }");

    if(m_rightSidebar) m_rightSidebar->setStyleSheet("background-color: #161925; border-left: 1px solid #1F2335;");
    if(m_titleBarWidget) m_titleBarWidget->setStyleSheet("background-color: #0F111A; border-bottom: 1px solid #1F2335;");

    auto getFabStyle = [&](int w) {
        int r = w / 2;
        int fs = r + 4;
        return QString("QPushButton { background-color: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); color: white; border-radius: %3px; font-size: %4px; font-weight: 300; border: none; text-align: center; padding-bottom: 4px; }")
            .arg(c, c_light).arg(r).arg(fs);
    };

    if(m_fabNote) m_fabNote->setStyleSheet(getFabStyle(m_fabNote->width()));
    if(m_fabFolder) m_fabFolder->setStyleSheet(getFabStyle(m_fabFolder->width()));
}

void MainWindow::updateTheme(QColor accentColor) { m_currentAccentColor = accentColor; applyTheme(); }
void MainWindow::onItemSizeChanged(int value) { m_currentProfile.iconSize = value; updateGrid(); }
void MainWindow::onGridSpacingChanged(int value) { m_currentProfile.gridSpacing = value; updateGrid(); }

void MainWindow::updateGrid() {
    if (m_fileListView) {
        int s = m_currentProfile.iconSize;
        if (s <= 20) s = 20;

        QSize itemS(s, s);
        m_fileListView->setItemSize(itemS);
        m_fileListView->setIconSize(itemS);

        if (m_currentProfile.snapToGrid) {
            int gridW = itemS.width() + m_currentProfile.gridSpacing;
            int gridH = itemS.height() + m_currentProfile.gridSpacing;
            m_fileListView->setGridSize(QSize(gridW, gridH));
        } else {
            m_fileListView->setGridSize(QSize());
        }
    }
}

void MainWindow::applyProfile(const UiProfile &profile) {
    m_currentProfile = profile;
    updateGrid();
    if (m_fileListView) m_fileListView->viewport()->update();

    ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools);
    if (tb) {
        if (tb->currentStyle() == ModernToolbar::Radial) tb->setScale(profile.toolbarScale);
        if(m_sliderToolbarScale) {
            m_sliderToolbarScale->blockSignals(true);
            m_sliderToolbarScale->setValue(profile.toolbarScale * 100);
            m_sliderToolbarScale->blockSignals(false);
        }
    }

    int btnSize = profile.buttonSize;
    int iconSize = btnSize - 16;
    if (iconSize < 16) iconSize = 16;

    const auto buttons = this->findChildren<ModernButton*>();
    for (ModernButton* btn : buttons) {
        btn->setFixedSize(btnSize, btnSize);
        btn->setIconSize(QSize(iconSize, iconSize));
    }

    int fabS = btnSize + 16;
    if (m_fabNote) {
        m_fabNote->setFixedSize(fabS, fabS);
        QString c = m_currentAccentColor.name();
        QString c_light = m_currentAccentColor.lighter(130).name();
        int r = fabS / 2;
        int fs = r + 4;
        m_fabNote->setStyleSheet(QString("QPushButton { background-color: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); color: white; border-radius: %3px; font-size: %4px; font-weight: 300; border: none; text-align: center; padding-bottom: 4px; }")
                                     .arg(c, c_light).arg(r).arg(fs));
    }

    if (m_comboProfiles) {
        int idx = m_comboProfiles->findData(profile.id);
        if (idx != -1 && m_comboProfiles->currentIndex() != idx) {
            m_comboProfiles->blockSignals(true);
            m_comboProfiles->setCurrentIndex(idx);
            m_comboProfiles->blockSignals(false);
        }
    }

    QEvent resizeEv(QEvent::Resize);
    QApplication::sendEvent(this, &resizeEv);
    this->repaint();
}

QIcon MainWindow::createModernIcon(const QString &name, const QColor &color) {
    int size = 64; QPixmap pixmap(size, size); pixmap.fill(Qt::transparent); QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    if (name == "folder") { p.drawRect(14, 20, 36, 28); p.drawLine(14,20, 24,12); p.drawLine(24,12, 50,20); }
    else if (name == "cloud") { p.drawEllipse(12, 28, 16, 16); p.drawEllipse(24, 20, 20, 20); p.drawEllipse(40, 28, 14, 14); }
    else if (name == "share") { p.drawEllipse(44, 16, 8, 8); p.drawEllipse(44, 48, 8, 8); p.drawEllipse(16, 32, 8, 8); p.drawLine(22, 30, 42, 20); p.drawLine(22, 34, 42, 46); }
    else if (name == "device") { p.drawRoundedRect(20, 14, 24, 36, 4, 4); p.drawLine(20, 42, 44, 42); p.setPen(QPen(color, 2)); p.drawPoint(32, 46); }
    else if (name == "more_vert") { p.setBrush(color); p.setPen(Qt::NoPen); p.drawEllipse(28, 14, 8, 8); p.drawEllipse(28, 28, 8, 8); p.drawEllipse(28, 42, 8, 8); }
    else if (name == "arrow_left") { QPainterPath path; path.moveTo(42, 12); path.lineTo(20, 32); path.lineTo(42, 52); p.drawPath(path); }
    else if (name == "menu") { p.drawLine(12,20,52,20); p.drawLine(12,32,52,32); p.drawLine(12,44,52,44); }
    else if(name=="settings") { p.drawEllipse(20,20,24,24); p.drawLine(32,10,32,20); p.drawLine(32,44,32,54); p.drawLine(10,32,20,32); p.drawLine(44,32,54,32); }
    else if(name=="add") { p.drawLine(32, 14, 32, 50); p.drawLine(14, 32, 50, 32); }
    else if(name=="close") { p.drawLine(20,20,44,44); p.drawLine(44,20,20,44); }
    else if (name == "drive") { p.drawPolygon(QPolygonF() << QPointF(32, 10) << QPointF(54, 50) << QPointF(10, 50)); }
    else if (name == "dropbox") { p.drawLine(16, 24, 32, 36); p.drawLine(32, 36, 48, 24); p.drawLine(48, 24, 32, 12); p.drawLine(32, 12, 16, 24); p.drawLine(16, 24, 16, 40); p.drawLine(16, 40, 32, 52); p.drawLine(32, 52, 48, 40); p.drawLine(48, 40, 48, 24); }
    else if (name == "onedrive") { p.drawEllipse(20, 24, 16, 14); p.drawEllipse(36, 20, 14, 14); }
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
    setupTitleBar();
    m_centralContainer = new QWidget(this); setCentralWidget(m_centralContainer);
    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralContainer); mainLayout->setContentsMargins(0, 0, 0, 0); mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBarWidget);
    m_mainSplitter = new QSplitter(Qt::Horizontal, m_centralContainer); mainLayout->addWidget(m_mainSplitter);
    m_fileModel = new QFileSystemModel(this); m_fileModel->setRootPath(m_rootPath); m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot); m_fileModel->setReadOnly(false);

    connect(m_fileModel, &QFileSystemModel::rowsInserted, this, &MainWindow::updateSidebarBadges);
    connect(m_fileModel, &QFileSystemModel::rowsRemoved, this, &MainWindow::updateSidebarBadges);

    setupSidebar();
    m_rightStack = new QStackedWidget(this);
    m_overviewContainer = new QWidget(this); m_overviewContainer->installEventFilter(this);
    QVBoxLayout *overviewLayout = new QVBoxLayout(m_overviewContainer); overviewLayout->setContentsMargins(30, 30, 30, 30);

    QHBoxLayout *topBar = new QHBoxLayout();
    btnBackOverview = new ModernButton(this); btnBackOverview->setIcon(createModernIcon("arrow_left", Qt::white)); btnBackOverview->setToolTip("Zurück"); btnBackOverview->hide(); connect(btnBackOverview, &QAbstractButton::clicked, this, &MainWindow::onNavigateUp); topBar->addWidget(btnBackOverview);
    topBar->addStretch(); overviewLayout->addLayout(topBar);

    m_fileListView = new FreeGridView(this); m_fileListView->setModel(m_fileModel); m_fileListView->setRootIndex(m_fileModel->index(m_rootPath));
    m_fileListView->setSpacing(20); m_fileListView->setFrameShape(QFrame::NoFrame); m_fileListView->setItemDelegate(new ModernItemDelegate(this));
    connect(m_fileListView, &QListView::doubleClicked, this, &MainWindow::onFileDoubleClicked);
    m_fileListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileListView, &QWidget::customContextMenuRequested, [this](const QPoint &pos){ QModelIndex index = m_fileListView->indexAt(pos); if (index.isValid()) showContextMenu(m_fileListView->mapToGlobal(pos), index); });
    overviewLayout->addWidget(m_fileListView);

    m_fabNote = new QPushButton("+", m_overviewContainer); m_fabNote->setCursor(Qt::PointingHandCursor); m_fabNote->setFixedSize(56, 56);
    QGraphicsDropShadowEffect* shadow1 = new QGraphicsDropShadowEffect(this); shadow1->setBlurRadius(30); shadow1->setOffset(0, 8); shadow1->setColor(QColor(0,0,0,100)); m_fabNote->setGraphicsEffect(shadow1); connect(m_fabNote, &QPushButton::clicked, this, &MainWindow::onNewPage);

    m_editorContainer = new QWidget(this); m_editorContainer->installEventFilter(this);
    QHBoxLayout *editorMainLayout = new QHBoxLayout(m_editorContainer); editorMainLayout->setContentsMargins(0,0,0,0); editorMainLayout->setSpacing(0);
    m_editorCenterWidget = new QWidget(m_editorContainer); QVBoxLayout *centerLayout = new QVBoxLayout(m_editorCenterWidget); centerLayout->setContentsMargins(0,0,0,0); centerLayout->setSpacing(0);
    QHBoxLayout *editorHeader = new QHBoxLayout();
    btnEditorMenu = new ModernButton(this); btnEditorMenu->setIcon(createModernIcon("menu", Qt::white)); connect(btnEditorMenu, &QAbstractButton::clicked, this, &MainWindow::onToggleSidebar); editorHeader->addWidget(btnEditorMenu);
    ModernButton *backBtn = new ModernButton(this); backBtn->setIcon(createModernIcon("arrow_left", Qt::white)); connect(backBtn, &QAbstractButton::clicked, this, &MainWindow::onBackToOverview); editorHeader->addWidget(backBtn);
    editorHeader->addStretch();
    btnEditorSettings = new ModernButton(this); btnEditorSettings->setIcon(createModernIcon("settings", Qt::white)); connect(btnEditorSettings, &QAbstractButton::clicked, this, &MainWindow::onToggleRightSidebar); editorHeader->addWidget(btnEditorSettings);
    centerLayout->addLayout(editorHeader);
    m_editorTabs = new QTabWidget(this); m_editorTabs->setDocumentMode(true); m_editorTabs->setTabsClosable(true); connect(m_editorTabs, &QTabWidget::tabCloseRequested, [this](int index){ m_editorTabs->removeTab(index); if (m_editorTabs->count() == 0) onBackToOverview(); }); connect(m_editorTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged); centerLayout->addWidget(m_editorTabs);
    ModernToolbar* overlayToolbar = new ModernToolbar(m_editorCenterWidget); overlayToolbar->move(20, 80); overlayToolbar->hide(); m_floatingTools = overlayToolbar;
    connect(overlayToolbar, &ModernToolbar::toolChanged, [this](ToolMode m){ setActiveTool((CanvasView::ToolType)m); });
    connect(overlayToolbar, &ModernToolbar::undoRequested, this, &MainWindow::onUndo); connect(overlayToolbar, &ModernToolbar::redoRequested, this, &MainWindow::onRedo);
    connect(overlayToolbar, &ModernToolbar::penConfigChanged, [this](QColor c, int w){ m_penColor = c; m_penWidth = w; CanvasView* cv = getCurrentCanvas(); if(cv) { cv->setPenColor(c); cv->setPenWidth(w); } });
    connect(overlayToolbar, &ModernToolbar::scaleChanged, [this](qreal newScale){
        if(m_sliderToolbarScale) { m_sliderToolbarScale->blockSignals(true); m_sliderToolbarScale->setValue(newScale * 100); m_sliderToolbarScale->blockSignals(false); }
    });
    editorMainLayout->addWidget(m_editorCenterWidget);
    setupRightSidebar();
    editorMainLayout->addWidget(m_rightSidebar);
    m_rightStack->addWidget(m_overviewContainer); m_rightStack->addWidget(m_editorContainer); m_mainSplitter->addWidget(m_rightStack); m_mainSplitter->setStretchFactor(0, 0); m_mainSplitter->setStretchFactor(1, 1); m_mainSplitter->setCollapsible(0, true); m_mainSplitter->setStyleSheet("QSplitter::handle { background-color: transparent; }");
    setWindowTitle("Blop");
}

void MainWindow::setupSidebar() {
    m_sidebarStrip = new QWidget(this);
    m_sidebarStrip->setFixedWidth(60);
    m_sidebarStrip->hide();
    QVBoxLayout* stripLayout = new QVBoxLayout(m_sidebarStrip);
    stripLayout->setContentsMargins(0, 10, 0, 0); stripLayout->setSpacing(20);
    btnStripMenu = new ModernButton(m_sidebarStrip); btnStripMenu->setIcon(createModernIcon("menu", Qt::white)); btnStripMenu->setFixedSize(40, 40); connect(btnStripMenu, &QAbstractButton::clicked, this, &MainWindow::onToggleSidebar);
    stripLayout->addWidget(btnStripMenu, 0, Qt::AlignHCenter); stripLayout->addStretch();
    m_mainSplitter->addWidget(m_sidebarStrip);

    m_sidebarContainer = new QWidget(this); m_sidebarContainer->setFixedWidth(250);
    QVBoxLayout *layout = new QVBoxLayout(m_sidebarContainer); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);

    QWidget *header = new QWidget(m_sidebarContainer); header->setFixedHeight(70);
    QHBoxLayout *headerLay = new QHBoxLayout(header); headerLay->setContentsMargins(20, 20, 20, 0);
    QLabel *lblTitle = new QLabel("Blop Notizen", header); lblTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: white;"); headerLay->addWidget(lblTitle);
    m_closeSidebarBtn = new QPushButton("«", header); m_closeSidebarBtn->setFixedSize(24,24); m_closeSidebarBtn->setCursor(Qt::PointingHandCursor); m_closeSidebarBtn->setStyleSheet("QPushButton { background: transparent; color: #888; border: none; font-size: 16px; } QPushButton:hover { color: white; background: #333; border-radius: 4px; }");
    connect(m_closeSidebarBtn, &QPushButton::clicked, this, &MainWindow::onToggleSidebar); headerLay->addStretch(); headerLay->addWidget(m_closeSidebarBtn); layout->addWidget(header);

    m_navSidebar = new QListWidget(m_sidebarContainer); m_navSidebar->setItemDelegate(new SidebarNavDelegate(this)); m_navSidebar->setFrameShape(QFrame::NoFrame); m_navSidebar->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    QDir rootDir(m_rootPath); int rootCount = rootDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).count(); QString rootCountStr = QString::number(rootCount);
    auto addItem = [this, rootCountStr](QString name, QString icon, bool isHeader = false) {
        QListWidgetItem *item = new QListWidgetItem(m_navSidebar); item->setText(name); item->setData(Qt::UserRole + 9, 0);
        if (!isHeader) { item->setIcon(createModernIcon(icon, QColor(200, 200, 200))); if (name == "Alle" || name == "Blop Notizen") { item->setData(Qt::UserRole + 2, rootCountStr); item->setData(Qt::UserRole + 10, m_rootPath); } }
        item->setData(Qt::UserRole + 1, isHeader);
        if (isHeader) { item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); if (name == "Clouds") item->setData(Qt::UserRole + 4, "clouds_header"); } else { item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); if (name == "Blop Notizen" || name == "Alle") { item->setData(Qt::UserRole + 6, true); } if (name == "Google Drive" || name == "OneDrive" || name == "Dropbox") { item->setData(Qt::UserRole + 5, "clouds_item"); } }
    };

    addItem("Alle", "folder"); addItem("Blop Notizen", "folder"); addItem("Gerät Dateien", "device"); addItem("Von mir geteilt", "share"); addItem("Mit mir geteilt", "share");
    addItem("Clouds", "", true); addItem("Google Drive", "drive"); addItem("OneDrive", "onedrive"); addItem("Dropbox", "dropbox");
    m_navSidebar->setCurrentRow(1);
    connect(m_navSidebar, &QListWidget::itemClicked, this, &MainWindow::onNavItemClicked); layout->addWidget(m_navSidebar); updateSidebarBadges();

    QWidget *bottomBar = new QWidget(m_sidebarContainer); bottomBar->setFixedHeight(60);
    QHBoxLayout *bottomLay = new QHBoxLayout(bottomBar); bottomLay->setContentsMargins(15, 0, 15, 15);
    m_btnSidebarSettings = new QPushButton(bottomBar); m_btnSidebarSettings->setIcon(createModernIcon("settings", QColor(150,150,150))); m_btnSidebarSettings->setIconSize(QSize(24, 24)); m_btnSidebarSettings->setFixedSize(40, 40); m_btnSidebarSettings->setCursor(Qt::PointingHandCursor); m_btnSidebarSettings->setStyleSheet("QPushButton { background: #252526; border-radius: 20px; border: 1px solid #444; } QPushButton:hover { background: #333; border: 1px solid #555; }");
    connect(m_btnSidebarSettings, &QPushButton::clicked, this, &MainWindow::onOpenSettings); bottomLay->addWidget(m_btnSidebarSettings); bottomLay->addStretch(); layout->addWidget(bottomBar);

    m_fabFolder = new QPushButton("+", m_sidebarContainer); m_fabFolder->setCursor(Qt::PointingHandCursor); m_fabFolder->setFixedSize(40, 40);
    QGraphicsDropShadowEffect* shadowFolder = new QGraphicsDropShadowEffect(this); shadowFolder->setBlurRadius(20); shadowFolder->setOffset(0, 4); shadowFolder->setColor(QColor(0,0,0,80)); m_fabFolder->setGraphicsEffect(shadowFolder); connect(m_fabFolder, &QPushButton::clicked, this, &MainWindow::onCreateFolder); m_mainSplitter->addWidget(m_sidebarContainer);
}

void MainWindow::updateSidebarBadges() {
    QDir rootDir(m_rootPath); int rootCount = rootDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).count(); QString rootCountStr = QString::number(rootCount);
    for(int i = 0; i < m_navSidebar->count(); ++i) {
        QListWidgetItem* item = m_navSidebar->item(i);
        if (item->text() == "Alle" || item->text() == "Blop Notizen") { item->setData(Qt::UserRole + 2, rootCountStr); }
    }
}

void MainWindow::onNavItemClicked(QListWidgetItem *item) {
    if (!item) return;
    bool isHeader = item->data(Qt::UserRole + 1).toBool();
    if (isHeader) { if (item->text() == "Clouds") toggleSection(item); return; }
    QString path = item->data(Qt::UserRole + 10).toString();
    bool isExpandable = item->data(Qt::UserRole + 6).toBool();

    if (!path.isEmpty() && QFileInfo(path).isDir()) {
        m_fileModel->setRootPath(path);
        m_fileListView->setModel(m_fileModel);
        m_fileListView->setRootIndex(m_fileModel->index(path));
        m_fabNote->show(); m_fabFolder->show(); updateOverviewBackButton();
        if (isExpandable) toggleFolderContent(item);
        return;
    }

    if (!path.isEmpty() && QFileInfo(path).isFile()) {
        onFileDoubleClicked(m_fileModel->index(path));
        return;
    }

    QString name = item->text();
    if (name == "Gerät Dateien") {
        m_fileModel->setRootPath(QDir::rootPath());
        m_fileListView->setModel(m_fileModel);
        m_fileListView->setRootIndex(QModelIndex());
        m_fabNote->hide(); m_fabFolder->hide();
    }
    else if (name == "Google Drive" || name == "OneDrive" || name == "Dropbox") {
        m_fileListView->setRootIndex(QModelIndex());
        m_fabNote->hide(); m_fabFolder->hide();
        QMessageBox::information(this, "Cloud Integration", "Die Verbindung zu " + name + " wird in einer zukünftigen Version verfügbar sein.");
    }
}

void MainWindow::toggleSection(QListWidgetItem* headerItem) {
    bool isCollapsed = headerItem->data(Qt::UserRole + 3).toBool(); bool newState = !isCollapsed; headerItem->setData(Qt::UserRole + 3, newState);
    for(int i = 0; i < m_navSidebar->count(); ++i) {
        QListWidgetItem* item = m_navSidebar->item(i);
        if (item->data(Qt::UserRole + 5).toString() == "clouds_item") item->setHidden(newState);
    }
}

void MainWindow::toggleFolderContent(QListWidgetItem* parentItem) {
    bool isExpanded = parentItem->data(Qt::UserRole + 3).toBool(); QString parentPath = parentItem->data(Qt::UserRole + 10).toString();
    if(parentPath.isEmpty()) { if (parentItem->text() == "Blop Notizen" || parentItem->text() == "Alle") parentPath = m_rootPath; else return; }
    int currentDepth = parentItem->data(Qt::UserRole + 9).toInt();
    if (isExpanded) {
        int row = m_navSidebar->row(parentItem) + 1;
        while (row < m_navSidebar->count()) {
            QListWidgetItem* child = m_navSidebar->item(row); int childDepth = child->data(Qt::UserRole + 9).toInt();
            if (childDepth > currentDepth) delete m_navSidebar->takeItem(row); else break;
        }
        parentItem->setData(Qt::UserRole + 3, false);
    } else {
        QDir dir(parentPath); QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        int insertRow = m_navSidebar->row(parentItem) + 1;
        for (const QFileInfo& info : list) {
            QListWidgetItem* child = new QListWidgetItem(); child->setText(info.fileName()); child->setData(Qt::UserRole + 10, info.absoluteFilePath()); child->setData(Qt::UserRole + 9, currentDepth + 1); child->setData(Qt::UserRole + 8, parentItem->text());
            if (info.isDir()) { child->setIcon(createModernIcon("folder", QColor(180, 180, 180))); child->setData(Qt::UserRole + 6, true); child->setData(Qt::UserRole + 3, false); } else { QPixmap pix(64,64); pix.fill(Qt::transparent); QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing); p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(180,180,180), 2)); p.drawRoundedRect(16, 10, 32, 44, 4, 4); p.drawLine(20, 18, 44, 18); p.drawLine(20, 26, 44, 26); child->setIcon(QIcon(pix)); }
            m_navSidebar->insertItem(insertRow++, child);
        }
        parentItem->setData(Qt::UserRole + 3, true);
    }
}

void MainWindow::onNewPage() {
    NewNoteDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString name = dlg.getNoteName();
        bool isInfinite = dlg.isInfiniteFormat();

        // Dateinamen bereinigen
        QString safeName = name;
        safeName.replace("/", "_").replace("\\", "_");

        QString path = m_fileModel->rootPath() + "/" + safeName + ".blop";

        // Datei initial erstellen mit Header
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            QDataStream out(&file);
            out << (quint32)0xB10B0002; // Version 2
            out << isInfinite;          // Format Flag
            out << (int)0;              // 0 Items
            file.close();
        }
    }
}

void MainWindow::onCreateFolder() {
    bool ok; QString text = QInputDialog::getText(this, "Neuer Ordner", "Name:", QLineEdit::Normal, "Neuer Ordner", &ok);
    if (ok && !text.isEmpty()) { m_fileModel->mkdir(m_fileModel->index(m_fileModel->rootPath()), text); }
}

void MainWindow::onSidebarContextMenu(const QPoint &pos) {}

void MainWindow::performCopy(const QModelIndex &index) {
    QString srcPath = m_fileModel->filePath(index); QFileInfo info(srcPath); QString newName = info.baseName() + " Kopie"; if (!info.suffix().isEmpty()) newName += "." + info.suffix();
    QString dstPath = info.dir().filePath(newName); int i = 1; while (QFile::exists(dstPath)) { newName = info.baseName() + QString(" Kopie %1").arg(i++); if (!info.suffix().isEmpty()) newName += "." + info.suffix(); dstPath = info.dir().filePath(newName); }
    if (info.isDir()) { copyRecursive(srcPath, dstPath); } else { QFile::copy(srcPath, dstPath); }
}

bool MainWindow::copyRecursive(const QString &src, const QString &dst) {
    QDir dir(src); if (!dir.exists()) return false; QDir().mkpath(dst);
    for (const QString &entry : dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString srcItem = src + "/" + entry; QString dstItem = dst + "/" + entry;
        if (QFileInfo(srcItem).isDir()) { if (!copyRecursive(srcItem, dstItem)) return false; } else { if (!QFile::copy(srcItem, dstItem)) return false; }
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
    m_rightSidebar = new QWidget(this); m_rightSidebar->setFixedWidth(250); m_rightSidebar->hide();
    QVBoxLayout *layout = new QVBoxLayout(m_rightSidebar); layout->setContentsMargins(20, 20, 20, 20); layout->setSpacing(15);
    QHBoxLayout *header = new QHBoxLayout; QLabel *title = new QLabel("Einstellungen"); title->setStyleSheet("font-size: 16px; font-weight: bold; color: white;"); header->addWidget(title); header->addStretch();
    ModernButton *closeBtn = new ModernButton(this); closeBtn->setIcon(createModernIcon("close", Qt::white)); closeBtn->setFixedSize(30,30); connect(closeBtn, &QAbstractButton::clicked, this, &MainWindow::onToggleRightSidebar); header->addWidget(closeBtn); layout->addLayout(header);
    m_lblActiveNote = new QLabel("Keine Notiz", m_rightSidebar); m_lblActiveNote->setStyleSheet("color: #5E5CE6; font-size: 14px; font-weight: bold; margin-bottom: 5px;"); m_lblActiveNote->setWordWrap(true); m_lblActiveNote->setAlignment(Qt::AlignCenter); layout->addWidget(m_lblActiveNote);

    layout->addWidget(new QLabel("Seitenformat (Festgelegt):", m_rightSidebar));

    // Buttons nur noch zur Anzeige (disabled), da Immutable
    m_btnFormatInfinite = new QPushButton("Unendlich");
    m_btnFormatInfinite->setCheckable(true);
    m_btnFormatInfinite->setEnabled(false); // Immutable!
    m_btnFormatInfinite->setStyleSheet("QPushButton { background: #333; color: #888; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; color: white; border: 1px solid #5E5CE6; }");

    m_btnFormatA4 = new QPushButton("DIN A4");
    m_btnFormatA4->setCheckable(true);
    m_btnFormatA4->setEnabled(false); // Immutable!
    m_btnFormatA4->setStyleSheet("QPushButton { background: #333; color: #888; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; color: white; border: 1px solid #5E5CE6; }");

    QButtonGroup *grp = new QButtonGroup(this);
    grp->addButton(m_btnFormatInfinite);
    grp->addButton(m_btnFormatA4);
    grp->setExclusive(true);

    layout->addWidget(m_btnFormatInfinite);
    layout->addWidget(m_btnFormatA4);

    layout->addWidget(new QLabel("Seitenlayout:", m_rightSidebar));
    m_btnStyleBlank = new QPushButton("Leer"); m_btnStyleLined = new QPushButton("Liniert"); m_btnStyleSquared = new QPushButton("Kariert"); m_btnStyleDotted = new QPushButton("Gepunktet");
    m_btnStyleBlank->setCheckable(true); m_btnStyleLined->setCheckable(true); m_btnStyleSquared->setCheckable(true); m_btnStyleDotted->setCheckable(true);
    QString btnStyleTemplate = "QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: %1; border: 1px solid %1; }"; QString accentColorName = m_currentAccentColor.name(); QString styleSheet = btnStyleTemplate.arg(accentColorName);
    m_btnStyleBlank->setStyleSheet(styleSheet); m_btnStyleLined->setStyleSheet(styleSheet); m_btnStyleSquared->setStyleSheet(styleSheet); m_btnStyleDotted->setStyleSheet(styleSheet);
    m_grpPageStyle = new QButtonGroup(this); m_grpPageStyle->addButton(m_btnStyleBlank, (int)PageStyle::Blank); m_grpPageStyle->addButton(m_btnStyleLined, (int)PageStyle::Lined); m_grpPageStyle->addButton(m_btnStyleSquared, (int)PageStyle::Squared); m_grpPageStyle->addButton(m_btnStyleDotted, (int)PageStyle::Dotted); m_grpPageStyle->setExclusive(true);
    connect(m_grpPageStyle, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, &MainWindow::onPageStyleButtonToggled);
    QWidget* styleBtnsContainer = new QWidget(m_rightSidebar); QHBoxLayout* styleBtnsLayout = new QHBoxLayout(styleBtnsContainer); styleBtnsLayout->setContentsMargins(0,0,0,0); styleBtnsLayout->addWidget(m_btnStyleBlank); styleBtnsLayout->addWidget(m_btnStyleLined); styleBtnsLayout->addWidget(m_btnStyleSquared); styleBtnsLayout->addWidget(m_btnStyleDotted); layout->addWidget(styleBtnsContainer);

    layout->addWidget(new QLabel("Raster Abstand (px):", m_rightSidebar));
    m_sliderGridSpacing = new QSlider(Qt::Horizontal); m_sliderGridSpacing->setRange(10, 80); m_sliderGridSpacing->setValue(40); m_sliderGridSpacing->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    connect(m_sliderGridSpacing, &QSlider::valueChanged, this, &MainWindow::onPageGridSpacingSliderChanged); layout->addWidget(m_sliderGridSpacing);

    layout->addWidget(new QLabel("Seitenfarbe:", m_rightSidebar));
    m_btnColorWhite = new QPushButton("Hell"); m_btnColorWhite->setCheckable(true); m_btnColorWhite->setChecked(true); m_btnColorWhite->setCursor(Qt::PointingHandCursor); m_btnColorWhite->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnColorWhite, &QPushButton::clicked, [this](){ setPageColor(false); });
    m_btnColorDark = new QPushButton("Dunkel"); m_btnColorDark->setCheckable(true); m_btnColorDark->setCursor(Qt::PointingHandCursor); m_btnColorDark->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnColorDark, &QPushButton::clicked, [this](){ setPageColor(true); });
    QButtonGroup *grpColor = new QButtonGroup(this); grpColor->addButton(m_btnColorWhite); grpColor->addButton(m_btnColorDark); grpColor->setExclusive(true); layout->addWidget(m_btnColorWhite); layout->addWidget(m_btnColorDark);

    layout->addWidget(new QLabel("Eingabemodus:", m_rightSidebar));
    m_btnInputPen = new QPushButton("Nur Stift\n(1 Finger scrollt)"); m_btnInputPen->setCheckable(true); m_btnInputPen->setChecked(true); m_btnInputPen->setCursor(Qt::PointingHandCursor); m_btnInputPen->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; text-align: left; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnInputPen, &QPushButton::clicked, [this](){ updateInputMode(true); });
    m_btnInputTouch = new QPushButton("Touch & Stift\n(2 Finger scrollen)"); m_btnInputTouch->setCheckable(true); m_btnInputTouch->setCursor(Qt::PointingHandCursor); m_btnInputTouch->setStyleSheet("QPushButton { background: #333; color: white; border: 1px solid #444; padding: 10px; border-radius: 5px; text-align: left; } QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; }");
    connect(m_btnInputTouch, &QPushButton::clicked, [this](){ updateInputMode(false); });
    QButtonGroup *grpInput = new QButtonGroup(this); grpInput->addButton(m_btnInputPen); grpInput->addButton(m_btnInputTouch); grpInput->setExclusive(true); layout->addWidget(m_btnInputPen); layout->addWidget(m_btnInputTouch);

    layout->addWidget(new QLabel("Werkzeugleiste:", m_rightSidebar));
    m_comboToolbarStyle = new QComboBox(); m_comboToolbarStyle->addItems({"Vertikal", "Radial (Voll)", "Radial (Halb)"}); m_comboToolbarStyle->setStyleSheet("QComboBox { background: #333; color: white; border: 1px solid #444; padding: 5px; border-radius: 5px; } QComboBox::drop-down { border: 0px; } QComboBox QAbstractItemView { background: #333; color: white; selection-background-color: #5E5CE6; }"); m_comboToolbarStyle->setCursor(Qt::PointingHandCursor);
    connect(m_comboToolbarStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index){ ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools); if (tb) { if (index == 0) tb->setStyle(ModernToolbar::Normal); else if (index == 1) { tb->setStyle(ModernToolbar::Radial); tb->setRadialType(ModernToolbar::FullCircle); } else { tb->setStyle(ModernToolbar::Radial); tb->setRadialType(ModernToolbar::HalfEdge); } } });
    layout->addWidget(m_comboToolbarStyle);

    layout->addWidget(new QLabel("UI-Profil:", m_rightSidebar));
    m_comboProfiles = new QComboBox(); m_comboProfiles->setStyleSheet(m_comboToolbarStyle->styleSheet()); m_comboProfiles->setCursor(Qt::PointingHandCursor);
    for (const auto& p : m_profileManager->profiles()) { m_comboProfiles->addItem(p.name, p.id); } m_comboProfiles->setCurrentText(m_currentProfile.name);
    connect(m_comboProfiles, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int){ QString id = m_comboProfiles->currentData().toString(); m_profileManager->setCurrentProfile(id); });
    connect(m_profileManager, &UiProfileManager::listChanged, [this](){ m_comboProfiles->blockSignals(true); m_comboProfiles->clear(); for (const auto& p : m_profileManager->profiles()) { m_comboProfiles->addItem(p.name, p.id); } int idx = m_comboProfiles->findData(m_currentProfile.id); if(idx != -1) m_comboProfiles->setCurrentIndex(idx); m_comboProfiles->blockSignals(false); });
    layout->addWidget(m_comboProfiles);

    layout->addWidget(new QLabel("Größe Werkzeugleiste:", m_rightSidebar));
    m_sliderToolbarScale = new QSlider(Qt::Horizontal); m_sliderToolbarScale->setRange(50, 150); m_sliderToolbarScale->setValue(100); m_sliderToolbarScale->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    connect(m_sliderToolbarScale, &QSlider::valueChanged, [this](int val){ ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools); if(tb) tb->setScale(val / 100.0); }); layout->addWidget(m_sliderToolbarScale);

    layout->addStretch();
}

void MainWindow::updateInputMode(bool penOnly) { m_penOnlyMode = penOnly; CanvasView* cv = getCurrentCanvas(); if (cv) cv->setPenMode(penOnly); }

void MainWindow::onPageStyleButtonToggled(QAbstractButton *button, bool checked) {
    if (!checked) return;
    PageStyle style = (PageStyle)m_grpPageStyle->id(button);
    CanvasView* cv = getCurrentCanvas(); if (cv) { cv->setPageStyle(style); cv->setGridSize(m_sliderGridSpacing->value()); }
}

void MainWindow::onPageGridSpacingSliderChanged(int value) { m_gridSpacingTimer->start(); }
void MainWindow::applyDelayedGridSpacing() { CanvasView* cv = getCurrentCanvas(); if (cv) { cv->setGridSize(m_sliderGridSpacing->value()); } }

void MainWindow::animateSidebar(bool show) {
    int start = show ? 0 : 250; int end = show ? 250 : 0; m_isSidebarOpen = show; if (show) { m_sidebarContainer->setVisible(true); updateSidebarState(); }
    QPropertyAnimation *anim = new QPropertyAnimation(m_sidebarContainer, "maximumWidth"); anim->setDuration(250); anim->setStartValue(start); anim->setEndValue(end); anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, [this, show](){ if (!show) { m_sidebarContainer->hide(); updateSidebarState(); } }); anim->start(QAbstractAnimation::DeleteWhenStopped);
}
void MainWindow::onToggleSidebar() { bool isVisible = m_sidebarContainer->isVisible() && m_sidebarContainer->width() > 0; animateSidebar(!isVisible); }
void MainWindow::updateSidebarState() { bool isEditor = (m_rightStack->currentWidget() == m_editorContainer); if (m_floatingTools) { m_floatingTools->setVisible(isEditor); } if (m_isSidebarOpen) { m_sidebarStrip->hide(); btnEditorMenu->hide(); return; } if (isEditor) { m_sidebarStrip->hide(); btnEditorMenu->show(); } else { m_sidebarStrip->show(); btnEditorMenu->hide(); } }
void MainWindow::animateRightSidebar(bool show) { int start = show ? 0 : 250; int end = show ? 250 : 0; m_rightSidebar->setVisible(true); QPropertyAnimation *anim = new QPropertyAnimation(m_rightSidebar, "maximumWidth"); anim->setDuration(250); anim->setStartValue(start); anim->setEndValue(end); anim->setEasingCurve(QEasingCurve::OutCubic); connect(anim, &QPropertyAnimation::finished, [this, show](){ if (!show) m_rightSidebar->hide(); }); anim->start(QAbstractAnimation::DeleteWhenStopped); }

void MainWindow::onToggleRightSidebar() {
    bool isVisible = m_rightSidebar->isVisible() && m_rightSidebar->width() > 0;
    animateRightSidebar(!isVisible);
    if (!isVisible) {
        CanvasView *cv = getCurrentCanvas();
        if (cv) {
            bool inf = cv->isInfinite();
            // Nur noch Anzeige, kein Signal-Trigger
            m_btnFormatInfinite->setChecked(inf);
            m_btnFormatA4->setChecked(!inf);

            m_btnInputPen->setChecked(m_penOnlyMode); m_btnInputTouch->setChecked(!m_penOnlyMode);
            bool isDark = (cv->pageColor() == UIStyles::SceneBackground); m_btnColorDark->setChecked(isDark); m_btnColorWhite->setChecked(!isDark);
            int styleId = (int)cv->pageStyle(); QAbstractButton *styleBtn = m_grpPageStyle->button(styleId); if (styleBtn) styleBtn->setChecked(true);
            m_sliderGridSpacing->blockSignals(true); m_sliderGridSpacing->setValue(cv->gridSize()); m_sliderGridSpacing->blockSignals(false);
        }
        ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools);
        if (tb) {
            if (tb->currentStyle() == ModernToolbar::Normal) { m_comboToolbarStyle->setCurrentIndex(0); }
            else { if (tb->radialType() == ModernToolbar::FullCircle) m_comboToolbarStyle->setCurrentIndex(1); else m_comboToolbarStyle->setCurrentIndex(2); }
            if(m_sliderToolbarScale) m_sliderToolbarScale->setValue(tb->scale() * 100);
        }
    }
}

void MainWindow::onFileDoubleClicked(const QModelIndex &index) {
    if (m_fileModel->isDir(index)) { m_fileListView->setRootIndex(index); m_fileModel->setRootPath(m_fileModel->filePath(index)); updateOverviewBackButton(); } else {
        CanvasView *canvas = new CanvasView(this); canvas->setPenColor(m_penColor); canvas->setPenWidth(m_penWidth); canvas->setPenMode(m_penOnlyMode); canvas->setFilePath(m_fileModel->filePath(index)); canvas->loadFromFile(); connect(canvas, &CanvasView::contentModified, this, &MainWindow::onContentModified);
        m_editorTabs->addTab(canvas, index.data().toString()); m_editorTabs->setCurrentWidget(canvas); m_rightStack->setCurrentWidget(m_editorContainer); setActiveTool(m_activeToolType); updateSidebarState();
    }
}
void MainWindow::onBackToOverview() { m_rightStack->setCurrentWidget(m_overviewContainer); updateSidebarState(); }
void MainWindow::showContextMenu(const QPoint &globalPos, const QModelIndex &index) { QMenu menu(this); menu.addAction("Öffnen", [this, index](){ onFileDoubleClicked(index); }); menu.addAction("Umbenennen", [this, index](){ startRename(index); }); menu.addAction("Löschen", [this, index](){ m_fileModel->remove(index); }); menu.exec(globalPos); }
void MainWindow::startRename(const QModelIndex &index) { m_indexToRename = index; showRenameOverlay(index.data().toString()); }
void MainWindow::showRenameOverlay(const QString &currentName) { if (!m_renameOverlay) { m_renameOverlay = new QWidget(this); m_renameOverlay->resize(size()); m_renameOverlay->setStyleSheet("background-color: rgba(0,0,0,200);"); m_renameInput = new QLineEdit(m_renameOverlay); m_renameInput->setFixedSize(300, 40); m_renameInput->setStyleSheet("QLineEdit { background: #333; color: white; border: 1px solid #555; font-size: 16px; padding: 5px; }"); connect(m_renameInput, &QLineEdit::returnPressed, this, &MainWindow::finishRename); } m_renameInput->move(width()/2 - 150, height()/2 - 20); m_renameInput->setText(currentName); m_renameOverlay->show(); m_renameInput->setFocus(); }
void MainWindow::finishRename() { if (m_renameOverlay && m_indexToRename.isValid()) { QString newName = m_renameInput->text(); if (!newName.isEmpty()) { m_fileModel->setData(m_indexToRename, newName, Qt::EditRole); } m_renameOverlay->hide(); } }
bool MainWindow::eventFilter(QObject *obj, QEvent *event) { return QMainWindow::eventFilter(obj, event); }
void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    int fabOffset = isTouchMode() ? 100 : 80;
    if (m_fabNote && m_overviewContainer) { m_fabNote->move(m_overviewContainer->width() - fabOffset, m_overviewContainer->height() - fabOffset); m_fabNote->raise(); }
    if (m_fabFolder && m_sidebarContainer) { int sidebarW = m_sidebarContainer->width(); int sidebarH = m_sidebarContainer->height(); int btnS = m_fabFolder->width(); m_fabFolder->move(sidebarW - btnS - 20, sidebarH - btnS - 20); m_fabFolder->raise(); }
    if (m_renameOverlay) { m_renameOverlay->resize(size()); if (m_renameInput) m_renameInput->move(width()/2 - 150, height()/2 - 20); }
    if (m_editorTabs && m_floatingTools) { int top = 0; if (m_editorTabs->tabBar()) { QPoint tabsPos = m_editorTabs->mapTo(m_editorCenterWidget, QPoint(0,0)); top = tabsPos.y() + m_editorTabs->tabBar()->height(); } ModernToolbar* tb = qobject_cast<ModernToolbar*>(m_floatingTools); if (tb) tb->setTopBound(top); }
}

void MainWindow::onOpenSettings() {
    QWidget *overlay = new QWidget(this); overlay->setGeometry(this->rect()); overlay->setStyleSheet("background-color: rgba(0, 0, 0, 150);"); overlay->show();
    bool keepOpen = true;
    while (keepOpen) {
        SettingsDialog dlg(m_profileManager, this);
        ModernToolbar* toolbar = qobject_cast<ModernToolbar*>(m_floatingTools);
        if(toolbar) { bool isRad = (toolbar->currentStyle() == ModernToolbar::Radial); bool isHalf = (toolbar->radialType() == ModernToolbar::HalfEdge); dlg.setToolbarConfig(isRad, isHalf); }
        connect(&dlg, &SettingsDialog::accentColorChanged, this, &MainWindow::updateTheme);
        connect(&dlg, &SettingsDialog::toolbarStyleChanged, [this, toolbar](bool radial){ if(toolbar) toolbar->setStyle(radial ? ModernToolbar::Radial : ModernToolbar::Normal); });
        int res = dlg.exec();
        if (res == SettingsDialog::EditProfileCode) {
            overlay->hide(); QString id = dlg.profileIdToEdit(); UiProfile p = m_profileManager->profileById(id); UiProfile original = p;
            ProfileEditorDialog editor(p, this); connect(&editor, &ProfileEditorDialog::previewRequested, this, &MainWindow::applyProfile);
            if (editor.exec() == QDialog::Accepted) { m_profileManager->updateProfile(editor.getProfile(), true); applyProfile(editor.getProfile()); }
            else { m_profileManager->updateProfile(original, true); applyProfile(m_profileManager->currentProfile()); }
            overlay->show();
        } else { keepOpen = false; }
    }
    delete overlay;
}

void MainWindow::setPageColor(bool dark) { CanvasView *cv = getCurrentCanvas(); if (cv) { cv->setPageColor(dark ? UIStyles::SceneBackground : UIStyles::PageBackground); } }
void MainWindow::setActiveTool(CanvasView::ToolType tool) { m_activeToolType = tool; ModernToolbar* toolbar = qobject_cast<ModernToolbar*>(m_floatingTools); if(toolbar) { toolbar->setToolMode((ToolMode)tool); } CanvasView *cv = getCurrentCanvas(); if (cv) { cv->setTool(tool); } }
void MainWindow::switchToSelectTool() { onToolSelect(); }
void MainWindow::onToolSelect() { setActiveTool(CanvasView::ToolType::Select); }
void MainWindow::onToolPen() { if (m_activeToolType == CanvasView::ToolType::Pen) {} else { setActiveTool(CanvasView::ToolType::Pen); } }
void MainWindow::onToolEraser() { setActiveTool(CanvasView::ToolType::Eraser); }
void MainWindow::onToolLasso() { setActiveTool(CanvasView::ToolType::Lasso); }
void MainWindow::onUndo() { CanvasView *cv = getCurrentCanvas(); if (cv) cv->undo(); }
void MainWindow::onRedo() { CanvasView *cv = getCurrentCanvas(); if (cv) cv->redo(); }
void MainWindow::onTabChanged(int index) {
    CanvasView *cv = getCurrentCanvas(); if (cv) { cv->setPenMode(m_penOnlyMode); } setActiveTool(m_activeToolType);
    if (m_lblActiveNote) { QString text = "Keine Notiz"; if (index >= 0) { text = m_editorTabs->tabText(index); } m_lblActiveNote->setText(text); }
    if (m_rightSidebar && m_rightSidebar->isVisible()) {
        if (cv) {
            bool inf = cv->isInfinite(); m_btnFormatInfinite->setChecked(inf); m_btnFormatA4->setChecked(!inf); m_btnInputPen->setChecked(m_penOnlyMode); m_btnInputTouch->setChecked(!m_penOnlyMode);
            bool isDark = (cv->pageColor() == UIStyles::SceneBackground); m_btnColorDark->setChecked(isDark); m_btnColorWhite->setChecked(!isDark);
            int styleId = (int)cv->pageStyle(); QAbstractButton *styleBtn = m_grpPageStyle->button(styleId); if (styleBtn) styleBtn->setChecked(true);
            m_sliderGridSpacing->blockSignals(true); m_sliderGridSpacing->setValue(cv->gridSize()); m_sliderGridSpacing->blockSignals(false);
        }
    }
    updateSidebarState();
}
