#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTreeView>
#include <QListView>
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

#include "freegridview.h"
#include "canvasview.h"
#include "UiProfileManager.h"

class MainWindow;

// --- SidebarItemDelegate ---
class SidebarItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SidebarItemDelegate(MainWindow *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

private:
    MainWindow *m_window;
};

// --- ModernButton ---
class ModernButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(float scale READ scale WRITE setScale)
public:
    explicit ModernButton(QWidget *parent = nullptr);
    float scale() const { return m_scale; }
    void setScale(float s);
    void setAccentColor(QColor c);
protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
private:
    float m_scale;
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
    friend class SidebarItemDelegate;

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
    void onNewPage();
    void onCreateFolder();

    void onToggleSidebar();
    void onOpenSettings();

    void updateTheme(QColor accentColor);
    void updateInputMode(bool penOnly); // Hier fehlte wahrscheinlich die Deklaration
    void applyProfile(const UiProfile& profile);

    void onFolderSelected(const QModelIndex &index);
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
    void setCanvasFormat(bool infinite);
    void setPageColor(bool dark);

    void onContentModified();
    void performAutoSave();

    void onNavigateUp();

    void onWinMinimize();
    void onWinMaximize();
    void onWinClose();

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

    CanvasView* getCurrentCanvas();
    void setActiveTool(CanvasView::ToolType tool);

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

    bool m_isSidebarOpen;

    QWidget *m_sidebarContainer;
    QWidget *m_sidebarHeader;
    QTreeView *m_folderTree;
    QFileSystemModel *m_fileModel;
    QPushButton *m_fabFolder;
    QPushButton *m_closeSidebarBtn;
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

    QComboBox *m_comboProfiles;
    QComboBox *m_comboToolbarStyle;
    QSlider *m_sliderToolbarScale; // Hier fehlte wahrscheinlich die Deklaration

    QWidget *m_floatingTools;

    ModernButton *btnSettings;
    ModernButton *btnEditorSettings;
    ModernButton *btnBackOverview;
    ModernButton *btnNewNote;

    QString m_rootPath;
    QColor m_currentAccentColor;
    QColor m_penColor;
    int m_penWidth;

    int m_currentItemSize;
    int m_currentSpacing;

    bool m_penOnlyMode;
    bool m_touchMode;

    QTimer *m_autoSaveTimer;

    CanvasView::ToolType m_activeToolType;

    QModelIndex m_indexToRename;
    QWidget *m_renameOverlay;
    QLineEdit *m_renameInput;
};

#endif // MAINWINDOW_H
