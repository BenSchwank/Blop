#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QToolBar>
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

#include "freegridview.h"
#include "canvasview.h"

class MainWindow;

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

// --- Drag Handle ---
class DragHandle : public QWidget
{
    Q_OBJECT
public:
    explicit DragHandle(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
private:
    QPoint m_dragStartPosition;
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

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QIcon createModernIcon(const QString &name, const QColor &color);
    QColor currentAccentColor() const { return m_currentAccentColor; }

    bool isTouchMode() const { return m_touchMode; }

    void moveFloatingToolbar(const QPoint &diff);

    void showContextMenu(const QPoint &globalPos, const QModelIndex &index);
    void startRename(const QModelIndex &index);

    void switchToSelectTool();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onNewPage();
    void onCreateFolder();

    void onToggleFormat();
    void onToggleSidebar();
    void onOpenSettings();

    void updateTheme(QColor accentColor);
    void updateInputMode(bool penOnly);
    void updateUiMode(bool touchMode);

    void onFolderSelected(const QModelIndex &index);
    void onFileDoubleClicked(const QModelIndex &index);
    void onBackToOverview();

    void onToolSelect();
    void onToolPen();
    void onToolEraser();
    void onToolLasso();
    void onUndo();
    void onRedo();

    void showPenSettingsMenu();

    void onToggleFloatingTools();
    void finishRename();

    // NEU: Slots für die 2 Slider (Grid Size & Spacing)
    void onItemSizeChanged(int value);
    void onGridSpacingChanged(int value);
    // HINWEIS: onGridSizeChanged wurde entfernt!

    void onTabChanged(int index);

    void onToggleRightSidebar();
    void setCanvasFormat(bool infinite);
    void setPageColor(bool dark);

    void onContentModified();
    void performAutoSave();

private:
    void setupUi();
    void setupToolbar();
    void setupSidebar();
    void setupRightSidebar();
    void createDefaultFolder();
    void applyTheme();
    void applyUiScaling();

    // Helper um das Grid zu aktualisieren
    void updateGrid();

    void showRenameOverlay(const QString &currentName);
    void updateActiveToolIndicator(const QString &iconName);

    void animateSidebar(bool show);
    void animateRightSidebar(bool show);

    CanvasView* getCurrentCanvas();
    void setActiveTool(CanvasView::ToolType tool);

    QSplitter *m_mainSplitter;
    QStackedWidget *m_rightStack;

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

    QWidget *m_floatingTools;
    DragHandle *m_dragHandle;
    QWidget *m_toolsContainer;
    ModernButton *m_minimizeBtn;
    ModernButton *m_activeToolIcon;
    bool m_toolsCollapsed;
    bool m_toolbarInitialized;

    ModernButton *btnSidebar;
    ModernButton *btnSettings;
    ModernButton *btnEditorSettings;

    // KEIN m_gridSlider mehr!

    ModernButton *btnUndo;
    ModernButton *btnRedo;
    ModernButton *btnSelect;
    ModernButton *btnPen;
    ModernButton *btnEraser;
    ModernButton *btnLasso;
    ModernButton *btnFormat;
    ModernButton *btnNewNote;

    QString m_rootPath;
    QColor m_currentAccentColor;
    QColor m_penColor;
    int m_penWidth;

    // Werte für das Grid
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
