#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QFileSystemModel>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include "canvasview.h"
#include "freegridview.h"
#include "tools/ToolManager.h" // WICHTIG: Include für ToolManager
#include "uiprofilemanager.h"

// Forward Declarations
class MainWindow;
class PageManager;
class QShowEvent;
#ifdef Q_OS_ANDROID
class QQuickView;
class QVBoxLayout;
#endif

#ifdef BLOP_HAS_WEBENGINE
#include <QWebEnginePage>
#ifndef Q_OS_ANDROID
class QWebEngineView;
#endif

class InterceptingWebPage : public QWebEnginePage {
  Q_OBJECT
public:
  explicit InterceptingWebPage(QObject *parent = nullptr) : QWebEnginePage(parent) {}
protected:
  bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
signals:
  void googleLoginRequested();
};
#endif

// --- SidebarNavDelegate ---
class SidebarNavDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit SidebarNavDelegate(MainWindow *parent);
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;

private:
  MainWindow *m_window;
};

// --- ModernButton ---
class ModernButton : public QToolButton {
  Q_OBJECT
  Q_PROPERTY(double buttonScale READ buttonScale WRITE setButtonScale)
public:
  explicit ModernButton(QWidget *parent = nullptr);

  double buttonScale() const { return m_scale; }
  void setButtonScale(double s);

  void setAccentColor(QColor c);
  void setHoverScaleEnabled(bool enabled) { m_hoverScaleEnabled = enabled; }

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  double m_scale;
  QPropertyAnimation *m_anim;
  QColor m_accentColor;
  bool m_hoverScaleEnabled{true};
};

// --- ModernItemDelegate ---
class ModernItemDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit ModernItemDelegate(MainWindow *parent);
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

private:
  MainWindow *m_window;
};

class MainWindow : public QMainWindow {
  Q_OBJECT
  friend class ModernItemDelegate;
  friend class SidebarNavDelegate;

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  QIcon createModernIcon(const QString &name, const QColor &color);
  QColor currentAccentColor() const { return m_currentAccentColor; }
  ToolManager *toolManager() const { return m_toolManager; }

  bool isTouchMode() const { return m_currentProfile.isTouchOptimized(); }

  void showContextMenu(const QPoint &globalPos, const QModelIndex &index);
  void startRename(const QModelIndex &index);

  void switchToSelectTool();

  void restoreWindowState();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
#ifdef Q_OS_WIN
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

signals:
  void injectToken(const QString &token);

public slots:
  void requestGoogleLogin();
  void onSessionCheck(const QString &sessionData);
  void showAuthOverlay(const QUrl &url);

private slots:
  void checkForUpdates();

  void onNewPage();
  void onCreateFolder();

  void onToggleSidebar();
  void onOpenSettings();

  void updateTheme(QColor accentColor);
  void updateInputMode(bool penOnly);
  void applyProfile(const UiProfile &profile);

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

  void onItemDropped(const QModelIndex &sourceIndex,
                     const QModelIndex &targetIndex);

  void onToggleFloatingTools();
  void finishRename();

  void addNoteTab(const QString &title);
  void closeNoteTab(int index);

  void onShowNewTabPopup();

  void onItemSizeChanged(int value);
  void onGridSpacingChanged(int value);

  void onTabChanged(int index);

  void onToggleRightSidebar();
  void onTogglePageManager();
  void onShareClicked();

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

  void onModeChanged(int index);

private:
  void setupUi();
  void setupTools(); // Initialisiert und registriert Tools
  void setupTitleBar();
  void setupSidebar();
  void setupRightSidebar();
  void createDefaultFolder();
  void applyTheme();

  void updateGrid();
  void updateSidebarState();
  void updateOverviewBackButton();
#ifdef Q_OS_ANDROID
  /// Sync Notizen/Study tab visuals (combo does not drive onModeChanged on Android).
  void applyAndroidTabStyles(int index);
#endif
  /// Sidebar rect in MainWindow coords (below title bar on desktop, under toolbar on Android).
  QRect sidebarPushContentRect() const;
  /// Keep push offset + sidebar geometry in sync (no overlap with main content).
  void syncSidebarPushLayout();

  void showRenameOverlay(const QString &currentName);

  void animateSidebar(bool show);
  void animateRightSidebar(bool show);

  void performCopy(const QModelIndex &index);
  bool copyRecursive(const QString &src, const QString &dst);

  void toggleSection(QListWidgetItem *headerItem);
  void toggleFolderContent(QListWidgetItem *parentItem);

  CanvasView *getCurrentCanvas();
  void setActiveTool(CanvasView::ToolType tool);

  // --- Web Integration ---
  void setupWebBrowser();
  void updateSidebarUser(const QString &username); // syncs login from webview
  QWidget *m_studyContainer{nullptr};
#ifdef Q_OS_ANDROID
  // Embedded Study UI (QML + QtWebView). Must be hidden when leaving Study or
  // the native surface can stay above Qt and block taps (e.g. header tabs).
  QQuickView *m_studyQQuickView{nullptr};
  QWidget *m_androidHeader{nullptr};
  /// In-note top search for Android header (hidden outside editor mode).
  QLineEdit *m_androidTopSearchBar{nullptr};
  QVBoxLayout *m_studyVBoxLayout{nullptr};
  QWidget *m_studyWindowContainer{nullptr}; // QWidget::createWindowContainer(QQuickView)
  QPushButton *m_btnAndroidNotes{nullptr};
  QPushButton *m_btnAndroidStudy{nullptr};
  /// Shown only while editing a note (overview uses floating btnEditorMenu).
  ModernButton *m_btnAndroidToolbarMenu{nullptr};
  /// Opens right note settings while editing on Android.
  ModernButton *m_btnAndroidToolbarSettings{nullptr};
  /// Orange page manager button (only for A4 notes).
  ModernButton *m_btnAndroidToolbarPageManager{nullptr};
  /// Export current note while editing on Android.
  ModernButton *m_btnAndroidToolbarExport{nullptr};
#endif
  QDialog *m_authOverlay{nullptr};
  QStackedWidget *m_mainContentStack{nullptr};
  QComboBox *m_modeSelector{nullptr};
  /// Desktop title bar: visible Notizen/Study control (logic in m_modeSelector).
  QPushButton *m_btnMode{nullptr};
#if defined(BLOP_HAS_WEBENGINE) && !defined(Q_OS_ANDROID)
  QWebEngineView *m_studyWebView{nullptr};
#endif
  // ----------------------------

