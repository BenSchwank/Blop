#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QFileSystemModel>
#include <QIcon>
#include <QLabel>
#include <QPersistentModelIndex>
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
#include <QUrl>
#include <QVector>

#include "canvasview.h"
#include "freegridview.h"
#include "notemanager.h"
#include "tools/ToolManager.h" // WICHTIG: Include für ToolManager
#include "uiprofilemanager.h"

// Forward Declarations
class DocumentTabBar;
class LibraryTagsPanel;
class LibraryOrgBar;
class MainWindow;
class PageManager;
class PageThumbnailSidebar;
class PenPresetBar;
class QSortFilterProxyModel;
class QShowEvent;
struct WebBookmark {
  QString title;
  QUrl url;
};
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

#ifdef Q_OS_ANDROID
  /// Static method to reset OAuth timer - can be called from GoogleAuthManager
  static void resetOAuthTimer();
#endif

#ifdef Q_OS_ANDROID
  /// Sync Notizen/Study tab visuals (narrow-aware: smaller padding/font on
  /// phones). Called from the free-function syncAndroidHeaderGeometry on
  /// every resize, and from onModeChanged on tab switches.
  void applyAndroidTabStyles(int index);
#endif

  void showContextMenu(const QPoint &globalPos, const QModelIndex &index);
  void startRename(const QModelIndex &index);

  // Set by AndroidTileDelegate::editorEvent when the user taps the
  // three-dots pill on a Welcome tile so the QListView::clicked handler
  // knows to skip opening the note (which would otherwise occlude the
  // menu). Declared unconditionally because androidtiledelegate.cpp
  // compiles on every platform via BLOP_SOURCES even though the click
  // suppression only ever fires on Android.
  void setAndroidPillClickPending(bool v) { m_androidPillClickPending = v; }

  void switchToSelectTool();

  void restoreWindowState();

  bool isAuthNavigationLocked() const { return m_authNavigationLocked; }

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
  void oauthFailed(const QString &reason);

public slots:
  /// Re-apply theme-aware stylesheets after BlopTheme::themeChanged. v3.17.0.
  void applyThemeRefresh();
  void refreshOpenEditorSceneBackgrounds();
  void requestGoogleLogin();
  void onSessionCheck(const QString &sessionData);
  void resetAndroidWebViewStorage();
  void resetAndroidWebViewStorageFull();
  void nudgeAndroidWebViewStopOnly();
  void applyAndroidStudyWebViewNetworkCache();
  void scheduleAndroidStudyWebViewNetworkCache();
  bool showAuthOverlay(const QUrl &url);
  void switchToNotesFromWebQmlBar();
  void openWebBookmarkMenuFromWebQmlBar();
  QVariantList webBookmarksForQml() const;
  bool addWebBookmarkFromQml(const QString &urlInput, const QString &titleInput);
  bool removeWebBookmarkFromQml(int index);
  void openWebBookmarkFromQml(int index);
  void notifyStudyFirstLoadDone();
  void showAndroidStudyBootRetry();
  /// Returns "&blop_usr=...&blop_sid=..." (URL-encoded) built from the
  /// natively-persisted session, or an empty string when none is stored.
  /// Appended to the Study entry URL so the embedded WebView can hydrate its
  /// localStorage synchronously before the auth guard runs — closing the race
  /// where AuthCheck redirects to /login before C++'s async injectToken lands.
  QString savedStudySessionParam() const;

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

  void onToolSelect();
  void onToolPen();
  void onToolEraser();
  void onToolLasso();
  void onUndo();
  void onRedo();

  void onItemDropped(const QModelIndex &sourceIndex,
                     const QModelIndex &targetIndex);

  void finishRename();

  void addNoteTab(const QString &title);
  void closeNoteTab(int index);

  void onShowNewTabPopup();

  void onItemSizeChanged(int value);
  void onGridSpacingChanged(int value);

  void onTabChanged(int index);

  void setPageSettingsOverlayVisible(bool show);
  void syncPageSettingsPanelFromEditor();
  void onTogglePageManager();
  void onEditorNoteOverflowMenu();

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
  void flushPendingA4Save();
  void setupTitleBar();
  void setupSidebar();
  void setupRightSidebar();
  /// v3.18.5: (re-)apply theme-aware stylesheets to every control in
  /// the right-side Page-Settings sheet. Called once from
  /// setupRightSidebar() and again from applyThemeRefresh() so the
  /// Light/Dark switch reskins the inner buttons/sliders/combos.
  void refreshPageSettingsTheme();
  void createDefaultFolder();
  void applyTheme();
  void applyLibraryFilters();
  void rebuildPageSettingsTags();
  QString currentEditorNotePath() const;
  void setLibraryRootFromSource(const QModelIndex &sourceIndex);

  void updateGrid();
  void updateSidebarState();
  void updateOverviewBackButton();
  /// Sidebar rect in MainWindow coords (below title bar on desktop, under toolbar on Android).
  QRect sidebarPushContentRect() const;
  /// Drawer width: capped on narrow Android screens so the panel never exceeds ~86% of window width.
  int effectiveSidebarWidthPx() const;
  /// Keep push offset + sidebar geometry in sync (no overlap with main content).
  void syncSidebarPushLayout();
