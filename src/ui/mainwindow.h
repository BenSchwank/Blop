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
class NoteLeftRail;
class RadialToolbarFab;
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

  void onModeCha