  // --- Sidebar user section labels (updated on webview login) ---
  QLabel *m_lblSidebarUser{nullptr};
  QLabel *m_lblSidebarAvatar{nullptr};
  // ----------------------------

  UiProfileManager *m_profileManager{nullptr};
  UiProfile m_currentProfile;
  ToolManager *m_toolManager{nullptr};

  QWidget *m_centralContainer{nullptr};
  /// Left column width for push-style sidebar (desktop only; Android uses layout margins).
  QWidget *m_desktopSidebarPushSpacer{nullptr};

  QWidget *m_titleBarWidget{nullptr};
  QWidget *m_topNavControls{nullptr};
  QWidget *m_tabBarWidget{nullptr};
  QHBoxLayout *m_tabBarLayout{nullptr};
  QWidget *m_tabScrollArea{nullptr};
  QPushButton *m_btnHomeTab{nullptr};
  QList<QPushButton *> m_noteTabButtons;
  int m_activeTabIndex{-1}; // -1 = Home
  QLineEdit *m_titleSearchBar{nullptr};
  QPushButton *m_btnTitleSettings{nullptr};
  QPushButton *m_btnTitleShare{nullptr};
  QWidget *m_editorTitleControls{nullptr};
  QPushButton *m_btnWinMin{nullptr};
  QPushButton *m_btnWinMax{nullptr};
  QPushButton *m_btnWinClose{nullptr};
  QPoint m_windowDragPos;
  bool m_isDragging{false};

  QSplitter *m_mainSplitter{nullptr};
  QStackedWidget *m_rightStack{nullptr};

  QWidget *m_sidebarStrip{nullptr};
  ModernButton *btnStripMenu{nullptr};
  ModernButton *btnEditorMenu{nullptr};

  ModernButton *btnOverviewMenu{nullptr};

  bool m_isSidebarOpen;
  bool m_lastIsEditor{false};

  QWidget *m_sidebarContainer{nullptr};
  QListWidget *m_navSidebar{nullptr};
  QFileSystemModel *m_fileModel{nullptr};
  QPushButton *m_closeSidebarBtn{nullptr};

  QPushButton *m_btnSidebarSettings{nullptr};

  QWidget *m_overviewContainer{nullptr};
  FreeGridView *m_fileListView{nullptr};
  QLabel *m_lblEmptyState{nullptr};
  QPushButton *m_fabNote{nullptr};

  QWidget *m_editorContainer{nullptr};
  QWidget *m_editorCenterWidget{nullptr};
  QTabWidget *m_editorTabs{nullptr};

  QWidget *m_rightSidebar{nullptr};
  QLabel *m_lblActiveNote{nullptr};

  // Quick-Tags Sidebar
  QWidget     *m_tagsContainer{nullptr};
  QHBoxLayout *m_tagsFlowLayout{nullptr};
  QLabel      *m_lblMetaCreated{nullptr};
  QLabel      *m_lblMetaModified{nullptr};
  QPushButton *m_btnFormatInfinite{nullptr};
  QPushButton *m_btnFormatA4{nullptr};
  QPushButton *m_btnColorWhite{nullptr};
  QPushButton *m_btnColorDark{nullptr};
  QPushButton *m_btnInputPen{nullptr};
  QPushButton *m_btnInputTouch{nullptr};
  QPushButton *m_btnUiDesktop{nullptr};
  QPushButton *m_btnUiTouch{nullptr};

  QButtonGroup *m_grpPageStyle{nullptr};
  QPushButton *m_btnStyleBlank{nullptr};
  QPushButton *m_btnStyleLined{nullptr};
  QPushButton *m_btnStyleSquared{nullptr};
  QPushButton *m_btnStyleDotted{nullptr};
  QSlider *m_sliderGridSpacing{nullptr};

  QComboBox *m_comboProfiles{nullptr};
  QComboBox *m_comboToolbarStyle{nullptr};
  QSlider *m_sliderToolbarScale{nullptr};

  QWidget *m_floatingTools{nullptr};

  PageManager *m_pageManager{nullptr};

  ModernButton *btnEditorSettings{nullptr};
  ModernButton *m_btnPages{nullptr};
  ModernButton *btnBackOverview{nullptr};

  QString m_rootPath;
  QColor m_currentAccentColor;
  QColor m_penColor;
  int m_penWidth;

  int m_currentItemSize;
  int m_currentSpacing;

  bool m_penOnlyMode;
  bool m_touchMode;

  QTimer *m_autoSaveTimer{nullptr};
  QTimer *m_gridSpacingTimer{nullptr};

  CanvasView::ToolType m_activeToolType;

  QModelIndex m_indexToRename;
  QWidget *m_renameOverlay{nullptr};
  QLineEdit *m_renameInput{nullptr};

  QNetworkAccessManager *m_netManager{nullptr};
};

#endif // MAINWINDOW_H
