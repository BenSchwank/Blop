#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTreeView>
#include <QListView>
#include <QListWidget>
#include <QFileSystemModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QPushButton>
#include <QIcon>
#include <QColor>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QButtonGroup>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "freegridview.h"
#include "canvasview.h"
#include "uiprofilemanager.h"

// Forward Declarations
class MainWindow;
class PageManager;

// --- SidebarNavDelegate ---
class SidebarNavDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SidebarNavDelegate(MainWindow *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    MainWindow *m_window;
};

// --- ModernButton ---
class ModernButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(double buttonScale READ buttonScale WRITE setButtonScale)
public:
    explicit ModernButton(QWidget *parent = nullptr);

    double buttonScale() const { return m_scale; }
    void setButtonScale(double s);

    void setAccentColor(QColor c);
protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
private:
    double m_scale;
    QPropertyAnimation *m_anim;
    QColor m_accentColor;
};

// --- ModernItemDelegate ---
class ModernItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ModernItemDelegate(MainWindow *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;
private:
    MainWindow *m_window;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class ModernItemDelegate;
    friend class SidebarNavDelegate;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QIcon createModernIcon(const QString &name, const QColor &color);
    QColor currentAccentColor() const { return m_currentAccentColor; }

    bool isTouchMode() const { return m_currentProfile.isTouchOptimized(); }

    void showContextMenu(const QPoint &globalPos, const QModelIndex &index);
    void startRename(const QModelIndex &index);

    void switchToSelectTool();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void checkForUpdates();

    void onNewPage();
    void onCreateFolder();

    void onToggleSidebar();
    void onOpenSettings();

    void updateTheme(QColor accentColor);
    void updateInputMode(bool penOnly);
    void applyProfile(const UiProfile& profile);

    void onNavItemClicked(QListWidgetItem *item);
    void onFileDoubleClicked(const QModelIndex &index);
    void onBackToOverview();

    void onSidebarContextMenu(const QPoint &pos);

    void onToolSelect();
    void onToolPen();
    void onToolEraser();
    void onToolLasso();
    void onUndo();
    void onRedo();

    void onToggleFloatingTools();
    void finishRename();

    void onItemSizeChanged(int value);
    void onGridSpacingChanged(int value);

    void onTabChanged(int index);

    void onToggleRightSidebar();
    void onTogglePageManager(); // NEU

    void setPageColor(bool dark);

    void onPageStyleButtonToggled(QAbstractButton *button, bool checked);
    void onPageGridSpacingSliderChanged(int value);
    void applyDelayedGridSpacing();

    void onContentModified();
    void performAutoSave();

    void updateSidebarBadges();

    void onNavigateUp();

    void onWinMinimize();
    void onWinMaximize();
    void onWinClose();

    // Neuer Slot für Web/Notiz Umschaltung
    void onModeChanged(int index);

private:
    void setupUi();
    void setupTitleBar();
    void setupSidebar();
    void setupRightSidebar();
    void createDefaultFolder();
    void applyTheme();

    void updateGrid();
    void updateSidebarState();
    void updateOverviewBackButton();

    void showRenameOverlay(const QString &currentName);

    void animateSidebar(bool show);
    void animateRightSidebar(bool show);

    void performCopy(const QModelIndex &index);
    bool copyRecursive(const QString &src, const QString &dst);

    void toggleSection(QListWidgetItem* headerItem);
    void toggleFolderContent(QListWidgetItem* parentItem);

    CanvasView* getCurrentCanvas();
    void setActiveTool(CanvasView::ToolType tool);

    // --- NEU: Web Integration ---
    void setupWebBrowser();
    QWidget* m_studyContainer; // Container für Web View
    QStackedWidget* m_mainContentStack; // Wechselt zwischen Notizen und Study
    QComboBox* m_modeSelector; // Dropdown "Notizen" vs "Study"
    // ----------------------------

    UiProfileManager *m_profileManager;
    UiProfile m_currentProfile;

    QWidget *m_centralContainer;

    QWidget *m_titleBarWidget;
    QPushButton *m_btnWinMin;
    QPushButton *m_btnWinMax;
    QPushButton *m_btnWinClose;
    QPoint m_windowDragPos;

    QSplitter *m_mainSplitter;
    QStackedWidget *m_rightStack;

    QWidget *m_sidebarStrip;
    ModernButton *btnStripMenu;
    ModernButton *btnEditorMenu;

    ModernButton *btnOverviewMenu;

    bool m_isSidebarOpen;

    QWidget *m_sidebarContainer;
    QListWidget *m_navSidebar;
    QFileSystemModel *m_fileModel;
    QPushButton *m_fabFolder;
    QPushButton *m_closeSidebarBtn;

    QPushButton *m_btnSidebarSettings;

    QWidget *m_overviewContainer;
    FreeGridView *m_fileListView;
    QPushButton *m_fabNote;

    QWidget *m_editorContainer;
    QWidget *m_editorCenterWidget;
    QTabWidget *m_editorTabs;

    QWidget *m_rightSidebar;
    QLabel *m_lblActiveNote;
    QPushButton *m_btnFormatInfinite;
    QPushButton *m_btnFormatA4;
    QPushButton *m_btnColorWhite;
    QPushButton *m_btnColorDark;
    QPushButton *m_btnInputPen;
    QPushButton *m_btnInputTouch;
    QPushButton *m_btnUiDesktop;
    QPushButton *m_btnUiTouch;

    QButtonGroup *m_grpPageStyle;
    QPushButton *m_btnStyleBlank;
    QPushButton *m_btnStyleLined;
    QPushButton *m_btnStyleSquared;
    QPushButton *m_btnStyleDotted;
    QSlider *m_sliderGridSpacing;

    QComboBox *m_comboProfiles;
    QComboBox *m_comboToolbarStyle;
    QSlider *m_sliderToolbarScale;

    QWidget *m_floatingTools;
    PageManager *m_pageManager; // NEU: Member für den PageManager

    ModernButton *btnEditorSettings;
    ModernButton *m_btnPages; // NEU: Button für PageManager
    ModernButton *btnBackOverview;

    QString m_rootPath;
    QColor m_currentAccentColor;
    QColor m_penColor;
    int m_penWidth;

    int m_currentItemSize;
    int m_currentSpacing;

    bool m_penOnlyMode;
    bool m_touchMode;

    QTimer *m_autoSaveTimer;
    QTimer *m_gridSpacingTimer;

    CanvasView::ToolType m_activeToolType;

    QModelIndex m_indexToRename;
    QWidget *m_renameOverlay;
    QLineEdit *m_renameInput;

    QNetworkAccessManager *m_netManager{nullptr};
};

#endif // MAINWINDOW_H