#ifdef Q_OS_ANDROID
  void updateAndroidSidebarScrimGeometry();
#endif

  void showRenameOverlay(const QString &currentName);

  void animateSidebar(bool show);

  void performCopy(const QModelIndex &index);
  bool copyRecursive(const QString &src, const QString &dst);

  void toggleFolderContent(QListWidgetItem *parentItem);

  CanvasView *getCurrentCanvas();
  void setActiveTool(CanvasView::ToolType tool);
  int noteHeaderHeight() const;
  /// Keep PenPresetBar pinned under the floating/docked toolbar as one cluster.
  void syncPenPresetBarGeometry();

  // --- Web Integration ---
  void setupWebBrowser();
  void updateSidebarUser(const QString &username); // syncs login from webview
  void loadWebBookmarksFromSettings();
  void saveWebBookmarksToSettings() const;
  void rebuildModeSelectorItems();
  QUrl normalizedUserWebUrl(QString input) const;
  void showAddWebBookmarkDialog();
  void showManageWebBookmarksDialog();
  void openModeMenuAtButton();
  void openWebBookmarkOverflowMenuFromWidget(QWidget *anchor);
  void applyDesktopWebSubviewForModeIndex(int modeIndex);
#ifdef Q_OS_ANDROID
  void invokeAndroidWebDestination(int kind, const QString &url = QString());
  void dismissAndroidOAuthOverlay();
#endif
  void resetEmbeddedWebToStudy();
  QWidget *m_studyContainer{nullptr};
#ifdef Q_OS_ANDROID
  // Embedded Study UI (QML + QtWebView) using QQuickView for proper SurfaceView compositing.
  QQuickView *m_studyQQuickView{nullptr};
  QWidget *m_androidHeader{nullptr};
  /// In-note top search for Android header (hidden outside editor mode).
  QLineEdit *m_androidTopSearchBar{nullptr};
  QVBoxLayout *m_studyVBoxLayout{nullptr};
  QWidget *m_studyWindowContainer{nullptr}; // host widget for embedded Study view
  /// Timestamp (ms since epoch) of when the user last left the Study tab.
  /// Used by onModeChanged() to decide whether the SurfaceView likely lost
  /// its surface during the absence and needs a budget-free refresh. Zero
  /// means "never deactivated since startup".
  qint64 m_lastStudyDeactivationMs{0};
  QPushButton *m_btnAndroidNotes{nullptr};
  QPushButton *m_btnAndroidStudy{nullptr};
  QPushButton *m_btnAndroidAddWebBookmark{nullptr};
  /// Overview/editor shortcut back to the file list (left of hamburger).
  ModernButton *m_btnAndroidHome{nullptr};
  /// Shown only while editing a note (overview uses floating btnEditorMenu).
  ModernButton *m_btnAndroidToolbarMenu{nullptr};
  void syncStudyChromeTheme();
  /// Orange page manager button (only for A4 notes).
  ModernButton *m_btnAndroidToolbarPageManager{nullptr};
  /// Export current note while editing on Android.
  ModernButton *m_btnAndroidToolbarExport{nullptr};
  /// Dimmed overlay behind drawer; taps close the sidebar (Material-style).
  QWidget *m_androidSidebarScrim{nullptr};
  /// Android OAuth uses Chrome Custom Tabs (see showAuthOverlay); overlay slot unused.
  QWidget *m_androidOAuthOverlay{nullptr};
  /// Study boot spinner above QtWebView SurfaceView (sibling of content stack).
  QWidget *m_androidStudyBootOverlay{nullptr};
  QPushButton *m_androidStudyBootRetryBtn{nullptr};
  QTimer *m_androidStudyBootRetryTimer{nullptr};
  void ensureAndroidStudyBootOverlay();
  void syncAndroidStudyBootOverlayGeometry();
  void setAndroidStudyBootOverlayVisible(bool visible);
#endif
  QDialog *m_authOverlay{nullptr};
  QStackedWidget *m_mainContentStack{nullptr};
  QComboBox *m_modeSelector{nullptr};
  /// Desktop title bar: visible Notizen/Study control (logic in m_modeSelector).
  QPushButton *m_btnMode{nullptr};
  QPushButton *m_btnAddWebBookmark{nullptr};
#if defined(BLOP_HAS_WEBENGINE) && !defined(Q_OS_ANDROID)
  QStackedWidget *m_webViewStack{nullptr};
  QWebEngineView *m_studyWebView{nullptr};
  QWebEngineView *m_customWebView{nullptr};
  QTimer *m_studySsoTimer{nullptr};
#endif
  QVector<WebBookmark> m_webBookmarks;
  // ----------------------------
  /// True while auth is unresolved: prevent switching from Study/Login to Notes.
  bool m_authNavigationLocked{false};
  /// Prevents duplicate OAuth browser launches from repeated poll triggers.
  bool m_googleLoginInFlight{false};
  /// Start timestamp for login in-flight guard; stale requests are auto-recovered.
  qint64 m_googleLoginInFlightSinceMs{0};
  /// Prevents multiple overlapping Android bookmark sheets.
  bool m_androidWebMenuOpen{false};

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
  /// Home + note tabs row in title bar (hidden in Study / web bookmarks).
  DocumentTabBar *m_documentTabBar{nullptr};
  /// Persistent left page thumbnail strip, Drawboard-style.
  PageThumbnailSidebar *m_pageThumbnailSidebar{nullptr};
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
  QSortFilterProxyModel *m_libraryProxy{nullptr};
  LibraryTagsPanel *m_libraryTagsPanel{nullptr};
  LibraryOrgBar *m_libraryOrgBar{nullptr};
  QLineEdit *m_overviewSearchBar{nullptr};
  QLabel *m_lblEmptyState{nullptr};
  QPushButton *m_fabNote{nullptr};

  // Set true by AndroidTileDelegate when the user taps the three-dots
  // pill on a Welcome tile. Consumed by the QListView::clicked lambda
  // so that a single tap on the pill opens the context menu instead of
  // also auto-opening the underlying note. Defined unconditionally so
  // the inline setter above compiles on Windows too.
  bool m_androidPillClickPending{false};

  // v119 perf: dedupe QEvent::Move bursts on the floating toolbar
  // (eventFilter); only re-evaluate the dock condition when y crosses
  // an 8-px threshold instead of on every pixel of a drag.
  int m_lastDockCheckY{-1000};

  QWidget *m_editorContainer{nullptr};
  QWidget *m_editorCenterWidget{nullptr};
  QTabWidget *m_editorTabs{nullptr};

  /// Floating note page header (title, page count, metadata).
  QWidget *m_noteHeader{nullptr};
  QLabel *m_lblNoteHeaderTitle{nullptr};
  QLabel *m_lblNoteHeaderMeta{nullptr};

  QWidget *m_pageSettingsOverlay{nullptr};
  QWidget *m_pageSettingsCard{nullptr};
  /// v3.17.0: BlopModal hosting the PageSettings card while visible. Null
  /// when the panel is dismissed. The card is reparented back to
  /// `m_pageSettingsOverlay` after dismissal so the next show finds it.
  class BlopModal *m_pageSettingsModal{nullptr};
  QLabel *m_lblActiveNote{nullptr};
  /// v3.18.5: cached references for refreshPageSettingsTheme(). The
  /// QTabWidget + the two tab pages are not Member-tracked by Qt's
  /// child mechanism in a way that's easy to iterate, so we store
  /// raw pointers here. Never own; the parent QWidget tree does.
  class QTabWidget *m_pageSettingsTabs{nullptr};
  QWidget *m_pageSettingsTabOptions{nullptr};
  QWidget *m_pageSettingsTabTags{nullptr};

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
  PenPresetBar *m_penPresetBar{nullptr};

  PageManager *m_pageManager{nullptr};

  /// A4-Notiz: ⋯-Menü in der Desktop-Titelleiste (kein Floating-Button)
  ModernButton *m_btnEditorNoteOverflow{nullptr};
  /// A4-Notiz: Seitenmanager (gleiche Rolle wie Android-Topbar-Button)
  ModernButton *m_btnTitleBarPageManager{nullptr};
  ModernButton *btnBackOverview{nullptr};

  QString m_rootPath;
  QColor m_currentAccentColor;
  QColor m_penColor;
  int m_penWidth;

  // v3.17.5: applyTheme() no-op gating. See implementation comment.
  QRgb m_lastAppliedAccentRgb{0};
  int m_lastAppliedModeKey{-1};
  bool m_themeApplied{false};

  int m_currentItemSize;
  int m_currentSpacing;

  bool m_penOnlyMode;
  bool m_touchMode;

  QTimer *m_autoSaveTimer{nullptr};
  QTimer *m_gridSpacingTimer{nullptr};
  NoteManager m_noteManager;
  QTimer *m_a4SaveDebounce{nullptr};
  Note *m_pendingA4SaveNote{nullptr};
  QString m_pendingA4SavePath;

  CanvasView::ToolType m_activeToolType;

  QModelIndex m_indexToRename;
  /// Kept for API compatibility; rename UI is BlopModal-based now.
  QWidget *m_renameOverlay{nullptr};
  QLineEdit *m_renameInput{nullptr};

  QNetworkAccessManager *m_netManager{nullptr};
};

#endif // MAINWINDOW_H
