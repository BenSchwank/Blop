#include "mainwindow.h"
#include "UIStyles.h"
#include "canvasview.h"
#include "moderntoolbar.h"
#include "notechrome.h"
#include "notechromeedge.h"
#include "toolpropertiespanel.h"
#include "allpagesoverlay.h"
#include "blop_inwindow_menu.h"
#include "androidphonetoolbar.h"
#include "documenttabbar.h"
#include "librarytagspanel.h"
#include "librarytagstore.h"
#include "libraryorgstore.h"
#include "libraryorgbar.h"
#include "cloudstoragestore.h"
#include "pagethumbnailsidebar.h"
#include "noteleftrail.h"
#include "radialtoolbarfab.h"
#include "penpresetbar.h"
#include "newnotedialog.h"
#include "overlayscrollindicator.h"
#include "profileeditordialog.h"
#include "settingsdialog.h"
#include "editoroverlays.h"

// --- WICHTIGE ZUSÄTZLICHE INCLUDES ---
#include "Note.h"
#include "ToolMode.h"
#include "markdowneditor.h"
#include "multipagenoteview.h"
#include "noteeditor.h"
#include "notemanager.h"
#include "pagemanager.h"
#include "blopstyle.h"
#include "blop_dialogs.h"
#include "uiscale.h"
#include "tools/ToolManager.h"
#include "googleauthmanager.h"
#ifdef Q_OS_ANDROID
#include "androidcontentpicker.h"
#include "androidicons.h"
#include "androidtiledelegate.h"
#endif

// Tools Includes
#include "tools/EraserTool.h"
#include "tools/ImageTool.h"
#include "tools/LassoTool.h"
#include "tools/RulerTool.h"
#include "tools/ShapeTool.h"
#include "tools/TextTool.h" // Neu: TextTool
#include "tools/StickyNoteTool.h"
#include "tools/HandTool.h"
#include "tools/WritingTools.h" // Enthält PenTool, PencilTool, HighlighterTool
// -------------------------------------

#include <QApplication>
#include <QColor>
#include <QMessageBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QRadioButton>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QBoxLayout>
#include <QComboBox>
#include <QTabBar>
#include <QWindow>
#include <QDataStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QQmlContext>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QtMath>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProgressDialog>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif
#include <QScroller>
#include <QScrollerProperties>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QVariantAnimation>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QFileDialog>
#include <QUuid>
#include <QStyle>
#include <QTimer>
#include <QSignalBlocker>
#include <QToolBar>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <QCloseEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QUrl>
#include <QUrlQuery>
#include <QMetaObject>
#include <QGuiApplication>
#include <QClipboard>
#include <QFrame>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QKeyEvent>
#include <QShortcut>
#include <QPushButton>

#include "blop_diag.h"
#include "blop_inwindow_menu.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "blopripple.h"

#ifdef Q_OS_ANDROID
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlError>
#include <QSslSocket>
#include <QTemporaryFile>
#include <QWidget>
#include <QElapsedTimer>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>
#else
#ifdef BLOP_HAS_WEBENGINE
#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#endif
#endif
// ============================================================================
// KONFIGURATION FÜR ANDROID SCALING & MARGINS
// ============================================================================
// Overview margins (used on both Android and desktop)
static const int MARGIN_OVERVIEW = 20;

#ifdef Q_OS_ANDROID
static const int SIDEBAR_WIDTH = 236;
// Sidebar list only (Overview grid still uses FONT_SIZE_BASE below)
static const int ROW_HEIGHT_HEADER = 36;
static const int ROW_HEIGHT_ITEM = 46;
static const int SIDEBAR_NAV_FONT_PT = 12;
static const int FONT_SIZE_BASE = 14;
static const int FONT_SIZE_HEADER = 22;
static const int MARGIN_ANDROID_TOP = 50;
static const int MARGIN_ANDROID_SIDE = 16;
static const int FAB_SIZE_ANDROID = 56;
static const int FAB_DISTANCE_FROM_BOTTOM = 30;
#else
static const int SIDEBAR_WIDTH = 236;
static const int ROW_HEIGHT_HEADER = 34;
static const int ROW_HEIGHT_ITEM = 38;
static const int FONT_SIZE_BASE = 10;
static const int FONT_SIZE_HEADER = 18;
#endif

// IMPORTANT: Update this version string for every new release build!
// Keep in sync with CMakeLists.txt project(Blop VERSION x.x.x)
#ifndef BLOP_VERSION_STR
#define BLOP_VERSION_STR "3.18.12"
#endif
static const char *BLOP_VERSION = BLOP_VERSION_STR;

/// Embedded Blop Study base URL (Android QML appends "/?native=1" for embedded entry).
static const QString kBlopStudyUrl(QStringLiteral("https://blop-study.com"));

namespace {

class LibraryFilterProxy : public QSortFilterProxyModel {
public:
  enum class SmartView { All, Favorites, Recent, Untagged };
  enum class SortMode { Name, Modified };

  explicit LibraryFilterProxy(QObject *parent = nullptr)
      : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
  }

  void setSearchText(const QString &text) {
    if (m_search == text)
      return;
    m_search = text.trimmed();
    refreshFilter();
  }

  void setRequiredTags(const QStringList &tags) {
    if (m_tags == tags)
      return;
    m_tags = tags;
    refreshFilter();
  }

  void setSmartView(SmartView view) {
    if (m_smartView == view)
      return;
    m_smartView = view;
    refreshFilter();
  }

  void setSortMode(SortMode mode) {
    if (m_sortMode == mode)
      return;
    m_sortMode = mode;
    invalidate();
    sort(0, Qt::AscendingOrder);
  }

  SmartView smartView() const { return m_smartView; }
  SortMode sortMode() const { return m_sortMode; }

protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex &sourceParent) const override {
    const QAbstractItemModel *src = sourceModel();
    if (!src)
      return true;
    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);
    if (!idx.isValid())
      return false;

    const auto *fsm = qobject_cast<const QFileSystemModel *>(src);
    const QString name = idx.data(Qt::DisplayRole).toString();
    const QString path =
        fsm ? fsm->filePath(idx) : idx.data(Qt::UserRole).toString();
    const bool isDir = fsm && fsm->isDir(idx);

    // Folders must always pass so proxy roots stay mappable (Alle / nested
    // folders open in the main grid even with Favorites/Recent/search on).
    if (isDir)
      return true;

    if (!m_search.isEmpty() && !name.contains(m_search, Qt::CaseInsensitive))
      return false;

    switch (m_smartView) {
    case SmartView::Favorites:
      if (!LibraryOrgStore::isFavorite(path))
        return false;
      break;
    case SmartView::Recent: {
      const QStringList recent = LibraryOrgStore::recentPaths(24);
      if (!recent.contains(path))
        return false;
      break;
    }
    case SmartView::Untagged:
      if (!LibraryTagStore::tagsForPath(path).isEmpty())
        return false;
      break;
    case SmartView::All:
    default:
      break;
    }

    if (m_tags.isEmpty())
      return true;

    // Folders already returned above; notes must match required tags.
    const QStringList noteTags = LibraryTagStore::tagsForPath(path);
    for (const QString &need : m_tags) {
      bool hit = false;
      for (const QString &have : noteTags) {
        if (have.compare(need, Qt::CaseInsensitive) == 0) {
          hit = true;
          break;
        }
      }
      if (!hit)
        return false;
    }
    return true;
  }

  bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
    const auto *fsm = qobject_cast<const QFileSystemModel *>(sourceModel());
    if (!fsm)
      return QSortFilterProxyModel::lessThan(left, right);

    const bool leftDir = fsm->isDir(left);
    const bool rightDir = fsm->isDir(right);
    // Folders first for Name sort; for Recent/Modified keep mixed by date.
    if (m_sortMode == SortMode::Name && leftDir != rightDir)
      return leftDir && !rightDir;

    if (m_sortMode == SortMode::Modified || m_smartView == SmartView::Recent) {
      const QString leftPath = fsm->filePath(left);
      const QString rightPath = fsm->filePath(right);
      if (m_smartView == SmartView::Recent) {
        const QStringList recent = LibraryOrgStore::recentPaths(48);
        const int li = recent.indexOf(leftPath);
        const int ri = recent.indexOf(rightPath);
        if (li >= 0 || ri >= 0)
          return (li >= 0 ? li : 9999) < (ri >= 0 ? ri : 9999);
      }
      const QDateTime lt = QFileInfo(leftPath).lastModified();
      const QDateTime rt = QFileInfo(rightPath).lastModified();
      if (lt != rt)
        return lt > rt; // newest first
    }

    const QString ln = left.data(Qt::DisplayRole).toString();
    const QString rn = right.data(Qt::DisplayRole).toString();
    return QString::localeAwareCompare(ln, rn) < 0;
  }

private:
  void refreshFilter() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange();
#else
    invalidateFilter();
#endif
    sort(0, Qt::AscendingOrder);
  }

  QString m_search;
  QStringList m_tags;
  SmartView m_smartView{SmartView::All};
  SortMode m_sortMode{SortMode::Name};
};

QString blopWebMenuStyleSheet() {
  // v3.17.0: theme-aware. Reads live tokens from BlopTheme so Light/Dark
  // mode reskins menus without a separate codepath. Composing per-call is
  // cheap (string formatting) and keeps the QMenu-vs-BlopInWindowMenu
  // visual parity that v3.16.10 introduced.
  auto rgba = [](const QColor &c) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
  };
  const QString surface = BlopTheme::surfaceElevated().name(QColor::HexRgb);
  const QString border = rgba(BlopTheme::accentBorder());
  const QString sepBg = rgba(BlopTheme::borderDefault());
  const QString textCol = BlopTheme::textPrimary().name(QColor::HexRgb);
  const QString accentSel = rgba(BlopTheme::accentSubtle());
  const QString onAccent = BlopTheme::textOnAccent().name(QColor::HexRgb);
  return QStringLiteral(
             "QMenu { background-color: %1; border: 1px solid %2; "
             "border-radius: 12px; padding: 6px; }"
             "QMenu::separator { height: 1px; background: %3; "
             "margin: 6px 12px; }"
             "QMenu::item { color: %4; padding: 10px 22px; border-radius: 8px; "
             "font-size: 13px; font-weight: 500; }"
             "QMenu::item:selected { background-color: %5; color: %6; }")
      .arg(surface, border, sepBg, textCol, accentSel, onAccent);
}

#ifdef Q_OS_ANDROID
// Lightweight in-window toast. Replaces QMessageBox on Android, where any
// QDialog::exec() crashes the single-window surface (Qt 6.10 inproc). The
// toast is a plain QLabel child of window() (no top-level flags), auto-hides
// after `durationMs`, and never blocks the event loop.
void showAndroidToast(QWidget *anchor, const QString &text,
                      int durationMs = 2200) {
  QWidget *win = anchor ? anchor->window() : nullptr;
  if (!win)
    return;
  auto *toast = new QLabel(text, win);
  toast->setAttribute(Qt::WA_DeleteOnClose);
  toast->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  toast->setAlignment(Qt::AlignCenter);
  toast->setWordWrap(true);
  toast->setStyleSheet(QStringLiteral(
      "QLabel { background: rgba(20,20,30,0.92); color: #E8E4FF;"
      " border: 1px solid rgba(124,92,252,0.45); border-radius: 10px;"
      " padding: 10px 18px; font-size: 14px; }"));
  const int maxW = qMin(win->width() - UiScale::dp(40), UiScale::dp(360));
  toast->setMaximumWidth(maxW);
  toast->adjustSize();
  toast->move((win->width() - toast->width()) / 2,
              win->height() - toast->height() - UiScale::dp(60));
  toast->show();
  toast->raise();
  QTimer::singleShot(durationMs, toast, &QWidget::close);
}
#endif // Q_OS_ANDROID

// In-window crash report overlay. Reads the previous run's crash dump via
// BlopDiag::takeCrashReportIfPresent() and displays it as a child QWidget of
// the parent window (NOT a top-level QDialog -- that path is exactly what
// crashes on Android). Provides Copy/Mail/Dismiss buttons.
void showCrashReportOverlay(QWidget *parent, const QString &report) {
  if (!parent || report.isEmpty()) {
    return;
  }
  QWidget *win = parent->window();
  if (!win) {
    return;
  }

  auto *backdrop = new QWidget(win);
  backdrop->setObjectName(QStringLiteral("BlopCrashBackdrop"));
  backdrop->setAttribute(Qt::WA_DeleteOnClose);
  backdrop->setStyleSheet(
      "QWidget#BlopCrashBackdrop { background: rgba(0,0,0,0.65); }");
  backdrop->setGeometry(win->rect());
  backdrop->show();
  backdrop->raise();

  auto *frame = new QFrame(backdrop);
  frame->setObjectName(QStringLiteral("BlopCrashFrame"));
  frame->setStyleSheet(
      "QFrame#BlopCrashFrame {"
      "  background: #14121F;"
      "  border: 1px solid rgba(124,92,252,0.5);"
      "  border-radius: 12px;"
      "}"
      "QLabel { color: #E8E4FF; background: transparent; }"
      "QLabel#BlopCrashTitle { font-size: 15px; font-weight: 700;"
      " color: #FF6B6B; }"
      "QLabel#BlopCrashSubtitle { font-size: 12px; color: #A09FB8; }"
      "QPlainTextEdit {"
      "  background: #0D0B14; color: #DCDCFF;"
      "  border: 1px solid rgba(255,255,255,0.08); border-radius: 8px;"
      "  font-family: 'Consolas','Courier New',monospace; font-size: 11px;"
      "}"
      "QPushButton {"
      "  background: rgba(124,92,252,0.20); color: #E8E4FF;"
      "  border: 1px solid rgba(124,92,252,0.45); border-radius: 8px;"
      "  padding: 8px 14px; font-size: 12px; font-weight: 600;"
      "}"
      "QPushButton:pressed { background: rgba(124,92,252,0.35); }"
      "QPushButton#BlopCrashDismiss {"
      "  background: rgba(255,255,255,0.06);"
      "  border: 1px solid rgba(255,255,255,0.15);"
      "}");

  auto *vlay = new QVBoxLayout(frame);
  vlay->setContentsMargins(UiScale::dp(16), UiScale::dp(14),
                           UiScale::dp(16), UiScale::dp(14));
  vlay->setSpacing(UiScale::dp(8));

  auto *title = new QLabel(QObject::tr("Letzter Lauf ist abgestürzt"), frame);
  title->setObjectName(QStringLiteral("BlopCrashTitle"));
  vlay->addWidget(title);

  auto *subtitle = new QLabel(
      QObject::tr("Der Stack Trace unten hilft uns, die Ursache zu finden. "
                  "Bitte mit \"Kopieren\" einsenden."),
      frame);
  subtitle->setObjectName(QStringLiteral("BlopCrashSubtitle"));
  subtitle->setWordWrap(true);
  vlay->addWidget(subtitle);

  auto *textEdit = new QPlainTextEdit(frame);
  textEdit->setReadOnly(true);
  textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
  textEdit->setPlainText(report);
  textEdit->setMinimumHeight(UiScale::dp(220));
  vlay->addWidget(textEdit, 1);

  auto *btnRow = new QHBoxLayout();
  btnRow->setSpacing(UiScale::dp(8));

  auto *btnCopy = new QPushButton(QObject::tr("Kopieren"), frame);
  auto *btnMail = new QPushButton(QObject::tr("Per Mail senden"), frame);
  auto *btnDismiss = new QPushButton(QObject::tr("Verwerfen"), frame);
  btnDismiss->setObjectName(QStringLiteral("BlopCrashDismiss"));

  btnRow->addWidget(btnCopy);
  btnRow->addWidget(btnMail);
  btnRow->addStretch(1);
  btnRow->addWidget(btnDismiss);
  vlay->addLayout(btnRow);

  QObject::connect(btnCopy, &QPushButton::clicked, [report]() {
    if (auto *cb = QGuiApplication::clipboard()) {
      cb->setText(report);
    }
  });
  QObject::connect(btnMail, &QPushButton::clicked, [report]() {
    QUrl mailto;
    mailto.setScheme(QStringLiteral("mailto"));
    mailto.setPath(QStringLiteral(""));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("subject"),
                   QStringLiteral("Blop Crash Report"));
    // mailto bodies have a hard length limit (~2000 chars on Android Gmail).
    QString body = report;
    if (body.size() > 1800) {
      body = body.left(1800) + QStringLiteral("\n... (truncated)");
    }
    q.addQueryItem(QStringLiteral("body"), body);
    mailto.setQuery(q);
    QDesktopServices::openUrl(mailto);
  });
  QObject::connect(btnDismiss, &QPushButton::clicked, backdrop,
                   &QWidget::close);

  // Position frame: 90% width, centered, max ~80% height.
  const int margin = UiScale::dp(16);
  const int frameW = qMin(win->width() - 2 * margin, UiScale::dp(560));
  const int frameH = qMin(int(win->height() * 0.84),
                          UiScale::dp(560));
  frame->setGeometry((win->width() - frameW) / 2,
                     (win->height() - frameH) / 2, frameW, frameH);

  // Re-position with the backdrop on resize.
  backdrop->installEventFilter(backdrop);
  QObject::connect(win, &QObject::destroyed, backdrop, &QWidget::deleteLater);
}

void applyBlopWebSheetStyle(QDialog *dlg) {
  // v3.18.1: themed()-Wrap, damit die Web-Dialoge im Light-Mode nicht als
  // dunkle Fremdkörper erscheinen (Dark-Mode bleibt unverändert).
  dlg->setStyleSheet(BlopTheme::themed(QString::fromUtf8(
      R"(QDialog { background-color: #14121F; }
QLabel { color: #E8E4FF; font-size: 13px; background: transparent; }
QLineEdit { background-color: #1A1829; color: #F2F1FF; border: 1px solid #2C2940; border-radius: 10px; padding: 10px 14px; font-size: 13px; min-height: 20px; selection-background-color: rgba(124,92,252,0.45); }
QLineEdit:focus { border: 1px solid rgba(124,92,252,0.75); }
QListWidget { background-color: #1A1829; color: #E8E4FF; border: 1px solid #2C2940; border-radius: 10px; padding: 4px; outline: none; font-size: 13px; }
QListWidget::item { padding: 10px 12px; border-radius: 6px; }
QListWidget::item:selected { background: rgba(124,92,252,0.38); color: #FFFFFF; }
QListWidget::item:hover { background: rgba(255,255,255,0.06); })")));
}

QString blopPrimaryButtonStyle() {
  return QString::fromUtf8(
      R"(QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #9B79FF, stop:1 #5E5CE6); color: #FFFFFF; border: none; border-radius: 10px; padding: 10px 22px; font-size: 13px; font-weight: 600; min-width: 104px; }
QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #B198FF, stop:1 #7A6AFF); }
QPushButton:pressed { background: #4E4ACC; })");
}

QString blopGhostButtonStyle() {
  return BlopTheme::themed(QString::fromUtf8(
      R"(QPushButton { background: transparent; color: #C8C4E8; border: 1px solid rgba(255,255,255,0.14); border-radius: 10px; padding: 10px 22px; font-size: 13px; font-weight: 600; min-width: 92px; }
QPushButton:hover { background: rgba(255,255,255,0.08); color: #F0EEFF; border-color: rgba(124,92,252,0.45); })"));
}

QByteArray postJsonSync(QNetworkAccessManager *nam, const QUrl &url,
                        const QJsonObject &payload, int *statusCodeOut = nullptr,
                        QNetworkReply::NetworkError *errorOut = nullptr) {
  if (!nam) {
    if (statusCodeOut)
      *statusCodeOut = 0;
    if (errorOut)
      *errorOut = QNetworkReply::UnknownNetworkError;
    return QByteArray();
  }
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  const QString sid =
      QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
          .value(QStringLiteral("session_id"))
          .toString()
          .trimmed();
  if (!sid.isEmpty())
    req.setRawHeader("X-Session-Id", sid.toUtf8());
  QNetworkReply *reply =
      nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  if (statusCodeOut)
    *statusCodeOut =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (errorOut)
    *errorOut = reply->error();
  const QByteArray raw = reply->readAll();
  reply->deleteLater();
  return raw;
}

QByteArray getSync(QNetworkAccessManager *nam, const QUrl &url,
                   int *statusCodeOut = nullptr,
                   QNetworkReply::NetworkError *errorOut = nullptr) {
  if (!nam) {
    if (statusCodeOut)
      *statusCodeOut = 0;
    if (errorOut)
      *errorOut = QNetworkReply::UnknownNetworkError;
    return QByteArray();
  }
  QNetworkRequest req(url);
  const QString sid =
      QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
          .value(QStringLiteral("session_id"))
          .toString()
          .trimmed();
  if (!sid.isEmpty())
    req.setRawHeader("X-Session-Id", sid.toUtf8());
  QNetworkReply *reply = nam->get(req);
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();
  if (statusCodeOut)
    *statusCodeOut =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (errorOut)
    *errorOut = reply->error();
  const QByteArray raw = reply->readAll();
  reply->deleteLater();
  return raw;
}

QString chooseCloudFolderId(QWidget *parent, QNetworkAccessManager *nam,
                            const QString &username) {
  int status = 0;
  QNetworkReply::NetworkError err = QNetworkReply::NoError;
  QUrl foldersUrl(kBlopStudyUrl + "/api/folders");
  QUrlQuery query;
  query.addQueryItem("username", username);
  const QString sid =
      QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
          .value(QStringLiteral("session_id"))
          .toString()
          .trimmed();
  if (!sid.isEmpty())
    query.addQueryItem(QStringLiteral("session_id"), sid);
  foldersUrl.setQuery(query);
  const QByteArray raw = getSync(nam, foldersUrl, &status, &err);
  if (err != QNetworkReply::NoError || status < 200 || status >= 300) {
    BlopDialogs::notify(parent, QStringLiteral("Ordner laden fehlgeschlagen"),
                        QStringLiteral("Serverantwort (%1):\n%2")
                            .arg(status)
                            .arg(QString::fromUtf8(raw)));
    return QString();
  }
  const QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isArray()) {
    BlopDialogs::notify(parent, QStringLiteral("Ordner laden fehlgeschlagen"),
                        QStringLiteral("Unerwartete Antwort vom Server."));
    return QString();
  }
  QStringList labels;
  QHash<QString, QString> labelToId;
  for (const QJsonValue &v : doc.array()) {
    const QJsonObject o = v.toObject();
    const QString id = o.value("id").toString().trimmed();
    if (id.isEmpty())
      continue;
    const QString name = o.value("name").toString().trimmed();
    const QString label = name.isEmpty() ? id : QString("%1 (%2)").arg(name, id);
    labels << label;
    labelToId.insert(label, id);
  }
  if (labels.isEmpty()) {
    BlopDialogs::notify(parent, QStringLiteral("Keine Ordner"),
                        QStringLiteral("Es wurden keine Cloud-Ordner gefunden."));
    return QString();
  }
  const QString chosen = BlopDialogs::promptChoice(
      parent, QStringLiteral("Cloud-Zielordner"),
      QStringLiteral("Wähle den Zielordner:"), labels, 0);
  if (chosen.isEmpty())
    return QString();
  return labelToId.value(chosen);
}

QString resolveCloudFileId(QWidget *parent, QNetworkAccessManager *nam,
                           const QString &username,
                           const QString &localFilePathOrName) {
  const QString localName = QFileInfo(localFilePathOrName).fileName().trimmed();
  const QString localBase = QFileInfo(localFilePathOrName).baseName().trimmed();

  int status = 0;
  QNetworkReply::NetworkError err = QNetworkReply::NoError;
  QUrl foldersUrl(kBlopStudyUrl + "/api/folders");
  QUrlQuery folderQuery;
  folderQuery.addQueryItem("username", username);
  const QString sid =
      QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
          .value(QStringLiteral("session_id"))
          .toString()
          .trimmed();
  if (!sid.isEmpty())
    folderQuery.addQueryItem(QStringLiteral("session_id"), sid);
  foldersUrl.setQuery(folderQuery);
  const QByteArray foldersRaw = getSync(nam, foldersUrl, &status, &err);
  if (err != QNetworkReply::NoError || status < 200 || status >= 300)
    return QString();
  const QJsonDocument foldersDoc = QJsonDocument::fromJson(foldersRaw);
  if (!foldersDoc.isArray())
    return QString();

  QString fallbackId;
  for (const QJsonValue &folderVal : foldersDoc.array()) {
    const QJsonObject folderObj = folderVal.toObject();
    const QString folderId = folderObj.value("id").toString().trimmed();
    if (folderId.isEmpty())
      continue;
    QUrl filesUrl(kBlopStudyUrl + "/api/files/" + QUrl::toPercentEncoding(folderId));
    QUrlQuery filesQuery;
    filesQuery.addQueryItem("username", username);
    if (!sid.isEmpty())
      filesQuery.addQueryItem(QStringLiteral("session_id"), sid);
    filesUrl.setQuery(filesQuery);

    int filesStatus = 0;
    QNetworkReply::NetworkError filesErr = QNetworkReply::NoError;
    const QByteArray filesRaw = getSync(nam, filesUrl, &filesStatus, &filesErr);
    if (filesErr != QNetworkReply::NoError || filesStatus < 200 || filesStatus >= 300)
      continue;
    const QJsonDocument filesDoc = QJsonDocument::fromJson(filesRaw);
    if (!filesDoc.isArray())
      continue;
    for (const QJsonValue &fileVal : filesDoc.array()) {
      const QJsonObject fileObj = fileVal.toObject();
      const QString cloudId = fileObj.value("id").toString().trimmed();
      const QString cloudName = fileObj.value("name").toString().trimmed();
      if (cloudId.isEmpty())
        continue;
      if (!localName.isEmpty() && cloudName.compare(localName, Qt::CaseInsensitive) == 0)
        return cloudId;
      if (!localBase.isEmpty() && QFileInfo(cloudName).baseName().compare(localBase, Qt::CaseInsensitive) == 0) {
        if (fallbackId.isEmpty())
          fallbackId = cloudId;
      }
    }
  }
  return fallbackId;
}

#ifdef Q_OS_ANDROID
void applyAndroidImmersiveUi() {
  // Keep system bars hidden app-wide; re-apply on key UI lifecycle moments
  // because Android can restore bars after focus/layer transitions.
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid())
      return;

    QJniObject window =
        activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
    if (!window.isValid())
      return;

    // NOTE: deliberately NOT calling addFlags(FLAG_DRAWS_SYSTEM_BAR_
    // BACKGROUNDS, 0x80000000). With it, OEM SystemUI (especially on
    // Samsung One UI) ends up re-tinting a "protected" strip at the
    // top - that black bar the user kept reporting. Without the flag
    // plus the transparent statusBarColor below, the app's own
    // background fills the top edge.
    window.callMethod<void>("setStatusBarColor", "(I)V", 0x00000000);
    window.callMethod<void>("setNavigationBarColor", "(I)V", 0x00000000);

    // Modern edge-to-edge: tell the window not to fit system bars so
    // app content extends into the area where the status bar / nav
    // bar are drawn. API 30+, no-op on older versions (the
    // IMMERSIVE_STICKY decorView flags below cover those).
    const int sdkInt = QJniObject::getStaticField<jint>(
        "android/os/Build$VERSION", "SDK_INT");
    if (sdkInt >= 30) {
      window.callMethod<void>("setDecorFitsSystemWindows", "(Z)V",
                              jboolean(false));
    }

    QJniObject decorView =
        window.callObjectMethod("getDecorView", "()Landroid/view/View;");
    if (decorView.isValid()) {
      // v3.18.52: edge-to-edge WITHOUT hiding the system bars. The status /
      // navigation bars stay visible (and transparent, see setStatusBarColor
      // above) while app content draws behind them. Previously we also set
      // HIDE_NAVIGATION (0x2) | FULLSCREEN (0x4) | IMMERSIVE_STICKY (0x1000),
      // which *hid* the status bar; Qt's QScreen::availableGeometry still
      // reported the status-bar height, so the header reserved a top inset for
      // a bar that wasn't there — the phantom "Balken" strip that left the app
      // not flush with the display. Keeping only the LAYOUT_* flags makes the
      // computed inset match the real, visible status bar.
      // SYSTEM_UI_FLAG_LAYOUT_STABLE | LAYOUT_HIDE_NAVIGATION | LAYOUT_FULLSCREEN
      const jint uiFlags = 0x00000100 | 0x00000200 | 0x00000400;
      decorView.callMethod<void>("setSystemUiVisibility", "(I)V", uiFlags);
    }
  });
}

void syncAndroidHeaderGeometry(MainWindow *window) {
  if (!window)
    return;
  QWidget *chrome = window->findChild<QWidget *>(QStringLiteral("AndroidTopChrome"));
  QWidget *inner =
      window->findChild<QWidget *>(QStringLiteral("AndroidTopHeaderInner"));
  if (!chrome || !inner)
    return;
  
  // v3.18.27: During login (authNavigationLocked), hide header completely to avoid margin
  if (window->isAuthNavigationLocked()) {
    chrome->setVisible(false);
    chrome->setFixedHeight(0);
    return;
  } else {
    chrome->setVisible(true);
  }
  auto *headerLay = qobject_cast<QHBoxLayout *>(inner->layout());
  if (!headerLay)
    return;

  const int clampedInset = UiScale::safeTopPx(window);
  // Anchor responsive sizing to the *usable* viewport (screen minus a small
  // safety margin against Android system cutouts that availableGeometry
  // doesn't always reflect). This is what guarantees the right-edge buttons
  // (Page-Manager, Export-three-dots) stay reachable on phones.
  const int screenW = UiScale::androidUsableViewportWidthPx(window);
  const int contentW = UiScale::androidContentWidthPx(window);

  // Tablet vs phone via physical screen size (mm) - independent of any
  // OEM logicalDpi quirks. Phones get the compact layout; tablets keep
  // the roomier desktop-style layout with the inline search bar.
  const bool isTablet = UiScale::isAndroidTablet(window);
  const bool narrowPhone = !isTablet;
  const int horizontalPad = narrowPhone
                                ? UiScale::dp(8)
                                : UiScale::safeHorizontalPaddingPx(window);
  // Always reserve dp(12) on the right edge on Android so the Export
  // three-dots / Page-Manager pill never sits flush against a curved
  // corner or gesture inset.
  const int rightPad = UiScale::dp(12);
  // v3.18.7: was UiScale::dp(4). See setupUi() comment — keeps the chip
  // row flush against the status-bar inset without a discrete dark lip.
  const int topExtra = 0;

  const int headerHeight =
      qBound(UiScale::dp(46), int(screenW * 0.09), UiScale::dp(60));
  // Smaller chrome buttons on narrow phones - mandatory headroom for tabs +
  // searchbar + right-side menu.
  const int btnW = narrowPhone
                       ? qBound(UiScale::dp(34), int(screenW * 0.10),
                                UiScale::dp(46))
                       : qBound(UiScale::dp(40), int(screenW * 0.11),
                                UiScale::dp(56));
  const int btnH =
      qBound(UiScale::dp(28), int(headerHeight * 0.7), UiScale::dp(38));
  const int compactPillH =
      qBound(UiScale::dp(26), int(headerHeight * 0.58), UiScale::dp(34));
  const int rowHeight = headerHeight + clampedInset + topExtra;
  QWidget *docTabs =
      window->findChild<QWidget *>(QStringLiteral("AndroidDocumentTabBar"));
  const int tabsH =
      (docTabs && docTabs->isVisible()) ? qMax(UiScale::dp(36), docTabs->height()) : 0;
  const int totalHeight = rowHeight + tabsH;

  chrome->setFixedHeight(totalHeight);
  inner->setFixedHeight(rowHeight);
  headerLay->setContentsMargins(horizontalPad, clampedInset + topExtra,
                                rightPad, UiScale::dp(4));
  headerLay->setSpacing(narrowPhone ? UiScale::dp(3) : UiScale::dp(8));

  auto syncBtn = [&](QWidget *w) {
    if (!w)
      return;
    w->setFixedSize(btnW, btnH);
  };
  syncBtn(window->findChild<QWidget *>(QStringLiteral("AndroidHeaderBtnHome")));
  syncBtn(window->findChild<QWidget *>(QStringLiteral("AndroidHeaderBtnMenu")));
  syncBtn(
      window->findChild<QWidget *>(QStringLiteral("AndroidHeaderBtnPageMgr")));
  syncBtn(window->findChild<QWidget *>(QStringLiteral("AndroidHeaderBtnExport")));
  // v3.18.5: square icon (22dp) inside the home pill instead of
  // icon-stretched-to-button. Other header buttons stay full-rect.
  if (auto *home = window->findChild<QAbstractButton *>(
          QStringLiteral("AndroidHeaderBtnHome"))) {
    home->setIconSize(QSize(UiScale::dp(22), UiScale::dp(22)));
  }

  // Pill height is the only purely-geometric value here. Padding/font-size/
  // max-width live in applyAndroidTabStyles() (narrow-phone-aware QSS) so
  // they survive tab-switch resets that re-apply colour-state stylesheets.
  auto syncPillHeight = [&](QWidget *w) {
    if (!w)
      return;
    w->setFixedHeight(compactPillH);
  };
  syncPillHeight(window->findChild<QWidget *>(QStringLiteral("AndroidHeaderTabNotes")));
  syncPillHeight(window->findChild<QWidget *>(QStringLiteral("AndroidHeaderTabStudy")));
  // Re-apply the narrow-aware QSS in case orientation/inset changed.
  if (QStackedWidget *stack =
          window->findChild<QStackedWidget *>(QStringLiteral("MainContentStack"))) {
    window->applyAndroidTabStyles(stack->currentIndex());
  } else {
    // Fallback: assume Notes-active if stack not found yet (early setup).
    window->applyAndroidTabStyles(0);
  }

  if (QWidget *plus =
          window->findChild<QWidget *>(QStringLiteral("AndroidHeaderBtnPlus"))) {
    plus->setFixedSize(qBound(UiScale::dp(28), int(screenW * 0.085),
                              UiScale::dp(36)),
                       compactPillH);
  }

  if (QLineEdit *searchBar = window->findChild<QLineEdit *>(
          QStringLiteral("androidTopSearchBar"))) {
    searchBar->setFixedHeight(btnH);
    // Reserve room for the two right-side editor buttons (Page-Manager +
    // Export) plus their gaps so the search bar shrinks first instead of
    // squeezing them off-screen.
    const int rightReserved = 2 * btnW + 3 * headerLay->spacing();
    const int searchMin = narrowPhone ? UiScale::dp(60) : UiScale::dp(100);
    const int searchMaxCap = qBound(UiScale::dp(140),
                                     int(screenW * 0.40),
                                     UiScale::dp(360));
    const int searchMax = qMax(searchMin,
                               qMin(contentW - rightReserved, searchMaxCap));
    searchBar->setMinimumWidth(searchMin);
    searchBar->setMaximumWidth(searchMax);

    // Phones (anything < 600 dp wide via DPI-robust check) hide the search
    // bar entirely so the right-side menu always has room. Switch its size
    // policy to Ignored too — an Expanding QLineEdit can keep claiming
    // horizontal space even while invisible.
    if (!isTablet) {
      searchBar->setProperty("blopHiddenBecauseTooNarrow", true);
      searchBar->hide();
      searchBar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    } else {
      searchBar->setProperty("blopHiddenBecauseTooNarrow", false);
      searchBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
      // Visibility owned by editor-mode logic in updateSidebarState.
    }
  }
}

QRect androidSafeOverlayRect(QWidget *host) {
  if (!host)
    return QRect();
  QRect r = host->rect();
  const int topInset = UiScale::safeTopPx(host);
  const int bottomInset = UiScale::safeBottomPx(host);
  r.adjust(0, topInset, 0, -bottomInset);
  // Clamp the horizontal extents to the actual device viewport so that on
  // phones where the host widget is reported wider than the visible screen
  // (e.g. cutouts, gesture insets) overlays don't spill past the right edge.
  const int realW = UiScale::androidScreenWidthPx(host);
  if (realW > 0 && r.width() > realW) {
    r.setX(qMax(0, (host->width() - realW) / 2));
    r.setWidth(realW);
  }
  return r;
}
#endif

} // namespace

// ============================================================================
// 1. DELEGATES & BUTTONS
// ============================================================================

SidebarNavDelegate::SidebarNavDelegate(MainWindow *parent)
    : QStyledItemDelegate(parent), m_window(parent) {}

QSize SidebarNavDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const {
  bool isHeader = index.data(Qt::UserRole + 1).toBool();
  return QSize(option.rect.width(),
               isHeader ? ROW_HEIGHT_HEADER : ROW_HEIGHT_ITEM);
}

void SidebarNavDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
#ifdef Q_OS_ANDROID
  const int navFont = SIDEBAR_NAV_FONT_PT;
#else
  const int navFont = FONT_SIZE_BASE;
#endif

  bool isHeader = index.data(Qt::UserRole + 1).toBool();
  bool isExpandable = index.data(Qt::UserRole + 6).toBool();
  int depth = index.data(Qt::UserRole + 9).toInt();
  int indent = 12 * depth;

  QString text = index.data(Qt::DisplayRole).toString();

  // v3.18.5: theme-aware sidebar painting. Custom paint never picks up
  // QSS rules, so we read the live BlopTheme tokens directly. This
  // restores readable text and selection state in Light mode where the
  // old hardcoded grey-on-white was nearly invisible.
  const QColor secondaryText = BlopTheme::textSecondary();
  const QColor primaryText = BlopTheme::textPrimary();
  const QColor accentTint = BlopTheme::accentSubtle();
  const bool isDark = BlopTheme::instance().isDark();
  const QColor hoverTint = isDark ? QColor(255, 255, 255, 16)
                                  : QColor(0, 0, 0, 16);
  const QColor dividerColor = BlopTheme::borderSubtle();

  if (isHeader) {
    painter->setPen(secondaryText);
    QFont f = m_window->font();
    f.setBold(true);
    f.setPointSize(navFont - 2);
    painter->setFont(f);

    int arrowX = option.rect.left() + 15;
    int arrowY = option.rect.center().y();
    QPainterPath arrow;
    bool collapsed = index.data(Qt::UserRole + 3).toBool();
    if (collapsed) {
      arrow.moveTo(arrowX, arrowY - 4);
      arrow.lineTo(arrowX + 5, arrowY);
      arrow.lineTo(arrowX, arrowY + 4);
    } else {
      arrow.moveTo(arrowX, arrowY - 2);
      arrow.lineTo(arrowX + 8, arrowY - 2);
      arrow.lineTo(arrowX + 4, arrowY + 3);
    }
    painter->setBrush(secondaryText);
    painter->setPen(Qt::NoPen);
    painter->drawPath(arrow);
    QRect textRect = option.rect.adjusted(30, 0, -15, 0);
    painter->setPen(secondaryText);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    QFontMetrics fm(f);
    int textW = fm.horizontalAdvance(text);
    int lineX = textRect.left() + textW + 10;
    int lineY = option.rect.center().y();
    painter->setPen(dividerColor);
    painter->drawLine(lineX, lineY, option.rect.right() - 15, lineY);
  } else {
#ifdef Q_OS_ANDROID
    QRect rect = option.rect.adjusted(8 + indent, 1, -8, -1);
#else
    QRect rect = option.rect.adjusted(8 + indent, 2, -8, -2);
#endif
    bool selected = (option.state & QStyle::State_Selected);
    bool hover = (option.state & QStyle::State_MouseOver);

    if (selected) {
      painter->setBrush(accentTint);
      painter->setPen(Qt::NoPen);
      painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
    } else if (hover) {
      painter->setBrush(hoverTint);
      painter->setPen(Qt::NoPen);
      painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
    }

    int iconOffset = 12;
    if (isExpandable) {
      int arrowX = rect.left() + 6;
      int arrowY = rect.center().y();
      QPainterPath arrow;
      bool isExpanded = index.data(Qt::UserRole + 3).toBool();
      if (isExpanded) {
        arrow.moveTo(arrowX, arrowY - 2);
        arrow.lineTo(arrowX + 8, arrowY - 2);
        arrow.lineTo(arrowX + 4, arrowY + 3);
      } else {
        arrow.moveTo(arrowX, arrowY - 4);
        arrow.lineTo(arrowX + 5, arrowY);
        arrow.lineTo(arrowX, arrowY + 4);
      }
      painter->setBrush(selected ? m_window->currentAccentColor() : secondaryText);
      painter->setPen(Qt::NoPen);
      painter->drawPath(arrow);
      iconOffset = 22;
    }

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    int iconSize = 24;
#ifdef Q_OS_ANDROID
    iconSize = 24;
#endif

    QRect iconRect(rect.left() + iconOffset, rect.center().y() - iconSize / 2,
                   iconSize, iconSize);
    QIcon::Mode mode = selected ? QIcon::Selected : QIcon::Normal;
    if (selected) {
      icon.paint(painter, iconRect, Qt::AlignCenter, mode, QIcon::On);
    } else {
      // v3.18.5: 0.7 opacity on a near-grey icon is invisible in Light
      // mode; 0.85 still reads as "muted" against either palette.
      painter->setOpacity(isDark ? 0.7 : 0.85);
      icon.paint(painter, iconRect, Qt::AlignCenter, mode, QIcon::Off);
      painter->setOpacity(1.0);
    }

    QRect textRect = rect;
    textRect.setLeft(iconRect.right() + 15);
    textRect.setRight(rect.right() - 40);
    painter->setPen(selected ? m_window->currentAccentColor() : primaryText);

    QFont f = m_window->font();
    f.setPointSize(navFont);
    if (selected)
      f.setBold(true);
    painter->setFont(f);

    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight,
                                                        textRect.width()));

    QString count = index.data(Qt::UserRole + 2).toString();
    if (!count.isEmpty()) {
#ifdef Q_OS_ANDROID
      int badgeW = 28;
      int badgeH = 18;
#else
      int badgeW = 30;
      int badgeH = 20;
#endif
      QRect badgeRect(rect.right() - badgeW - 5, rect.center().y() - badgeH / 2,
                      badgeW, badgeH);
      painter->setPen(m_window->currentAccentColor());
      QFont bf = f;
      bf.setBold(true);
      bf.setPointSize(navFont - 2);
      painter->setFont(bf);
      painter->drawText(badgeRect, Qt::AlignCenter, count);
    }
  }
  painter->restore();
}

ModernItemDelegate::ModernItemDelegate(MainWindow *parent)
    : QStyledItemDelegate(parent), m_window(parent) {}
QSize ModernItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  if (m_window && m_window->m_fileListView) {
    const QSize grid = m_window->m_fileListView->gridSize();
    if (grid.width() > 32 && grid.height() > 32) {
      const int spacing = m_window->m_fileListView->spacing();
      return QSize(qMax(32, grid.width() - spacing),
                   qMax(32, grid.height() - spacing));
    }
    const QSize icons = m_window->m_fileListView->iconSize();
    if (icons.width() > 32 && icons.height() > 32)
      return icons;
  }
  return QSize(UiScale::dp(140), UiScale::dp(140));
}
void ModernItemDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  QRect rect = option.rect.adjusted(6, 6, -6, -6);

  const bool hovered = option.state & QStyle::State_MouseOver;
  const bool selected = option.state & QStyle::State_Selected;
  if (hovered) {
    painter->translate(rect.center());
    painter->scale(1.015, 1.015);
    painter->translate(-rect.center());
  }

  // Soft tile plate — icon-forward, less boxy than the old solid card.
  QColor bgColor = BlopTheme::surfaceBase();
  bgColor.setAlpha(hovered ? 230 : 200);
  if (selected) {
    bgColor = m_window->currentAccentColor();
    bgColor.setAlpha(48);
  } else if (hovered) {
    bgColor = BlopTheme::surfaceElevated();
    bgColor.setAlpha(220);
  }

  painter->setBrush(bgColor);
  QColor border = selected ? m_window->currentAccentColor()
                           : BlopTheme::borderSubtle();
  if (!selected)
    border.setAlpha(hovered ? 90 : 55);
  painter->setPen(QPen(border, 1.0));
  const int radius = UiScale::dp(18);
  painter->drawRoundedRect(rect, radius, radius);

  QString fileName = index.data(Qt::DisplayRole).toString();
  QIcon icon;
  const bool isBnote = fileName.endsWith(QLatin1String(".bnote"), Qt::CaseInsensitive);
  const bool isBlop = fileName.endsWith(QLatin1String(".blop"), Qt::CaseInsensitive);
  bool isFolder = index.data(Qt::UserRole + 1).toBool();
  if (!isFolder && m_window && m_window->m_fileModel) {
    QModelIndex src = index;
    if (m_window->m_libraryProxy)
      src = m_window->m_libraryProxy->mapToSource(index);
    if (src.isValid())
      isFolder = m_window->m_fileModel->isDir(src);
  }
  if (!isFolder && !isBnote && !isBlop && !fileName.contains(QLatin1Char('.')))
    isFolder = true;

  if (isBnote) {
    icon = m_window->createModernIcon(QStringLiteral("note_bnote"),
                                      QColor(QStringLiteral("#B8B4E8")));
  } else if (isBlop) {
    icon = m_window->createModernIcon(QStringLiteral("note_blop"),
                                      m_window->currentAccentColor());
  } else if (isFolder) {
    icon = m_window->createModernIcon(QStringLiteral("folder"),
                                      QColor(QStringLiteral("#E8C26A")));
  } else {
    icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (icon.isNull())
      icon = m_window->createModernIcon(QStringLiteral("folder"),
                                        QColor(QStringLiteral("#E8C26A")));
  }

  // Clean caption: strip Blop extensions so tiles read as product names.
  QString text = fileName;
  if (isBnote)
    text.chop(6);
  else if (isBlop)
    text.chop(5);

  bool isWideList = rect.width() > (rect.height() * 1.5);

#ifdef Q_OS_ANDROID
  const double iconShrink = 0.92;
  isWideList = false;
#else
  const double iconShrink = 0.72;
#endif

  painter->setPen(BlopTheme::textPrimary());

  if (isWideList) {
    int iconDim = rect.height() - 24;
    if (iconDim < 16)
      iconDim = 16;
    iconDim = qMax(16, (int)(iconDim * iconShrink));
    QRect iconRect(rect.left() + 14, rect.center().y() - iconDim / 2, iconDim,
                   iconDim);
    icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
    QRect textRect = rect;
    textRect.setLeft(iconRect.right() + 14);
    textRect.setRight(rect.right() - 44);
    QFont f = painter->font();
    f.setBold(true);
    f.setPointSize(FONT_SIZE_BASE);
    painter->setFont(f);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight,
                                                        textRect.width()));
  } else {
    int textH = UiScale::dp(34);
    int maxIconH = rect.height() - textH - UiScale::dp(14);
    int maxIconW = rect.width() - UiScale::dp(24);
    int iconDim = qMin(maxIconW, maxIconH);
    iconDim = qMax(22, (int)(iconDim * iconShrink));
#ifdef Q_OS_ANDROID
    iconDim = qMax(iconDim, qMin(maxIconW, qMin(maxIconH, UiScale::dp(80))));
#endif

    int contentHeight = iconDim + textH + UiScale::dp(6);
    int startY = rect.top() + (rect.height() - contentHeight) / 2;
    QRect iconRect(rect.center().x() - iconDim / 2, startY, iconDim, iconDim);

    icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);

    if (textH > 0) {
      QRect textRect(rect.left() + UiScale::dp(8), iconRect.bottom() + UiScale::dp(6),
                     rect.width() - UiScale::dp(16), textH);
      QFont f = painter->font();
      f.setPointSize(FONT_SIZE_BASE - 1);
      f.setWeight(QFont::DemiBold);
      painter->setFont(f);
      QTextOption opt;
      opt.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
      opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
      painter->drawText(textRect, text, opt);
    }
  }

  QIcon menuIcon = m_window->createModernIcon(
      QStringLiteral("more_pill"), QColor(QStringLiteral("#C8CDDC")));
#ifdef Q_OS_ANDROID
  const int pillW = UiScale::dp(28);
  const int pillH = UiScale::dp(20);
#else
  const int pillW = 28;
  const int pillH = 20;
#endif
  QRect menuRect(rect.right() - pillW - 6, rect.top() + 6, pillW, pillH);
  if (isWideList)
    menuRect.moveTop(rect.center().y() - pillH / 2);
  menuIcon.paint(painter, menuRect, Qt::AlignCenter);

  // Organization badges: color label stripe + favorite star.
  QString path;
  if (auto *proxy = qobject_cast<const QSortFilterProxyModel *>(index.model())) {
    const QModelIndex src = proxy->mapToSource(index);
    if (auto *fsm =
            qobject_cast<const QFileSystemModel *>(proxy->sourceModel()))
      path = fsm->filePath(src);
  } else if (auto *fsm =
                 qobject_cast<const QFileSystemModel *>(index.model())) {
    path = fsm->filePath(index);
  }
  if (!path.isEmpty()) {
    const auto label = LibraryOrgStore::colorLabel(path);
    if (label != LibraryOrgStore::ColorLabel::None) {
      QColor stripe = LibraryOrgStore::colorForLabel(label);
      stripe.setAlpha(230);
      painter->setPen(Qt::NoPen);
      painter->setBrush(stripe);
      painter->drawRoundedRect(
          QRect(rect.left() + 2, rect.top() + 10, 4, rect.height() - 20), 2, 2);
    }
    if (LibraryOrgStore::isFavorite(path)) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(QColor(QStringLiteral("#E6B450")));
      const QPointF c(rect.left() + 16, rect.top() + 16);
      QPolygonF star;
      for (int i = 0; i < 5; ++i) {
        const qreal a = -M_PI / 2 + i * 2 * M_PI / 5;
        star << QPointF(c.x() + qCos(a) * 6.5, c.y() + qSin(a) * 6.5);
        const qreal b = a + M_PI / 5;
        star << QPointF(c.x() + qCos(b) * 3.0, c.y() + qSin(b) * 3.0);
      }
      painter->drawPolygon(star);
    }
  }

  painter->restore();
}
bool ModernItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                     const QStyleOptionViewItem &option,
                                     const QModelIndex &index) {
  if (event->type() == QEvent::MouseButtonRelease) {
    QMouseEvent *me = static_cast<QMouseEvent *>(event);
    QRect rect = option.rect.adjusted(4, 4, -4, -4);
    int clickArea = 50;
    QRect menuRect(rect.right() - clickArea, rect.top(), clickArea,
                   rect.height());
    if (menuRect.contains(me->pos())) {
      m_window->showContextMenu(QCursor::pos(), index);
      return true;
    }
  }
  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// --- ModernButton ---
ModernButton::ModernButton(QWidget *parent)
    : QToolButton(parent), m_scale(1.0), m_accentColor(Qt::white) {
  m_anim = new QPropertyAnimation(this, "buttonScale", this);
  m_anim->setDuration(150);
  m_anim->setEasingCurve(QEasingCurve::OutQuad);

  setCursor(Qt::PointingHandCursor);
  setStyleSheet("border: none; background: transparent;");
  setIconSize(QSize(28, 28));
}
void ModernButton::setButtonScale(double s) {
  m_scale = s;
  update();
}
void ModernButton::setAccentColor(QColor c) {
  m_accentColor = c;
  update();
}
void ModernButton::enterEvent(QEnterEvent *event) {
  if (!m_hoverScaleEnabled) {
    QToolButton::enterEvent(event);
    return;
  }
  m_anim->stop();
  m_anim->setEndValue(1.2);
  m_anim->start();
  QToolButton::enterEvent(event);
}
void ModernButton::leaveEvent(QEvent *event) {
  if (!m_hoverScaleEnabled) {
    QToolButton::leaveEvent(event);
    return;
  }
  m_anim->stop();
  m_anim->setEndValue(1.0);
  m_anim->start();
  QToolButton::leaveEvent(event);
}
void ModernButton::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  QIcon ic = icon();
  if (ic.isNull())
    return;
  int w = width();
  int h = height();
  const int iw = qMax(1, iconSize().width());
  const int ih = qMax(1, iconSize().height());
  p.translate(w / 2, h / 2);
  p.scale(m_scale, m_scale);
  p.translate(-iw / 2, -ih / 2);
  QIcon::Mode mode = isChecked() ? QIcon::Active : QIcon::Normal;
  ic.paint(&p, 0, 0, iw, ih, Qt::AlignCenter, mode);
}

// ============================================================================
// 2. MAINWINDOW IMPLEMENTATION
// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_renameOverlay(nullptr) {
  // All pointer members are nullptr-initialized via in-class initializers in mainwindow.h

  // Single custom chrome bar (Cursor-style). No native Windows title bar —
  // window controls live in setupTitleBar(); drag/min/max via HTTEST + Win32
  // styles applied in showEvent (without WS_CAPTION).
  setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

#ifdef Q_OS_ANDROID
  QFont f = this->font();
  f.setPointSize(FONT_SIZE_BASE);
  this->setFont(f);
#endif
  qDebug() << "MainWindow: Konstruktor start";

#ifdef Q_OS_ANDROID
  // Register JNI callbacks early so the first pickOpen/pickSave call
  // in this session does not race with the JVM class-loader.
  AndroidContentPicker::instance();
#endif

  m_profileManager = new UiProfileManager(this);
  connect(m_profileManager, &UiProfileManager::profileChanged, this,
          &MainWindow::applyProfile);

  // FIX: Singleton verwenden statt new
  m_toolManager = &ToolManager::instance();

  m_currentAccentColor = UIStyles::Accent;
  m_penColor = Qt::black;
  m_penWidth = 3;
  m_activeToolType = CanvasView::ToolType::Pen;
  m_penOnlyMode = true; // Default to pen-only input for writing-first behavior
  m_lblActiveNote = nullptr;
  m_isSidebarOpen = false;

  m_autoSaveTimer = new QTimer(this);
  m_autoSaveTimer->setInterval(1500);
  m_autoSaveTimer->setSingleShot(true);
  connect(m_autoSaveTimer, &QTimer::timeout, this,
          &MainWindow::performAutoSave);

  m_gridSpacingTimer = new QTimer(this);
  m_gridSpacingTimer->setInterval(100);
  m_gridSpacingTimer->setSingleShot(true);
  connect(m_gridSpacingTimer, &QTimer::timeout, this,
          &MainWindow::applyDelayedGridSpacing);

  m_a4SaveDebounce = new QTimer(this);
  m_a4SaveDebounce->setSingleShot(true);
  m_a4SaveDebounce->setInterval(1500);
  connect(m_a4SaveDebounce, &QTimer::timeout, this, [this]() {
    if (!m_pendingA4SaveNote || m_pendingA4SavePath.isEmpty()) return;
    Note copy = *m_pendingA4SaveNote;
    const QString p = m_pendingA4SavePath;
    m_noteManager.saveNoteAsync(copy, p, [p](bool ok) {
      if (!ok) qWarning() << "A4 async save failed" << p;
    });
  });

  createDefaultFolder();

  loadWebBookmarksFromSettings();

  setupTools(); // <-- WICHTIG: ToolManager Initialisierung hat gefehlt!
  qDebug() << "MainWindow: setupTools beendet";
  setupUi();
  qDebug() << "MainWindow: setupUi beendet";

  applyProfile(m_profileManager->currentProfile());
  qDebug() << "MainWindow: applyProfile beendet";
  applyTheme();
  qDebug() << "MainWindow: applyTheme beendet";

#ifdef Q_OS_ANDROID
  m_isSidebarOpen = false;
  m_sidebarContainer->hide();
  m_sidebarStrip->hide();
  // Let mode-aware visibility decide which hamburger is shown.
  updateSidebarState();
  m_mainSplitter->setHandleWidth(0);
#else
  qDebug() << "MainWindow: updateSidebarState davor";
  updateSidebarState();
  qDebug() << "MainWindow: updateSidebarState danach";
#endif

  // Initial mode: guest → Study, logged-in → Notes (setCurrentIndex emits onModeChanged)
  QString savedUser = QSettings("Blop", "BlopApp").value("username").toString();
  qInfo() << "MainWindow: startup savedUser=" << savedUser
          << "isEmpty=" << savedUser.trimmed().isEmpty()
          << "authLocked=" << m_authNavigationLocked
          << "sidebarOpen=" << m_isSidebarOpen;
  updateSidebarUser(savedUser);
  qInfo() << "MainWindow: after updateSidebarUser authLocked=" << m_authNavigationLocked
          << "sidebarOpen=" << m_isSidebarOpen
          << "mainStack=" << (m_mainContentStack ? m_mainContentStack->currentIndex() : -1);
  // Start unfolded in Notes for logged-in users.
  if (!savedUser.trimmed().isEmpty()) {
    animateSidebar(true);
  }

  auto failOAuthFlow = [this](const QString &reason) {
#ifdef Q_OS_ANDROID
    dismissAndroidOAuthOverlay();
#endif
    m_googleLoginInFlight = false;
    m_googleLoginInFlightSinceMs = 0;
    // Guests must stay gated on the login/Study surface; only clear the
    // temporary Google-login lock, don't unlock Notes for anonymous users.
    const QString user =
        QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
            .value(QStringLiteral("username"))
            .toString()
            .trimmed();
    m_authNavigationLocked = user.isEmpty();
#ifdef Q_OS_ANDROID
    if (m_androidHeader) {
      m_androidHeader->setVisible(!m_authNavigationLocked);
      if (!m_authNavigationLocked)
        m_androidHeader->raise();
    }
#endif
    emit oauthFailed(reason);
  };

  // Connect GoogleAuthManager's browser prompts to custom overlay logic
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::requireBrowser,
          this, [this, failOAuthFlow](const QUrl &url) {
#ifdef Q_OS_ANDROID
              // Avoid blocking dialogs on Android GL surface; fail fast and unlock UI.
              if (!QSslSocket::supportsSsl()) {
                  qCritical() << "Google login aborted: TLS unsupported. OpenSSL missing?"
                              << "TLS version:" << QSslSocket::sslLibraryVersionString();
                  failOAuthFlow(QStringLiteral("tls_unavailable"));
                  return;
              }
#endif
              if (!showAuthOverlay(url)) {
                qWarning() << "Google login aborted: could not open browser URL" << url;
                failOAuthFlow(QStringLiteral("browser_open_failed"));
                return;
              }
          });
          
  // Close the overlay when login completes successfully
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::authenticated, this, [this]() {
      m_googleLoginInFlight = false;
      m_googleLoginInFlightSinceMs = 0;
#ifdef Q_OS_ANDROID
      dismissAndroidOAuthOverlay();
#endif
      if (m_authOverlay) {
          m_authOverlay->accept();
          m_authOverlay->deleteLater();
          m_authOverlay = nullptr;
      }
  });


  // Verify the Google access token via our own backend and inject session into the WebView
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::idTokenReceived, this, [this, failOAuthFlow](const QString &token) {
      qDebug() << "Received Google token in MainWindow, posting to backend for verification...";
      if (token.trimmed().isEmpty()) {
        qWarning() << "Google idTokenReceived was empty";
        failOAuthFlow(QStringLiteral("empty_id_token"));
        return;
      }
      auto attemptVerify = std::make_shared<std::function<void(int)>>();
      *attemptVerify = [this, token, attemptVerify, failOAuthFlow](int attempt) {
        QNetworkAccessManager *nam = new QNetworkAccessManager(this);
        QNetworkRequest req(QUrl("https://blop-study.com/api/auth/google/verify"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject body;
        body["token"] = token;
#ifdef Q_OS_ANDROID
        body["client_id"] = QStringLiteral(
            "571766217-5pcb10b1bgdv5g31vjgfvftdudufjc4s.apps.googleusercontent.com");
#else
        body["client_id"] = QStringLiteral(
            "571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com");
#endif
        QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);

        QNetworkReply *reply = nam->post(req, postData);
        connect(reply, &QNetworkReply::finished, this, [this, reply, nam, attempt, attemptVerify, failOAuthFlow]() {
          const QString errorText = reply->errorString();
          const QByteArray raw = reply->readAll();
          const QString rawText = QString::fromUtf8(raw);
          const int statusCode =
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
          const QNetworkReply::NetworkError networkError = reply->error();
          const bool transientErr11 =
              errorText.contains("Resource temporarily unavailable", Qt::CaseInsensitive) ||
              rawText.contains("Errno 11", Qt::CaseInsensitive);

          reply->deleteLater();
          nam->deleteLater();

          if (transientErr11 && attempt < 3) {
            qWarning() << "Google verify transient Errno11, retrying attempt"
                       << (attempt + 1);
            QTimer::singleShot(400 * attempt, this, [attemptVerify, attempt]() {
              (*attemptVerify)(attempt + 1);
            });
            return;
          }

          if (networkError != QNetworkReply::NoError || statusCode >= 400) {
            qWarning() << "Backend Google verify failed:" << errorText
                       << "status:" << statusCode << "body:" << rawText;
            failOAuthFlow(QStringLiteral("backend_verify_failed"));
            return;
          }

          QJsonDocument doc = QJsonDocument::fromJson(raw);
          if (doc.isNull() || !doc.isObject()) {
            qWarning() << "Backend Google verify invalid JSON:" << rawText;
            failOAuthFlow(QStringLiteral("invalid_backend_json"));
            return;
          }
          QJsonObject obj = doc.object();
          QString sessionId = obj.value("session_id").toString();
          QString username = obj.value("username").toString();
          if (sessionId.isEmpty() || username.isEmpty()) {
            qWarning() << "Backend returned empty session_id or username!";
            failOAuthFlow(QStringLiteral("missing_session_or_username"));
            return;
          }
          qDebug() << "Backend verified Google login! username:" << username;
          m_googleLoginInFlight = false;

          QSettings st("Blop", "BlopApp");
          st.setValue("session_id", sessionId);
          st.sync();
          // Unlock native UI even when Study WebView is black / runJavaScript fails.
          updateSidebarUser(username);

          // Sync Study WebView localStorage when the embedded view works (optional).
          QString js = QString(
                           "localStorage.setItem('session_id', '%1');"
                           "localStorage.setItem('username', '%2');"
                           "window.location.href = '/';")
                           .arg(sessionId, username);

#ifdef Q_OS_ANDROID
          emit injectToken(js); // We reuse injectToken to pass raw JS to the QML WebView
#else
#ifdef BLOP_HAS_WEBENGINE
          if (m_studyWebView && m_studyWebView->page())
            m_studyWebView->page()->runJavaScript(js);
#endif
#endif
        });
      };
      (*attemptVerify)(1);
  });

  // Warn user if OAuth fails locally or via server
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::authenticationFailed, this, [this, failOAuthFlow](const QString& error) {
      failOAuthFlow(error);
      QString friendly = QStringLiteral(
          "Google-Anmeldung ist fehlgeschlagen. Bitte erneut versuchen.");
      if (error.contains(QStringLiteral("state_mismatch")))
        friendly = QStringLiteral(
            "Die Anmeldung wurde unterbrochen (App neu gestartet). "
            "Bitte tippe noch einmal auf „Über Google anmelden“.");
      else if (error.contains(QStringLiteral("oauth_redirect_missing")))
        friendly = QStringLiteral(
            "Google hat dich nicht zurück in die App gebracht "
            "(oft bleibt der Browser bei google.com hängen). "
            "Schließe den Google-Tab und tippe erneut auf "
            "„Über Google anmelden“.");
      else if (error.contains(QStringLiteral("token_exchange")))
        friendly = QStringLiteral(
            "Google konnte den Login nicht bestätigen. Prüfe die "
            "Internetverbindung und versuche es erneut.");
      else if (error.contains(QStringLiteral("oauth_error")))
        friendly = QStringLiteral(
            "Google hat die Anmeldung abgebrochen. Bitte erneut versuchen.");
      else if (error.contains(QStringLiteral("backend_verify")))
        friendly = QStringLiteral(
            "Server-Bestätigung fehlgeschlagen. Bitte später erneut versuchen.");
      else if (error.contains(QStringLiteral("browser_open")))
        friendly = QStringLiteral(
            "Es konnte kein Browser geöffnet werden. Installiere Chrome "
            "oder einen anderen Browser und versuche es erneut.");
      BlopDialogs::notify(this, QStringLiteral("Google Login"), friendly);
  });

  QTimer::singleShot(100, this, &MainWindow::updateGrid);
  // Delay update check by 5s so the web view has time to render before the
  // modal blocks
#ifndef Q_OS_ANDROID
  QTimer::singleShot(5000, this, &MainWindow::checkForUpdates);
#endif

  // Crash-Replay: if the previous run was killed by SIGSEGV/SIGABRT/etc.,
  // BlopDiag wrote /AppData/last_crash.txt; install() rotated it to
  // previous_crash.txt at startup. Show its contents in an in-window overlay
  // so the user can copy the stack trace for triage. 600ms delay so we land
  // AFTER the first paint (overlay would otherwise sit behind chrome).
  QTimer::singleShot(600, this, [this]() {
    const QString report = BlopDiag::takeCrashReportIfPresent();
    if (!report.isEmpty()) {
      showCrashReportOverlay(this, report);
    }
  });

  // v3.17.0: re-skin theme-aware surfaces whenever the user toggles
  // Light/Dark mode in the Settings dialog. The slot is intentionally
  // narrow -- it touches only the surfaces we have already converted to
  // theme tokens (Phase B3). Other widgets still using hard-coded hex
  // values will be migrated incrementally in v3.17.1+.
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          &MainWindow::applyThemeRefresh);
}
MainWindow::~MainWindow() {}

void MainWindow::refreshOpenEditorSceneBackgrounds() {
  if (!m_editorTabs)
    return;
  const QColor sceneBg = UIStyles::SceneBackground;
  for (int i = 0; i < m_editorTabs->count(); ++i) {
    QWidget *tab = m_editorTabs->widget(i);
    if (!tab)
      continue;
    if (auto *editor = qobject_cast<NoteEditor *>(tab)) {
      if (auto *view = editor->findChild<MultiPageNoteView *>()) {
        view->setBackgroundBrush(sceneBg);
        view->viewport()->update();
        view->update();
      }
    } else if (auto *canvas = qobject_cast<CanvasView *>(tab)) {
      canvas->setBackgroundBrush(sceneBg);
      if (canvas->viewport())
        canvas->viewport()->update();
      canvas->update();
    } else if (auto *wrapper = tab->findChild<CanvasView *>()) {
      wrapper->setBackgroundBrush(sceneBg);
      if (wrapper->viewport())
        wrapper->viewport()->update();
      wrapper->update();
    }
  }
}

#ifdef Q_OS_ANDROID
void MainWindow::syncStudyChromeTheme() {
  const QColor chrome = BlopTheme::surfaceBackground();
  if (m_studyContainer) {
    m_studyContainer->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(chrome.name(QColor::HexRgb)));
  }
  if (m_studyQQuickView) {
    m_studyQQuickView->setColor(chrome);
    if (QObject *root = m_studyQQuickView->rootObject())
      root->setProperty("studyChromeColor", chrome);
  }
}
#endif

void MainWindow::applyThemeRefresh() {
  // Keep note-editor chrome in sync with Settings Design mode so Hell/Dunkel
  // works app-wide (library + note page).
#ifndef Q_OS_ANDROID
  NoteChrome::setMode(BlopTheme::instance().mode() == BlopTheme::Mode::Light
                          ? NoteChrome::Mode::Light
                          : NoteChrome::Mode::Dark);
#endif
  if (m_centralContainer) {
    m_centralContainer->setStyleSheet(
        QStringLiteral("QWidget#CentralContainer { background-color: %1; }")
            .arg(BlopTheme::surfaceBackground().name(QColor::HexRgb)));
  }
#ifdef Q_OS_ANDROID
  if (m_androidHeader) {
    // v3.18.7: chrome blends into the page background — see setupUi()
    // for the rationale. Mirror that here so theme toggles take effect
    // on already-constructed widgets.
    m_androidHeader->setStyleSheet(
        QStringLiteral("QWidget#AndroidTopChrome { background-color: %1; "
                       "border: none; }")
            .arg(BlopTheme::surfaceBackground().name(QColor::HexRgb)));
    if (QWidget *inner = m_androidHeader->findChild<QWidget *>(
            QStringLiteral("AndroidTopHeaderInner"))) {
      inner->setStyleSheet(QStringLiteral(
          "QWidget#AndroidTopHeaderInner { background: transparent; }"));
    }
  }
  syncStudyChromeTheme();
#endif
  refreshOpenEditorSceneBackgrounds();
  // v3.17.1: re-run the whole stylesheet construction so every QSS
  // wrapped in BlopTheme::themed() (mainwindow chrome, sidebar, title
  // bar, overview, editor tab strip) picks up the new token values.
  // applyTheme() is the central QSS factory -- calling it is equivalent
  // to "re-skin the entire main window now".
  applyTheme();

  // Toolbars: ask each one to rebuild its own stylesheet. ModernToolbar
  // and AndroidPhoneToolbar both expose setAccentColor() which already
  // re-skins their internal QSS off the current accent + base colors;
  // since UIStyles + BlopStyle now read from BlopTheme, calling that
  // is enough.
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
#ifndef Q_OS_ANDROID
    const bool editorOpen =
        m_documentTabBar && m_documentTabBar->noteChromeMode();
    tb->setAccentColor(editorOpen ? NoteChrome::accent()
                                  : m_currentAccentColor);
#else
    tb->setAccentColor(m_currentAccentColor);
#endif
  }
  if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools))
    phone->setAccentColor(m_currentAccentColor);
  if (m_penPresetBar)
    m_penPresetBar->setAccentColor(m_currentAccentColor);
  if (m_documentTabBar) {
#ifndef Q_OS_ANDROID
    if (m_documentTabBar->noteChromeMode()) {
      m_documentTabBar->setAccentColor(NoteChrome::accent());
    } else
#endif
      m_documentTabBar->setAccentColor(m_currentAccentColor);
  }
    if (m_pageThumbnailSidebar)
      m_pageThumbnailSidebar->setAccentColor(
#ifndef Q_OS_ANDROID
          (m_documentTabBar && m_documentTabBar->noteChromeMode())
              ? NoteChrome::accent()
              :
#endif
              m_currentAccentColor);
    if (m_noteLeftRail)
      m_noteLeftRail->setAccentColor(
#ifndef Q_OS_ANDROID
          (m_documentTabBar && m_documentTabBar->noteChromeMode())
              ? NoteChrome::accent()
              :
#endif
              m_currentAccentColor);
    if (m_radialFab)
      m_radialFab->setAccentColor(m_currentAccentColor);
    if (m_libraryTagsPanel)
      m_libraryTagsPanel->setAccentColor(m_currentAccentColor);
    if (m_libraryOrgBar)
      m_libraryOrgBar->setAccentColor(m_currentAccentColor);
  if (m_noteHeader)
    m_noteHeader->setStyleSheet(
        QStringLiteral("QWidget#NoteHeader { background: transparent; border-bottom: 1px solid %1; }")
            .arg(BlopTheme::borderSubtle().name(QColor::HexArgb)));

  // PageManager: re-skin scrim + panel using the new tokens. The panel
  // QSS was set once in the ctor; this method is the documented post-
  // theme-change refresh hook.
  if (m_pageManager) {
    m_pageManager->applyThemeRefresh();
  }
  // v3.18.5: re-skin every control inside the Page-Settings sheet so
  // theme toggles take effect on already-constructed widgets.
  refreshPageSettingsTheme();

#ifndef Q_OS_ANDROID
  {
    const bool inNote =
        m_documentTabBar && m_documentTabBar->noteChromeMode();
    if (inNote)
      applyNoteChromeTheme();
    else
      refreshNoteTitleChrome(false);
  }
#endif

  // Nudge the file-list viewport so AndroidTileDelegate re-paints with
  // the new card color.
  if (m_fileListView && m_fileListView->viewport())
    m_fileListView->viewport()->update();
  // Force a polish pass on the entire MainWindow so any QSS-tagged
  // widgets pick up the new tokens without needing per-widget churn.
  if (style()) {
    style()->unpolish(this);
    style()->polish(this);
  }
#ifdef Q_OS_ANDROID
  if (m_mainContentStack)
    applyAndroidTabStyles(m_mainContentStack->currentIndex());
  if (m_btnAndroidHome) {
    const QColor fg = BlopTheme::textPrimary();
    m_btnAndroidHome->setIcon(
        createModernIcon(QStringLiteral("home"), fg));
    m_btnAndroidHome->setIconSize(QSize(UiScale::dp(22), UiScale::dp(22)));
    m_btnAndroidHome->setStyleSheet(
        BlopStyle::toolButtonAccentQss(m_currentAccentColor, UiScale::dp(16)));
  }
#endif
  // v3.18.5: re-tint sidebar nav icons and force a delegate repaint so
  // the Light/Dark switch is fully reflected in the custom-painted
  // nav list (QSS rules don't reach the delegate).
  if (m_navSidebar) {
    const QColor navIcon = BlopTheme::textSecondary();
    for (int i = 0; i < m_navSidebar->count(); ++i) {
      QListWidgetItem *it = m_navSidebar->item(i);
      if (!it)
        continue;
      const bool isHeader = it->data(Qt::UserRole + 1).toBool();
      if (isHeader)
        continue;
      const QString iconKey = it->data(Qt::UserRole + 11).toString();
      if (!iconKey.isEmpty()) {
        it->setIcon(createModernIcon(iconKey, navIcon));
      }
    }
    if (m_navSidebar->viewport())
      m_navSidebar->viewport()->update();
  }
  update();
}

void MainWindow::setupTools() {
  // FIX: Konkrete Tool-Instanzen erstellen und ToolMode enum nutzen
  if (m_toolManager) {
    // Register Tools
    m_toolManager->registerTool(new PenTool());
    m_toolManager->registerTool(
        new PencilTool()); // NEU: Bleistift auch registrieren
    m_toolManager->registerTool(new EraserTool());

    // FIX: HighlighterTool statt MarkerTool
    m_toolManager->registerTool(new HighlighterTool());

    m_toolManager->registerTool(new LassoTool());
    m_toolManager->registerTool(new RulerTool());
    m_toolManager->registerTool(new ImageTool());
    m_toolManager->registerTool(new ShapeTool());
    m_toolManager->registerTool(new TextTool()); // <--- TextTool Registrierung
    m_toolManager->registerTool(new StickyNoteTool());
    m_toolManager->registerTool(new HandTool());

    m_toolManager->selectTool(ToolMode::Pen);
  }
}

void MainWindow::checkForUpdates() {
#ifdef Q_OS_ANDROID
  // Android updates are handled via Play Store, not via in-app updater dialog.
  return;
#endif
  if (!m_netManager) {
    m_netManager = new QNetworkAccessManager(this);
  }

  // Use the latest stable versioned release (e.g. v3.5.3.9), NOT nightly
  QUrl url(
      "https://api.github.com/repos/BenSchwank/Blop-releases/releases/latest");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    "Blop-App/" + QString(BLOP_VERSION));
  request.setRawHeader("Accept", "application/vnd.github+json");

  QNetworkReply *reply = m_netManager->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
      return; // Silent fail on network error

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject())
      return;
    QJsonObject root = doc.object();

    auto normalizeVersion = [](QString v) -> QString {
      v = v.trimmed();
      if (v.startsWith('v', Qt::CaseInsensitive))
        v = v.mid(1);
      return v;
    };

    // Parse tag name, e.g. "v3.5.3.9" -> "3.5.3.9"
    QString tagName = normalizeVersion(root.value("tag_name").toString());
    const QString localVersion = normalizeVersion(QString(BLOP_VERSION));

    // Numeric version comparison: "3.5.3.9" vs BLOP_VERSION "3.3.3"
    auto parseVersion = [](const QString &v) -> QList<int> {
      QList<int> parts;
      for (const QString &p : v.split('.'))
        parts << p.toInt();
      while (parts.size() < 4)
        parts << 0; // normalize to 4 segments
      return parts;
    };
    QList<int> remote = parseVersion(tagName);
    QList<int> local = parseVersion(localVersion);

    bool isNewer = false;
    for (int i = 0; i < 4; ++i) {
      if (remote[i] > local[i]) {
        isNewer = true;
        break;
      }
      if (remote[i] < local[i])
        break;
    }
    if (!isNewer)
      return; // Already up to date

    // Check if the user already dismissed this specific version
    QSettings settings("Blop", "BlopApp");
    QString dismissedVersion =
        settings.value("dismissedUpdateVersion").toString();
    if (dismissedVersion == tagName)
      return; // User already chose "Später" for this version

    // Find the correct download URL for this platform
    QJsonArray assets = root.value("assets").toArray();
    QString downloadUrl;
    QString releasePageUrl = root.value("html_url").toString();

#ifdef Q_OS_ANDROID
    // Android: prefer signed APK
    for (const QJsonValue &a : assets) {
      QString name = a.toObject().value("name").toString();
      if (name == "Blop-signed.apk") {
        downloadUrl = a.toObject().value("browser_download_url").toString();
        break;
      }
    }
#else
    // Windows / Desktop: prefer Windows installer
    for (const QJsonValue &a : assets) {
      QString name = a.toObject().value("name").toString();
      if (name == "Blop_Windows_Installer.exe") {
        downloadUrl = a.toObject().value("browser_download_url").toString();
        break;
      }
    }
#endif

    if (downloadUrl.isEmpty())
      downloadUrl = releasePageUrl; // Fallback: open release page

    // Show update dialog
    const QString currentVer = localVersion;
    const bool downloadNow = BlopDialogs::confirm(
        this, QStringLiteral("Update verfügbar"),
        QStringLiteral("Blop v%1 ist verfügbar!\nDeine Version: v%2\n\nJetzt herunterladen?")
            .arg(tagName, currentVer),
        QStringLiteral("Jetzt herunterladen"), QStringLiteral("Später"));
    if (downloadNow) {
#ifdef Q_OS_ANDROID
      QDesktopServices::openUrl(QUrl(downloadUrl));
#else
      // --- In-App Updater Download & Execute für Windows ---
      if (downloadUrl == releasePageUrl) {
          // Falls kein Installer gefunden wurde, öffne Fallback im Browser
          QDesktopServices::openUrl(QUrl(downloadUrl));
      } else {
          QProgressDialog *progress = new QProgressDialog("Update wird heruntergeladen...", "Abbrechen", 0, 100, this);
          progress->setWindowTitle("Blop Update");
          progress->setWindowModality(Qt::WindowModal);
          progress->setMinimumDuration(0);
          progress->setValue(0);

          QUrl dlUrl(downloadUrl);
          QNetworkRequest dlReq(dlUrl);
          dlReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

          QNetworkReply *dlReply = m_netManager->get(dlReq);

          QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/Blop_Update.exe";
          QFile *file = new QFile(tempPath);
          if (!file->open(QIODevice::WriteOnly)) {
              BlopDialogs::notify(this, QStringLiteral("Fehler"),
                                  QStringLiteral("Lokale Datei konnte nicht erstellt werden."));
              delete progress;
              dlReply->deleteLater();
              return;
          }

          connect(dlReply, &QNetworkReply::readyRead, this, [dlReply, file]() {
              file->write(dlReply->readAll());
          });

          connect(dlReply, &QNetworkReply::downloadProgress, this, [progress](qint64 bytesReceived, qint64 bytesTotal) {
              if (bytesTotal > 0) {
                  progress->setMaximum(bytesTotal);
                  progress->setValue(bytesReceived);
              }
          });

          connect(progress, &QProgressDialog::canceled, this, [dlReply, file, tempPath]() {
              dlReply->abort();
              file->close();
              QFile::remove(tempPath);
              file->deleteLater();
              dlReply->deleteLater();
          });

          connect(dlReply, &QNetworkReply::finished, this, [this, dlReply, file, tempPath, progress]() {
              progress->deleteLater();
              file->close();

              if (dlReply->error() == QNetworkReply::NoError) {
                  QProcess::startDetached(tempPath, QStringList());
                  QApplication::quit();
              } else {
                  QFile::remove(tempPath);
                  if (dlReply->error() != QNetworkReply::OperationCanceledError) {
                      BlopDialogs::notify(this, QStringLiteral("Fehler"),
                                          QStringLiteral("Fehler beim Download des Updates:\n%1")
                                              .arg(dlReply->errorString()));
                  }
              }
              file->deleteLater();
              dlReply->deleteLater();
          });
      }
#endif
    } else {
      // User clicked "Später" — remember this version, don't show again for it
      QSettings("Blop", "BlopApp").setValue("dismissedUpdateVersion", tagName);
    }
  });
}

void MainWindow::loadWebBookmarksFromSettings() {
  m_webBookmarks.clear();
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const int n = s.beginReadArray(QStringLiteral("webBookmarks"));
  for (int i = 0; i < n; ++i) {
    s.setArrayIndex(i);
    QString title = s.value(QStringLiteral("title")).toString();
    const QUrl url = s.value(QStringLiteral("url")).toUrl();
    if (url.isValid() && (url.scheme() == QLatin1String("http") ||
                          url.scheme() == QLatin1String("https"))) {
      if (title.isEmpty())
        title = url.host().isEmpty() ? url.toDisplayString() : url.host();
      m_webBookmarks.push_back({title, url});
    }
  }
  s.endArray();
}

void MainWindow::saveWebBookmarksToSettings() const {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.beginWriteArray(QStringLiteral("webBookmarks"));
  for (int i = 0; i < m_webBookmarks.size(); ++i) {
    s.setArrayIndex(i);
    s.setValue(QStringLiteral("title"), m_webBookmarks[i].title);
    s.setValue(QStringLiteral("url"), m_webBookmarks[i].url);
  }
  s.endArray();
  s.sync();
}

void MainWindow::rebuildModeSelectorItems() {
  if (!m_modeSelector)
    return;
  const int prev = m_modeSelector->currentIndex();
  QSignalBlocker blocker(m_modeSelector);
  m_modeSelector->clear();
  m_modeSelector->addItem(tr("Notizen"));
  m_modeSelector->addItem(tr("Study"));
  for (const WebBookmark &bm : m_webBookmarks) {
    const int idx = m_modeSelector->count();
    m_modeSelector->addItem(bm.title);
    m_modeSelector->setItemData(idx, bm.url, Qt::UserRole);
  }
  const int maxIdx = qMax(0, m_modeSelector->count() - 1);
  const int newIdx = qBound(0, prev, maxIdx);
  m_modeSelector->setCurrentIndex(newIdx);
  if (m_btnMode)
    m_btnMode->setText(m_modeSelector->itemText(newIdx) + QStringLiteral("  \u25be"));
}

QUrl MainWindow::normalizedUserWebUrl(QString input) const {
  input = input.trimmed();
  if (input.isEmpty())
    return {};
  if (!input.contains(QLatin1String("://")))
    input = QStringLiteral("https://") + input;
  const QUrl u = QUrl::fromUserInput(input);
  if (!u.isValid() || (u.scheme() != QLatin1String("http") &&
                       u.scheme() != QLatin1String("https")))
    return {};
  return u;
}

void MainWindow::openModeMenuAtButton() {
  if (!m_btnMode || !m_modeSelector)
    return;
#ifdef Q_OS_ANDROID
  QList<BlopInWindowMenu::Item> items;
  items.append({tr("Notizen"), QIcon(),
                [this]() { if (m_modeSelector) m_modeSelector->setCurrentIndex(0); }});
  items.append({tr("Study"), QIcon(),
                [this]() { if (m_modeSelector) m_modeSelector->setCurrentIndex(1); }});
  if (!m_webBookmarks.isEmpty())
    items.append({QString(), QIcon(), {}, false, true});
  for (int i = 0; i < m_webBookmarks.size(); ++i) {
    const int idx = 2 + i;
    items.append({m_webBookmarks[i].title, QIcon(), [this, idx]() {
      if (m_modeSelector && idx < m_modeSelector->count())
        m_modeSelector->setCurrentIndex(idx);
    }});
  }
  items.append({QString(), QIcon(), {}, false, true});
  items.append({tr("URL hinzufügen…"), QIcon(),
                [this]() { showAddWebBookmarkDialog(); }});
  items.append({tr("Web-Lesezeichen verwalten…"), QIcon(),
                [this]() { showManageWebBookmarksDialog(); }});
  BlopInWindowMenu::show(this, m_btnMode->mapToGlobal(QPoint(0, m_btnMode->height())), items);
#else
  QMenu menu(this);
  menu.setStyleSheet(blopWebMenuStyleSheet());
  QAction *aNotes = menu.addAction(tr("Notizen"));
  aNotes->setData(0);
  QAction *aStudy = menu.addAction(tr("Study"));
  aStudy->setData(1);
  if (!m_webBookmarks.isEmpty())
    menu.addSeparator();
  for (int i = 0; i < m_webBookmarks.size(); ++i) {
    QAction *a = menu.addAction(m_webBookmarks[i].title);
    a->setData(2 + i);
  }
  menu.addSeparator();
  QAction *aAdd = menu.addAction(tr("URL hinzufügen…"));
  aAdd->setData(-1);
  QAction *aManage = menu.addAction(tr("Web-Lesezeichen verwalten…"));
  aManage->setData(-2);
  QAction *chosen =
      menu.exec(m_btnMode->mapToGlobal(QPoint(0, m_btnMode->height())));
  if (!chosen)
    return;
  const int tag = chosen->data().toInt();
  if (tag == -1) {
    showAddWebBookmarkDialog();
    return;
  }
  if (tag == -2) {
    showManageWebBookmarksDialog();
    return;
  }
  if (tag >= 0 && tag < m_modeSelector->count())
    m_modeSelector->setCurrentIndex(tag);
#endif
}

void MainWindow::showAddWebBookmarkDialog() {
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Webseite hinzufügen"));
  dlg.setModal(true);
  dlg.setMinimumWidth(420);
  applyBlopWebSheetStyle(&dlg);

  auto *rootLay = new QVBoxLayout(&dlg);
  rootLay->setContentsMargins(24, 20, 24, 20);
  rootLay->setSpacing(16);

  auto *lblHead = new QLabel(tr("Neue Webseite"), &dlg);
  lblHead->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: #F4F2FF; font-size: 20px; font-weight: 700; "
                     "letter-spacing: 0.3px;")));
  rootLay->addWidget(lblHead);

  auto *lblSub = new QLabel(
      tr("Speichere eine Seite als Lesezeichen — du wechselst später über das "
         "Menü neben „Notizen“."),
      &dlg);
  lblSub->setWordWrap(true);
  lblSub->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: rgba(232,228,255,0.72); font-size: 12px;")));
  rootLay->addWidget(lblSub);

  auto *lblUrl = new QLabel(tr("Adresse"), &dlg);
  lblUrl->setStyleSheet(BlopTheme::themed(
      QStringLiteral("font-weight: 600; color: #D8D4F5;")));
  rootLay->addWidget(lblUrl);
  auto *edUrl = new QLineEdit(&dlg);
  edUrl->setPlaceholderText(QStringLiteral("https://…"));
  rootLay->addWidget(edUrl);

  auto *lblTit = new QLabel(tr("Anzeigename (optional)"), &dlg);
  lblTit->setStyleSheet(BlopTheme::themed(
      QStringLiteral("font-weight: 600; color: #D8D4F5;")));
  rootLay->addWidget(lblTit);
  auto *edTitle = new QLineEdit(&dlg);
  edTitle->setPlaceholderText(tr("z. B. GeoGebra"));
  rootLay->addWidget(edTitle);

  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch(1);
  auto *btnCancel = new QPushButton(tr("Abbrechen"), &dlg);
  btnCancel->setCursor(Qt::PointingHandCursor);
  btnCancel->setStyleSheet(blopGhostButtonStyle());
  auto *btnAdd = new QPushButton(tr("Hinzufügen"), &dlg);
  btnAdd->setDefault(true);
  btnAdd->setCursor(Qt::PointingHandCursor);
  btnAdd->setStyleSheet(blopPrimaryButtonStyle());
  btnRow->addWidget(btnCancel);
  btnRow->addWidget(btnAdd);
  rootLay->addLayout(btnRow);

  connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
  connect(btnAdd, &QPushButton::clicked, &dlg, &QDialog::accept);
  connect(edUrl, &QLineEdit::returnPressed, &dlg, &QDialog::accept);

  if (dlg.exec() != QDialog::Accepted)
    return;
  const QUrl url = normalizedUserWebUrl(edUrl->text());
  if (!url.isValid()) {
    QMessageBox warn(this);
    warn.setIcon(QMessageBox::Warning);
    warn.setWindowTitle(tr("Ungültige Adresse"));
    warn.setText(tr("Bitte eine gültige http- oder https-Adresse eingeben."));
    warn.setStyleSheet(BlopTheme::themed(QString::fromUtf8(
        R"(QMessageBox { background-color: #14121F; }
QLabel { color: #E8E4FF; font-size: 13px; min-width: 280px; }
QPushButton { background: #5E5CE6; color: white; border: none; border-radius: 8px; padding: 8px 20px; font-weight: 600; min-width: 88px; }
QPushButton:hover { background: #7D7AFF; })")));
    warn.exec();
    return;
  }
  QString title = edTitle->text().trimmed();
  if (title.isEmpty())
    title = url.host().isEmpty() ? url.toDisplayString() : url.host();
  m_webBookmarks.push_back({title, url});
  saveWebBookmarksToSettings();
  rebuildModeSelectorItems();
  const int newIdx = m_modeSelector->count() - 1;
#ifdef Q_OS_ANDROID
  QSignalBlocker b(m_modeSelector);
  m_modeSelector->setCurrentIndex(newIdx);
  onModeChanged(newIdx);
#else
  m_modeSelector->setCurrentIndex(newIdx);
#endif
}

void MainWindow::showManageWebBookmarksDialog() {
  if (m_webBookmarks.isEmpty()) {
    QMessageBox info(this);
    info.setIcon(QMessageBox::Information);
    info.setWindowTitle(tr("Web-Lesezeichen"));
    info.setText(tr("Noch keine gespeicherten Seiten."));
    info.setStyleSheet(BlopTheme::themed(QString::fromUtf8(
        R"(QMessageBox { background-color: #14121F; }
QLabel { color: #E8E4FF; font-size: 13px; }
QPushButton { background: #5E5CE6; color: white; border: none; border-radius: 8px; padding: 8px 20px; font-weight: 600; min-width: 88px; }
QPushButton:hover { background: #7D7AFF; })")));
    info.exec();
    return;
  }
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Web-Lesezeichen"));
  dlg.resize(440, 360);
  applyBlopWebSheetStyle(&dlg);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setContentsMargins(22, 18, 22, 18);
  lay->setSpacing(14);
  auto *hdr = new QLabel(tr("Gespeicherte Seiten"), &dlg);
  hdr->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: #F4F2FF; font-size: 18px; font-weight: 700;")));
  lay->addWidget(hdr);
  auto *sub = new QLabel(tr("Auswahl markieren und entfernen."), &dlg);
  sub->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: rgba(232,228,255,0.65); font-size: 12px;")));
  lay->addWidget(sub);
  auto *list = new QListWidget(&dlg);
  const auto refillList = [&]() {
    list->clear();
    for (int i = 0; i < m_webBookmarks.size(); ++i) {
      QListWidgetItem *it =
          new QListWidgetItem(m_webBookmarks[i].title + QStringLiteral(" — ") +
                              m_webBookmarks[i].url.toDisplayString());
      it->setData(Qt::UserRole, i);
      list->addItem(it);
    }
  };
  refillList();
  lay->addWidget(list, 1);
  auto *btnRow = new QHBoxLayout();
  QPushButton *btnDel = new QPushButton(tr("Entfernen"), &dlg);
  btnDel->setCursor(Qt::PointingHandCursor);
  btnDel->setStyleSheet(
      QString::fromUtf8(
          R"(QPushButton { background: rgba(232, 72, 85, 0.18); color: #FFB4B8; border: 1px solid rgba(232,72,85,0.45); border-radius: 10px; padding: 10px 20px; font-size: 13px; font-weight: 600; }
QPushButton:hover { background: rgba(232, 72, 85, 0.28); color: #FFFFFF; })"));
  QPushButton *btnClose = new QPushButton(tr("Fertig"), &dlg);
  btnClose->setCursor(Qt::PointingHandCursor);
  btnClose->setStyleSheet(blopPrimaryButtonStyle());
  btnRow->addWidget(btnDel);
  btnRow->addStretch(1);
  btnRow->addWidget(btnClose);
  lay->addLayout(btnRow);
  connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
  connect(btnDel, &QPushButton::clicked, this, [&]() {
    const QList<QListWidgetItem *> sel = list->selectedItems();
    if (sel.isEmpty())
      return;
    const int bmIdx = sel.front()->data(Qt::UserRole).toInt();
    if (bmIdx < 0 || bmIdx >= m_webBookmarks.size())
      return;
    m_webBookmarks.removeAt(bmIdx);
    saveWebBookmarksToSettings();
    rebuildModeSelectorItems();
    const int cur = m_modeSelector ? m_modeSelector->currentIndex() : 0;
    onModeChanged(cur);
    refillList();
  });
  dlg.exec();
}

void MainWindow::openWebBookmarkOverflowMenuFromWidget(QWidget *anchor) {
  if (!anchor || !m_modeSelector)
    return;
#ifdef Q_OS_ANDROID
  qInfo() << "Android uses QML bookmark sheet only; widget menu suppressed";
  return;
#endif

  QMenu menu(this);
  menu.setStyleSheet(blopWebMenuStyleSheet());
  for (int i = 0; i < m_webBookmarks.size(); ++i) {
    QAction *a = menu.addAction(m_webBookmarks[i].title);
    a->setData(2 + i);
  }
  if (!m_webBookmarks.isEmpty())
    menu.addSeparator();
  QAction *aAdd = menu.addAction(tr("URL hinzufügen…"));
  aAdd->setData(-1);
  QAction *aManage = menu.addAction(tr("Web-Lesezeichen verwalten…"));
  aManage->setData(-2);
  QAction *chosen =
      menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
  if (!chosen)
    return;
  const int tag = chosen->data().toInt();
  if (tag == -1) {
    showAddWebBookmarkDialog();
    return;
  }
  if (tag == -2) {
    showManageWebBookmarksDialog();
    return;
  }
  if (tag >= 2 && tag < m_modeSelector->count()) {
#ifdef Q_OS_ANDROID
    QSignalBlocker b(m_modeSelector);
    m_modeSelector->setCurrentIndex(tag);
    onModeChanged(tag);
#else
    m_modeSelector->setCurrentIndex(tag);
#endif
  }
}

void MainWindow::applyDesktopWebSubviewForModeIndex(int modeIndex) {
#if defined(BLOP_HAS_WEBENGINE) && !defined(Q_OS_ANDROID)
  if (modeIndex <= 0) {
    if (m_studySsoTimer)
      m_studySsoTimer->stop();
    return;
  }
  if (!m_webViewStack || !m_studyWebView) {
    if (modeIndex >= 2 && m_modeSelector) {
      const QUrl u = m_modeSelector->itemData(modeIndex, Qt::UserRole).toUrl();
      if (u.isValid())
        QDesktopServices::openUrl(u);
    }
    return;
  }
  if (modeIndex == 1) {
    m_webViewStack->setCurrentWidget(m_studyWebView);
    if (m_studySsoTimer)
      m_studySsoTimer->start();
    return;
  }
  if (m_customWebView) {
    m_webViewStack->setCurrentWidget(m_customWebView);
    if (m_studySsoTimer)
      m_studySsoTimer->stop();
    if (m_modeSelector) {
      const QUrl u = m_modeSelector->itemData(modeIndex, Qt::UserRole).toUrl();
      if (u.isValid())
        m_customWebView->load(u);
    }
  }
#endif
}

void MainWindow::resetEmbeddedWebToStudy() {
#ifdef Q_OS_ANDROID
  invokeAndroidWebDestination(0);
#else
#if defined(BLOP_HAS_WEBENGINE)
  if (m_webViewStack && m_studyWebView)
    m_webViewStack->setCurrentWidget(m_studyWebView);
#endif
#endif
}

#ifdef Q_OS_ANDROID
void MainWindow::invokeAndroidWebDestination(int kind, const QString &url) {
  if (!m_studyQQuickView)
    return;
  QObject *root = m_studyQQuickView->rootObject();
  if (!root)
    return;
  const QVariant k(kind);
  const QVariant u(url);
  const bool ok = QMetaObject::invokeMethod(
      root, "setWebDestination", Qt::QueuedConnection, Q_ARG(QVariant, k),
      Q_ARG(QVariant, u));
  if (!ok)
    qWarning() << "QML setWebDestination invoke failed";
  // JNI cache scheduling is handled by the guarded onModeChanged deferred path.
}

void MainWindow::dismissAndroidOAuthOverlay() {
  if (!m_androidOAuthOverlay)
    return;
  QWidget *w = m_androidOAuthOverlay;
  m_androidOAuthOverlay = nullptr;
  w->hide();
  w->deleteLater();
}

void MainWindow::ensureAndroidStudyBootOverlay() {
  if (m_androidStudyBootOverlay || !m_centralContainer)
    return;

  auto *overlay = new QWidget(m_centralContainer);
  overlay->setObjectName(QStringLiteral("AndroidStudyBootOverlay"));
  overlay->setAttribute(Qt::WA_StyledBackground, true);
  overlay->setStyleSheet(
      QStringLiteral("QWidget#AndroidStudyBootOverlay { background-color: %1; }")
          .arg(BlopTheme::surfaceBackground().name(QColor::HexRgb)));

  auto *lay = new QVBoxLayout(overlay);
  lay->setContentsMargins(UiScale::dp(24), UiScale::dp(24), UiScale::dp(24),
                          UiScale::dp(24));
  lay->setSpacing(UiScale::dp(16));
  lay->setAlignment(Qt::AlignCenter);

  auto *lbl = new QLabel(tr("Lade Anmeldung..."), overlay);
  lbl->setAlignment(Qt::AlignCenter);
  lbl->setWordWrap(true);
  lbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: %1; font-size: 15px; font-weight: 600; background: transparent;")
                           .arg(BlopTheme::textPrimary().name())));
  lay->addWidget(lbl);

  auto *progress = new QProgressBar(overlay);
  progress->setRange(0, 0);
  progress->setFixedWidth(UiScale::dp(220));
  progress->setTextVisible(false);
  lay->addWidget(progress, 0, Qt::AlignHCenter);

  m_androidStudyBootRetryBtn = new QPushButton(tr("Erneut versuchen"), overlay);
  m_androidStudyBootRetryBtn->setStyleSheet(BlopTheme::primaryButtonQss());
  m_androidStudyBootRetryBtn->setFixedHeight(UiScale::dp(42));
  m_androidStudyBootRetryBtn->setMinimumWidth(UiScale::dp(180));
  m_androidStudyBootRetryBtn->hide();
  lay->addWidget(m_androidStudyBootRetryBtn, 0, Qt::AlignHCenter);

  connect(m_androidStudyBootRetryBtn, &QPushButton::clicked, this, [this]() {
    qInfo() << "MainWindow: study boot overlay retry tapped";
    if (m_androidStudyBootRetryBtn)
      m_androidStudyBootRetryBtn->hide();
    if (m_androidStudyBootRetryTimer)
      m_androidStudyBootRetryTimer->start();
    if (m_studyQQuickView && m_studyQQuickView->rootObject()) {
      QObject *root = m_studyQQuickView->rootObject();
      QMetaObject::invokeMethod(
          root, "scheduleStudyWebViewRecreate", Qt::QueuedConnection,
          Q_ARG(QVariant, QVariant(QStringLiteral("userRetryCpp"))));
    }
  });

  m_androidStudyBootRetryTimer = new QTimer(overlay);
  m_androidStudyBootRetryTimer->setSingleShot(true);
  m_androidStudyBootRetryTimer->setInterval(6000);
  connect(m_androidStudyBootRetryTimer, &QTimer::timeout, this,
          &MainWindow::showAndroidStudyBootRetry);

  overlay->hide();
  m_androidStudyBootOverlay = overlay;
}

void MainWindow::syncAndroidStudyBootOverlayGeometry() {
  if (!m_androidStudyBootOverlay || !m_mainContentStack || !m_centralContainer)
    return;
  const QPoint topLeft =
      m_mainContentStack->mapTo(m_centralContainer, QPoint(0, 0));
  m_androidStudyBootOverlay->setGeometry(
      QRect(topLeft, m_mainContentStack->size()));
}

void MainWindow::setAndroidStudyBootOverlayVisible(bool visible) {
  // v3.18.13: The C++ boot overlay is disabled. The QML startupLoadingOverlay
  // (AndroidWebView.qml) already provides a spinner + retry button and does
  // not block touch events. The C++ overlay was covering the QML layer and
  // preventing interaction even after the WebView had loaded.
  Q_UNUSED(visible)
}

#endif

void MainWindow::notifyStudyFirstLoadDone() {
#ifdef Q_OS_ANDROID
  qInfo() << "MainWindow: notifyStudyFirstLoadDone";
  setAndroidStudyBootOverlayVisible(false);

  // Restore saved session into the WebView localStorage so the user stays
  // logged in after a tab switch or app restart. The Study page will already
  // be on the login screen at this point; injecting session_id + username
  // triggers the SPA to navigate away from /login automatically.
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QString sessionId = st.value(QStringLiteral("session_id")).toString();
  const QString username  = st.value(QStringLiteral("username")).toString();
  if (!sessionId.isEmpty() && !username.isEmpty()) {
    qInfo() << "MainWindow: restoring session into Study WebView for user" << username;
    const QString js = QString(
        "localStorage.setItem('session_id', '%1');"
        "localStorage.setItem('username', '%2');"
        "if (window.location.pathname === '/login' || window.location.pathname === '/register') {"
        "  window.location.href = '/'; }")
        .arg(sessionId, username);
    emit injectToken(js);
  }
#endif
}

QString MainWindow::savedStudySessionParam() const {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QString username = st.value(QStringLiteral("username")).toString();
  const QString sessionId = st.value(QStringLiteral("session_id")).toString();
  if (username.isEmpty() || sessionId.isEmpty())
    return QString();
  const QString usrEnc = QString::fromUtf8(QUrl::toPercentEncoding(username));
  const QString sidEnc = QString::fromUtf8(QUrl::toPercentEncoding(sessionId));
  return QStringLiteral("&blop_usr=%1&blop_sid=%2").arg(usrEnc, sidEnc);
}

void MainWindow::showAndroidStudyBootRetry() {
#ifdef Q_OS_ANDROID
  if (!m_androidStudyBootOverlay || !m_androidStudyBootOverlay->isVisible())
    return;
  qInfo() << "MainWindow: study boot overlay retry button armed";
  if (m_androidStudyBootRetryBtn)
    m_androidStudyBootRetryBtn->show();
#endif
}

void MainWindow::setupTitleBar() {
  m_titleBarWidget = new QWidget(this);
  m_titleBarWidget->setFixedHeight(52);
  m_titleBarWidget->setStyleSheet(
      "background: #0B0912;"
      "border-bottom: 1px solid rgba(120,130,160,0.12);");

  QHBoxLayout *mainLayout = new QHBoxLayout(m_titleBarWidget);
  mainLayout->setContentsMargins(10, 0, 0, 0);
  mainLayout->setSpacing(6);

  // ── Hamburger ─────────────────────────────────────────────────────────────
  btnEditorMenu = new ModernButton(m_titleBarWidget);
  btnEditorMenu->setIcon(createModernIcon("menu", QColor("#B8B4E0")));
  btnEditorMenu->setFixedSize(36, 36);
  btnEditorMenu->setToolTip("Navigation");
  btnEditorMenu->setStyleSheet(
      "QToolButton {"
      "  background: transparent; border: none; border-radius: 10px;"
      "}"
      "QToolButton:hover {"
      "  background: rgba(255,255,255,0.07);"
      "}");
  connect(btnEditorMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  mainLayout->addWidget(btnEditorMenu);
  mainLayout->addSpacing(6);

  // ── Blop Brand ────────────────────────────────────────────────────────────
  m_lblBrand = new QLabel("Blop", m_titleBarWidget);
  m_lblBrand->setObjectName(QStringLiteral("TitleBarBrand"));
  m_lblBrand->setStyleSheet(
      "color: #F0EEFF; font-size: 17px; font-weight: 800;"
      "letter-spacing: 0.4px; background: transparent; border: none;");
  mainLayout->addWidget(m_lblBrand);

  // ── Separator ─────────────────────────────────────────────────────────────
  m_titleBarSep = new QFrame(m_titleBarWidget);
  m_titleBarSep->setObjectName(QStringLiteral("TitleBarSep"));
  m_titleBarSep->setFrameShape(QFrame::VLine);
  m_titleBarSep->setFixedSize(1, 16);
  m_titleBarSep->setStyleSheet("background: rgba(255,255,255,0.10); border: none;");
  mainLayout->addSpacing(12);
  mainLayout->addWidget(m_titleBarSep);
  mainLayout->addSpacing(12);

  // ── Top Navigation Container ───────────────────────────────────────────────
  m_topNavControls = new QWidget(m_titleBarWidget);
  m_topNavControls->setStyleSheet("background: transparent; border: none;");
  QHBoxLayout *navLayout = new QHBoxLayout(m_topNavControls);
  navLayout->setContentsMargins(0, 0, 0, 0);
  navLayout->setSpacing(8);

  // ── Mode Selector (Breadcrumb) ─────────────────────────────────────────────
  m_modeSelector = new QComboBox(m_topNavControls);
  m_modeSelector->setFixedSize(0, 0);
  m_modeSelector->setMaximumSize(0, 0);
  connect(m_modeSelector, &QComboBox::currentIndexChanged,
          this, &MainWindow::onModeChanged);
  rebuildModeSelectorItems();

  QPushButton *btnMode = new QPushButton(
      m_modeSelector->itemText(m_modeSelector->currentIndex()) + QStringLiteral("  \u25be"),
      m_topNavControls);
  btnMode->setFixedHeight(34);
  btnMode->setCursor(Qt::PointingHandCursor);
  btnMode->setStyleSheet(
      "QPushButton {"
      "  background: rgba(255,255,255,0.05);"
      "  border: 1px solid rgba(120,130,160,0.16);"
      "  border-radius: 11px;"
      "  color: rgba(220,216,255,0.92);"
      "  font-size: 12px; font-weight: 650;"
      "  padding: 0 14px;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.16);"
      "  border-color: rgba(124,92,252,0.40);"
      "  color: #FFFFFF;"
      "}");
  connect(m_modeSelector, &QComboBox::currentIndexChanged,
          [btnMode, this](int idx) {
            btnMode->setText(m_modeSelector->itemText(idx) + QStringLiteral("  \u25be"));
          });
  m_btnMode = btnMode;
  // QComboBox stays hidden; Qt may refuse showPopup() when visible==false — use QMenu instead.
  m_modeSelector->hide();
  connect(btnMode, &QPushButton::clicked, this, &MainWindow::openModeMenuAtButton);
  navLayout->addWidget(btnMode);

  m_btnAddWebBookmark = new QPushButton(QStringLiteral("+"), m_topNavControls);
  m_btnAddWebBookmark->setFixedSize(32, 32);
  m_btnAddWebBookmark->setCursor(Qt::PointingHandCursor);
  m_btnAddWebBookmark->setToolTip(tr("Webseite hinzufügen"));
  m_btnAddWebBookmark->setStyleSheet(
      "QPushButton {"
      "  background: rgba(255,255,255,0.06);"
      "  border: none;"
      "  border-radius: 8px;"
      "  color: rgba(200,196,255,0.95);"
      "  font-size: 18px; font-weight: 600;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.22);"
      "}");
  connect(m_btnAddWebBookmark, &QPushButton::clicked, this,
          &MainWindow::showAddWebBookmarkDialog);
  navLayout->addWidget(m_btnAddWebBookmark);
  navLayout->addSpacing(8);

  // ── TABS ──────────────────────────────────────────────────────────────────
  // Floating squircle document tabs, Drawboard-inspired.
  m_documentTabBar = new DocumentTabBar(m_topNavControls);
  m_documentTabBar->setAccentColor(m_currentAccentColor);
  connect(m_documentTabBar, &DocumentTabBar::currentChanged, this,
          [this](int index) {
            if (m_editorTabs && index >= 0 && index != m_editorTabs->currentIndex())
              m_editorTabs->setCurrentIndex(index);
          });
  connect(m_documentTabBar, &DocumentTabBar::tabCloseRequested, this,
          [this](int index) {
            if (m_editorTabs) {
              if (auto *ed = qobject_cast<NoteEditor *>(
                      m_editorTabs->widget(index))) {
                if (ed->view())
                  ed->view()->persistViewState(
                      ed->view()->property("viewStateKey").toString());
              }
              m_editorTabs->removeTab(index);
              if (m_editorTabs->count() == 0)
                onBackToOverview();
            }
          });
  connect(m_documentTabBar, &DocumentTabBar::homeClicked, this,
          &MainWindow::onBackToOverview);

  // "+ Tab" Button
  m_btnNewTab = new QPushButton(m_topNavControls);
  m_btnNewTab->setObjectName(QStringLiteral("TitleBarNewTab"));
  m_btnNewTab->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  m_btnNewTab->setCursor(Qt::PointingHandCursor);
  m_btnNewTab->setToolTip("Neue Notiz öffnen");
  m_btnNewTab->setIcon(
      createModernIcon(QStringLiteral("add"), QColor(200, 190, 255, 220)));
  m_btnNewTab->setIconSize(QSize(18, 18));
  m_btnNewTab->setStyleSheet(
      "QPushButton {"
      "  background: transparent;"
      "  border: none;"
      "  border-radius: 8px;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.18);"
      "}");
  connect(m_btnNewTab, &QPushButton::clicked, this,
          &MainWindow::onShowNewTabPopup);

  navLayout->addWidget(m_documentTabBar);
  navLayout->addWidget(m_btnNewTab);
  navLayout->addSpacing(10);
  // Rest der Leiste nach rechts: Suche und Aktions-Icons
  navLayout->addStretch(1);

  // ── Suchleiste ─────────────────────────────────────────────────────────────
  m_titleSearchBar = new QLineEdit(m_topNavControls);
  m_titleSearchBar->setPlaceholderText(
      QStringLiteral("Notizen durchsuchen…"));
  m_titleSearchBar->setFixedHeight(34);
  m_titleSearchBar->setMinimumWidth(UiScale::dp(150));
  m_titleSearchBar->setMaximumWidth(UiScale::dp(280));
  m_titleSearchBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_titleSearchBar->setStyleSheet(
      "QLineEdit {"
      "  background: rgba(255,255,255,0.05);"
      "  border: 1px solid rgba(120,130,160,0.16);"
      "  border-radius: 11px;"
      "  color: #D8D5FF; font-size: 12px;"
      "  padding: 0 14px;"
      "}"
      "QLineEdit:focus {"
      "  background: rgba(124,92,252,0.10);"
      "  border: 1px solid rgba(124,92,252,0.48);"
      "}"
      "QLineEdit::placeholder { color: rgba(255,255,255,0.32); }");
  navLayout->addWidget(m_titleSearchBar);
  navLayout->addSpacing(8);

  // Tags & Seiten-Optionen nur noch über Notiz-Menü (⋯) → „Optionen & Tags…“
  // Höhe = ROW_HEIGHT_ITEM (wie Sidebar-Nav-Zeilen „Alle / Blop Notizen / …“).
  const int kTitleBarNavH = ROW_HEIGHT_ITEM;

  m_btnEditorNoteOverflow = new ModernButton(m_topNavControls);
  m_btnEditorNoteOverflow->setIcon(
      createModernIcon(QStringLiteral("more_pill"), QColor(QStringLiteral("#C8CDDC"))));
  m_btnEditorNoteOverflow->setText(QString());
  m_btnEditorNoteOverflow->setToolButtonStyle(Qt::ToolButtonIconOnly);
  m_btnEditorNoteOverflow->setFixedSize(kTitleBarNavH, kTitleBarNavH);
  m_btnEditorNoteOverflow->setIconSize(
      QSize(kTitleBarNavH - 4, kTitleBarNavH - 4));
  m_btnEditorNoteOverflow->setToolTip(QStringLiteral("Notiz-Menü"));
  m_btnEditorNoteOverflow->setCursor(Qt::PointingHandCursor);
  m_btnEditorNoteOverflow->setStyleSheet(
      "QToolButton {"
      "  background: transparent; border: none; border-radius: 8px;"
      "}"
      "QToolButton:hover {"
      "  background: rgba(124,92,252,0.22);"
      "}");
  connect(m_btnEditorNoteOverflow, &QAbstractButton::clicked, this,
          &MainWindow::onEditorNoteOverflowMenu);
  m_btnEditorNoteOverflow->hide();
  navLayout->addWidget(m_btnEditorNoteOverflow);
  navLayout->addSpacing(4);

#ifndef Q_OS_ANDROID
  m_btnTitleBarPageManager = new ModernButton(m_topNavControls);
  m_btnTitleBarPageManager->setToolTip(QStringLiteral("Seitenmanager"));
  m_btnTitleBarPageManager->setToolButtonStyle(Qt::ToolButtonIconOnly);
  {
    QPixmap pm(QStringLiteral(":/assets/android_btn_pages.png"));
    if (!pm.isNull())
      m_btnTitleBarPageManager->setIcon(QIcon(pm));
    else
      m_btnTitleBarPageManager->setIcon(createModernIcon(
          QStringLiteral("pages_pill"), QColor(QStringLiteral("#C8CDDC"))));
  }
  {
    const int pmW = qRound(kTitleBarNavH * 1.72);
    m_btnTitleBarPageManager->setFixedSize(pmW, kTitleBarNavH);
    m_btnTitleBarPageManager->setIconSize(QSize(pmW - 8, kTitleBarNavH - 4));
  }
  m_btnTitleBarPageManager->setHoverScaleEnabled(false);
  m_btnTitleBarPageManager->setStyleSheet(
      QStringLiteral(
          "QToolButton { background: transparent; border: none; border-radius: 8px; "
          "padding: 0; }"
          "QToolButton:hover { background: rgba(124,92,252,0.22); }"
          "QToolButton:pressed { background: rgba(124,92,252,0.32); }"));
  connect(m_btnTitleBarPageManager, &QAbstractButton::clicked, this,
          &MainWindow::onTogglePageManager);
  m_btnTitleBarPageManager->hide();
  navLayout->addWidget(m_btnTitleBarPageManager);
  navLayout->addSpacing(4);
#endif

  navLayout->addWidget(m_modeSelector); // hidden 0×0 — am Ende, kein Layout-Zwang

  // === INJECT TOP NAV CONTAINER ===
  mainLayout->addWidget(m_topNavControls, 1); // Expand to fill middle space
  mainLayout->addSpacing(16); // Gap before window controls

  // ── Window Controls ────────────────────────────────────────────────────────
  struct WinBtn {
    QPushButton **ptr;
    QString sym;
    bool isClose;
  };
  QList<WinBtn> winBtns = {
      {&m_btnWinMin, QStringLiteral("\u2212"), false},   // −
      {&m_btnWinMax, QStringLiteral("\u25A1"), false},   // □
      {&m_btnWinClose, QStringLiteral("\u00D7"), true},   // ×
  };
  for (auto &wb : winBtns) {
    *wb.ptr = new QPushButton(wb.sym, m_titleBarWidget);
    (*wb.ptr)->setFixedSize(46, 50);
    (*wb.ptr)->setCursor(Qt::PointingHandCursor);
    (*wb.ptr)->setStyleSheet(wb.isClose
                                 ? "QPushButton {"
                                   "  background: transparent; border: none;"
                                   "  color: rgba(255,255,255,0.50);"
                                   "  font-size: 14px; font-weight: 400;"
                                   "}"
                                   "QPushButton:hover { background: #E81123; color: white; }"
                                 : "QPushButton {"
                                   "  background: transparent; border: none;"
                                   "  color: rgba(255,255,255,0.50);"
                                   "  font-size: 14px; font-weight: 400;"
                                   "}"
                                   "QPushButton:hover {"
                                   "  background: rgba(255,255,255,0.10); color: rgba(255,255,255,0.90);"
                                   "}");
    mainLayout->addWidget(*wb.ptr);
  }
  connect(m_btnWinMin, &QPushButton::clicked, this, &MainWindow::onWinMinimize);
  connect(m_btnWinMax, &QPushButton::clicked, this, &MainWindow::onWinMaximize);
  connect(m_btnWinClose, &QPushButton::clicked, this, &MainWindow::onWinClose);

  // Drag / double-click maximize: empty title areas (incl. stretch spacer).
  m_titleBarWidget->installEventFilter(this);
  if (m_topNavControls)
    m_topNavControls->installEventFilter(this);
}


// Window Dragging Implementation
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
#ifndef Q_OS_ANDROID
  // Cross-platform title-bar drag / double-click maximize (also covers
  // Linux and acts as a fallback when Win32 HTTEST is unavailable).
  auto startTitleDragIfEmpty = [this](QWidget *origin, const QPoint &localPos) -> bool {
    if (!m_titleBarWidget || !windowHandle())
      return false;
    QWidget *hit = origin->childAt(localPos);
    // Walk interactive check — same rules as WM_NCHITTEST.
    auto interactive = [](QWidget *w, QWidget *titleBar) {
      for (QWidget *cur = w; cur && cur != titleBar;
           cur = cur->parentWidget()) {
        if (qobject_cast<QAbstractButton *>(cur) ||
            qobject_cast<QLineEdit *>(cur) ||
            qobject_cast<QComboBox *>(cur) ||
            qobject_cast<QAbstractSlider *>(cur) ||
            qobject_cast<QTabBar *>(cur))
          return true;
        if (cur->metaObject() &&
            QByteArray(cur->metaObject()->className())
                    .indexOf("DocumentTabBar") >= 0)
          return true;
        if (cur->property("blopTitleInteractive").toBool())
          return true;
      }
      return false;
    };
    if (interactive(hit, m_titleBarWidget))
      return false;
    return windowHandle()->startSystemMove();
  };

  if (m_titleBarWidget &&
      (obj == m_titleBarWidget || obj == m_topNavControls)) {
    if (event->type() == QEvent::MouseButtonDblClick) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton) {
        onWinMaximize();
        return true;
      }
    }
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton) {
        if (auto *w = qobject_cast<QWidget *>(obj)) {
          if (startTitleDragIfEmpty(w, me->pos()))
            return true;
        }
      }
    }
  }
#endif
#ifdef Q_OS_ANDROID
  if (obj == m_androidSidebarScrim && event->type() == QEvent::MouseButtonPress) {
    animateSidebar(false);
    return true;
  }
#endif
  if (obj == m_editorCenterWidget && event->type() == QEvent::Resize && m_floatingTools) {
    if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools)) {
      const int h = phone->preferredHeightPx();
      const int margin = UiScale::dp(8);
      const int avail = qMax(UiScale::dp(180), m_editorCenterWidget->width());
      const int w = qMin(avail - 2 * margin, UiScale::dp(360));
      const int y = qMax(0, m_editorCenterWidget->height() - h -
                                UiScale::safeBottomPx(this) - UiScale::dp(8));
      phone->setGeometry((avail - w) / 2, y, w, h);
      phone->raise();
      syncPenPresetBarGeometry();
      // Phone: single bottom chrome = AndroidPhoneToolbar only.
      // Hide the desktop page/zoom notch so it cannot stack/overlap.
      if (m_noteBottomChrome)
        m_noteBottomChrome->hide();
    } else if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
      const int headerH = noteHeaderHeight();
      tb->setTopBound(headerH);
#ifdef Q_OS_ANDROID
      if (tb->isDockedMode()) {
        int idealW = tb->calculateMinLength();
        idealW = qMax(220, int(idealW * 0.90));
        const int availableW = qMax(240, m_editorCenterWidget->width());
        const int dockW = qMin(idealW, availableW);
        int dockX = qMax(0, (availableW - dockW) / 2);
        if (dockX + dockW > m_editorCenterWidget->width())
          dockX = m_editorCenterWidget->width() - dockW;
        tb->setGeometry(dockX, headerH, dockW, 48);
        positionNoteChrome();
      } else {
        positionNoteChrome();
      }
#else
      // Desktop Drawboard chrome owns geometry (left pages + top markup bar).
      positionNoteChrome();
#endif
      tb->raise();
      syncPenPresetBarGeometry();
    }
    if (m_pageSettingsOverlay && m_editorCenterWidget) {
      m_pageSettingsOverlay->setGeometry(
          0, 0, m_editorCenterWidget->width(), m_editorCenterWidget->height());
      if (m_pageSettingsCard)
        m_pageSettingsCard->setMaximumHeight(
            qMax(200, m_editorCenterWidget->height() - 64));
      if (m_pageSettingsOverlay->isVisible())
        m_pageSettingsOverlay->raise();
    }
  } else if (obj == m_pageSettingsOverlay &&
             event->type() == QEvent::MouseButtonPress) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (m_pageSettingsCard &&
        !m_pageSettingsCard->geometry().contains(me->pos())) {
      setPageSettingsOverlayVisible(false);
      return true;
    }
  } else if (obj == m_floatingTools && event->type() == QEvent::Move) {
    // Chips follow the toolbar every move so floating bar + presets stay one cluster.
    syncPenPresetBarGeometry();
    // v119 perf: QEvent::Move fires on EVERY pixel of a drag and was
    // re-evaluating the dock condition + (when triggered) running an
    // animation 60+ times per second. Gate on a small y-threshold so
    // we only re-evaluate when the toolbar's y actually crossed a
    // meaningful boundary.
    const int newY = m_floatingTools->y();
    if (qAbs(newY - m_lastDockCheckY) >= 8) {
      m_lastDockCheckY = newY;
      // Re-dock Markup Toolbar when dragged to the top.
      if (newY <= 10) {
        if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
          tb->setDockMode(true);
          positionNoteChrome();
          syncPenPresetBarGeometry();
        }
      }
    }
  } else if (obj == m_lblNoteZoom &&
             event->type() == QEvent::MouseButtonDblClick) {
    if (auto *view = currentNoteView()) {
      view->fitPage();
      updateNoteBottomChrome();
    } else if (CanvasView *cv = getCurrentCanvas()) {
      cv->fitPage();
      updateNoteBottomChrome();
    }
    return true;
  } else if (m_noteBottomChrome &&
             (obj == m_noteBottomChrome || obj == m_btnNoteChromeGrip)) {
#ifndef Q_OS_ANDROID
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonRelease) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (event->type() == QEvent::MouseButtonPress &&
          me->button() == Qt::RightButton) {
        showNoteChromeEdgeMenu(me->globalPosition().toPoint());
        return true;
      }
      if (event->type() == QEvent::MouseButtonPress &&
          me->button() == Qt::LeftButton) {
        bool startDrag = (obj == m_btnNoteChromeGrip);
        if (!startDrag && obj == m_noteBottomChrome) {
          QWidget *hit = m_noteBottomChrome->childAt(me->pos());
          startDrag = !hit;
          if (hit && m_btnNoteChromeGrip &&
              (hit == m_btnNoteChromeGrip ||
               m_btnNoteChromeGrip->isAncestorOf(hit)))
            startDrag = true;
          if (hit && (qobject_cast<QAbstractButton *>(hit) ||
                      qobject_cast<QLabel *>(hit)))
            startDrag = false;
        }
        if (startDrag) {
          m_noteChromeDragging = true;
          m_noteChromeDragHotspot = (obj == m_btnNoteChromeGrip)
              ? me->pos() + m_btnNoteChromeGrip->pos()
              : me->pos();
          m_noteBottomChrome->grabMouse();
          m_noteBottomChrome->raise();
          return true;
        }
      }
      if (m_noteChromeDragging && event->type() == QEvent::MouseMove) {
        const QPoint parentPos =
            m_editorCenterWidget
                ? m_editorCenterWidget->mapFromGlobal(
                      me->globalPosition().toPoint())
                : me->globalPosition().toPoint();
        QPoint topLeft = parentPos - m_noteChromeDragHotspot;
        if (m_editorCenterWidget) {
          const QRect bounds = m_editorCenterWidget->rect();
          topLeft.setX(qBound(bounds.left(), topLeft.x(),
                              bounds.right() - m_noteBottomChrome->width() + 1));
          topLeft.setY(qBound(bounds.top(), topLeft.y(),
                              bounds.bottom() - m_noteBottomChrome->height() + 1));
        }
        m_noteBottomChrome->move(topLeft);
        return true;
      }
      if (m_noteChromeDragging && event->type() == QEvent::MouseButtonRelease &&
          me->button() == Qt::LeftButton) {
        m_noteChromeDragging = false;
        m_noteBottomChrome->releaseMouse();
        const QPoint center = m_noteBottomChrome->geometry().center();
        setNoteChromeEdge(nearestNoteChromeEdge(center));
        return true;
      }
    }
#endif
  }
  return QMainWindow::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// addNoteTab / closeNoteTab  (floating squircle document tabs)
// ---------------------------------------------------------------------------
void MainWindow::addNoteTab(const QString &title) {
  if (!m_documentTabBar)
    return;
  m_documentTabBar->setHomeActive(false);
  int idx = m_documentTabBar->addTab(title);
  m_documentTabBar->setCurrentIndex(idx);
}


void MainWindow::onShowNewTabPopup() {
  if (!m_fileModel) return;

  // Schwebendes Centered-Overlay erzeugen (QDialog)
  QDialog *overlay = new QDialog(this, Qt::FramelessWindowHint | Qt::Popup);
  overlay->setAttribute(Qt::WA_DeleteOnClose);
  overlay->setAttribute(Qt::WA_TranslucentBackground); // WICHTIG: Erlaubt weiche Rundungen ohne schwarze Ecken!

  overlay->setMinimumSize(UiScale::dp(320), UiScale::dp(380));
  overlay->resize(UiScale::dp(450), UiScale::dp(550));
  overlay->setStyleSheet(
      "QDialog {"
      "  background-color: #1A1A24;" // Blop Theme Dark
      "  border: 1px solid rgba(255, 255, 255, 0.1);"
      "  border-radius: 20px;" // Deutliche Rundung für Centered Popups sieht premium aus
      "}"
      "QListWidget {"
      "  background: transparent;"
      "  border: none;"
      "  outline: none;"
      "  color: #E0E0E0;"
      "}"
      "QListWidget::item {"
      "  padding: 12px 20px;"
      "  border-radius: 8px;"
      "  font-size: 14px;"
      "  margin: 4px 16px;"
      "}"
      "QListWidget::item:hover {"
      "  background-color: rgba(255, 255, 255, 0.06);"
      "}"
      "QListWidget::item:selected {"
      "  background-color: #5E5CE6;"
      "  color: white;"
      "}");

  QVBoxLayout *overlayLayout = new QVBoxLayout(overlay);
  overlayLayout->setContentsMargins(0, 20, 0, 20);
  overlayLayout->setSpacing(10);

  // Header Title
  QLabel *lblTitle = new QLabel("Eine Notiz auswählen", overlay);
  lblTitle->setStyleSheet("color: white; font-size: 18px; font-weight: bold; margin-bottom: 10px;");
  lblTitle->setAlignment(Qt::AlignCenter);
  overlayLayout->addWidget(lblTitle);

  // List Widget
  QListWidget *listWidget = new QListWidget(overlay);
  listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // Create physical file action
  QListWidgetItem *itemNew = new QListWidgetItem("+ Leere Notiz erstellen", listWidget);
  itemNew->setData(Qt::UserRole, "CREATE_NEW");
  itemNew->setTextAlignment(Qt::AlignCenter);

  // Populate files
  QModelIndex rootIndex = m_fileModel->index(m_fileModel->rootPath());
  int rowCount = m_fileModel->rowCount(rootIndex);
  for (int i = 0; i < rowCount; ++i) {
    QModelIndex idx = m_fileModel->index(i, 0, rootIndex);
    if (!m_fileModel->isDir(idx)) {
      QString fileName = idx.data().toString();
      QListWidgetItem *noteItem = new QListWidgetItem(fileName, listWidget);
      noteItem->setData(Qt::UserRole, m_fileModel->filePath(idx));
    }
  }

  overlayLayout->addWidget(listWidget);

  // Fenster direkt zentrieren
  QPoint parentCenter = mapToGlobal(rect().center());
  overlay->move(parentCenter.x() - overlay->width() / 2,
                parentCenter.y() - overlay->height() / 2);

  // Event handler for selection
  connect(listWidget, &QListWidget::itemClicked, this, [this, overlay](QListWidgetItem *item) {
    QString action = item->data(Qt::UserRole).toString();
    if (action == "CREATE_NEW") {
      // ECHTE DATEI ERSTELLEN
      QString newName = "Neue Notiz.blop";
      QString fullPath = m_fileModel->rootPath() + "/" + newName;
      int counter = 1;
      while (QFile::exists(fullPath)) {
        newName = QString("Neue Notiz (%1).blop").arg(counter++);
        fullPath = m_fileModel->rootPath() + "/" + newName;
      }
      
      QFile file(fullPath);
      if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out << (quint32)0xB10B0002; // Blop Magic Number (v2)
        out << true;               // isInfinite flag (WICHTIG!)
        out << (int)0;             // items count
        file.close();
      }
      
      QModelIndex newIdx = m_fileModel->index(fullPath);
      onFileDoubleClicked(newIdx);

    } else if (!action.isEmpty()) {
      QModelIndex idx = m_fileModel->index(action);
      if (idx.isValid()) {
        onFileDoubleClicked(idx);
      }
    }
    overlay->accept(); // Close the dialog
  });

  overlay->show(); // Benutzt show() statt exec() um Modalitäts-Glasscheibe zu verhindern
}

void MainWindow::closeNoteTab(int index) {
  if (!m_documentTabBar)
    return;
  m_documentTabBar->removeTab(index);
  if (m_documentTabBar->count() == 0)
    m_documentTabBar->setHomeActive(true);
}


void MainWindow::onWinMinimize() {
  setWindowState((windowState() & ~Qt::WindowMaximized) | Qt::WindowMinimized);
}
void MainWindow::onWinClose() { close(); }
void MainWindow::onWinMaximize() {
  if (isMaximized() || (windowState() & Qt::WindowMaximized))
    setWindowState(windowState() & ~Qt::WindowMaximized);
  else
    setWindowState(windowState() | Qt::WindowMaximized);
  // Cursor-style: swap □ / ❐ glyph on the single chrome bar.
  if (m_btnWinMax) {
    m_btnWinMax->setText(isMaximized() ? QStringLiteral("\u2750")  // ❐ restore
                                       : QStringLiteral("\u25A1")); // □ maximize
  }
}
void MainWindow::mousePressEvent(QMouseEvent *event) {
  QMainWindow::mousePressEvent(event);
}
void MainWindow::mouseMoveEvent(QMouseEvent *event) {
  QMainWindow::mouseMoveEvent(event);
}
void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
  QMainWindow::mouseReleaseEvent(event);
}

#ifdef Q_OS_WIN
namespace {
bool titleBarRegionIsInteractive(QWidget *hit, QWidget *titleBar) {
  for (QWidget *cur = hit; cur && cur != titleBar;
       cur = cur->parentWidget()) {
    if (qobject_cast<QAbstractButton *>(cur) ||
        qobject_cast<QLineEdit *>(cur) || qobject_cast<QComboBox *>(cur) ||
        qobject_cast<QAbstractSlider *>(cur) ||
        qobject_cast<QTabBar *>(cur))
      return true;
    if (cur->metaObject() &&
        QByteArray(cur->metaObject()->className())
                .indexOf("DocumentTabBar") >= 0)
      return true;
    if (cur->property("blopTitleInteractive").toBool())
      return true;
  }
  return false;
}
} // namespace

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  MSG *msg = static_cast<MSG *>(message);
  if (msg->message == WM_NCHITTEST) {
    long x = GET_X_LPARAM(msg->lParam);
    long y = GET_Y_LPARAM(msg->lParam);
    QPoint pos = mapFromGlobal(QPoint(x, y));

    // Edge resize only when not maximized.
    if (!isMaximized()) {
      int borderSize = 6;
      bool left = pos.x() < borderSize;
      bool right = pos.x() > width() - borderSize;
      bool top = pos.y() < borderSize;
      bool bottom = pos.y() > height() - borderSize;

      if (top && left) { *result = HTTOPLEFT; return true; }
      if (top && right) { *result = HTTOPRIGHT; return true; }
      if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
      if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
      if (left) { *result = HTLEFT; return true; }
      if (right) { *result = HTRIGHT; return true; }
      if (top) { *result = HTTOP; return true; }
      if (bottom) { *result = HTBOTTOM; return true; }
    }

    // Title bar: only real controls are HTCLIENT — empty stretch areas
    // (m_topNavControls) must stay HTCAPTION so the window can be dragged.
    if (m_titleBarWidget && m_titleBarWidget->isVisible()) {
      const QRect titleRect = m_titleBarWidget->geometry();
      if (titleRect.contains(pos)) {
        QWidget *child = m_titleBarWidget->childAt(
            m_titleBarWidget->mapFromGlobal(QPoint(x, y)));
        if (titleBarRegionIsInteractive(child, m_titleBarWidget))
          *result = HTCLIENT;
        else
          *result = HTCAPTION;
        return true;
      }
    }

    *result = HTCLIENT;
    return true;
  }
  return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::onContentModified() { m_autoSaveTimer->start(); }
void MainWindow::performAutoSave() {
  CanvasView *cv = getCurrentCanvas();
  if (cv) {
    cv->saveToFile();
    updateSidebarBadges();
  }
}
CanvasView *MainWindow::getCurrentCanvas() {
  if (!m_editorTabs) return nullptr;
  QWidget *current = m_editorTabs->currentWidget();
  if (!current) return nullptr;
  // Direct cast (when not wrapped)
  if (auto *cv = qobject_cast<CanvasView *>(current))
    return cv;
  // Search inside wrapper widget (e.g. canvas + overlay button)
  return current->findChild<CanvasView *>();
}

void MainWindow::applyTheme() {
  // v3.17.5: gate against no-op invocations. applyTheme() is the central
  // QSS factory and currently issues ~30 setStyleSheet() calls + Android
  // icon-tinting work; calling it when nothing actually changed costs
  // hundreds of ms on Android. We key on (accent, mode) so a Light/Dark
  // toggle or an accent change still re-applies, but back-to-back
  // applyTheme() invocations from the same state become free.
  const QRgb accentKey = m_currentAccentColor.rgba();
  const int modeKey = static_cast<int>(BlopTheme::instance().mode());
  if (m_themeApplied && accentKey == m_lastAppliedAccentRgb &&
      modeKey == m_lastAppliedModeKey) {
    return;
  }
  m_lastAppliedAccentRgb = accentKey;
  m_lastAppliedModeKey = modeKey;
  m_themeApplied = true;
  QString c = m_currentAccentColor.name();
  QString c_light = m_currentAccentColor.lighter(130).name();
#ifdef Q_OS_ANDROID
  const QString c_scrollPress = m_currentAccentColor.darker(118).name();
  const char *sbW = "12px";
  const char *sbH = "12px";
#endif
  if (m_fileListView)
    m_fileListView->setAccentColor(m_currentAccentColor);
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
#ifndef Q_OS_ANDROID
    const bool editorOpen =
        m_documentTabBar && m_documentTabBar->noteChromeMode();
    tb->setAccentColor(editorOpen ? NoteChrome::accent()
                                  : m_currentAccentColor);
#else
    tb->setAccentColor(m_currentAccentColor);
#endif
  }
  if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools))
    phone->setAccentColor(m_currentAccentColor);
  if (m_documentTabBar) {
#ifndef Q_OS_ANDROID
    if (m_documentTabBar->noteChromeMode())
      m_documentTabBar->setAccentColor(NoteChrome::accent());
    else
#endif
      m_documentTabBar->setAccentColor(m_currentAccentColor);
  }
    if (m_pageThumbnailSidebar)
      m_pageThumbnailSidebar->setAccentColor(
#ifndef Q_OS_ANDROID
          (m_documentTabBar && m_documentTabBar->noteChromeMode())
              ? NoteChrome::accent()
              :
#endif
              m_currentAccentColor);
    if (m_noteLeftRail)
      m_noteLeftRail->setAccentColor(
#ifndef Q_OS_ANDROID
          (m_documentTabBar && m_documentTabBar->noteChromeMode())
              ? NoteChrome::accent()
              :
#endif
              m_currentAccentColor);
    if (m_radialFab)
      m_radialFab->setAccentColor(m_currentAccentColor);
    if (m_libraryTagsPanel)
      m_libraryTagsPanel->setAccentColor(m_currentAccentColor);
    if (m_libraryOrgBar)
      m_libraryOrgBar->setAccentColor(m_currentAccentColor);

  // Blop Notes Redesign (Etappe 1): #0D0B14 Main, #14121F Sidebar
  // Custom scrollbars: Android only (Windows desktop uses native Qt scrollbar to avoid layout glitches).
  QString style =
      QString(
          "QMainWindow { background-color: #0D0B14; } "
          "* { outline: none; } "
          "QListView { background: transparent; border: none; outline: 0; } "
          "QTabWidget::pane { border: 0; background: #0D0B14; } "
          "QTabBar::tab { background: #0D0B14; color: #888; padding: 10px "
          "25px; border-top-left-radius: 12px; border-top-right-radius: 12px; "
          "margin-right: 2px;} "
          "QTabBar::tab:selected { background: %1; color: white; } "
          "QMenu { background-color: #0D0B14; border: 1px solid #201E2E; "
          "border-radius: 10px; padding: 10px; color: #E0E0E0; } "
          "QMenu::item { padding: 8px 20px; border-radius: 5px; } "
          "QMenu::item:selected { background-color: %2; color: white; } ")
          .arg(c, c_light);
#ifdef Q_OS_ANDROID
  style += QString(
      "QScrollBar:vertical {"
      "  border: none;"
      "  background: rgba(255,255,255,0.05);"
      "  width: %3;"
      "  margin: 0px;"
      "  border-radius: 5px;"
      "}"
      "QScrollBar::handle:vertical {"
      "  background: rgba(255,255,255,0.28);"
      "  min-height: 48px;"
      "  border-radius: 5px;"
      "  margin: 2px 1px;"
      "}"
      "QScrollBar::handle:vertical:hover { background: %1; }"
      "QScrollBar::handle:vertical:pressed { background: %2; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
      "  border: none; background: none; height: 0px; width: 0px;"
      "}"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
      "  background: transparent;"
      "}"
      "QScrollBar:horizontal {"
      "  border: none;"
      "  background: rgba(255,255,255,0.05);"
      "  height: %4;"
      "  margin: 0px;"
      "  border-radius: 5px;"
      "}"
      "QScrollBar::handle:horizontal {"
      "  background: rgba(255,255,255,0.28);"
      "  min-width: 48px;"
      "  border-radius: 5px;"
      "  margin: 1px 2px;"
      "}"
      "QScrollBar::handle:horizontal:hover { background: %1; }"
      "QScrollBar::handle:horizontal:pressed { background: %2; }"
      "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
      "  border: none; background: none; width: 0px; height: 0px;"
      "}"
      "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
      "  background: transparent;"
      "}")
      .arg(c_light, c_scrollPress, QLatin1String(sbW), QLatin1String(sbH));
#endif
  // v3.17.1: route through BlopTheme::themed so Light mode swaps the
  // dark surface hex (#0D0B14, #201E2E, #888, #E0E0E0) for light tokens
  // while leaving %1/%2 accent placeholders intact.
  this->setStyleSheet(BlopTheme::themed(style));

  if (m_sidebarContainer)
    m_sidebarContainer->setStyleSheet(BlopTheme::themed(
        "background-color: #0B0912; border-right: 1px solid rgba(120,130,160,0.12);"));
  if (m_sidebarStrip)
    m_sidebarStrip->setStyleSheet(BlopTheme::themed(
        "background-color: #0B0912; border-right: 1px solid rgba(120,130,160,0.12);"));
  if (m_navSidebar)
    m_navSidebar->setStyleSheet(
#ifdef Q_OS_ANDROID
        "QListWidget { background-color: transparent; border: none; outline: "
        "0; margin-left: 6px; margin-right: 6px; padding: 3px 1px; } "
        "QListWidget::item { border: none; }");
#else
        "QListWidget { background-color: transparent; border: none; outline: "
        "0; margin-left: 4px; margin-right: 4px; padding: 2px 1px; } QListWidget::item { border: "
        "none; }");
#endif

  if (m_pageSettingsCard)
    m_pageSettingsCard->setStyleSheet(
        QStringLiteral(
            "QWidget#PageSettingsCard { background: %1; border: 1px solid %2; "
            "border-radius: 18px; }")
            .arg(UIStyles::PageBackground.name(),
                 QColor(124, 92, 252, 90).name(QColor::HexArgb)));

  if (m_titleBarWidget) {
    // Keep overview purple chrome; while editing, NoteChrome owns the shell.
    bool editorChrome = false;
#ifndef Q_OS_ANDROID
    if (m_documentTabBar && m_documentTabBar->noteChromeMode())
      editorChrome = true;
#endif
    if (editorChrome)
      refreshNoteTitleChrome(true);
    else
      m_titleBarWidget->setStyleSheet(BlopTheme::themed(
          "background-color: #0B0912; border-bottom: 1px solid rgba(120,130,160,0.12);"));
  }

  // Overview: redesigned library chrome (v3.22.1).
  if (m_overviewContainer) {
    m_overviewContainer->setStyleSheet(BlopTheme::themed(
        QString(
            "QLabel#overviewLibraryTitle {"
            "  color: %3; font-size: 28px; font-weight: 800;"
            "  letter-spacing: -0.6px; background: transparent;"
            "}"
            "QLabel#overviewLibrarySubtitle {"
            "  color: rgba(180,188,215,0.55); font-size: 12px; font-weight: 500;"
            "  background: transparent;"
            "}"
            "QLineEdit#overviewSearchBar {"
            "  background-color: %4; color: %3;"
            "  border: 1px solid %5; border-radius: 16px;"
            "  min-height: 42px; max-height: 42px;"
            "  padding: 0 18px; font-size: 14px;"
            "}"
            "QLineEdit#overviewSearchBar:focus { border: 1px solid %1; }"
            "QPushButton#overviewBtnNewNote {"
            "  background-color: %1; color: #FFFFFF; border-radius: 14px;"
            "  padding: 0 18px; font-weight: 700; font-size: 13px;"
            "  border: none; min-height: 36px; max-height: 36px;"
            "}"
            "QPushButton#overviewBtnNewNote:hover {"
            "  background-color: %2; }"
            "QPushButton#overviewBtnNewFolder,"
            "QPushButton#overviewBtnTags {"
            "  background-color: rgba(255,255,255,0.04); color: %3; border-radius: 14px;"
            "  padding: 0 16px; font-weight: 600; font-size: 13px;"
            "  border: 1px solid %5; min-height: 36px; max-height: 36px;"
            "}"
            "QPushButton#overviewBtnNewFolder:hover,"
            "QPushButton#overviewBtnTags:hover {"
            "  background-color: rgba(255,255,255,0.07); border-color: %1; }")
            .arg(c, c_light,
                 BlopTheme::textPrimary().name(QColor::HexRgb),
                 BlopTheme::surfaceMuted().name(QColor::HexRgb),
                 BlopTheme::borderSubtle().name(QColor::HexArgb))));
  }

  if (m_btnSidebarSettings) {
    m_btnSidebarSettings->setStyleSheet(BlopTheme::themed(
        QString(
            "QPushButton { background: transparent; color: #888; border: none; "
            "font-size: 10px; padding: 0; text-align: left; } "
            "QPushButton:hover { color: %1; }")
            .arg(c)));
  }
  if (m_lblSidebarAvatar) {
    m_lblSidebarAvatar->setStyleSheet(
        QString("background: rgba(%1,%2,%3,0.30); "
                "border-radius: 10px; color: white; font-weight: 700; font-size: 12px;")
            .arg(m_currentAccentColor.red())
            .arg(m_currentAccentColor.green())
            .arg(m_currentAccentColor.blue()));
  }
#ifdef Q_OS_ANDROID
  // v3.17.5: cache per (resourcePath, tint.rgba()). loadTightIcon is O(W*H)
  // twice (crop + tint) and was previously run from scratch on every
  // applyTheme(), even when the user only toggled an unrelated checkbox.
  // The cache is static-local so it persists across applyTheme() calls
  // for the entire process lifetime; size is bounded by the number of
  // distinct icon-PNGs in the Android UI (~6) times distinct accents (4).
  static QHash<QPair<QString, QRgb>, QIcon> s_tightIconCache;
  auto loadTightIcon = [](const QString &resourcePath, const QIcon &fallback,
                          const QColor &tint) -> QIcon {
    const QRgb tintKey = tint.isValid() ? tint.rgba() : 0;
    const auto key = qMakePair(resourcePath, tintKey);
    auto it = s_tightIconCache.constFind(key);
    if (it != s_tightIconCache.constEnd())
      return *it;
    QPixmap pm(resourcePath);
    if (pm.isNull())
      return fallback;
    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    int minX = img.width(), minY = img.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < img.height(); ++y) {
      const QRgb *row = reinterpret_cast<const QRgb *>(img.scanLine(y));
      for (int x = 0; x < img.width(); ++x) {
        if (qAlpha(row[x]) > 8) {
          minX = qMin(minX, x);
          minY = qMin(minY, y);
          maxX = qMax(maxX, x);
          maxY = qMax(maxY, y);
        }
      }
    }
    if (maxX < minX || maxY < minY) {
      QIcon ic(pm);
      s_tightIconCache.insert(key, ic);
      return ic;
    }
    QRect crop(minX, minY, maxX - minX + 1, maxY - minY + 1);
    const int pad = 2;
    crop.adjust(-pad, -pad, pad, pad);
    crop = crop.intersected(QRect(0, 0, img.width(), img.height()));
    QImage out = img.copy(crop).convertToFormat(QImage::Format_ARGB32);
    if (tint.isValid()) {
      // Accent filter for PNG buttons: bias brighter pixels toward accent.
      for (int y = 0; y < out.height(); ++y) {
        QRgb *row = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
          QColor px = QColor::fromRgba(row[x]);
          if (px.alpha() == 0)
            continue;
          const int light = qGray(px.red(), px.green(), px.blue());
          if (light >= 70) {
            const qreal mix = (light >= 130) ? 0.60 : 0.28;
            const int r = int(px.red() * (1.0 - mix) + tint.red() * mix);
            const int g = int(px.green() * (1.0 - mix) + tint.green() * mix);
            const int b = int(px.blue() * (1.0 - mix) + tint.blue() * mix);
            row[x] = qRgba(r, g, b, px.alpha());
          }
        }
      }
    }
    QIcon ic(QPixmap::fromImage(out));
    s_tightIconCache.insert(key, ic);
    return ic;
  };
  if (m_btnAndroidToolbarMenu) {
    m_btnAndroidToolbarMenu->setStyleSheet(
        BlopStyle::toolButtonAccentQss(m_currentAccentColor));
    QIcon menuIcon = loadTightIcon(":/assets/android_btn_menu.png",
                                   createModernIcon("menu_pill", QColor("#C8CDDC")),
                                   m_currentAccentColor);
    m_btnAndroidToolbarMenu->setIcon(menuIcon);
  }
  if (m_btnAndroidToolbarExport) {
    m_btnAndroidToolbarExport->setHoverScaleEnabled(false);
    m_btnAndroidToolbarExport->setStyleSheet(
        BlopStyle::toolButtonAccentQss(m_currentAccentColor));
    m_btnAndroidToolbarExport->setFixedSize(UiScale::dp(56), UiScale::dp(32));
    m_btnAndroidToolbarExport->setIconSize(QSize(UiScale::dp(56), UiScale::dp(32)));
    m_btnAndroidToolbarExport->setIcon(
        createModernIcon("more_pill", QColor("#C8CDDC")));
  }
  if (m_btnAndroidToolbarPageManager) {
    m_btnAndroidToolbarPageManager->setStyleSheet(
        BlopStyle::toolButtonAccentQss(m_currentAccentColor));
    QIcon pagesIcon = loadTightIcon(":/assets/android_btn_pages.png",
                                    createModernIcon("pages_pill", QColor("#C8CDDC")),
                                    m_currentAccentColor);
    m_btnAndroidToolbarPageManager->setIcon(pagesIcon);
  }
  if (m_mainContentStack)
    applyAndroidTabStyles(m_mainContentStack->currentIndex());
#endif
}

void MainWindow::updateTheme(QColor accentColor) {
  m_currentAccentColor = accentColor;
  applyTheme();
}
void MainWindow::onItemSizeChanged(int value) {
  m_currentProfile.iconSize = value;
  updateGrid();
}
void MainWindow::onGridSpacingChanged(int value) {
  m_currentProfile.gridSpacing = value;
  updateGrid();
}

void MainWindow::updateGrid() {
  if (!m_fileListView)
    return;

  // Real Android phones and BLOP_SIMULATE_ANDROID_PHONE=1 use the denser
  // column-fit tile sizing so the main library stays usable on narrow widths.
  const bool phoneGrid = UiScale::isAndroidPhoneUi(this);
#ifdef Q_OS_ANDROID
  const bool usePhoneGrid = true;
  Q_UNUSED(phoneGrid);
#else
  const bool usePhoneGrid = phoneGrid;
#endif
  if (usePhoneGrid) {
    int screenWidth = 0;
    if (m_fileListView->viewport())
      screenWidth = m_fileListView->viewport()->width();
    if (screenWidth <= 0)
      screenWidth = m_fileListView->width();
    if (screenWidth <= 0)
      screenWidth = QApplication::primaryScreen()->availableGeometry().width();

    int s = m_currentProfile.iconSize;
    if (s <= 20)
      s = 100;
    s = qMax(s, UiScale::dp(140));

    int spacing = m_currentProfile.gridSpacing;
    if (spacing <= 0)
      spacing = 20;
    spacing = qMax(spacing, UiScale::dp(16));

    m_fileListView->setSpacing(spacing);

    int avail = screenWidth;
    int columns = (avail - spacing) / (s + spacing);
    if (columns < 1)
      columns = 1;
    if (columns > 6)
      columns = 6;

    int totalSpacing = (columns + 1) * spacing;
    int itemWidth = (avail - totalSpacing) / columns;
    while (itemWidth < 64 && columns > 1) {
      columns--;
      totalSpacing = (columns + 1) * spacing;
      itemWidth = (avail - totalSpacing) / columns;
    }
    itemWidth = qMax(itemWidth, UiScale::dp(120));
    const int itemHeight = itemWidth;

    m_fileListView->setItemSize(QSize(itemWidth, itemHeight));
    m_fileListView->setIconSize(QSize(itemWidth, itemWidth));
    m_fileListView->setGridSize(QSize(itemWidth + spacing, itemHeight + spacing));
    m_fileListView->setUniformItemSizes(true);
  } else {
    int s = m_currentProfile.iconSize;
    if (s <= 20)
      s = 132;
    s = qMax(s, 120);
    int spacing = m_currentProfile.gridSpacing;
    if (spacing <= 0)
      spacing = 18;
    spacing = qMax(spacing, 14);
    QSize itemS(s, s);
    m_fileListView->setSpacing(spacing);
    m_fileListView->setItemSize(itemS);
    m_fileListView->setIconSize(itemS);
    m_fileListView->setUniformItemSizes(true);
    if (m_currentProfile.snapToGrid) {
      int gridW = itemS.width() + spacing;
      int gridH = itemS.height() + spacing;
      m_fileListView->setGridSize(QSize(gridW, gridH));
    } else {
      m_fileListView->setGridSize(QSize(itemS.width() + spacing,
                                        itemS.height() + spacing));
    }
  }
}

void MainWindow::applyProfile(const UiProfile &profile) {
  m_currentProfile = profile;
  updateGrid();
  if (m_fileListView)
    m_fileListView->viewport()->update();

  ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools);
  if (tb) {
    if (tb->currentStyle() == ModernToolbar::Radial)
      tb->setScale(profile.toolbarScale);
    if (m_sliderToolbarScale) {
      m_sliderToolbarScale->blockSignals(true);
      m_sliderToolbarScale->setValue(profile.toolbarScale * 100);
      m_sliderToolbarScale->blockSignals(false);
    }
  }

  int btnSize = profile.buttonSize;
  int iconSize = btnSize - 12;
  if (iconSize < 16)
    iconSize = 16;

  const auto buttons = this->findChildren<ModernButton *>();
  for (ModernButton *btn : buttons) {
#ifdef Q_OS_ANDROID
    // Keep custom Android top-bar pill buttons at their dedicated size.
    if (btn == m_btnAndroidToolbarMenu || btn == m_btnAndroidToolbarPageManager ||
        btn == m_btnAndroidToolbarExport) {
      continue;
    }
#endif
#ifndef Q_OS_ANDROID
    // Desktop-Titelleiste: ⋯ + Seitenmanager — feste Maße aus setupTitleBar(),
    // nicht durch Profil-„Button-Größe“ überschreiben (war immer wieder winzig).
    if (btn == m_btnEditorNoteOverflow || btn == m_btnTitleBarPageManager) {
      continue;
    }
#endif
    btn->setFixedSize(btnSize, btnSize);
    btn->setIconSize(QSize(iconSize, iconSize));
  }
  int fabS;
#ifdef Q_OS_ANDROID
  fabS = FAB_SIZE_ANDROID;
#else
  fabS = btnSize + 16;
#endif

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
#ifdef Q_OS_ANDROID
  // Android takes a completely separate path: a Material-Design icon
  // library that renders into the requested pixel size on demand and
  // caches the result. This avoids the 64-coordinate Windows-flavoured
  // glyphs - they look tiny / blocky when QListView upscales them for
  // touch-sized welcome tiles. AndroidIcons covers every name used by
  // the Android UI (notes, folders, header pills, dialog buttons, etc.).
  // If a name isn't recognised we fall through to the Windows painter so
  // we never regress an existing call site.
  {
    QIcon androidIc = AndroidIcons::icon(name, color, /*dpSize=*/48);
    if (!androidIc.isNull()) {
      QPixmap pm = androidIc.pixmap(96, 96);
      if (!pm.isNull())
        return androidIc;
    }
  }
#endif
  const int size = 64;
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  if (name == "menu_pill" || name == "settings_pill" || name == "pages_pill" || name == "more_pill") {
    // Capsule matches Android top-bar assets; glyph tint follows `color` (same on Win/Android).
    const QRectF capsule(3, 14, 58, 36);
    const QColor glyph = color.isValid() ? color : QColor(QStringLiteral("#C8CDDC"));
    p.setPen(QPen(QColor(255, 255, 255, 48), 1.0));
    p.setBrush(QColor(27, 30, 58, 235));
    p.drawRoundedRect(capsule, 18, 18);

    p.setPen(QPen(glyph, 2.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    if (name == "menu_pill") {
      p.drawLine(22, 26, 42, 26);
      p.drawLine(22, 32, 39, 32);
      p.drawLine(22, 38, 42, 38);
    } else if (name == "settings_pill") {
      const QPointF c(32, 32);
      p.drawEllipse(c, 7.8, 7.8);
      p.drawEllipse(c, 3.8, 3.8);
      const double pi = 3.14159265358979323846;
      for (int i = 0; i < 8; ++i) {
        const double a = (i * pi) / 4.0;
        const double cs = qCos(a);
        const double sn = qSin(a);
        QPointF p1(c.x() + cs * 9.5, c.y() + sn * 9.5);
        QPointF p2(c.x() + cs * 12.8, c.y() + sn * 12.8);
        p.drawLine(p1, p2);
      }
    } else if (name == "pages_pill") {
      p.drawRoundedRect(24, 28, 10, 10, 1.8, 1.8);
      p.drawRoundedRect(28, 24, 10, 10, 1.8, 1.8);
      p.drawRoundedRect(32, 20, 10, 10, 1.8, 1.8);
    } else {
      // more_pill — vertical ⋮ (same graphic as Android overflow)
      p.setPen(Qt::NoPen);
      p.setBrush(glyph);
      p.drawEllipse(29, 23, 6, 6);
      p.drawEllipse(29, 29, 6, 6);
      p.drawEllipse(29, 35, 6, 6);
    }
    return QIcon(pixmap);
  }
  p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  if (name == "folder") {
    // Soft filled folder with tab — reads clearly at small tile sizes.
    p.setPen(Qt::NoPen);
    QColor fill = color;
    fill.setAlpha(210);
    p.setBrush(fill);
    QPainterPath tab;
    tab.moveTo(12, 20);
    tab.lineTo(12, 16);
    tab.cubicTo(12, 14, 13, 13, 15, 13);
    tab.lineTo(28, 13);
    tab.cubicTo(30, 13, 31, 14, 32, 16);
    tab.lineTo(34, 20);
    tab.closeSubpath();
    p.drawPath(tab);
    p.drawRoundedRect(QRectF(12, 20, 40, 28), 5, 5);
    QColor sheen = QColor(255, 255, 255, 40);
    p.setBrush(sheen);
    p.drawRoundedRect(QRectF(16, 24, 32, 8), 3, 3);
  } else if (name == "cloud") {
    p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(12, 28, 16, 16);
    p.drawEllipse(24, 20, 20, 20);
    p.drawEllipse(40, 28, 14, 14);
  } else if (name == "share") {
    p.drawEllipse(44, 16, 8, 8);
    p.drawEllipse(44, 48, 8, 8);
    p.drawEllipse(16, 32, 8, 8);
    p.drawLine(22, 30, 42, 20);
    p.drawLine(22, 34, 42, 46);
  } else if (name == "device") {
    p.drawRoundedRect(20, 14, 24, 36, 4, 4);
    p.drawLine(20, 42, 44, 42);
    p.setPen(QPen(color, 2));
    p.drawPoint(32, 46);
  } else if (name == "more_vert") {
    // Vertical 3-dot menu (dots sized for ~18px toolbar icons after downscale)
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(27, 14, 10, 10);
    p.drawEllipse(27, 27, 10, 10);
    p.drawEllipse(27, 40, 10, 10);
  } else if (name == "export") {
    // Box with arrow (export)
    p.drawRect(16, 20, 32, 26);
    p.drawLine(30, 18, 30, 10);
    p.drawLine(22, 18, 30, 10);
    p.drawLine(38, 18, 30, 10);
  } else if (name == "arrow_left") {
    QPainterPath path;
    path.moveTo(42, 12);
    path.lineTo(20, 32);
    path.lineTo(42, 52);
    p.drawPath(path);
  } else if (name == "home") {
    // Haus-Symbol (Titelleisten-Tabs)
    p.setPen(QPen(color, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath roof;
    roof.moveTo(32, 14);
    roof.lineTo(18, 28);
    roof.lineTo(46, 28);
    roof.closeSubpath();
    p.drawPath(roof);
    p.drawRoundedRect(22, 28, 20, 18, 2.5, 2.5);
  } else if (name == "menu") {
    // Compact toolbar-like menu icon (short rounded lines)
    p.setPen(QPen(color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(18, 20, 46, 20);
    p.drawLine(18, 32, 42, 32);
    p.drawLine(18, 44, 46, 44);
  } else if (name == "settings") {
    // Minimal gear icon inspired by provided reference.
    const QPointF c(32, 32);
    const qreal ringOuter = 14.0;
    const qreal ringInner = 7.0;
    p.setPen(QPen(color, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawEllipse(c, ringOuter, ringOuter);
    p.drawEllipse(c, ringInner, ringInner);
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < 8; ++i) {
      const double a = (i * pi) / 4.0;
      const double cs = qCos(a);
      const double sn = qSin(a);
      QPointF p1(c.x() + cs * 17.0, c.y() + sn * 17.0);
      QPointF p2(c.x() + cs * 21.5, c.y() + sn * 21.5);
      p.drawLine(p1, p2);
    }
  } else if (name == "search") {
    // Magnifier
    p.setPen(QPen(color, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawEllipse(18, 16, 20, 20);
    p.drawLine(35, 36, 52, 53);
  } else if (name == "add") {
    p.drawLine(32, 14, 32, 50);
    p.drawLine(14, 32, 50, 32);
  } else if (name == "close") {
    p.drawLine(20, 20, 44, 44);
    p.drawLine(44, 20, 20, 44);
  } else if (name == "drive") {
    p.drawPolygon(QPolygonF()
                  << QPointF(32, 10) << QPointF(54, 50) << QPointF(10, 50));
  } else if (name == "dropbox") {
    p.drawLine(16, 24, 32, 36);
    p.drawLine(32, 36, 48, 24);
    p.drawLine(48, 24, 32, 12);
    p.drawLine(32, 12, 16, 24);
    p.drawLine(16, 24, 16, 40);
    p.drawLine(16, 40, 32, 52);
    p.drawLine(32, 52, 48, 40);
    p.drawLine(48, 40, 48, 24);
  } else if (name == "onedrive") {
    p.drawEllipse(20, 24, 16, 14);
    p.drawEllipse(36, 20, 14, 14);
  } else if (name == "pages") {
    // Cleaner stacked-page icon (no inner lines, like reference style).
    p.setPen(QPen(color, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(16, 20, 24, 24, 3, 3);
    p.drawRoundedRect(22, 15, 24, 24, 3, 3);
    p.drawRoundedRect(28, 10, 24, 24, 3, 3);
  } else if (name == "note_bnote") {
    // Soft A4 page plate with ruled lines.
    p.setPen(Qt::NoPen);
    QColor plate = color;
    plate.setAlpha(36);
    p.setBrush(plate);
    p.drawRoundedRect(QRectF(15, 10, 34, 44), 6, 6);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(QRectF(15, 10, 34, 44), 6, 6);
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(22, 22, 42, 22);
    p.drawLine(22, 30, 42, 30);
    p.drawLine(22, 38, 36, 38);
  } else if (name == "note_blop") {
    // Infinite canvas tile with quiet grid.
    p.setPen(Qt::NoPen);
    QColor plate = color;
    plate.setAlpha(40);
    p.setBrush(plate);
    p.drawRoundedRect(QRectF(12, 12, 40, 40), 8, 8);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(QRectF(12, 12, 40, 40), 8, 8);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(25, 16, 25, 48);
    p.drawLine(39, 16, 39, 48);
    p.drawLine(16, 25, 48, 25);
    p.drawLine(16, 39, 48, 39);
  } else if (name == "select" || name == "lasso") {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, 2.2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(QRectF(14, 14, 36, 36), 4, 4);
    p.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(18, 18, 26, 18);
    p.drawLine(18, 18, 18, 26);
  } else if (name == "palette") {
    p.setPen(QPen(color, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(32, 32), 18, 18);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QPointF(22, 26), 3.5, 3.5);
    p.drawEllipse(QPointF(32, 20), 3.5, 3.5);
    p.drawEllipse(QPointF(42, 26), 3.5, 3.5);
    p.drawEllipse(QPointF(28, 38), 4.0, 4.0);
  } else if (name == "bookmark" || name == "bookmarks") {
    blopDrawToolbarGlyph64(&p, QStringLiteral("bookmark"), color);
  } else if (name == "history" || name == "clock") {
    blopDrawToolbarGlyph64(&p, QStringLiteral("history"), color);
  } else if (name == "undo" || name == "redo" || name == "chevron_left" ||
             name == "chevron_right" || name == "chevron_rail" ||
             name == "zoom_in" || name == "zoom_out" || name == "fit_width" ||
             name == "fit_page" || name == "library") {
    blopDrawToolbarGlyph64(&p, name, color);
  }
  return QIcon(pixmap);
}

void MainWindow::createDefaultFolder() {
#ifdef Q_OS_ANDROID
  QString basePath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
  QString basePath =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
  m_rootPath = basePath + "/BlopNotizen";
  QDir dir(m_rootPath);
  if (!dir.exists())
    dir.mkpath(".");
}

void MainWindow::setupUi() {
  qDebug() << "setupUi() start";
  // Custom TitleBar nur auf Android wurde deaktiviert - Desktop nutzt native Titelleiste
#ifndef Q_OS_ANDROID
  setupTitleBar();
#endif
  qDebug() << "setupUi() nach setupTitleBar";

  m_centralContainer = new QWidget(this);
  setCentralWidget(m_centralContainer);
  QVBoxLayout *mainLayout = new QVBoxLayout(m_centralContainer);
#ifdef Q_OS_ANDROID
  // Avoid a permanent black bottom strip on Android; per-view layouts handle
  // bottom safe spacing where needed.
  mainLayout->setContentsMargins(0, 0, 0, 0);
#else
  mainLayout->setContentsMargins(0, 0, 0, 0);
#endif
  mainLayout->setSpacing(0);

#ifdef Q_OS_ANDROID
  // QtWebView on Android uses a SurfaceView that can draw above unrelated Qt
  // siblings (including QToolBar). Keep Notizen/Study tabs inside centralWidget,
  // stacked above the content, so the tab row stays visible and tappable.
  // v3.17.0: theme-aware background. CentralContainer is the page-level
  // surface; AndroidTopChrome is the chrome strip above it.
  m_centralContainer->setStyleSheet(
      QStringLiteral("QWidget#CentralContainer { background-color: %1; }")
          .arg(BlopTheme::surfaceBackground().name(QColor::HexRgb)));
  m_centralContainer->setObjectName(QStringLiteral("CentralContainer"));

  QWidget *androidTopChrome = new QWidget(m_centralContainer);
  m_androidHeader = androidTopChrome;
  androidTopChrome->setObjectName(QStringLiteral("AndroidTopChrome"));
  // v3.18.7: chrome blends into the page background so the user sees only
  // floating chips + home button on top of the content surface, instead
  // of a discrete dark bar across the top edge.
  androidTopChrome->setStyleSheet(
      QStringLiteral("QWidget#AndroidTopChrome { background-color: %1; "
                     "border: none; }")
          .arg(BlopTheme::surfaceBackground().name(QColor::HexRgb)));
  const int androidTopInset = UiScale::safeTopPx(this);
  const int androidHeaderSidePad = UiScale::safeHorizontalPaddingPx(this);
  // v3.18.7: was UiScale::dp(4). Dropped to 0 — the chip row sits flush
  // under the system status-bar inset without an extra "lip" of padding.
  const int androidHeaderTopExtra = 0;
  // Anchor responsive sizing to the actual device screen width (works even
  // before the window has been shown), so we don't get oversized buttons
  // because width() still returns the Qt default 640px.
  const int androidScreenW = UiScale::androidScreenWidthPx(this);
  const int androidHeaderHeight =
      qBound(UiScale::dp(46), int(androidScreenW * 0.09), UiScale::dp(60));
  const int androidHeaderButtonW =
      qBound(UiScale::dp(40), int(androidScreenW * 0.11), UiScale::dp(56));
  const int androidHeaderButtonH =
      qBound(UiScale::dp(28), int(androidHeaderHeight * 0.7), UiScale::dp(38));
  const int androidCompactPillH =
      qBound(UiScale::dp(26), int(androidHeaderHeight * 0.58), UiScale::dp(34));
  const int androidHeaderTotalH =
      androidHeaderHeight + androidTopInset + androidHeaderTopExtra;

  QWidget *androidHeader = new QWidget(androidTopChrome);
  androidHeader->setObjectName(QStringLiteral("AndroidTopHeaderInner"));
  androidHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  androidHeader->setFixedHeight(androidHeaderTotalH);
  // v3.18.7: transparent so the AndroidTopChrome surface (which now
  // matches the page background) shows through. Eliminates the hard
  // dark band that used to read as a separate top bar above the chips.
  androidHeader->setStyleSheet(
      QStringLiteral("QWidget#AndroidTopHeaderInner { background: transparent; }"));
  QHBoxLayout *headerLay = new QHBoxLayout(androidHeader);
  headerLay->setContentsMargins(androidHeaderSidePad, androidTopInset + androidHeaderTopExtra,
                                androidHeaderSidePad, UiScale::dp(4));
  headerLay->setSpacing(UiScale::dp(8));

  auto loadTightIcon = [](const QString &resourcePath, const QIcon &fallback,
                          const QColor &tint) -> QIcon {
    QPixmap pm(resourcePath);
    if (pm.isNull())
      return fallback;
    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    int minX = img.width(), minY = img.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < img.height(); ++y) {
      const QRgb *row = reinterpret_cast<const QRgb *>(img.scanLine(y));
      for (int x = 0; x < img.width(); ++x) {
        if (qAlpha(row[x]) > 8) {
          minX = qMin(minX, x);
          minY = qMin(minY, y);
          maxX = qMax(maxX, x);
          maxY = qMax(maxY, y);
        }
      }
    }
    if (maxX < minX || maxY < minY)
      return QIcon(pm);
    QRect crop(minX, minY, maxX - minX + 1, maxY - minY + 1);
    const int pad = 2;
    crop.adjust(-pad, -pad, pad, pad);
    crop = crop.intersected(QRect(0, 0, img.width(), img.height()));
    QImage out = img.copy(crop).convertToFormat(QImage::Format_ARGB32);
    if (tint.isValid()) {
      for (int y = 0; y < out.height(); ++y) {
        QRgb *row = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
          QColor px = QColor::fromRgba(row[x]);
          if (px.alpha() == 0)
            continue;
          const int light = qGray(px.red(), px.green(), px.blue());
          if (light >= 70) {
            const qreal mix = (light >= 130) ? 0.60 : 0.28;
            const int r = int(px.red() * (1.0 - mix) + tint.red() * mix);
            const int g = int(px.green() * (1.0 - mix) + tint.green() * mix);
            const int b = int(px.blue() * (1.0 - mix) + tint.blue() * mix);
            row[x] = qRgba(r, g, b, px.alpha());
          }
        }
      }
    }
    return QIcon(QPixmap::fromImage(out));
  };

  m_btnAndroidHome = new ModernButton(androidHeader);
  m_btnAndroidHome->setObjectName(QStringLiteral("AndroidHeaderBtnHome"));
  m_btnAndroidHome->setIcon(
      createModernIcon(QStringLiteral("home"), BlopTheme::textPrimary()));
  m_btnAndroidHome->setFixedSize(androidHeaderButtonW, androidHeaderButtonH);
  // v3.18.5: square 22dp icon centered in the button instead of an icon
  // stretched to the full button rect (which looked smudged against the
  // accent pill in the previous build).
  m_btnAndroidHome->setIconSize(QSize(UiScale::dp(22), UiScale::dp(22)));
  m_btnAndroidHome->setHoverScaleEnabled(false);
  m_btnAndroidHome->setToolTip(tr("Zur Übersicht"));
  // v3.18.5: match the accent-pill background used by the menu/page
  // buttons next to it so the header reads as a single, coherent row
  // instead of one ghost button + two filled ones.
  m_btnAndroidHome->setStyleSheet(
      BlopStyle::toolButtonAccentQss(m_currentAccentColor, UiScale::dp(16)));
  connect(m_btnAndroidHome, &QAbstractButton::clicked, this, [this]() {
    if (m_rightStack && m_rightStack->currentWidget() == m_editorContainer) {
      onBackToOverview();
      return;
    }
    if (m_isSidebarOpen)
      animateSidebar(false);
  });
  headerLay->addWidget(m_btnAndroidHome, 0, Qt::AlignVCenter);

  // Hamburger: sidebar in overview + editor (separate from home).
  m_btnAndroidToolbarMenu = new ModernButton(androidHeader);
  m_btnAndroidToolbarMenu->setObjectName(QStringLiteral("AndroidHeaderBtnMenu"));
  {
    QIcon menuIcon = loadTightIcon(":/assets/android_btn_menu.png",
                                   createModernIcon("menu_pill", QColor("#A0A0C8")),
                                   m_currentAccentColor);
    m_btnAndroidToolbarMenu->setIcon(menuIcon);
  }
  m_btnAndroidToolbarMenu->setFixedSize(androidHeaderButtonW, androidHeaderButtonH);
  m_btnAndroidToolbarMenu->setIconSize(QSize(androidHeaderButtonW, androidHeaderButtonH));
  m_btnAndroidToolbarMenu->setHoverScaleEnabled(false);
  m_btnAndroidToolbarMenu->setStyleSheet(
      "QToolButton { background: transparent; border: none; padding: 0; }"
      "QToolButton:hover { background: transparent; }"
      "QToolButton:pressed { background: transparent; }"
  );
  m_btnAndroidToolbarMenu->hide();
  connect(m_btnAndroidToolbarMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  headerLay->addWidget(m_btnAndroidToolbarMenu, 0, Qt::AlignVCenter);

  // Keep mode switch directly next to the main menu icon on Android.
  headerLay->addSpacing(2);

  // Keep m_modeSelector QComboBox as the logic controller (hidden, 0x0)
  // Use visible tab buttons instead — QComboBox popups dismiss immediately on Android touch
  m_modeSelector = new QComboBox(androidHeader);
  m_modeSelector->setFixedSize(0, 0);
  m_modeSelector->setMaximumSize(0, 0);
  rebuildModeSelectorItems();
  // Desktop: combo drives onModeChanged. Android: only explicit onModeChanged (tabs +
  // updateSidebarUser) — otherwise setCurrentIndex fires onModeChanged twice per tap
  // and updateSidebarState() can leave the UI stuck in Notes.
#ifndef Q_OS_ANDROID
  connect(m_modeSelector, &QComboBox::currentIndexChanged,
          this, &MainWindow::onModeChanged);
#endif

  // Visible tab-style toggle buttons. The actual stylesheets are applied via
  // applyAndroidTabStyles() which is narrow-phone-aware (compact padding +
  // smaller font on phones so the right-side menu stays reachable). That
  // function is also re-invoked from syncAndroidHeaderGeometry() on every
  // resize and from onModeChanged() on tab switches, so the compact
  // override survives both kinds of state transitions.
  m_btnAndroidNotes = new QPushButton("Notizen", androidHeader);
  m_btnAndroidNotes->setObjectName(QStringLiteral("AndroidHeaderTabNotes"));
  m_btnAndroidNotes->setFixedHeight(androidCompactPillH);
  m_btnAndroidNotes->setCursor(Qt::PointingHandCursor);

  m_btnAndroidStudy = new QPushButton("Study", androidHeader);
  m_btnAndroidStudy->setObjectName(QStringLiteral("AndroidHeaderTabStudy"));
  m_btnAndroidStudy->setFixedHeight(androidCompactPillH);
  m_btnAndroidStudy->setCursor(Qt::PointingHandCursor);

  applyAndroidTabStyles(0); // default: notes active

  connect(m_btnAndroidNotes, &QPushButton::clicked, this, [this]() {
    BlopDiag::recordUiAction(QStringLiteral("tab_click:notes"));
    if (!m_modeSelector || m_modeSelector->count() <= 1) {
      qWarning() << "Android Notes tap ignored: mode selector not ready";
      return;
    }
    if (m_authNavigationLocked) {
      qInfo() << "Auth gate: Notes tap ignored while login pending";
      return;
    }
    QSignalBlocker b(m_modeSelector);
    m_modeSelector->setCurrentIndex(0);
    onModeChanged(0);
  });
  connect(m_btnAndroidStudy, &QPushButton::clicked, this, [this]() {
    BlopDiag::recordUiAction(QStringLiteral("tab_click:study"));
    qInfo() << "MainWindow: Study tab CLICKED, modeSelector count=" << (m_modeSelector ? m_modeSelector->count() : -1)
            << "authLocked=" << m_authNavigationLocked
            << "sidebarOpen=" << m_isSidebarOpen
            << "mainStack=" << (m_mainContentStack ? m_mainContentStack->currentIndex() : -1);
    if (!m_modeSelector || m_modeSelector->count() <= 1) {
      qWarning() << "Android Study tap ignored: mode selector not ready";
      return;
    }
    QSignalBlocker b(m_modeSelector);
    m_modeSelector->setCurrentIndex(1);
    onModeChanged(1);
    qInfo() << "MainWindow: Study tab after onModeChanged, mainStack=" << (m_mainContentStack ? m_mainContentStack->currentIndex() : -1);
  });

  headerLay->addWidget(m_btnAndroidNotes);
  headerLay->addWidget(m_btnAndroidStudy);

  m_btnAndroidAddWebBookmark = new QPushButton(QStringLiteral("+"), androidHeader);
  m_btnAndroidAddWebBookmark->setObjectName(
      QStringLiteral("AndroidHeaderBtnPlus"));
  m_btnAndroidAddWebBookmark->setFixedSize(
      qBound(UiScale::dp(28), int(androidScreenW * 0.085), UiScale::dp(36)),
      androidCompactPillH);
  m_btnAndroidAddWebBookmark->setCursor(Qt::PointingHandCursor);
  m_btnAndroidAddWebBookmark->setToolTip(tr("Web-Lesezeichen"));
  m_btnAndroidAddWebBookmark->setStyleSheet(BlopTheme::themed(
      "QPushButton {"
      "  background: rgba(255,255,255,0.08);"
      "  color: #F4F5FB;"
      "  border-radius: 13px;"
      "  font-size: 16px; font-weight: 700;"
      "  border: 1px solid rgba(255,255,255,0.16);"
      "}"
      "QPushButton:pressed { background: rgba(255,255,255,0.14); }"));
  connect(m_btnAndroidAddWebBookmark, &QPushButton::clicked, this, [this]() {
    if (!m_btnAndroidAddWebBookmark)
      return;
#ifdef Q_OS_ANDROID
    openWebBookmarkMenuFromWebQmlBar();
#else
    openWebBookmarkOverflowMenuFromWidget(m_btnAndroidAddWebBookmark);
#endif
  });
  headerLay->addWidget(m_btnAndroidAddWebBookmark);

  headerLay->addWidget(m_modeSelector); // hidden 0x0, logic only

  // Symmetric stretches around the search bar so the right-side action
  // buttons (Page-Manager, Export) always sit flush to the right edge,
  // regardless of search bar visibility / pill widths.
  headerLay->addStretch(1);
  m_androidTopSearchBar = new QLineEdit(androidHeader);
  m_androidTopSearchBar->setObjectName("androidTopSearchBar");
  m_androidTopSearchBar->setPlaceholderText(
      QStringLiteral("Notizen durchsuchen..."));
  m_androidTopSearchBar->setFrame(false);
  m_androidTopSearchBar->setAttribute(Qt::WA_StyledBackground, true);
  m_androidTopSearchBar->setFixedHeight(androidHeaderButtonH);
  // Width is finalised in syncAndroidHeaderGeometry() against the real screen
  // width so it cannot spill past the right edge on phones.
  m_androidTopSearchBar->setMinimumWidth(
      qBound(UiScale::dp(120), int(androidScreenW * 0.28), UiScale::dp(220)));
  m_androidTopSearchBar->setMaximumWidth(
      qBound(UiScale::dp(180), int(androidScreenW * 0.46), UiScale::dp(380)));
  m_androidTopSearchBar->setStyleSheet(BlopTheme::themed(
      "QLineEdit#androidTopSearchBar {"
      "  background: rgba(255,255,255,0.08);"
      "  color: #F2F1FF;"
      "  border: 1px solid rgba(255,255,255,0.12);"
      "  border-radius: 16px;"
      "  padding: 0 14px;"
      "  font-size: 12px;"
      "  selection-background-color: rgba(124,92,252,0.55);"
      "}"
      "QLineEdit#androidTopSearchBar:focus {"
      "  background: rgba(124,92,252,0.14);"
      "  border: 1px solid rgba(124,92,252,0.66);"
      "}"
      "QLineEdit#androidTopSearchBar::placeholder {"
      "  color: rgba(255,255,255,0.48);"
      "}"));
  m_androidTopSearchBar->hide();
  headerLay->addWidget(m_androidTopSearchBar, 0, Qt::AlignVCenter);
  headerLay->addStretch(1);

  auto jumpToMatchingEditorTab = [this]() {
    if (!m_androidTopSearchBar || !m_editorTabs)
      return;
    const QString needle = m_androidTopSearchBar->text().trimmed();
    if (needle.isEmpty())
      return;
    const int tabCount = m_editorTabs->count();
    if (tabCount <= 0)
      return;
    const int start = qMax(0, m_editorTabs->currentIndex());
    for (int step = 0; step < tabCount; ++step) {
      const int idx = (start + step) % tabCount;
      if (m_editorTabs->tabText(idx).contains(needle, Qt::CaseInsensitive)) {
        m_editorTabs->setCurrentIndex(idx);
        return;
      }
    }
  };
  connect(m_androidTopSearchBar, &QLineEdit::textChanged, this,
          [jumpToMatchingEditorTab](const QString &) { jumpToMatchingEditorTab(); });
  connect(m_androidTopSearchBar, &QLineEdit::returnPressed, this,
          jumpToMatchingEditorTab);

#ifdef Q_OS_ANDROID
  // Page manager (only for A4 notes) - orange button.
  m_btnAndroidToolbarPageManager = new ModernButton(androidHeader);
  m_btnAndroidToolbarPageManager->setObjectName(
      QStringLiteral("AndroidHeaderBtnPageMgr"));
  {
    QIcon pagesIcon = loadTightIcon(":/assets/android_btn_pages.png",
                                    createModernIcon("pages_pill", QColor("#C8CDDC")),
                                    m_currentAccentColor);
    m_btnAndroidToolbarPageManager->setIcon(pagesIcon);
  }
  m_btnAndroidToolbarPageManager->setFixedSize(androidHeaderButtonW, androidHeaderButtonH);
  m_btnAndroidToolbarPageManager->setIconSize(QSize(androidHeaderButtonW, androidHeaderButtonH));
  m_btnAndroidToolbarPageManager->setHoverScaleEnabled(false);
  m_btnAndroidToolbarPageManager->setStyleSheet(
      "QToolButton { background: transparent; border: none; padding: 0; }"
      "QToolButton:hover { background: transparent; }"
      "QToolButton:pressed { background: transparent; }");
  m_btnAndroidToolbarPageManager->hide();
  connect(m_btnAndroidToolbarPageManager, &QAbstractButton::clicked, this,
          &MainWindow::onTogglePageManager);
  headerLay->addWidget(m_btnAndroidToolbarPageManager, 0, Qt::AlignVCenter);

  m_btnAndroidToolbarExport = new ModernButton(androidHeader);
  m_btnAndroidToolbarExport->setObjectName(
      QStringLiteral("AndroidHeaderBtnExport"));
  m_btnAndroidToolbarExport->setToolTip(QStringLiteral("Notiz-Menü"));
  m_btnAndroidToolbarExport->setHoverScaleEnabled(false);
  m_btnAndroidToolbarExport->setStyleSheet(
      "QToolButton { background: transparent; border: none; padding: 0; }"
      "QToolButton:hover { background: transparent; }"
      "QToolButton:pressed { background: transparent; }");
  m_btnAndroidToolbarExport->setFixedSize(androidHeaderButtonW, androidHeaderButtonH);
  m_btnAndroidToolbarExport->setIconSize(QSize(androidHeaderButtonW, androidHeaderButtonH));
  m_btnAndroidToolbarExport->setIcon(
      createModernIcon("more_pill", QColor("#C8CDDC")));
  m_btnAndroidToolbarExport->hide();
  connect(m_btnAndroidToolbarExport, &QAbstractButton::clicked, this,
          &MainWindow::onEditorNoteOverflowMenu);
  headerLay->addWidget(m_btnAndroidToolbarExport, 0, Qt::AlignVCenter);
#endif

  // Document tabs strip (editor only) — Drawboard-style open-note switching.
  m_documentTabBar = new DocumentTabBar(androidTopChrome);
  m_documentTabBar->setObjectName(QStringLiteral("AndroidDocumentTabBar"));
  m_documentTabBar->setAccentColor(m_currentAccentColor);
  m_documentTabBar->setHomeVisible(false); // header Home button already exists
  m_documentTabBar->setFixedHeight(UiScale::dp(40));
  m_documentTabBar->hide();
  connect(m_documentTabBar, &DocumentTabBar::currentChanged, this,
          [this](int index) {
            if (m_editorTabs && index >= 0 && index != m_editorTabs->currentIndex())
              m_editorTabs->setCurrentIndex(index);
          });
  connect(m_documentTabBar, &DocumentTabBar::tabCloseRequested, this,
          [this](int index) {
            if (m_editorTabs) {
              m_editorTabs->removeTab(index);
              if (m_editorTabs->count() == 0)
                onBackToOverview();
            }
          });
  connect(m_documentTabBar, &DocumentTabBar::homeClicked, this,
          &MainWindow::onBackToOverview);

  {
    QVBoxLayout *chromeLay = new QVBoxLayout(androidTopChrome);
    chromeLay->setContentsMargins(0, 0, 0, 0);
    chromeLay->setSpacing(0);
    chromeLay->addWidget(androidHeader);
    chromeLay->addWidget(m_documentTabBar);
  }
  androidTopChrome->setFixedHeight(androidHeaderTotalH);
  mainLayout->addWidget(androidTopChrome, 0);
  syncAndroidHeaderGeometry(this);

  // Apply fullscreen and dark system bar styling on Android UI thread.
  QTimer::singleShot(300, this, []() { applyAndroidImmersiveUi(); });

#else
  if (m_titleBarWidget) {
    mainLayout->addWidget(m_titleBarWidget);
  }

  // ── Single ModernToolbar instance replaces the docked bar ────────────────
  // (m_floatingTools initialized in setupRightSidebar/setupFloatingToolbar)
#endif

  m_mainContentStack = new QStackedWidget(this);
  m_mainContentStack->setObjectName(QStringLiteral("MainContentStack"));

  QWidget *notesPage = new QWidget(m_mainContentStack);
  QHBoxLayout *notesLayout = new QHBoxLayout(notesPage);
  notesLayout->setContentsMargins(0, 0, 0, 0);
  notesLayout->setSpacing(0);

  m_mainSplitter = new QSplitter(Qt::Horizontal, notesPage);
  notesLayout->addWidget(m_mainSplitter);

  m_fileModel = new QFileSystemModel(this);
  m_fileModel->setRootPath(m_rootPath);
  m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
  m_fileModel->setReadOnly(false);
  connect(m_fileModel, &QFileSystemModel::rowsInserted, this,
          &MainWindow::updateSidebarBadges);
  connect(m_fileModel, &QFileSystemModel::rowsRemoved, this,
          &MainWindow::updateSidebarBadges);
  connect(m_fileModel, &QFileSystemModel::directoryLoaded, this,
          [this](const QString &path) {
            if (m_pendingLibraryRootPath.isEmpty())
              return;
            const QString pending =
                QFileInfo(m_pendingLibraryRootPath).canonicalFilePath();
            const QString loaded = QFileInfo(path).canonicalFilePath();
            if (pending.isEmpty() || loaded.isEmpty() || pending != loaded)
              return;
            const QModelIndex idx = m_fileModel->index(m_pendingLibraryRootPath);
            if (idx.isValid())
              setLibraryRootFromSource(idx);
            m_pendingLibraryRootPath.clear();
            updateOverviewBackButton();
            updateSidebarBadges();
          });

  setupSidebar();
  qDebug() << "setupUi() nach setupSidebar";

  m_rightStack = new QStackedWidget(this);
  m_overviewContainer = new QWidget(this);
#ifdef Q_OS_ANDROID
  m_overviewContainer->setStyleSheet(
      BlopTheme::themed(QStringLiteral("background-color: #0D0D12;")));
#endif
  // KEIN installEventFilter - damit Klicks auf Buttons korrekt weitergeleitet werden!
  QVBoxLayout *overviewLayout = new QVBoxLayout(m_overviewContainer);
#ifdef Q_OS_ANDROID
  // Align with Windows overview: same horizontal breathing room; keep only a
  // modest bottom reserve so content does not look vertically "pushed up".
  overviewLayout->setContentsMargins(MARGIN_OVERVIEW + UiScale::dp(4), UiScale::dp(20),
                                     MARGIN_OVERVIEW + UiScale::dp(4),
                                     UiScale::dp(28) + UiScale::androidBottomInsetPx(this));
#else
  overviewLayout->setContentsMargins(UiScale::dp(28), UiScale::dp(18),
                                     UiScale::dp(28), UiScale::dp(24));
#endif

  QHBoxLayout *overviewTopRow = new QHBoxLayout();
  btnOverviewMenu = new ModernButton(this);
  btnOverviewMenu->setIcon(createModernIcon("menu", Qt::white));
  connect(btnOverviewMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
#if defined(Q_OS_ANDROID)
  // Android already has the hamburger in the androidHeader bar — hide duplicate
  btnOverviewMenu->hide();
#else
  // Shown only while the library sidebar is collapsed (see updateSidebarState).
  btnOverviewMenu->setToolTip(tr("Seitenleiste einblenden"));
  btnOverviewMenu->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btnOverviewMenu->hide();
#endif
  overviewTopRow->addWidget(btnOverviewMenu);
  btnBackOverview = new ModernButton(this);
  btnBackOverview->setIcon(createModernIcon("arrow_left", Qt::white));
  btnBackOverview->hide();
  connect(btnBackOverview, &QAbstractButton::clicked, this,
          &MainWindow::onNavigateUp);
  overviewTopRow->addWidget(btnBackOverview);
  overviewTopRow->addStretch();
  overviewLayout->addLayout(overviewTopRow);

  // --- Library header (Drawboard-quiet: title + search + actions) ---
  QVBoxLayout *headerLayout = new QVBoxLayout();

#ifdef Q_OS_ANDROID
  // Compact library chrome for phone + tablet.
  headerLayout->setContentsMargins(UiScale::dp(10), UiScale::dp(8),
                                   UiScale::dp(10), UiScale::dp(12));
  headerLayout->setSpacing(UiScale::dp(10));

  // Top chrome already owns the hamburger — don't duplicate it in the library header.
  btnEditorMenu = new ModernButton(m_overviewContainer);
  btnEditorMenu->setIcon(createModernIcon("menu", Qt::white));
  btnEditorMenu->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btnEditorMenu->hide();

  // Title + quiet ghost actions on one row (Drawboard library chrome).
  QHBoxLayout *titleRow = new QHBoxLayout();
  titleRow->setContentsMargins(0, 0, 0, 0);
  titleRow->setSpacing(UiScale::dp(8));

  m_lblLibraryTitle = new QLabel(QStringLiteral("Notizen"), m_overviewContainer);
  m_lblLibraryTitle->setObjectName(QStringLiteral("overviewLibraryTitle"));
  m_lblLibraryTitle->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: #F4F5FB; font-size: 20px; font-weight: 700;"
                     " letter-spacing: -0.3px; background: transparent;")));
  titleRow->addWidget(m_lblLibraryTitle, 1);

  QPushButton *btnNewNote = new QPushButton(QStringLiteral("+ Notiz"), m_overviewContainer);
  btnNewNote->setObjectName("overviewBtnNewNote");
  btnNewNote->setFixedHeight(UiScale::dp(34));
  btnNewNote->setCursor(Qt::PointingHandCursor);
  btnNewNote->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  color: #E8E4FF;"
      "  border-radius: 12px;"
      "  padding: 0 12px;"
      "  font-weight: 600;"
      "  font-size: 12px;"
      "  border: 1px solid rgba(124,92,252,0.55);"
      "}"
      "QPushButton:pressed { background-color: rgba(124,92,252,0.16); }"
  );
  connect(btnNewNote, &QPushButton::clicked, this, &MainWindow::onNewPage);
  BlopRipple::attachPressFeedback(btnNewNote, 0.94);
  titleRow->addWidget(btnNewNote, 0);

  QPushButton *btnNewFolder = new QPushButton(QStringLiteral("Ordner"), m_overviewContainer);
  btnNewFolder->setObjectName("overviewBtnNewFolder");
  btnNewFolder->setFixedHeight(UiScale::dp(34));
  btnNewFolder->setCursor(Qt::PointingHandCursor);
  btnNewFolder->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  color: rgba(232,228,255,0.85);"
      "  border-radius: 12px;"
      "  padding: 0 12px;"
      "  font-weight: 500;"
      "  font-size: 12px;"
      "  border: 1px solid rgba(120,130,160,0.28);"
      "}"
      "QPushButton:pressed { background-color: rgba(255,255,255,0.06); }"
  );
  connect(btnNewFolder, &QPushButton::clicked, this, &MainWindow::onCreateFolder);
  BlopRipple::attachPressFeedback(btnNewFolder, 0.94);
  titleRow->addWidget(btnNewFolder, 0);

  // Tags live in the left sidebar (Drawboard-style) — no header shelf button.
  headerLayout->addLayout(titleRow);

  m_overviewSearchBar = new QLineEdit(m_overviewContainer);
  m_overviewSearchBar->setObjectName("overviewSearchBar");
  m_overviewSearchBar->setPlaceholderText("Notizen durchsuchen...");
  m_overviewSearchBar->setFrame(false);
  m_overviewSearchBar->setAttribute(Qt::WA_StyledBackground, true);
  m_overviewSearchBar->setFixedHeight(UiScale::dp(36));
  m_overviewSearchBar->setStyleSheet(BlopTheme::themed(
      "QLineEdit {"
      "  background-color: #1A1829;"
      "  color: #F4F5FB;"
      "  border: 1px solid #201E2E;"
      "  border-radius: 12px;"
      "  padding: 0 14px;"
      "  font-size: 12px;"
      "}"
      "QLineEdit:focus {"
      "  border: 1px solid #5E5CE6;"
      "}"
  ));
  headerLayout->addWidget(m_overviewSearchBar);

#else
  // Desktop — Drawboard-ähnliche Bibliothek: Titel, Action-Bar (Suche·Sort·Aktionen) hoch oben.
  headerLayout->setContentsMargins(UiScale::dp(4), UiScale::dp(4),
                                   UiScale::dp(4), UiScale::dp(12));
  headerLayout->setSpacing(UiScale::dp(10));

  m_lblLibraryTitle = new QLabel(QStringLiteral("Notizen"), m_overviewContainer);
  m_lblLibraryTitle->setObjectName(QStringLiteral("overviewLibraryTitle"));
  m_lblLibraryTitle->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: #F4F5FB; font-size: 26px; font-weight: 800;"
      " letter-spacing: -0.5px; background: transparent;")));
  headerLayout->addWidget(m_lblLibraryTitle);

  m_lblLibrarySubtitle = new QLabel(
      QStringLiteral("Bibliothek · Ordner · Cloud"), m_overviewContainer);
  m_lblLibrarySubtitle->setObjectName(QStringLiteral("overviewLibrarySubtitle"));
  m_lblLibrarySubtitle->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: rgba(180,188,215,0.55); font-size: 12px; font-weight: 500;"
      " background: transparent;")));
  headerLayout->addWidget(m_lblLibrarySubtitle);

  m_overviewSearchBar = new QLineEdit(m_overviewContainer);
  m_overviewSearchBar->setObjectName("overviewSearchBar");
  m_overviewSearchBar->setPlaceholderText(
      QStringLiteral("Durchsuchen Sie Ihre Notizen, Ordner oder Tags"));
  m_overviewSearchBar->setFrame(false);
  m_overviewSearchBar->setAttribute(Qt::WA_StyledBackground, true);
  m_overviewSearchBar->setFixedHeight(UiScale::dp(40));
  m_overviewSearchBar->setStyleSheet(BlopTheme::themed(
      "QLineEdit {"
      "  background-color: #14121F;"
      "  color: #F4F5FB;"
      "  border: 1px solid rgba(120,130,160,0.18);"
      "  border-radius: 12px;"
      "  padding: 0 14px;"
      "  font-size: 13px;"
      "}"
      "QLineEdit:focus {"
      "  border: 1px solid rgba(124,92,252,0.65);"
      "  background-color: #171524;"
      "}"
  ));

  // Org bar created early so Sort can sit in the action row (Drawboard).
  m_libraryOrgBar = new LibraryOrgBar(m_overviewContainer);
  m_libraryOrgBar->setAccentColor(m_currentAccentColor);
  connect(m_libraryOrgBar, &LibraryOrgBar::smartViewChanged, this,
          [this](LibraryOrgBar::SmartView) { applyLibraryFilters(); });
  connect(m_libraryOrgBar, &LibraryOrgBar::sortModeChanged, this,
          [this](LibraryOrgBar::SortMode) { applyLibraryFilters(); });

  QPushButton *btnNewFolder = new QPushButton(m_overviewContainer);
  btnNewFolder->setObjectName("overviewBtnNewFolder");
  btnNewFolder->setToolTip(QStringLiteral("Neuer Ordner"));
  btnNewFolder->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btnNewFolder->setCursor(Qt::PointingHandCursor);
  btnNewFolder->setIcon(createModernIcon(QStringLiteral("folder"),
                                          QColor(QStringLiteral("#E8E4FF"))));
  btnNewFolder->setIconSize(QSize(UiScale::dp(18), UiScale::dp(18)));
  btnNewFolder->setStyleSheet(
      "QPushButton {"
      "  background-color: rgba(255,255,255,0.04);"
      "  border-radius: 12px;"
      "  border: 1px solid rgba(120,130,160,0.22);"
      "}"
      "QPushButton:hover { background-color: rgba(255,255,255,0.07); border-color: rgba(124,92,252,0.45); }"
  );
  connect(btnNewFolder, &QPushButton::clicked, this, &MainWindow::onCreateFolder);
  BlopRipple::attachPressFeedback(btnNewFolder, 0.94);

  QPushButton *btnNewNote = new QPushButton(QStringLiteral("+"), m_overviewContainer);
  btnNewNote->setObjectName("overviewBtnNewNote");
  btnNewNote->setToolTip(QStringLiteral("Neue Notiz"));
  btnNewNote->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btnNewNote->setCursor(Qt::PointingHandCursor);
  btnNewNote->setStyleSheet(
      "QPushButton {"
      "  background-color: #7C5CFC;"
      "  color: #FFFFFF;"
      "  border-radius: 12px;"
      "  font-weight: 800;"
      "  font-size: 20px;"
      "  border: none;"
      "}"
      "QPushButton:hover { background-color: #8B6CFF; }"
      "QPushButton:pressed { background-color: #6A4DE6; }"
  );
  connect(btnNewNote, &QPushButton::clicked, this, &MainWindow::onNewPage);
  BlopRipple::attachPressFeedback(btnNewNote, 0.94);

  auto *actionRow = new QHBoxLayout();
  actionRow->setContentsMargins(0, 0, 0, 0);
  actionRow->setSpacing(UiScale::dp(10));
  actionRow->addWidget(m_overviewSearchBar, 1);
  m_libraryOrgBar->placeSortInActionBar(actionRow);
  actionRow->addWidget(btnNewFolder, 0);
  actionRow->addWidget(btnNewNote, 0);
  headerLayout->addLayout(actionRow);
#endif

  overviewLayout->addLayout(headerLayout);

  if (m_overviewSearchBar) {
    connect(m_overviewSearchBar, &QLineEdit::textChanged, this,
            [this](const QString &) { applyLibraryFilters(); });
  }

  auto *libraryBody = new QWidget(m_overviewContainer);
  libraryBody->setObjectName(QStringLiteral("LibraryBody"));
  auto *libraryBodyLay = new QHBoxLayout(libraryBody);
  libraryBodyLay->setContentsMargins(0, 0, 0, 0);
  libraryBodyLay->setSpacing(0);

  auto *libraryMain = new QWidget(libraryBody);
  auto *libraryMainLay = new QVBoxLayout(libraryMain);
  libraryMainLay->setContentsMargins(0, 0, 0, 0);
  libraryMainLay->setSpacing(0);

  m_libraryProxy = new LibraryFilterProxy(this);
  m_libraryProxy->setSourceModel(m_fileModel);

#ifdef Q_OS_ANDROID
  m_libraryOrgBar = new LibraryOrgBar(libraryMain);
  m_libraryOrgBar->setAccentColor(m_currentAccentColor);
  connect(m_libraryOrgBar, &LibraryOrgBar::smartViewChanged, this,
          [this](LibraryOrgBar::SmartView) { applyLibraryFilters(); });
  connect(m_libraryOrgBar, &LibraryOrgBar::sortModeChanged, this,
          [this](LibraryOrgBar::SortMode) { applyLibraryFilters(); });
#else
  if (m_libraryOrgBar)
    m_libraryOrgBar->setParent(libraryMain);
#endif
  if (m_libraryOrgBar)
    libraryMainLay->addWidget(m_libraryOrgBar, 0);

  m_fileListView = new FreeGridView(this);
  m_fileListView->setModel(m_libraryProxy);
  // Prefer navigateLibraryToPath so fetchMore + directoryLoaded keep the grid
  // populated (bare mapFromSource often yields an empty view at first paint).
  navigateLibraryToPath(m_rootPath);
  m_fileListView->setSpacing(16);
  m_fileListView->setFrameShape(QFrame::NoFrame);
#ifdef Q_OS_ANDROID
  m_fileListView->setStyleSheet(
      "QListView { background: transparent; border: none; }"
      "QListView::item { background: transparent; }");
#endif
#ifdef Q_OS_ANDROID
  // Android uses a dedicated Material-Design delegate that pulls glyphs
  // from the cached AndroidIcons library; the Windows path stays on the
  // legacy painter-driven ModernItemDelegate.
  m_fileListView->setItemDelegate(new AndroidTileDelegate(this, m_fileListView));
#else
  m_fileListView->setItemDelegate(new ModernItemDelegate(this));
#endif
  // On Android, use TouchGesture; LeftMouseButtonGesture can cause "stuck" interactions
  // and mis-detected taps when using finger input.
#ifdef Q_OS_ANDROID
  if (m_fileListView->viewport())
    m_fileListView->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
  QScroller::grabGesture(m_fileListView, QScroller::TouchGesture);
#else
  QScroller::grabGesture(m_fileListView, QScroller::LeftMouseButtonGesture);
#endif
  // Dateien und Ordner: ein Tap / ein Klick öffnet. Kurze Entprellung verhindert
  // doppeltes Öffnen bei schnellem Doppelklick auf dieselbe Notiz.
  auto mapToSource = [this](const QModelIndex &proxyIndex) -> QModelIndex {
    if (!m_libraryProxy || !proxyIndex.isValid())
      return QModelIndex();
    return m_libraryProxy->mapToSource(proxyIndex);
  };
  connect(m_fileListView, &QListView::clicked, this,
          [this, mapToSource](const QModelIndex &index) {
#ifdef Q_OS_ANDROID
            // The AndroidTileDelegate has already opened the context
            // menu for a tap on the three-dots pill - consume the
            // matching click so we don't *also* open the note in the
            // background and occlude the menu.
            if (m_androidPillClickPending) {
              m_androidPillClickPending = false;
              return;
            }
#endif
            const QModelIndex src = mapToSource(index);
            if (!m_fileModel || !src.isValid())
              return;
            if (m_fileModel->isDir(src)) {
              navigateLibraryToPath(m_fileModel->filePath(src));
              return;
            }
            static QElapsedTimer debounce;
            static QModelIndex lastIdx;
            if (lastIdx == src && debounce.isValid() &&
                debounce.elapsed() < 450)
              return;
            lastIdx = src;
            debounce.restart();
            onFileDoubleClicked(src);
          });
  connect(m_fileListView, &QListView::doubleClicked, this,
          [this, mapToSource](const QModelIndex &index) {
            const QModelIndex src = mapToSource(index);
            if (m_fileModel && src.isValid() && m_fileModel->isDir(src))
              onFileDoubleClicked(src);
          });
  connect(m_fileListView, &FreeGridView::itemDropped, this,
          &MainWindow::onItemDropped);
  m_fileListView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_fileListView, &QWidget::customContextMenuRequested,
          [this, mapToSource](const QPoint &pos) {
            QModelIndex index = mapToSource(m_fileListView->indexAt(pos));
            if (index.isValid())
              showContextMenu(m_fileListView->mapToGlobal(pos), index);
          });
  libraryMainLay->addWidget(m_fileListView, 1);
  applyLibraryFilters();

  m_lblEmptyState = new QLabel(
      QStringLiteral("Noch keine Notizen.\nLeg mit + Notiz deine erste an."),
      libraryMain);
  m_lblEmptyState->setAlignment(Qt::AlignCenter);
  m_lblEmptyState->setStyleSheet(
      QStringLiteral("color: rgba(180,188,215,0.48); font-size: 15px; font-weight: 500;"
                     " letter-spacing: -0.1px;"));
  m_lblEmptyState->hide();
  libraryMainLay->addWidget(m_lblEmptyState);
  libraryBodyLay->addWidget(libraryMain, 1);
  // Tags panel is hosted in the left Super sidebar (setupSidebar).
  overviewLayout->addWidget(libraryBody, 1);



  m_editorContainer = new QWidget(this);
  // KEIN installEventFilter - damit Klicks auf Buttons korrekt weitergeleitet werden!
  
  // Das primäre Editor-Layout
  QHBoxLayout *editorMainLayout = new QHBoxLayout(m_editorContainer);
  editorMainLayout->setContentsMargins(0, 0, 0, 0);
  editorMainLayout->setSpacing(0);

  m_editorCenterWidget = new QWidget(m_editorContainer);
  m_editorCenterWidget->setObjectName(QStringLiteral("EditorCenter"));
  // Allow the floating bottom notch to paint its full border-radius without
  // the parent clipping the lower edge.
  m_editorCenterWidget->setAttribute(Qt::WA_OpaquePaintEvent, false);
  QVBoxLayout *centerLayout = new QVBoxLayout(m_editorCenterWidget);
  centerLayout->setContentsMargins(0, 0, 0, 0);
  centerLayout->setSpacing(0);

  // ── Floating Toolbar ─────────────────────────────────────────────────────
  ModernToolbar *topToolbar = nullptr;
  AndroidPhoneToolbar *phoneToolbar = nullptr;
  // Real Android phones + desktop BLOP_SIMULATE_ANDROID_PHONE=1.
  const bool usePhoneToolbar = UiScale::isAndroidPhoneUi(this);
  if (usePhoneToolbar) {
    phoneToolbar = new AndroidPhoneToolbar(m_editorCenterWidget);
    phoneToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    phoneToolbar->resize(UiScale::dp(360), UiScale::dp(44));
    m_floatingTools = phoneToolbar;
  } else {
    topToolbar = new ModernToolbar(m_editorCenterWidget);
#ifdef Q_OS_ANDROID
    topToolbar->setOrientation(ModernToolbar::Horizontal);
    // Android Tablet: keep toolbar behavior deterministic (fixed/docked).
    topToolbar->setDraggable(false);
    topToolbar->setDockMode(true);
    topToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    topToolbar->setMinimumSize(0, 0);
    topToolbar->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    int idealW = topToolbar->calculateMinLength();
    topToolbar->setFixedHeight(UiScale::dp(56));
    topToolbar->resize(idealW, UiScale::dp(56));
#else
    // Desktop Drawboard: vertical Favorites / tool rail on the right.
    topToolbar->applyDrawboardVerticalRail();
    topToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(topToolbar, &ModernToolbar::dockModeChanged, this,
            [this](bool) { positionNoteChrome(); });
    connect(topToolbar, &ModernToolbar::toolOptionsRequested, this, [this]() {
      m_toolPropertiesVisible = true;
      if (m_toolPropertiesPanel) {
        m_toolPropertiesPanel->show();
        m_toolPropertiesPanel->syncFromToolManager();
      }
      if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools))
        tb->setPropertiesPanelOpen(true);
      positionNoteChrome();
    });
    connect(topToolbar, &ModernToolbar::propertiesPanelToggleRequested, this,
            [this]() {
              m_toolPropertiesVisible = !m_toolPropertiesVisible;
              if (m_toolPropertiesPanel)
                m_toolPropertiesPanel->setVisible(m_toolPropertiesVisible);
              if (m_toolPropertiesVisible && m_toolPropertiesPanel)
                m_toolPropertiesPanel->syncFromToolManager();
              if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools))
                tb->setPropertiesPanelOpen(m_toolPropertiesVisible);
              positionNoteChrome();
            });
    connect(topToolbar, &ModernToolbar::markupLibraryRequested, this, [this]() {
      QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
      const QString id =
          s.value(QStringLiteral("ui/markup_library_pending_insert")).toString();
      s.remove(QStringLiteral("ui/markup_library_pending_insert"));
      if (id.isEmpty())
        return;
      if (MultiPageNoteView *view = currentNoteView())
        view->insertMarkupLibraryItem(id);
    });
    connect(topToolbar, &ModernToolbar::railDockEdgeChanged, this,
            [this](ModernToolbar::RailDockEdge) { positionNoteChrome(); });
    topToolbar->setAccentColor(NoteChrome::accent());

    // Drawboard-like tool shortcuts (editor surface, ignore when typing).
    auto bindToolShortcut = [this, topToolbar](const QKeySequence &seq,
                                               ToolMode mode) {
      auto *sc = new QShortcut(seq, m_editorCenterWidget);
      sc->setContext(Qt::WidgetWithChildrenShortcut);
      connect(sc, &QShortcut::activated, this, [this, topToolbar, mode]() {
        QWidget *fw = QApplication::focusWidget();
        if (qobject_cast<QLineEdit *>(fw) || qobject_cast<QTextEdit *>(fw) ||
            qobject_cast<QPlainTextEdit *>(fw))
          return;
        const QList<RailSlot> rail = topToolbar->railSlots();
        int idx = -1;
        for (int i = 0; i < rail.size(); ++i) {
          if (rail[i].mode == mode) {
            idx = i;
            break;
          }
        }
        if (idx >= 0)
          topToolbar->applyRailSlot(idx);
        else {
          ToolManager::instance().selectTool(mode);
          topToolbar->setToolMode(mode);
        }
      });
    };
    bindToolShortcut(QKeySequence(Qt::Key_P), ToolMode::Pen);
    bindToolShortcut(QKeySequence(Qt::Key_H), ToolMode::Hand);
    bindToolShortcut(QKeySequence(Qt::Key_M), ToolMode::Highlighter);
    bindToolShortcut(QKeySequence(Qt::Key_E), ToolMode::Eraser);
    bindToolShortcut(QKeySequence(Qt::Key_V), ToolMode::Lasso);
    bindToolShortcut(QKeySequence(Qt::Key_T), ToolMode::Text);
    // Space is hold-to-pan in MultiPageNoteView (not a permanent Hand switch).

    auto bindFitShortcut = [this](const QKeySequence &seq, bool width) {
      auto *sc = new QShortcut(seq, m_editorCenterWidget);
      sc->setContext(Qt::WidgetWithChildrenShortcut);
      connect(sc, &QShortcut::activated, this, [this, width]() {
        if (auto *view = currentNoteView()) {
          if (width)
            view->fitToWidth();
          else
            view->fitPage();
          updateNoteBottomChrome();
        } else if (CanvasView *cv = getCurrentCanvas()) {
          if (width)
            cv->fitToWidth();
          else
            cv->fitPage();
          updateNoteBottomChrome();
        }
      });
    };
    bindFitShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), false);
    bindFitShortcut(QKeySequence(Qt::CTRL | Qt::Key_1), true);
#endif
    m_floatingTools = topToolbar;
  }
  m_floatingTools->raise();

#ifndef Q_OS_ANDROID
  // Radial FAB is optional (Radial toolbar style only). Keep constructed but
  // hidden in the default Drawboard vertical-rail layout to avoid overlap.
  m_radialFab = new RadialToolbarFab(m_editorCenterWidget);
  m_radialFab->setAccentColor(m_currentAccentColor);
  m_radialFab->hide();
  connect(m_radialFab, &RadialToolbarFab::toolSelected, this,
          [this](ToolMode mode) {
            ToolManager::instance().selectTool(mode);
            if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
              tb->setToolMode(mode);
              tb->openToolOptions();
            }
          });
  connect(m_radialFab, &RadialToolbarFab::undoRequested, this,
          &MainWindow::onUndo);
#endif

  // Preset chips — also shown with the vertical Favorites rail (quick ink presets).
  m_penPresetBar = new PenPresetBar(m_editorCenterWidget);
  m_penPresetBar->setAccentColor(NoteChrome::accent());
  m_penPresetBar->hide();
  connect(m_penPresetBar, &PenPresetBar::presetSelected, this,
          [this](const PenPreset &preset) {
            ToolManager::instance().selectTool(preset.mode);
            ToolConfig cfg = ToolManager::instance().config();
            cfg.penColor = preset.color;
            cfg.penWidth = preset.width;
            cfg.opacity = preset.opacity;
            ToolManager::instance().setConfig(cfg);
            if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
              tb->setToolMode(preset.mode);
              // Re-clicking a chip with Shift adds it as a Favorites rail slot.
              if (tb->isDrawboardVerticalRail() &&
                  (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier))
                tb->addCurrentToolAsRailSlot();
            }
          });

  // Install event filter to center the floating tools automatically on resize
  m_editorCenterWidget->installEventFilter(this);


  m_editorTabs = new QTabWidget(this);
  m_editorTabs->setDocumentMode(true);
  m_editorTabs->setTabsClosable(true);
  m_editorTabs->tabBar()->hide(); // We use the custom header tab bar instead
  // Style the tab bar to match the dark UI
  m_editorTabs->setStyleSheet(BlopTheme::themed(
      "QTabBar::tab {"
      "  background: transparent;"
      "  color: #888;"
      "  padding: 6px 16px;"
      "  border: none;"
      "  font-size: 12px;"
      "}"
      "QTabBar::tab:selected {"
      "  color: #F4F5FB;"
      "  border-bottom: 2px solid #5E5CE6;"
      "}"
      "QTabBar::tab:hover { color: #ccc; }"
      "QTabWidget::pane { border: none; }"));
  connect(m_editorTabs, &QTabWidget::tabCloseRequested, [this](int index) {
    if (auto *ed =
            qobject_cast<NoteEditor *>(m_editorTabs->widget(index))) {
      if (ed->view())
        ed->view()->persistViewState(
            ed->view()->property("viewStateKey").toString());
    }
    m_editorTabs->removeTab(index);
    if (m_documentTabBar)
      m_documentTabBar->removeTab(index);
    if (m_editorTabs->count() == 0)
      onBackToOverview();
  });
  connect(m_editorTabs, &QTabWidget::currentChanged, this,
          &MainWindow::onTabChanged);
  centerLayout->addWidget(m_editorTabs);

  // ── Note Page Header (kept for Android; hidden on desktop Drawboard chrome) ─
  m_noteHeader = new QWidget(m_editorCenterWidget);
  m_noteHeader->setObjectName(QStringLiteral("NoteHeader"));
  m_noteHeader->setFixedHeight(UiScale::dp(44));
  m_noteHeader->setStyleSheet(QStringLiteral(
      "QWidget#NoteHeader {"
      "  background: rgba(12, 10, 20, 0.92);"
      "  border-bottom: 1px solid rgba(120, 130, 160, 0.14);"
      "}"));
  QHBoxLayout *noteHeaderLayout = new QHBoxLayout(m_noteHeader);
  noteHeaderLayout->setContentsMargins(UiScale::dp(18), 0, UiScale::dp(12), 0);
  noteHeaderLayout->setSpacing(UiScale::dp(10));

  m_lblNoteHeaderTitle = new QLabel(m_noteHeader);
  m_lblNoteHeaderTitle->setStyleSheet(QStringLiteral(
      "color: rgba(244,245,251,0.96); font-size: 14px; font-weight: 700;"
      " letter-spacing: -0.2px; background: transparent;"));
  noteHeaderLayout->addWidget(m_lblNoteHeaderTitle);
  noteHeaderLayout->addStretch(1);

  m_lblNoteHeaderMeta = new QLabel(m_noteHeader);
  m_lblNoteHeaderMeta->setStyleSheet(QStringLiteral(
      "color: rgba(200,196,255,0.70); font-size: 11px; font-weight: 600;"
      " background: rgba(124,92,252,0.12); border: 1px solid rgba(124,92,252,0.28);"
      " border-radius: 10px; padding: 4px 10px;"));
  noteHeaderLayout->addWidget(m_lblNoteHeaderMeta);

  ModernButton *btnPageLayout = new ModernButton(m_noteHeader);
  btnPageLayout->setIcon(createModernIcon(QStringLiteral("palette"),
                                          QColor(200, 190, 255, 200)));
  btnPageLayout->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  btnPageLayout->setToolTip(QStringLiteral("Seitenlayout & Hintergrund"));
  btnPageLayout->setStyleSheet(QStringLiteral(
      "QToolButton { background: rgba(255,255,255,0.04); border: 1px solid rgba(120,130,160,0.18);"
      "  border-radius: 10px; }"
      "QToolButton:hover { background: rgba(124,92,252,0.18); border-color: rgba(124,92,252,0.40); }"));
  connect(btnPageLayout, &QAbstractButton::clicked, this, [this]() {
    if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget()))
      if (auto *view = editor->view())
        view->openPageLayoutForVisiblePage();
  });
  noteHeaderLayout->addWidget(btnPageLayout);

  m_noteHeader->hide();
#ifndef Q_OS_ANDROID
  // Desktop Drawboard: title bar tabs replace the note header band.
  m_noteHeader->setVisible(false);
  m_noteHeader->setFixedHeight(0);
#else
  centerLayout->insertWidget(0, m_noteHeader);
#endif

  // ── Drawboard utilities bar: edge-dockable undo/redo · page · zoom ────────
  {
    QSettings edgeSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    const int edgeVal =
        edgeSettings.value(QStringLiteral("ui/noteChromeEdge"),
                           int(NoteChromeEdge::Bottom))
            .toInt();
    if (edgeVal >= int(NoteChromeEdge::Top) &&
        edgeVal <= int(NoteChromeEdge::Right))
      m_noteChromeEdge = static_cast<NoteChromeEdge>(edgeVal);
    else
      m_noteChromeEdge = NoteChromeEdge::Bottom;
  }
  m_noteBottomChrome = new QWidget(m_editorCenterWidget);
  m_noteBottomChrome->setObjectName(QStringLiteral("NoteBottomChrome"));
  m_noteBottomChrome->setAttribute(Qt::WA_StyledBackground, true);
  m_noteBottomChrome->setCursor(Qt::ArrowCursor);
  m_noteChromeLayout = new QBoxLayout(QBoxLayout::LeftToRight, m_noteBottomChrome);
  m_noteChromeLayout->setContentsMargins(UiScale::dp(8), UiScale::dp(4),
                                         UiScale::dp(8), UiScale::dp(4));
  m_noteChromeLayout->setSpacing(UiScale::dp(4));

  m_btnNoteChromeGrip = new QPushButton(m_noteBottomChrome);
  m_btnNoteChromeGrip->setObjectName(QStringLiteral("NoteChromeGrip"));
  m_btnNoteChromeGrip->setToolTip(
      QStringLiteral("Ziehen zum Andocken · Rechtsklick: Rand wählen"));
  m_btnNoteChromeGrip->setCursor(Qt::SizeAllCursor);
  m_btnNoteChromeGrip->setFlat(true);
  m_btnNoteChromeGrip->setFocusPolicy(Qt::NoFocus);
  m_btnNoteChromeGrip->setFixedSize(UiScale::dp(28), UiScale::dp(36));
  m_btnNoteChromeGrip->installEventFilter(this);
  m_noteChromeLayout->addWidget(m_btnNoteChromeGrip, 0, Qt::AlignCenter);

  m_noteChromeLayout->addStretch(1);

  m_btnNoteUndo = new QPushButton(m_noteBottomChrome);
  m_btnNoteUndo->setObjectName(QStringLiteral("NoteBtnUndo"));
  m_btnNoteUndo->setToolTip(QStringLiteral("Rückgängig"));
  m_btnNoteUndo->setCursor(Qt::PointingHandCursor);
  m_btnNoteUndo->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  connect(m_btnNoteUndo, &QPushButton::clicked, this, &MainWindow::onUndo);
  m_noteChromeLayout->addWidget(m_btnNoteUndo, 0, Qt::AlignCenter);

  m_btnNoteRedo = new QPushButton(m_noteBottomChrome);
  m_btnNoteRedo->setObjectName(QStringLiteral("NoteBtnRedo"));
  m_btnNoteRedo->setToolTip(QStringLiteral("Wiederholen"));
  m_btnNoteRedo->setCursor(Qt::PointingHandCursor);
  m_btnNoteRedo->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  connect(m_btnNoteRedo, &QPushButton::clicked, this, &MainWindow::onRedo);
  m_noteChromeLayout->addWidget(m_btnNoteRedo, 0, Qt::AlignCenter);

  m_noteChromeSep1 = new QFrame(m_noteBottomChrome);
  m_noteChromeSep1->setObjectName(QStringLiteral("NoteChromeSep"));
  m_noteChromeSep1->setFrameShape(QFrame::NoFrame);
  m_noteChromeLayout->addWidget(m_noteChromeSep1, 0, Qt::AlignCenter);

  m_btnNotePagePrev = new QPushButton(m_noteBottomChrome);
  m_btnNotePagePrev->setObjectName(QStringLiteral("NoteBtnPagePrev"));
  m_btnNotePagePrev->setToolTip(QStringLiteral("Vorherige Seite"));
  m_btnNotePagePrev->setCursor(Qt::PointingHandCursor);
  m_btnNotePagePrev->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  connect(m_btnNotePagePrev, &QPushButton::clicked, this, [this]() {
    if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
      if (auto *view = editor->view()) {
        const int idx = view->currentPageIndex();
        if (idx > 0)
          view->scrollToPage(idx - 1, true);
        updateNoteBottomChrome();
      }
    }
  });
  m_noteChromeLayout->addWidget(m_btnNotePagePrev, 0, Qt::AlignCenter);

  m_lblNotePage = new QLabel(QStringLiteral("1 of 1"), m_noteBottomChrome);
  m_lblNotePage->setObjectName(QStringLiteral("NoteLblPage"));
  m_lblNotePage->setAlignment(Qt::AlignCenter);
  m_lblNotePage->setMinimumWidth(UiScale::dp(64));
  m_noteChromeLayout->addWidget(m_lblNotePage, 0, Qt::AlignCenter);

  m_btnNotePageNext = new QPushButton(m_noteBottomChrome);
  m_btnNotePageNext->setObjectName(QStringLiteral("NoteBtnPageNext"));
  m_btnNotePageNext->setToolTip(QStringLiteral("Nächste Seite"));
  m_btnNotePageNext->setCursor(Qt::PointingHandCursor);
  m_btnNotePageNext->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  connect(m_btnNotePageNext, &QPushButton::clicked, this, [this]() {
    if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
      if (auto *view = editor->view()) {
        const int idx = view->currentPageIndex();
        if (idx + 1 < view->pageCount())
          view->scrollToPage(idx + 1, true);
        updateNoteBottomChrome();
      }
    }
  });
  m_noteChromeLayout->addWidget(m_btnNotePageNext, 0, Qt::AlignCenter);

  m_noteChromeSep2 = new QFrame(m_noteBottomChrome);
  m_noteChromeSep2->setObjectName(QStringLiteral("NoteChromeSep"));
  m_noteChromeSep2->setFrameShape(QFrame::NoFrame);
  m_noteChromeLayout->addWidget(m_noteChromeSep2, 0, Qt::AlignCenter);

  m_btnNoteZoomOut = new QPushButton(m_noteBottomChrome);
  m_btnNoteZoomOut->setObjectName(QStringLiteral("NoteBtnZoomOut"));
  m_btnNoteZoomOut->setToolTip(QStringLiteral("Verkleinern"));
  m_btnNoteZoomOut->setCursor(Qt::PointingHandCursor);
  m_btnNoteZoomOut->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  connect(m_btnNoteZoomOut, &QPushButton::clicked, this, [this]() {
    if (auto *view = currentNoteView()) {
      view->zoomBy(1.0 / 1.1);
      updateNoteBottomChrome();
    } else if (CanvasView *cv = getCurrentCanvas()) {
      cv->scale(1.0 / 1.1, 1.0 / 1.1);
      updateNoteBottomChrome();
    }
  });
  m_noteChromeLayout->addWidget(m_btnNoteZoomOut, 0, Qt::AlignCenter);

  m_lblNoteZoom = new QLabel(QStringLiteral("100%"), m_noteBottomChrome);
  m_lblNoteZoom->setObjectName(QStringLiteral("NoteLblZoom"));
  m_lblNoteZoom->setAlignment(Qt::AlignCenter);
  m_lblNoteZoom->setMinimumWidth(UiScale::dp(48));
  m_lblNoteZoom->setToolTip(QStringLiteral("Doppelklick: an Inhalt anpassen (Ctrl+0)"));
  m_lblNoteZoom->installEventFilter(this);
  m_noteChromeLayout->addWidget(m_lblNoteZoom, 0, Qt::AlignCenter);

  m_btnNoteZoomIn = new QPushButton(m_noteBottomChrome);
  m_btnNoteZoomIn->setObjectName(QStringLiteral("NoteBtnZoomIn"));
  m_btnNoteZoomIn->setToolTip(QStringLiteral("Vergrößern"));
  m_btnNoteZoomIn->setCursor(Qt::PointingHandCursor);
  m_btnNoteZoomIn->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  connect(m_btnNoteZoomIn, &QPushButton::clicked, this, [this]() {
    if (auto *view = currentNoteView()) {
      view->zoomBy(1.1);
      updateNoteBottomChrome();
    } else if (CanvasView *cv = getCurrentCanvas()) {
      cv->scale(1.1, 1.1);
      updateNoteBottomChrome();
    }
  });
  m_noteChromeLayout->addWidget(m_btnNoteZoomIn, 0, Qt::AlignCenter);

  m_noteChromeLayout->addStretch(1);

  // Fit actions live in the note ⋯ menu + Ctrl+0/1 — keep buttons as hidden
  // hooks for shortcuts/automation without bloating the bar.
  m_btnNoteFitWidth = new QPushButton(m_noteBottomChrome);
  m_btnNoteFitWidth->setObjectName(QStringLiteral("NoteBtnFitWidth"));
  m_btnNoteFitWidth->hide();
  connect(m_btnNoteFitWidth, &QPushButton::clicked, this, [this]() {
    if (auto *view = currentNoteView()) {
      view->fitToWidth();
      updateNoteBottomChrome();
    } else if (CanvasView *cv = getCurrentCanvas()) {
      cv->fitToWidth();
      updateNoteBottomChrome();
    }
  });

  m_btnNoteFitPage = new QPushButton(m_noteBottomChrome);
  m_btnNoteFitPage->setObjectName(QStringLiteral("NoteBtnFitPage"));
  m_btnNoteFitPage->hide();
  connect(m_btnNoteFitPage, &QPushButton::clicked, this, [this]() {
    if (auto *view = currentNoteView()) {
      view->fitPage();
      updateNoteBottomChrome();
    } else if (CanvasView *cv = getCurrentCanvas()) {
      cv->fitPage();
      updateNoteBottomChrome();
    }
  });

  m_noteBottomChrome->installEventFilter(this);
  applyNoteChromeLayoutOrientation();
  refreshNoteChromeStyle();
  refreshNoteBottomChromeIcons();

  m_noteBottomChrome->hide();
  // Overlay on the note canvas — docks flush to an edge (not in the VBox).


#ifndef Q_OS_ANDROID
  // Canvas host — Drawboard charcoal around the white page.
  m_editorCenterWidget->setStyleSheet(
      QStringLiteral("QWidget { background: %1; }")
          .arg(NoteChrome::canvasBg().name(QColor::HexRgb)));
  if (m_editorTabs) {
    m_editorTabs->setStyleSheet(
        QStringLiteral(
            "QTabBar::tab { background: transparent; color: #888; "
            "padding: 6px 16px; border: none; font-size: 12px; }"
            "QTabBar::tab:selected { color: #E8E8E8; "
            "border-bottom: 2px solid %1; }"
            "QTabBar::tab:hover { color: #ccc; }"
            "QTabWidget::pane { border: none; background: %2; }")
            .arg(NoteChrome::accent().name(QColor::HexRgb),
                 NoteChrome::canvasBg().name(QColor::HexRgb)));
  }

  // ── Drawboard left menu strip ────────────────────────────────────────────
  loadPageChromePrefs();
  m_noteLeftRail = new NoteLeftRail(m_editorCenterWidget);
  m_noteLeftRail->setAccentColor(NoteChrome::accent());
  refreshNoteLeftRailIcons();
  connect(m_noteLeftRail, &NoteLeftRail::pagesToggled, this, [this](bool on) {
    if (m_pageThumbnailSidebar) {
      if (on) {
        m_pageThumbnailSidebar->setCollapsed(false);
        m_pageThumbnailSidebar->show();
        m_pageThumbnailSidebar->rebuild();
      } else {
        // Complete fold — hide the whole pages strip (no thin stub).
        m_pageThumbnailSidebar->setCollapsed(true);
        m_pageThumbnailSidebar->hide();
      }
    }
    refreshNoteLeftRailIcons();
    positionNoteChrome();
  });
  connect(m_noteLeftRail, &NoteLeftRail::allPagesClicked, this, [this]() {
    if (!m_allPagesOverlay)
      return;
    m_allPagesOverlay->setNoteView(currentNoteView());
    m_allPagesOverlay->setGeometry(0, 0, m_editorCenterWidget->width(),
                                   m_editorCenterWidget->height());
    m_allPagesOverlay->present();
  });
  connect(m_noteLeftRail, &NoteLeftRail::bookmarksClicked, this,
          &MainWindow::showNoteBookmarksMenu);
  connect(m_noteLeftRail, &NoteLeftRail::historyClicked, this,
          &MainWindow::showNoteHistoryMenu);
  connect(m_noteLeftRail, &NoteLeftRail::searchClicked, this, [this]() {
    if (m_titleSearchBar) {
      m_titleSearchBar->show();
      m_titleSearchBar->setFocus(Qt::OtherFocusReason);
    }
  });
  connect(m_noteLeftRail, &NoteLeftRail::propertiesClicked, this, [this]() {
    m_toolPropertiesVisible = !m_toolPropertiesVisible;
    if (m_toolPropertiesPanel)
      m_toolPropertiesPanel->setVisible(m_toolPropertiesVisible);
    if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools))
      tb->setPropertiesPanelOpen(m_toolPropertiesVisible);
    refreshNoteLeftRailIcons();
    positionNoteChrome();
  });
  connect(m_noteLeftRail, &NoteLeftRail::themeToggleClicked, this, [this]() {
    NoteChrome::toggleMode();
    // Mirror into app theme so Settings Design and editor stay aligned.
    BlopTheme::instance().setMode(NoteChrome::isDark() ? BlopTheme::Mode::Dark
                                                       : BlopTheme::Mode::Light);
    applyNoteChromeTheme();
  });
  m_noteLeftRail->hide();

  m_toolPropertiesPanel = new ToolPropertiesPanel(m_editorCenterWidget);
  m_toolPropertiesPanel->setAccentColor(NoteChrome::accent());
  m_toolPropertiesPanel->hide();
  connect(m_toolPropertiesPanel, &ToolPropertiesPanel::closeRequested, this,
          [this]() {
            m_toolPropertiesVisible = false;
            m_toolPropertiesPanel->hide();
            if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools))
              tb->setPropertiesPanelOpen(false);
            positionNoteChrome();
          });

  m_allPagesOverlay = new AllPagesOverlay(m_editorCenterWidget);
  m_allPagesOverlay->setAccentColor(NoteChrome::accent());
  connect(m_allPagesOverlay, &AllPagesOverlay::pageActivated, this,
          [this](int idx) {
            if (auto *view = currentNoteView()) {
              view->scrollToPage(idx, true);
              updateNoteBottomChrome();
              if (m_pageThumbnailSidebar)
                m_pageThumbnailSidebar->onCurrentPageChanged(idx);
            }
          });
  connect(m_allPagesOverlay, &AllPagesOverlay::pagesChanged, this, [this]() {
    if (m_pageThumbnailSidebar)
      m_pageThumbnailSidebar->rebuild();
    updateNoteBottomChrome();
  });
#endif

  // ── Page Thumbnail Sidebar ───────────────────────────────────────────────
#ifdef Q_OS_ANDROID
  m_pageThumbnailSidebar = new PageThumbnailSidebar(m_editorContainer);
#else
  m_pageThumbnailSidebar = new PageThumbnailSidebar(m_editorCenterWidget);
#endif
  m_pageThumbnailSidebar->setAccentColor(NoteChrome::accent());
#ifndef Q_OS_ANDROID
  m_pageThumbnailSidebar->setFloatingMode(true);
  {
    QSettings pageUi(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    const bool collapsed =
        pageUi.value(QStringLiteral("ui/pageRailCollapsed"), false).toBool();
    m_pageThumbnailSidebar->setCollapsed(collapsed);
    if (m_noteLeftRail)
      m_noteLeftRail->setPagesExpanded(!collapsed);
  }
  connect(m_pageThumbnailSidebar, &PageThumbnailSidebar::collapsedChanged, this,
          [this](bool collapsed) {
            QSettings pageUi(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            pageUi.setValue(QStringLiteral("ui/pageRailCollapsed"), collapsed);
            if (m_noteLeftRail)
              m_noteLeftRail->setPagesExpanded(!collapsed);
            // Keep thumbsAdjacent in sync when the strip fully folds away.
            if (m_noteLeftRail)
              m_noteLeftRail->setThumbsAdjacent(!collapsed &&
                                               m_pageThumbnailSidebar &&
                                               m_pageThumbnailSidebar->isVisible());
            positionNoteChrome();
          });
#else
  {
    QSettings pageUi(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    const bool collapsed =
        pageUi.value(QStringLiteral("ui/pageRailCollapsed"), false).toBool();
    m_pageThumbnailSidebar->setCollapsed(collapsed);
  }
  connect(m_pageThumbnailSidebar, &PageThumbnailSidebar::collapsedChanged, this,
          [](bool collapsed) {
            QSettings pageUi(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
            pageUi.setValue(QStringLiteral("ui/pageRailCollapsed"), collapsed);
          });
#endif
  connect(m_pageThumbnailSidebar, &PageThumbnailSidebar::pageSelected, this,
          [this](int pageIndex) {
            if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget()))
              if (auto *view = editor->view()) {
                view->scrollToPage(pageIndex, true);
                updateNoteBottomChrome();
              }
          });
  connect(m_pageThumbnailSidebar, &PageThumbnailSidebar::addPageRequested, this,
          [this]() {
            if (auto *view = currentNoteView()) {
              if (Note *n = view->note()) {
                const int idx = n->pages.size();
                n->ensurePage(idx);
                view->setNote(n);
                m_pageThumbnailSidebar->rebuild();
                view->scrollToPage(idx, true);
                updateNoteBottomChrome();
              }
            }
          });
  connect(m_pageThumbnailSidebar, &PageThumbnailSidebar::pagesMutated, this,
          [this]() {
            updateNoteBottomChrome();
            positionNoteChrome();
          });
  connect(m_pageThumbnailSidebar, &PageThumbnailSidebar::openAllPagesRequested,
          this, [this]() {
#ifndef Q_OS_ANDROID
            if (!m_allPagesOverlay || !m_editorCenterWidget)
              return;
            m_allPagesOverlay->setNoteView(currentNoteView());
            m_allPagesOverlay->setGeometry(0, 0, m_editorCenterWidget->width(),
                                           m_editorCenterWidget->height());
            m_allPagesOverlay->present();
#endif
          });
  m_pageThumbnailSidebar->hide();

#ifdef Q_OS_ANDROID
  editorMainLayout->addWidget(m_pageThumbnailSidebar, 0);
  editorMainLayout->addWidget(m_editorCenterWidget, 1);
#else
  // Canvas fills the editor; Drawboard chrome floats on top.
  editorMainLayout->addWidget(m_editorCenterWidget, 1);
#endif

  // Shared handlers: the same code runs whether the active toolbar is
  // ModernToolbar (desktop / Android tablet) or AndroidPhoneToolbar (phone).
  const auto onToolModeChanged = [this](ToolMode m) {
    CanvasView::ToolType type = CanvasView::ToolType::Pen;
    switch (m) {
      case ToolMode::Eraser: type = CanvasView::ToolType::Eraser; break;
      case ToolMode::Lasso: type = CanvasView::ToolType::Lasso; break;
      case ToolMode::Highlighter: type = CanvasView::ToolType::Highlighter; break;
      case ToolMode::Ruler: type = CanvasView::ToolType::Ruler; break;
      case ToolMode::Image: type = CanvasView::ToolType::Image; break;
      case ToolMode::Shape: type = CanvasView::ToolType::Shape; break;
      case ToolMode::Text: type = CanvasView::ToolType::Text; break;
      case ToolMode::Pencil: type = CanvasView::ToolType::Pencil; break;
      case ToolMode::StickyNote: type = CanvasView::ToolType::StickyNote; break;
      case ToolMode::Hand: type = CanvasView::ToolType::Hand; break;
      default: type = CanvasView::ToolType::Pen; break;
    }
    setActiveTool(type);
    // The phone toolbar has no separate rulerToggled signal; reproduce the
    // ModernToolbar behavior so the on-screen ruler appears/disappears too.
    const bool rulerActive = (m == ToolMode::Ruler);
    if (CanvasView *cv = getCurrentCanvas())
      cv->toggleRuler(rulerActive);
    if (m_editorTabs) {
      if (auto *activeEditor =
              qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
        if (auto *view = activeEditor->findChild<MultiPageNoteView *>()) {
          view->setToolMode(m);
          view->toggleRuler(rulerActive);
        }
      }
    }
  };
  const auto onPenConfigChanged = [this](QColor c, int w) {
    m_penColor = c;
    m_penWidth = w;
    if (CanvasView *cv = getCurrentCanvas()) {
      cv->setPenColor(c);
      cv->setPenWidth(w);
    }
  };

  if (topToolbar) {
    connect(topToolbar, &ModernToolbar::toolChanged, this,
            [this, onToolModeChanged](ToolMode m) {
              onToolModeChanged(m);
              if (m_toolPropertiesPanel && m_toolPropertiesVisible)
                m_toolPropertiesPanel->syncForMode(m);
              syncPenPresetBarGeometry();
            });
    connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
            [this, topToolbar, onToolModeChanged](AbstractTool *tool) {
              if (!tool)
                return;
              if (m_radialFab)
                m_radialFab->setActiveTool(tool->mode());
              if (topToolbar->toolMode() != tool->mode()) {
                // Ensure the topToolbar's own state updates, so icon highlights
                // and double-click radial menus actually trigger.
                topToolbar->setToolMode(tool->mode());
                onToolModeChanged(tool->mode());
              }
              if (m_toolPropertiesPanel && m_toolPropertiesVisible)
                m_toolPropertiesPanel->syncForMode(tool->mode());
            });

    connect(topToolbar, &ModernToolbar::rulerToggled, [this](bool active) {
        if (CanvasView *cv = getCurrentCanvas()) {
            cv->toggleRuler(active);
        }
        auto* activeEditor = qobject_cast<NoteEditor*>(m_editorTabs->currentWidget());
        if (activeEditor) {
            if (auto* view = activeEditor->findChild<MultiPageNoteView*>()) {
                view->toggleRuler(active);
            }
        }
    });
    connect(topToolbar, &ModernToolbar::undoRequested, this, &MainWindow::onUndo);
    connect(topToolbar, &ModernToolbar::redoRequested, this, &MainWindow::onRedo);
    connect(topToolbar, &ModernToolbar::backToOverviewRequested, this,
            &MainWindow::onBackToOverview);

    connect(topToolbar, &ModernToolbar::penConfigChanged, this, onPenConfigChanged);
    connect(topToolbar, &ModernToolbar::scaleChanged, [this](qreal newScale) {
      if (m_sliderToolbarScale) {
        m_sliderToolbarScale->blockSignals(true);
        m_sliderToolbarScale->setValue(newScale * 100);
        m_sliderToolbarScale->blockSignals(false);
      }
    });
  }
  if (phoneToolbar) {
    connect(phoneToolbar, &AndroidPhoneToolbar::toolChanged, this,
            onToolModeChanged);
    connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
            [this, phoneToolbar, onToolModeChanged](AbstractTool *tool) {
              if (tool && phoneToolbar->toolMode() != tool->mode()) {
                phoneToolbar->setToolMode(tool->mode());
                onToolModeChanged(tool->mode());
              }
            });
    connect(phoneToolbar, &AndroidPhoneToolbar::undoRequested, this,
            &MainWindow::onUndo);
    connect(phoneToolbar, &AndroidPhoneToolbar::redoRequested, this,
            &MainWindow::onRedo);
    connect(phoneToolbar, &AndroidPhoneToolbar::backToOverviewRequested, this,
            &MainWindow::onBackToOverview);
    connect(phoneToolbar, &AndroidPhoneToolbar::penConfigChanged, this,
            onPenConfigChanged);
  }
  setupRightSidebar();
  qDebug() << "setupUi() nach setupRightSidebar";
#ifdef Q_OS_ANDROID
  m_pageManager = new PageManager(this);
#else
  m_pageManager = new PageManager(m_editorCenterWidget);
#endif
  m_pageManager->hide();

  m_rightStack->addWidget(m_overviewContainer);
  m_rightStack->addWidget(m_editorContainer);
  m_mainSplitter->addWidget(m_rightStack);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setCollapsible(0, true);
  m_mainSplitter->setStyleSheet(
      "QSplitter::handle { background-color: transparent; }");
#ifdef Q_OS_ANDROID
  m_mainSplitter->setHandleWidth(0);
#endif

  setupWebBrowser();
  qDebug() << "setupUi() nach setupWebBrowser";

  m_mainContentStack->addWidget(notesPage);
  m_mainContentStack->addWidget(m_studyContainer);

#ifndef Q_OS_ANDROID
  m_desktopSidebarPushSpacer = new QWidget(m_centralContainer);
  m_desktopSidebarPushSpacer->setFixedWidth(0);
  m_desktopSidebarPushSpacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  QWidget *desktopContentRow = new QWidget(m_centralContainer);
  QHBoxLayout *desktopRowLay = new QHBoxLayout(desktopContentRow);
  desktopRowLay->setContentsMargins(0, 0, 0, 0);
  desktopRowLay->setSpacing(0);
  desktopRowLay->addWidget(m_desktopSidebarPushSpacer);
  desktopRowLay->addWidget(m_mainContentStack, 1);
  mainLayout->addWidget(desktopContentRow, 1);
#else
  mainLayout->addWidget(m_mainContentStack, 1);
#endif

  setWindowTitle("Blop");
  // updateSidebarUser() runs from MainWindow ctor after setupUi — avoid calling twice here

  qDebug() << "setupUi() Ende";
}

#ifdef BLOP_HAS_WEBENGINE
bool InterceptingWebPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) {
    if (url.scheme() == "blop" && (url.host() == "google-login" || url.path().contains("google-login"))) {
        emit googleLoginRequested();
        return false;
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}
#endif

#ifdef Q_OS_ANDROID
namespace {
void logAndroidSystemWebViewPackageOnce()
{
  static bool sLogged = false;
  if (sLogged)
    return;
  sLogged = true;

  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject pkg = QJniObject::callStaticObjectMethod(
        "android/webkit/WebView", "getCurrentWebViewPackage",
        "()Landroid/content/pm/PackageInfo;");
    if (!pkg.isValid()) {
      qInfo() << "BlopStudy: System WebView package: (unavailable)";
      return;
    }
    const QJniObject packageName =
        pkg.getObjectField("packageName", "Ljava/lang/String;");
    const QJniObject versionName =
        pkg.getObjectField("versionName", "Ljava/lang/String;");
    const QString pkgStr =
        packageName.isValid() ? packageName.toString() : QString();
    const QString verStr =
        versionName.isValid() ? versionName.toString() : QString();
    qInfo().nospace() << "BlopStudy: System WebView package: " << pkgStr << " "
                      << verStr;
  });
}
} // namespace
#endif

void MainWindow::setupWebBrowser() {
  m_studyContainer = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(m_studyContainer);
  layout->setContentsMargins(0, 0, 0, 0);
#ifdef Q_OS_ANDROID
  m_studyVBoxLayout = layout;
#endif

#ifdef Q_OS_ANDROID
  // v3.18.21: Use QQuickView instead of QQuickWidget to fix Android SurfaceView
  // compositing. QQuickView creates a separate Android Surface that properly
  // composites over the WebView SurfaceView, while QQuickWidget renders to
  // an FBO that blocks the WebView.
  QQuickView *view = new QQuickView();
  m_studyQQuickView = view;
  view->setResizeMode(QQuickView::SizeRootObjectToView);
  view->setColor(Qt::transparent);

  // Register MainWindow as 'blopAppBridge' to allow QML to trigger C++ slots for Login
  view->engine()->rootContext()->setContextProperty("blopAppBridge", this);

  // Load the bundled QML (contains QtWebView WebView)
  view->setSource(QUrl("qrc:/AndroidWebView.qml"));

  // Check if it's actually loaded
  if (view->status() == QQuickView::Error) {
      QString errorStr = "Fehler: Konnte Web-Modul nicht laden.\n";
      for (const QQmlError &e : view->errors()) {
          errorStr += e.toString() + "\n";
          qWarning() << "QML Error:" << e.toString();
      }
      QLabel *err = new QLabel(errorStr, m_studyContainer);
      err->setStyleSheet("color: white; font-weight: bold; padding: 20px;");
      err->setWordWrap(true);
      layout->addWidget(err);
      return;
  }

  // Create a container widget to hold the QQuickView's native window
  QWidget *container = QWidget::createWindowContainer(view, m_studyContainer);
  container->setObjectName("StudyQuickContainer");
  m_studyWindowContainer = container;
  // v3.18.25: Fix top margin issue - ensure no container margins
  container->setContentsMargins(0, 0, 0, 0);
  // Touch/focus hardening for Android
  container->setAttribute(Qt::WA_AcceptTouchEvents, true);
  container->setFocusPolicy(Qt::StrongFocus);
  // Cap the embedded Study container to the real device viewport so the
  // Bookmark sheet (anchored to QML parent.left/right) cannot extend past
  // the right edge if the host window is sized wider than the screen.
  container->setMaximumWidth(UiScale::androidScreenWidthPx(this));
  layout->addWidget(container);
  syncStudyChromeTheme();

  logAndroidSystemWebViewPackageOnce();

  // Do not immediately schedule JNI cache retries during startup; trigger only
  // once the Study surface is actually shown to avoid QtThread GL contention.

#else
#ifdef BLOP_HAS_WEBENGINE

#ifdef Q_OS_WIN
  // Ensure packaged builds can resolve WebEngine helpers/resources even when
  // started from unusual working directories (installer shortcuts, etc.).
  {
    const QString appDir = QCoreApplication::applicationDirPath();

    if (qgetenv("QTWEBENGINEPROCESS_PATH").isEmpty()) {
      const QString procPath = appDir + QStringLiteral("/QtWebEngineProcess.exe");
      if (QFile::exists(procPath))
        qputenv("QTWEBENGINEPROCESS_PATH",
                QDir::toNativeSeparators(procPath).toUtf8());
    }
    if (qgetenv("QTWEBENGINE_RESOURCES_PATH").isEmpty()) {
      const QString resPath = appDir + QStringLiteral("/resources");
      if (QDir(resPath).exists())
        qputenv("QTWEBENGINE_RESOURCES_PATH",
                QDir::toNativeSeparators(resPath).toUtf8());
    }
    if (qgetenv("QTWEBENGINE_LOCALES_PATH").isEmpty()) {
      const QString locPath1 =
          appDir + QStringLiteral("/translations/qtwebengine_locales");
      const QString locPath2 = appDir + QStringLiteral("/qtwebengine_locales");
      const QString pick = QDir(locPath1).exists() ? locPath1 : locPath2;
      if (QDir(pick).exists())
        qputenv("QTWEBENGINE_LOCALES_PATH",
                QDir::toNativeSeparators(pick).toUtf8());
    }
  }
#endif

#ifdef Q_OS_WIN
  // Optional Windows fallback for machines where embedded WebEngine still stays black.
  // Default behavior now uses embedded Study (same as Qt Creator / Debug).
  const bool forceBrowserFallback =
      (qEnvironmentVariableIntValue("BLOP_FORCE_BROWSER_LOGIN") == 1);
  if (forceBrowserFallback) {
    QWidget *fallback = new QWidget(m_studyContainer);
    fallback->setStyleSheet(BlopTheme::themed("background: #1e1e1e;"));
    QVBoxLayout *fallbackLayout = new QVBoxLayout(fallback);
    fallbackLayout->setContentsMargins(28, 28, 28, 28);
    fallbackLayout->setSpacing(14);

    QLabel *title = new QLabel(tr("Anmeldung"), fallback);
    title->setStyleSheet(BlopTheme::themed(
        "color: #E8E4FF; font-size: 22px; font-weight: 700;"));
    fallbackLayout->addWidget(title, 0, Qt::AlignLeft);

    QLabel *info = new QLabel(
        tr("Die eingebettete Study-Ansicht bleibt auf manchen Windows-Systemen schwarz.\n"
           "Bitte melde dich hier per Browser an. Danach wechselt Blop automatisch zu den Notizen."),
        fallback);
    info->setWordWrap(true);
    info->setStyleSheet(BlopTheme::themed("color: #C8C4E8; font-size: 13px;"));
    fallbackLayout->addWidget(info);

    QPushButton *btnGoogle = new QPushButton(tr("Mit Google anmelden"), fallback);
    btnGoogle->setCursor(Qt::PointingHandCursor);
    btnGoogle->setMinimumHeight(42);
    btnGoogle->setStyleSheet(
        "QPushButton { background: #4285F4; color: white; border: none; border-radius: 8px; "
        "padding: 0 18px; font-weight: 700; font-size: 14px; }"
        "QPushButton:hover { background: #3367d6; }");
    connect(btnGoogle, &QPushButton::clicked, this, &MainWindow::requestGoogleLogin);
    fallbackLayout->addWidget(btnGoogle, 0, Qt::AlignLeft);

    QPushButton *btnBrowser = new QPushButton(tr("Study im Browser öffnen"), fallback);
    btnBrowser->setCursor(Qt::PointingHandCursor);
    btnBrowser->setMinimumHeight(38);
    btnBrowser->setStyleSheet(BlopTheme::themed(
        "QPushButton { background: #2d2b42; color: #E8E4FF; border: 1px solid rgba(124,92,252,0.45); "
        "border-radius: 8px; padding: 0 16px; font-weight: 600; font-size: 13px; }"
        "QPushButton:hover { background: #3a3754; }"));
    connect(btnBrowser, &QPushButton::clicked, this, []() {
      QDesktopServices::openUrl(QUrl(kBlopStudyUrl));
    });
    fallbackLayout->addWidget(btnBrowser, 0, Qt::AlignLeft);

    fallbackLayout->addStretch(1);
    layout->addWidget(fallback, 1);
    return;
  }
#endif

  m_webViewStack = new QStackedWidget(m_studyContainer);

  QWebEngineView *view = new QWebEngineView(m_studyContainer);
  m_studyWebView = view;
  // Keep viewport fully opaque on Windows release builds. Transparent composition
  // can show as an all-black layer although hit testing still works.
  view->setStyleSheet("background: #1e1e1e;");
  view->setAutoFillBackground(true);
  view->setAttribute(Qt::WA_OpaquePaintEvent, true);
  // FIX: FramelessWindowHint + QWebEngineView auf Windows braucht WA_NativeWindow,
  // damit der interne Chromium-HWND korrekte Mouse-Events bekommt (sonst Glasscheibe).
  view->setAttribute(Qt::WA_NativeWindow);

  // Reduce GPU/compositor paths that often stay black in packaged Windows builds.
  if (QWebEngineSettings *ws = view->settings()) {
    ws->setAttribute(QWebEngineSettings::WebGLEnabled, false);
    ws->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, false);
    ws->setAttribute(QWebEngineSettings::PluginsEnabled, false);
  }

  // --- Auto-reload fix ---
  // After a fresh install the WebEngine sometimes loads a blank page.
  // Retry up to 2 times on failure, and force one reload after 4 seconds
  // to recover from the black-screen-on-first-launch issue.
  auto *retryCount = new int(0);
  connect(view, &QWebEngineView::loadFinished, m_studyContainer,
          [view, retryCount](bool ok) {
            if (!ok && *retryCount < 2) {
              ++(*retryCount);
              QTimer::singleShot(1500, view, [view]() { view->reload(); });
            }
          });

  // Force one reload 4 s after startup to fix the post-install blank page
  QTimer::singleShot(4000, view, [view]() { view->reload(); });

  InterceptingWebPage *customPage = new InterceptingWebPage(view);
  connect(customPage, &InterceptingWebPage::googleLoginRequested, this, []() {
      GoogleAuthManager::instance().login();
  });
  
  // Bridge: Send Google ID Token back into the Next.js Web App
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::idTokenReceived, this, [view](const QString& idToken) {
      if (!idToken.isEmpty()) {
          QString js = QString("if (typeof window.handleGoogleLoginSuccess === 'function') { window.handleGoogleLoginSuccess({ credential: '%1' }); }").arg(idToken);
          view->page()->runJavaScript(js);
      } else {
          qWarning() << "Google Auth granted but no id_token received!";
      }
  });
  view->setPage(customPage);
  customPage->setBackgroundColor(QColor(30, 30, 30));
  if (QWebEnginePage *p = view->page())
    p->setBackgroundColor(QColor(30, 30, 30));

  QString weRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + QStringLiteral("/BlopWebEngine");
  QDir().mkpath(weRoot);
  {
    QWebEngineProfile *pf = customPage->profile();
    pf->setPersistentStoragePath(weRoot + QStringLiteral("/storage"));
    pf->setCachePath(weRoot + QStringLiteral("/cache"));
    pf->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
  }

  m_customWebView = new QWebEngineView(m_studyContainer);
  m_customWebView->setStyleSheet(QStringLiteral("background: #1e1e1e;"));
  m_customWebView->setAutoFillBackground(true);
  m_customWebView->setAttribute(Qt::WA_OpaquePaintEvent, true);
  m_customWebView->setAttribute(Qt::WA_NativeWindow);
  if (QWebEngineSettings *ws = m_customWebView->settings()) {
    ws->setAttribute(QWebEngineSettings::WebGLEnabled, false);
    ws->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, false);
    ws->setAttribute(QWebEngineSettings::PluginsEnabled, false);
  }
  auto *customPlainPage = new QWebEnginePage(m_customWebView);
  m_customWebView->setPage(customPlainPage);
  customPlainPage->setBackgroundColor(QColor(30, 30, 30));
  {
    QWebEngineProfile *pf = customPlainPage->profile();
    const QString sub = weRoot + QStringLiteral("/custom");
    QDir().mkpath(sub);
    pf->setPersistentStoragePath(sub + QStringLiteral("/storage"));
    pf->setCachePath(sub + QStringLiteral("/cache"));
    pf->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
  }
  connect(customPlainPage, &QWebEnginePage::renderProcessTerminated, m_studyContainer,
          [this](QWebEnginePage::RenderProcessTerminationStatus status, int exitCode) {
            Q_UNUSED(exitCode);
            if (status != QWebEnginePage::RenderProcessTerminationStatus::NormalTerminationStatus)
              QTimer::singleShot(400, m_customWebView,
                                 [this]() { m_customWebView->reload(); });
          });

  m_webViewStack->addWidget(view);
  m_webViewStack->addWidget(m_customWebView);
  m_webViewStack->setCurrentWidget(view);
  layout->addWidget(m_webViewStack, 1);

  // Renderer/GPU crash leaves a black view; reload recovers (common in release-only setups).
  connect(customPage, &QWebEnginePage::renderProcessTerminated, m_studyContainer,
          [view](QWebEnginePage::RenderProcessTerminationStatus status, int exitCode) {
            Q_UNUSED(exitCode);
            if (status != QWebEnginePage::RenderProcessTerminationStatus::NormalTerminationStatus)
              QTimer::singleShot(400, view, [view]() { view->reload(); });
          });

  // Defer first navigation until after the window is shown so the native surface exists
  // (helps some Windows + frameless + WebEngine combinations that stay black).
  QTimer::singleShot(250, view, [view]() { view->load(QUrl(kBlopStudyUrl)); });

  // --- SSO Bridge: Poll localStorage to support SPA Routing ---
  // In Next.js, navigating to Dashboard after login doesn't trigger
  // loadFinished, so we must poll localStorage to detect login state changes.
  m_studySsoTimer = new QTimer(m_studyContainer);
  m_studySsoTimer->setInterval(1000);
  connect(m_studySsoTimer, &QTimer::timeout, this, [this, view]() {
    if (!m_webViewStack || m_webViewStack->currentWidget() != view)
      return;
    if (!view->page())
      return;
    view->page()->runJavaScript(
        R"js(
          (function() {
            window.isBlopNativeApp = true;
            if (localStorage.getItem('trigger_google_login') === '1') {
                localStorage.removeItem('trigger_google_login');
                return 'TRIGGER_GOOGLE_LOGIN';
            }
            var u = localStorage.getItem('username');
            var s = localStorage.getItem('session_id');
            return (u && s) ? u : '';
          })();
        )js",
        [this](const QVariant &result) {
          QString resStr = result.toString().trimmed();
          if (resStr == "TRIGGER_GOOGLE_LOGIN") {
            GoogleAuthManager::instance().login();
          } else if (!resStr.isEmpty()) {
            QString currentUser =
                QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
                    .value(QStringLiteral("username"))
                    .toString();
            if (resStr != currentUser)
              updateSidebarUser(resStr);
          } else {
            QString currentUser =
                QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
                    .value(QStringLiteral("username"))
                    .toString();
            if (!currentUser.isEmpty())
              updateSidebarUser("");
          }
        });
  });

#else
  QLabel *lblInfo = new QLabel(
      "Web-Modul fehlt.\nBitte 'Qt WebEngine' installieren.", m_studyContainer);
  lblInfo->setAlignment(Qt::AlignCenter);
  lblInfo->setStyleSheet(
      "color: #AAA; font-size: 16px; font-weight: bold; background: #1E1E1E;");
  layout->addWidget(lblInfo);
#endif
#endif
}

#ifdef Q_OS_ANDROID
void MainWindow::applyAndroidTabStyles(int index) {
  if (!m_btnAndroidNotes || !m_btnAndroidStudy)
    return;
  // The compact branch shrinks both the padding AND the font-size so the
  // Notizen / Study pills stop pushing the right-side header buttons
  // (Page-Manager, Export-three-dots) off the screen on phones.
  const int contentW = UiScale::androidContentWidthPx(this);
  const bool narrow = contentW < UiScale::dp(420);
  const QString sizeStyle =
      narrow
          ? QStringLiteral("padding: 1px 6px; font-size: 10px;")
          : QStringLiteral("padding: 3px 12px; font-size: 12px;");
  const QString accentSoft = m_currentAccentColor.lighter(130).name(QColor::HexArgb);
  const QString tabActive = BlopTheme::themed(
      QString("QPushButton {"
              "  background: %1;"
              "  color: #F4F5FB;"
              "  border-radius: 13px;"
              "  %4"
              "  font-weight: 600;"
              "  border: 1px solid %2;"
              "}"
              "QPushButton:pressed { background: %3; }")
          .arg(m_currentAccentColor.darker(150).name(QColor::HexArgb),
               accentSoft,
               m_currentAccentColor.darker(120).name(QColor::HexArgb),
               sizeStyle));
  const QString tabInactive = BlopTheme::themed(
      QString("QPushButton {"
              "  background: transparent;"
              "  color: #A7ACBB;"
              "  border-radius: 13px;"
              "  %1"
              "  font-weight: 600;"
              "  border: 1px solid rgba(255,255,255,0.08);"
              "}"
              "QPushButton:hover { background: rgba(255,255,255,0.04); color: #D8DBE8; }"
              "QPushButton:pressed { background: rgba(255,255,255,0.08); }")
          .arg(sizeStyle));
  m_btnAndroidNotes->setStyleSheet(index == 0 ? tabActive : tabInactive);
  m_btnAndroidStudy->setStyleSheet(index >= 1 ? tabActive : tabInactive);

  // Cap the maximum width so even with the narrow text we don't accidentally
  // expand on resize. Without a cap, "Notizen" still claims ~80 dp.
  const int pillMaxW = narrow ? UiScale::dp(64) : UiScale::dp(110);
  m_btnAndroidNotes->setMaximumWidth(pillMaxW);
  m_btnAndroidStudy->setMaximumWidth(pillMaxW);
}
#endif

namespace {
// v3.18.2: lightweight stack crossfade. Grabs the outgoing page as a pixmap,
// switches the stack hard, then fades the snapshot out on top of the new
// page. Deliberately avoids QGraphicsOpacityEffect (Android/Windows
// offscreen-pixmap cost) -- the overlay paints the snapshot itself with
// QPainter::setOpacity. Native child views (SurfaceView/WebEngine) are NOT
// wrapped; callers must skip pages that embed them.
class StackFadeOverlay : public QWidget {
public:
  StackFadeOverlay(QWidget *parent, const QPixmap &snapshot)
      : QWidget(parent), m_snapshot(snapshot) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setGeometry(parent->rect());
    show();
    raise();
    auto *anim = new QVariantAnimation(this);
    anim->setDuration(BlopMotion::kEmphasis);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(BlopMotion::kEaseStandard);
    connect(anim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) {
              m_opacity = v.toReal();
              update();
            });
    connect(anim, &QVariantAnimation::finished, this, &QWidget::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setOpacity(m_opacity);
    p.drawPixmap(rect(), m_snapshot);
  }

private:
  QPixmap m_snapshot;
  qreal m_opacity{1.0};
};

// Switches `stack` to `newIndex` with a snapshot crossfade of the outgoing
// page. No-op fade (hard switch) when the index doesn't change or the stack
// is hidden.
void crossfadeStackTo(QStackedWidget *stack, int newIndex) {
  if (!stack)
    return;
  const int oldIndex = stack->currentIndex();
  if (oldIndex == newIndex || !stack->isVisible()) {
    stack->setCurrentIndex(newIndex);
    return;
  }
  QPixmap snapshot;
  if (QWidget *out = stack->currentWidget())
    snapshot = out->grab();
  stack->setCurrentIndex(newIndex);
  if (!snapshot.isNull())
    new StackFadeOverlay(stack, snapshot);
}
} // namespace

void MainWindow::onModeChanged(int index) {
  BlopDiag::recordUiAction(
      QStringLiteral("onModeChanged:%1").arg(index));
  qInfo() << "MainWindow: onModeChanged index=" << index
          << "authLocked=" << m_authNavigationLocked
          << "sidebarOpen=" << m_isSidebarOpen
          << "studyContainer=" << (m_studyContainer != nullptr)
#ifdef Q_OS_ANDROID
          // m_studyQQuickView is declared only on Android; referencing it on
          // Windows/desktop breaks the build (undeclared identifier).
          << "studyQQuickView=" << (m_studyQQuickView != nullptr)
#endif
      ;
  const int mainStackIdx = (index <= 0) ? 0 : 1;

  // Linke Notizen-Sidebar schließen, wenn wir zu Study/Web wechseln — sonst zwei „Sidebars“.
  if (mainStackIdx != 0 && m_isSidebarOpen)
    animateSidebar(false);

#ifdef Q_OS_ANDROID
  if (mainStackIdx == 1) {
    const auto transientOverlays = findChildren<QWidget *>(
        QStringLiteral("AndroidTransientOverlay"), Qt::FindDirectChildrenOnly);
    for (QWidget *overlay : transientOverlays) {
      if (overlay && overlay->isVisible())
        overlay->close();
    }
  }
  syncAndroidHeaderGeometry(this);
  applyAndroidImmersiveUi();
  const bool showAndroidHeader = !m_authNavigationLocked;
  if (m_studyVBoxLayout) {
    // Study has its own QML top bar; keep web content flush to avoid double top bars.
    m_studyVBoxLayout->setContentsMargins(0, 0, 0, 0);
  }
  // Leaving Study: only disable input + clear focus. Do NOT hide() the
  // container -- the QStackedWidget swap below already paints the other
  // page over it. Hiding the QQuickWidget that hosts QtWebView's native
  // SurfaceView causes the surface to detach; on re-show the SurfaceView
  // re-attaches without a valid surface and paints black. v3.16.x users
  // saw exactly this regression. We track the deactivation timestamp so
  // re-entry can decide whether a surface refresh is needed.
  if (mainStackIdx == 0) {
    if (m_studyWindowContainer) {
      m_studyWindowContainer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
      m_studyWindowContainer->setEnabled(false);
      m_studyWindowContainer->clearFocus();
    }
    // v3.17.6: suspend the AndroidWebView poll timers while the Study tab is
    // hidden. We cannot hide the QQuickWidget itself (would detach the
    // SurfaceView and paint black on re-show), so the QML side reads a
    // tabActive property that we toggle here.
    if (m_studyQQuickView && m_studyQQuickView->rootObject()) {
      qInfo() << "MainWindow: BlopStudy setProperty tabActive=false";
      m_studyQQuickView->rootObject()->setProperty("tabActive", false);
    }
    m_lastStudyDeactivationMs = QDateTime::currentMSecsSinceEpoch();
  }
#endif
  if (m_mainContentStack) {
    // v3.18.2+: Android hard-switches the main stack (no grab() — SurfaceView
    // + performance). Desktop crossfades around native embeds the same way.
#ifdef Q_OS_ANDROID
    m_mainContentStack->setCurrentIndex(mainStackIdx);
#else
    crossfadeStackTo(m_mainContentStack, mainStackIdx);
#endif
#ifdef Q_OS_ANDROID
    if (m_androidHeader) {
      m_androidHeader->setVisible(showAndroidHeader);
      if (showAndroidHeader)
        m_androidHeader->raise();
    }
#endif
  }
#ifdef Q_OS_ANDROID
  if (mainStackIdx == 1 && m_studyWindowContainer && m_studyVBoxLayout) {
    m_studyWindowContainer->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_studyWindowContainer->setEnabled(true);
    // v3.17.6: re-enable the QML poll timers now that Study is visible again.
    if (m_studyQQuickView && m_studyQQuickView->rootObject()) {
      qInfo() << "MainWindow: BlopStudy setProperty tabActive=true";
      m_studyQQuickView->rootObject()->setProperty("tabActive", true);
    }
    if (m_studyVBoxLayout->indexOf(m_studyWindowContainer) < 0)
      m_studyVBoxLayout->addWidget(m_studyWindowContainer);
    // v3.18.7: simplified Study re-entry path. The v3.18.6 logic ran
    // refreshStudySurface() on every re-entry with awayMs > 1500,
    // which started webLoaderDeactivateTimer (50 ms) and tore down
    // the WebView that the new studyWebLoader.onLoaded handler had
    // just loaded — leaving the user staring at the QML root
    // Rectangle (#0B0B1A, ~black on phone) with no WebView attached.
    //
    // Now we rely on the QML side end-to-end:
    //   - studyWebLoader.onLoaded fires whenever the Loader actually
    //     instantiates the WebView (initial create OR tabLeaveUnload
    //     recreate) and triggers loadStudyEntryFresh deterministically.
    //   - ensureStudyLoaded() handles the "Loader is still alive but
    //     never got a URL" case by checking firstLoadDone and the
    //     current url. It is a no-op when the page is already loaded.
    if (m_studyQQuickView && m_studyQQuickView->rootObject()) {
      QObject *root = m_studyQQuickView->rootObject();
      qInfo() << "MainWindow: BlopStudy tab enter — ensureStudyLoaded()";
      QMetaObject::invokeMethod(root, "ensureStudyLoaded");
      if (m_authNavigationLocked) {
        QMetaObject::invokeMethod(
            root, "requestSurfaceActivation", Qt::QueuedConnection,
            Q_ARG(QVariant, QVariant(QStringLiteral("modeChanged"))));
      }
    }
    if (m_authNavigationLocked)
      setAndroidStudyBootOverlayVisible(true);
    else
      setAndroidStudyBootOverlayVisible(false);
    QTimer::singleShot(0, this, [this]() {
      if (!m_mainContentStack || m_mainContentStack->currentIndex() != 1)
        return;
      if (!m_studyWindowContainer || !m_studyVBoxLayout)
        return;
      syncAndroidHeaderGeometry(this);
      if (m_androidHeader)
        m_androidHeader->raise();
      if (m_androidStudyBootOverlay && m_androidStudyBootOverlay->isVisible())
        m_androidStudyBootOverlay->raise();
    });
  } else if (mainStackIdx == 0) {
    setAndroidStudyBootOverlayVisible(false);
  }
  if (m_mainContentStack) {
    if (QWidget *cur = m_mainContentStack->currentWidget()) {
#ifndef Q_OS_ANDROID
      cur->raise();
#else
      // Study page embeds a native SurfaceView; avoid reordering above the tab chrome.
      if (mainStackIdx == 0)
        cur->raise();
#endif
      cur->setEnabled(true);
#ifdef Q_OS_ANDROID
      // Study embeds a native SurfaceView; immediate focus here can re-enter GL paths.
      if (mainStackIdx != 1)
        cur->setFocus(Qt::OtherFocusReason);
#else
      cur->setFocus(Qt::OtherFocusReason);
#endif
    }
  }
  if (m_androidHeader) {
    m_androidHeader->setVisible(showAndroidHeader);
    if (showAndroidHeader)
      m_androidHeader->raise();
  }
  syncAndroidHeaderGeometry(this);
  // Study switches can transiently report stale availableGeometry on Android.
  // Re-sync shortly after layer reordering so the top tab row stays visible.
  QTimer::singleShot(80, this, [this]() {
    syncAndroidHeaderGeometry(this);
    applyAndroidImmersiveUi();
    if (m_androidHeader)
      m_androidHeader->setVisible(!m_authNavigationLocked);
    if (m_androidHeader && !m_authNavigationLocked)
      m_androidHeader->raise();
  });
#endif

#ifdef Q_OS_ANDROID
  applyAndroidTabStyles(index);
  const int deferredModeIndex = index;
  QTimer::singleShot(48, this, [this, deferredModeIndex]() {
    const int expectedStack = (deferredModeIndex <= 0) ? 0 : 1;
    if (!m_mainContentStack || m_mainContentStack->currentIndex() != expectedStack) {
      return;
    }
    if (deferredModeIndex >= 2 && m_modeSelector &&
        m_modeSelector->currentIndex() != deferredModeIndex) {
      return;
    }
    if (deferredModeIndex != 1 && deferredModeIndex < 2)
      return;
    if (!m_studyQQuickView || !m_studyQQuickView->rootObject()) {
      qWarning() << "deferred invokeAndroidWebDestination: study QML not ready, mode index"
                 << deferredModeIndex;
      return;
    }
    if (deferredModeIndex == 1)
      invokeAndroidWebDestination(0);
    else if (deferredModeIndex >= 2)
      invokeAndroidWebDestination(1, m_modeSelector
                                         ? m_modeSelector->itemData(deferredModeIndex, Qt::UserRole)
                                               .toUrl()
                                               .toString()
                                         : QString());
    if (deferredModeIndex >= 1) {
      QTimer::singleShot(180, this, [this, deferredModeIndex]() {
        const int expectedStackNow = (deferredModeIndex <= 0) ? 0 : 1;
        if (!m_mainContentStack || m_mainContentStack->currentIndex() != expectedStackNow)
          return;
        scheduleAndroidStudyWebViewNetworkCache();
      });
    }
  });
#endif

#ifndef Q_OS_ANDROID
  if (mainStackIdx == 1)
    applyDesktopWebSubviewForModeIndex(index);
  else
    applyDesktopWebSubviewForModeIndex(0);
  if (m_titleBarWidget) {
    m_titleBarWidget->raise();
    QTimer::singleShot(0, this, [this]() {
      if (m_titleBarWidget)
        m_titleBarWidget->raise();
    });
    QTimer::singleShot(100, this, [this]() {
      if (m_titleBarWidget)
        m_titleBarWidget->raise();
    });
  }
#endif

  // Ensure toolbar/mode selector visibility is correct for the selected mode.
  // Without this, the UI can remain in an "editor" state while the Study
  // WebView is shown, which may block switching back.
  updateSidebarState();
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
    tb->requestAdaptiveReflow();
    QTimer::singleShot(0, this, [this]() {
      if (auto *tbNow = qobject_cast<ModernToolbar *>(m_floatingTools))
        tbNow->requestAdaptiveReflow();
    });
  }
  // Phone toolbar reflows via parent resize event - nudge it the same way
  // ModernToolbar gets nudged on mode switches so geometry stays correct
  // immediately and on the next event loop tick.
  if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools)) {
    if (m_editorCenterWidget) {
      const int h = phone->preferredHeightPx();
      const int margin = UiScale::dp(8);
      const int avail =
          qMax(UiScale::dp(180), m_editorCenterWidget->width());
      const int w = qMin(avail - 2 * margin, UiScale::dp(360));
      const int y = qMax(0, m_editorCenterWidget->height() - h -
                                UiScale::safeBottomPx(this) -
                                UiScale::dp(8));
      phone->setGeometry((avail - w) / 2, y, w, h);
      phone->raise();
    }
  }
}

void MainWindow::requestGoogleLogin() {
    if (m_googleLoginInFlight) {
      const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
      // Only auto-recover after a very long hang (browser never returned).
      if (m_googleLoginInFlightSinceMs > 0 &&
          (nowMs - m_googleLoginInFlightSinceMs) > 10 * 60 * 1000) {
        qWarning() << "Google login in-flight guard was stale, unlocking retry";
        m_googleLoginInFlight = false;
        m_googleLoginInFlightSinceMs = 0;
#ifdef Q_OS_ANDROID
        GoogleAuthManager::instance().cancelPendingLogin();
#endif
        const QString user =
            QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
                .value(QStringLiteral("username"))
                .toString()
                .trimmed();
        m_authNavigationLocked = user.isEmpty();
      } else {
        qInfo() << "Google login already in flight, ignoring duplicate request";
        return;
      }
    }
    // Defer OAuth start out of WebView/JS callback stack to reduce Android reentrancy crashes.
    QTimer::singleShot(0, this, [this]() {
      if (m_googleLoginInFlight)
        return;
#ifdef Q_OS_ANDROID
      if (!QSslSocket::supportsSsl()) {
        qCritical() << "Google login start skipped: TLS unavailable";
        emit oauthFailed(QStringLiteral("tls_unavailable"));
        const QString user =
            QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
                .value(QStringLiteral("username"))
                .toString()
                .trimmed();
        m_authNavigationLocked = user.isEmpty();
        return;
      }
#endif
      m_googleLoginInFlight = true;
      m_googleLoginInFlightSinceMs = QDateTime::currentMSecsSinceEpoch();
      // Keep Study/login gate for guests; do not flip chrome mid-OAuth.
      // Unlock-on-resume / authenticationFailed handles abandoned flows.
      GoogleAuthManager::instance().login();
    });
}

void MainWindow::resetAndroidWebViewStorage() {
#ifdef Q_OS_ANDROID
  qInfo() << "Android WebView light reset requested from QML bridge";
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid()) {
      qWarning() << "Android WebView reset skipped: invalid activity";
      return;
    }

    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopWebViewReset", "clearWebViewStateLight",
        "(Landroid/app/Activity;)V", activity.object<jobject>());
    qInfo() << "Android WebView light reset JNI call dispatched";
  });
#endif
}

void MainWindow::resetAndroidWebViewStorageFull() {
#ifdef Q_OS_ANDROID
  qInfo() << "Android WebView full reset requested from QML bridge";
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid()) {
      qWarning() << "Android WebView full reset skipped: invalid activity";
      return;
    }
    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopWebViewReset", "clearWebViewState",
        "(Landroid/app/Activity;)V", activity.object<jobject>());
    qInfo() << "Android WebView full reset JNI call dispatched";
  });
#endif
}

void MainWindow::nudgeAndroidWebViewStopOnly() {
#ifdef Q_OS_ANDROID
  qInfo() << "Android WebView stop-only requested from QML bridge";
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid()) {
      qWarning() << "Android WebView stop-only skipped: invalid activity";
      return;
    }
    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopWebViewReset", "stopLoadingOnly",
        "(Landroid/app/Activity;)V", activity.object<jobject>());
    qInfo() << "Android WebView stop-only JNI call dispatched";
  });
#endif
}

void MainWindow::applyAndroidStudyWebViewNetworkCache() {
#ifdef Q_OS_ANDROID
  qInfo() << "Android Study WebView: apply cache settings (JNI)";
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid()) {
      qWarning() << "applyAndroidStudyWebViewNetworkCache skipped: invalid activity";
      return;
    }
    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopWebViewReset", "applyStudyWebViewNetworkCacheMode",
        "(Landroid/app/Activity;)V", activity.object<jobject>());
  });
#endif
}

void MainWindow::scheduleAndroidStudyWebViewNetworkCache() {
#ifdef Q_OS_ANDROID
  qInfo() << "Android Study WebView: schedule LOAD_NO_CACHE retries (JNI)";
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid()) {
      qWarning() << "scheduleAndroidStudyWebViewNetworkCache skipped: invalid activity";
      return;
    }
    // ~5.6s window at 200ms steps (late WebView attach on Play/split installs).
    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopWebViewReset", "scheduleApplyStudyWebViewNetworkCacheMode",
        "(Landroid/app/Activity;IJ)V", activity.object<jobject>(),
        static_cast<jint>(40), static_cast<jlong>(250));
  });
#endif
}

void MainWindow::switchToNotesFromWebQmlBar() {
#ifdef Q_OS_ANDROID
  if (m_authNavigationLocked) {
    qInfo() << "Auth gate: switchToNotesFromWebQmlBar ignored";
    return;
  }
  if (m_modeSelector) {
    QSignalBlocker b(m_modeSelector);
    m_modeSelector->setCurrentIndex(0);
  }
  onModeChanged(0);
#endif
}

void MainWindow::openWebBookmarkMenuFromWebQmlBar() {
#ifdef Q_OS_ANDROID
  if (!m_studyQQuickView)
    return;
  QObject *root = m_studyQQuickView->rootObject();
  if (!root)
    return;
  const bool ok = QMetaObject::invokeMethod(root, "openBookmarkSheet",
                                             Qt::QueuedConnection);
  if (!ok)
    qWarning() << "QML openBookmarkSheet invoke failed";
#endif
}

QVariantList MainWindow::webBookmarksForQml() const {
  QVariantList out;
  out.reserve(m_webBookmarks.size());
  for (const WebBookmark &bm : m_webBookmarks) {
    QVariantMap row;
    row.insert(QStringLiteral("title"), bm.title);
    row.insert(QStringLiteral("url"), bm.url.toString());
    out.push_back(row);
  }
  return out;
}

bool MainWindow::addWebBookmarkFromQml(const QString &urlInput,
                                       const QString &titleInput) {
  if (!m_modeSelector)
    return false;
  const QUrl url = normalizedUserWebUrl(urlInput);
  if (!url.isValid())
    return false;
  QString title = titleInput.trimmed();
  if (title.isEmpty())
    title = url.host().isEmpty() ? url.toDisplayString() : url.host();
  m_webBookmarks.push_back({title, url});
  saveWebBookmarksToSettings();
  rebuildModeSelectorItems();
  const int newIdx = m_modeSelector->count() - 1;
#ifdef Q_OS_ANDROID
  QSignalBlocker b(m_modeSelector);
  m_modeSelector->setCurrentIndex(newIdx);
  onModeChanged(newIdx);
#else
  m_modeSelector->setCurrentIndex(newIdx);
#endif
  return true;
}

bool MainWindow::removeWebBookmarkFromQml(int index) {
  if (index < 0 || index >= m_webBookmarks.size())
    return false;
  m_webBookmarks.removeAt(index);
  saveWebBookmarksToSettings();
  rebuildModeSelectorItems();
  const int cur = m_modeSelector ? m_modeSelector->currentIndex() : 0;
  onModeChanged(cur);
  return true;
}

void MainWindow::openWebBookmarkFromQml(int index) {
  if (!m_modeSelector)
    return;
  if (index < 0 || index >= m_webBookmarks.size())
    return;
  const int modeIdx = index + 2;
  if (modeIdx >= m_modeSelector->count())
    return;
#ifdef Q_OS_ANDROID
  QSignalBlocker b(m_modeSelector);
  m_modeSelector->setCurrentIndex(modeIdx);
  onModeChanged(modeIdx);
#else
  m_modeSelector->setCurrentIndex(modeIdx);
#endif
}

void MainWindow::onSessionCheck(const QString &sessionData) {
    if (!sessionData.isEmpty() && sessionData != "null") {
        m_googleLoginInFlight = false;
        m_googleLoginInFlightSinceMs = 0;
        QString currentUser = QSettings("Blop", "BlopApp").value("username").toString();
        if (sessionData != currentUser) {
            updateSidebarUser(sessionData);
        }
    }
}

void MainWindow::updateSidebarUser(const QString &username) {
  // Update avatar initial letter
  QString initial = username.isEmpty() ? "G" : username.left(1).toUpper();
  if (m_lblSidebarAvatar)
    m_lblSidebarAvatar->setText(initial);
  // Update username text
  if (m_lblSidebarUser)
    m_lblSidebarUser->setText(username.isEmpty() ? "Gast" : username);

  // Persist for next app launch
  QSettings("Blop", "BlopApp").setValue("username", username);

  // --- Auth Check Unlock ---
  bool wasLocked = (m_topNavControls && m_topNavControls->isHidden());

  if (!username.isEmpty()) {
    m_authNavigationLocked = false;
    // Logged in: Switch to Blop Notes mode
    if (m_topNavControls) m_topNavControls->show();
    
    if (m_modeSelector) {
#ifdef Q_OS_ANDROID
      QSignalBlocker b(m_modeSelector);
#endif
      m_modeSelector->setCurrentIndex(0); // Switch to Notes mode
    }
#ifdef Q_OS_ANDROID
    if (m_androidHeader)
      m_androidHeader->setVisible(true);
    onModeChanged(0);
    if (m_btnAndroidNotes) {
      m_btnAndroidNotes->setVisible(true);
      m_btnAndroidNotes->setEnabled(true);
    }
    if (m_btnAndroidStudy) {
      m_btnAndroidStudy->setVisible(true);
      m_btnAndroidStudy->setEnabled(true);
    }
    if (m_btnAndroidAddWebBookmark) {
      m_btnAndroidAddWebBookmark->setVisible(true);
      m_btnAndroidAddWebBookmark->setEnabled(true);
    }
#endif
    if (btnStripMenu)
      btnStripMenu->show();
    if (btnEditorMenu)
      btnEditorMenu->hide(); // Mode-specific logic in updateSidebarState decides visibility

    // Ensure sidebar is closed (not double-visible)
    if (m_isSidebarOpen) {
      onToggleSidebar();
    }
  } else {
    m_authNavigationLocked = true;
    // Logged out: Switch back to Study/Login web view
    if (m_topNavControls) m_topNavControls->hide();

    if (m_modeSelector) {
#ifdef Q_OS_ANDROID
      QSignalBlocker b(m_modeSelector);
#endif
      m_modeSelector->setCurrentIndex(1); // Force back to web login
#ifndef Q_OS_ANDROID
      m_modeSelector->hide(); // Desktop: hide selector on auth screen
#endif
    }
#ifdef Q_OS_ANDROID
    if (m_androidHeader) {
      m_androidHeader->setVisible(false);
      m_androidHeader->setFixedHeight(0);
    }
    syncAndroidHeaderGeometry(this);
    onModeChanged(1);
    // Keep login screen clean: hide Notes/Study pills until session is confirmed.
    if (m_btnAndroidNotes) {
      m_btnAndroidNotes->setVisible(false);
      m_btnAndroidNotes->setEnabled(false);
    }
    if (m_btnAndroidStudy) {
      m_btnAndroidStudy->setVisible(false);
      m_btnAndroidStudy->setEnabled(false);
    }
    // Web/bookmark actions stay hidden until we have a confirmed web session.
    if (m_btnAndroidAddWebBookmark) {
      m_btnAndroidAddWebBookmark->setVisible(false);
      m_btnAndroidAddWebBookmark->setEnabled(false);
    }
#endif
    if (btnStripMenu)
      btnStripMenu->hide(); // Hide the sidebar hamburger when logged out to fully trap user in login
    if (btnEditorMenu)
      btnEditorMenu->hide(); // Hide the Android Header menu when logged out

    if (m_isSidebarOpen)
      onToggleSidebar();
  }
  
  // Re-sync sidebar strip/full sidebar visibility after mode switch
  updateSidebarState();
}

int MainWindow::effectiveSidebarWidthPx() const {
#ifdef Q_OS_ANDROID
  const int basis = SIDEBAR_WIDTH;
  const int maxByScreen = qMax(120, int(width() * 0.86));
  return qMin(basis, maxByScreen);
#else
  return SIDEBAR_WIDTH;
#endif
}

QRect MainWindow::sidebarPushContentRect() const {
  if (!m_centralContainer)
    return QRect(0, 0, effectiveSidebarWidthPx(), height());
  const QPoint tl = m_centralContainer->mapTo(const_cast<MainWindow *>(this),
                                               QPoint(0, 0));
  int y = tl.y();
  int h = m_centralContainer->height();
#ifdef Q_OS_ANDROID
  // v3.18.7: start the drawer BELOW the chrome (status-bar inset + chip row).
  // The drawer is a sibling of m_centralContainer at the MainWindow level,
  // so it paints OVER the chrome regardless of any QWidget::raise() trick.
  // Insetting its geometry by the chrome height is the only reliable way
  // to keep the chrome (notes/study chips + home button) visible — and it
  // also removes the white logo-JPG artifact that used to be painted on
  // top of the chrome's status-bar inset region when the drawer opened.
  if (m_androidHeader && m_androidHeader->isVisible()) {
    const int chromeH = m_androidHeader->height();
    y += chromeH;
    h -= chromeH;
  }
#else
  if (m_titleBarWidget && m_titleBarWidget->isVisible()) {
    const int th = m_titleBarWidget->height();
    y = m_centralContainer->mapTo(const_cast<MainWindow *>(this), QPoint(0, th)).y();
    h -= th;
  }
#endif
  return QRect(tl.x(), y, effectiveSidebarWidthPx(), h);
}

void MainWindow::syncSidebarPushLayout() {
  if (!m_centralContainer)
    return;
  // Don't fight an in-flight fold/unfold animation.
  if (m_sidebarAnimating)
    return;
  QRect r = sidebarPushContentRect();
  int h = r.height();
  if (h <= 0)
    h = qMax(100, height() - r.y());

  if (!m_sidebarContainer || !m_sidebarContainer->isVisible()) {
#ifdef Q_OS_ANDROID
    if (QVBoxLayout *mainLayout =
            qobject_cast<QVBoxLayout *>(m_centralContainer->layout())) {
      // Android: page-specific layouts own safe-area insets (no global double inset).
      mainLayout->setContentsMargins(0, 0, 0, 0);
    }
#else
    if (m_desktopSidebarPushSpacer)
      m_desktopSidebarPushSpacer->setFixedWidth(0);
#endif
    return;
  }
  const int w = effectiveSidebarWidthPx();
#ifdef Q_OS_ANDROID
  if (QVBoxLayout *mainLayout =
          qobject_cast<QVBoxLayout *>(m_centralContainer->layout())) {
    // Overlay drawer: no horizontal push on Android; insets handled by active page.
    mainLayout->setContentsMargins(0, 0, 0, 0);
  }
#else
  if (m_desktopSidebarPushSpacer)
    m_desktopSidebarPushSpacer->setFixedWidth(w);
#endif
  m_sidebarContainer->setMaximumWidth(w);
  m_sidebarContainer->setGeometry(r.x(), r.y(), w, h);
  m_sidebarContainer->raise();
}

void MainWindow::setupSidebar() {
  m_sidebarStrip = new QWidget(this);
  m_sidebarStrip->setFixedWidth(60);
  m_sidebarStrip->hide();
  QVBoxLayout *stripLayout = new QVBoxLayout(m_sidebarStrip);
  stripLayout->setContentsMargins(0, 10, 0, 0);
  stripLayout->setSpacing(20);
  btnStripMenu = new ModernButton(m_sidebarStrip);
  btnStripMenu->setIcon(createModernIcon("menu", Qt::white));
  btnStripMenu->setFixedSize(40, 40);
  connect(btnStripMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  stripLayout->addWidget(btnStripMenu, 0, Qt::AlignHCenter);
  stripLayout->addStretch();

#ifndef Q_OS_ANDROID
  m_mainSplitter->addWidget(m_sidebarStrip);
#endif

  m_sidebarContainer = new QWidget(this);
  m_sidebarContainer->setMinimumWidth(0);
  m_sidebarContainer->setMinimumSize(0, 0);
  m_sidebarContainer->setMaximumWidth(SIDEBAR_WIDTH);
  m_sidebarContainer->setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
#ifdef Q_OS_ANDROID
  // On Android, give the sidebar a solid background and enable styled background
  // so touch events are fully absorbed and don't pass through to the main content.
  m_sidebarContainer->setAttribute(Qt::WA_StyledBackground, true);
  m_sidebarContainer->setStyleSheet(
      BlopTheme::themed("background-color: #0F111A;"));
#endif

  QVBoxLayout *layout = new QVBoxLayout(m_sidebarContainer);
  layout->setSizeConstraint(QLayout::SetNoConstraint);
#ifdef Q_OS_ANDROID
  layout->setContentsMargins(UiScale::dp(6), 0, UiScale::dp(6), 0);
#else
  layout->setContentsMargins(4, 0, 4, 0);
#endif

  layout->setSpacing(0);

  // --- HEADER: compact brand row (document-app chrome) ---
  QWidget *header = new QWidget(m_sidebarContainer);
#ifdef Q_OS_ANDROID
  header->setFixedHeight(UiScale::dp(44));
#else
  header->setFixedHeight(44);
#endif
#ifdef Q_OS_ANDROID
  header->setStyleSheet("border-bottom: none; background: transparent;");
#else
  header->setStyleSheet(BlopTheme::themed(
      "border-bottom: 1px solid rgba(120,130,160,0.16);"));
#endif
  QHBoxLayout *headerLay = new QHBoxLayout(header);
#ifdef Q_OS_ANDROID
  headerLay->setContentsMargins(UiScale::dp(12), UiScale::dp(8), UiScale::dp(10),
                                UiScale::dp(8));
  headerLay->setSpacing(UiScale::dp(8));
#else
  headerLay->setContentsMargins(12, 6, 10, 6);
  headerLay->setSpacing(8);
#endif

  QLabel *lblLogo = new QLabel(header);
#ifdef Q_OS_ANDROID
  lblLogo->setFixedSize(UiScale::dp(28), UiScale::dp(28));
#else
  lblLogo->setFixedSize(28, 28);
#endif
  lblLogo->setAlignment(Qt::AlignCenter);
  QPixmap sidebarLogo(":/assets/logo.jpg");
  if (!sidebarLogo.isNull()) {
    lblLogo->setPixmap(
        sidebarLogo.scaled(lblLogo->size(), Qt::KeepAspectRatioByExpanding,
                           Qt::SmoothTransformation));
    lblLogo->setStyleSheet("border-radius: 8px; border: none;");
  } else {
    lblLogo->setStyleSheet(
        "background-color: rgba(124,92,252,0.35); border-radius: 8px; color: white; "
        "font-weight: 700; font-size: 12px;");
    lblLogo->setText("B");
  }
  headerLay->addWidget(lblLogo);

  QLabel *lblTitle = new QLabel("Blop", header);
#ifdef Q_OS_ANDROID
  lblTitle->setStyleSheet(BlopTheme::themed(
      "font-size: 15px; font-weight: 700; color: #F4F2FF; "
      "background: transparent; border: none; letter-spacing: 0.2px;"));
#else
  lblTitle->setStyleSheet(BlopTheme::themed(
      "font-size: 15px; font-weight: 700; color: #F4F2FF; "
      "background: transparent; border: none; letter-spacing: 0.2px;"));
#endif
  headerLay->addWidget(lblTitle);
  headerLay->addStretch();

  m_closeSidebarBtn = new QPushButton("«", header);
  m_closeSidebarBtn->setFixedSize(24, 24);
  m_closeSidebarBtn->setFocusPolicy(Qt::NoFocus);
  m_closeSidebarBtn->setCursor(Qt::PointingHandCursor);
  m_closeSidebarBtn->setStyleSheet(BlopTheme::themed(
      "QPushButton { background: transparent; color: #888; border: none; "
      "font-size: 16px; outline: none; } QPushButton:hover { color: #F4F2FF; "
      "background: rgba(255,255,255,0.10); outline: none; "
      "border-radius: 6px; }"));
  connect(m_closeSidebarBtn, &QPushButton::clicked, this,
          &MainWindow::onToggleSidebar);
  headerLay->addWidget(m_closeSidebarBtn);
  layout->addWidget(header);

  m_navSidebar = new QListWidget(m_sidebarContainer);
  m_navSidebar->setItemDelegate(new SidebarNavDelegate(this));
  m_navSidebar->setFrameShape(QFrame::NoFrame);
  m_navSidebar->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_navSidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navSidebar->setTextElideMode(Qt::ElideRight);
  m_navSidebar->setSpacing(1);

  // On Android: scroll only vertically (no horizontal pan); smooth touch scroll on viewport.
#ifdef Q_OS_ANDROID
  if (m_navSidebar->viewport())
    m_navSidebar->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
  QScroller::grabGesture(m_navSidebar->viewport(), QScroller::TouchGesture);
  if (QScroller *navScroller = QScroller::scroller(m_navSidebar->viewport())) {
    QScrollerProperties sp = navScroller->scrollerProperties();
    // Prefer vertical drags; no horizontal rubber-band (list is not horizontally scrollable).
    sp.setScrollMetric(QScrollerProperties::AxisLockThreshold, QVariant(0.55));
    sp.setScrollMetric(
        QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(
        QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue(QScrollerProperties::OvershootWhenScrollable));
    sp.setScrollMetric(QScrollerProperties::DecelerationFactor, QVariant(0.14));
    navScroller->setScrollerProperties(sp);
  }
#else
  QScroller::grabGesture(m_navSidebar, QScroller::LeftMouseButtonGesture);
#endif

  QDir rootDir(m_rootPath);
  int rootCount =
      rootDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)
          .count();
  QString rootCountStr = QString::number(rootCount);
  auto addItem = [this, rootCountStr](QString name, QString icon,
                                      bool isHeader = false) {
    QListWidgetItem *item = new QListWidgetItem(m_navSidebar);
    item->setText(name);
    item->setData(Qt::UserRole + 9, 0);
    if (!isHeader) {
      item->setIcon(createModernIcon(icon, BlopTheme::textSecondary()));
      // v3.18.5: cache icon key so applyThemeRefresh() can re-tint it
      // when the user toggles Light/Dark mode without rebuilding the
      // whole sidebar.
      item->setData(Qt::UserRole + 11, icon);
      if (name == QStringLiteral("Alle") ||
          name == QStringLiteral("Blop Notizen")) {
        item->setData(Qt::UserRole + 2, rootCountStr);
        item->setData(Qt::UserRole + 10, m_rootPath);
      }
    }
    item->setData(Qt::UserRole + 1, isHeader);
    if (isHeader) {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    } else {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      if (name == QStringLiteral("Blop Notizen") ||
          name == QStringLiteral("Alle")) {
        item->setData(Qt::UserRole + 6, true);
      }
    }
  };

  addItem(QStringLiteral("Alle"), QStringLiteral("folder"));
  addItem(QStringLiteral("Blop Notizen"), QStringLiteral("folder"));
  addItem(QStringLiteral("Gerät"), QStringLiteral("device"));

  // Cloud sync folders (Google Drive / Nextcloud / … / custom).
  {
    auto *cloudsHeader = new QListWidgetItem(m_navSidebar);
    cloudsHeader->setText(QStringLiteral("Cloud-Speicher"));
    cloudsHeader->setData(Qt::UserRole + 1, true); // header
    cloudsHeader->setData(Qt::UserRole + 4, QStringLiteral("clouds_header"));
    cloudsHeader->setFlags(Qt::ItemIsEnabled);

    const QVector<CloudStorageEntry> clouds = CloudStorageStore::load();
    for (const CloudStorageEntry &e : clouds) {
      auto *item = new QListWidgetItem(m_navSidebar);
      item->setText(e.name);
      item->setIcon(createModernIcon(CloudStorageStore::iconForType(e.type),
                                     BlopTheme::textSecondary()));
      item->setData(Qt::UserRole + 11, CloudStorageStore::iconForType(e.type));
      item->setData(Qt::UserRole + 5, QStringLiteral("clouds_item"));
      item->setData(Qt::UserRole + 12, e.id);
      item->setData(Qt::UserRole + 13, e.type);
      if (!e.path.isEmpty() && QDir(e.path).exists())
        item->setData(Qt::UserRole + 10, e.path);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      const QString tip = e.path.isEmpty()
                              ? QStringLiteral("Tippen zum Verknüpfen eines Sync-Ordners")
                              : e.path;
      item->setToolTip(tip);
    }

    auto *addCloud = new QListWidgetItem(m_navSidebar);
    addCloud->setText(QStringLiteral("Eigene Cloud hinzufügen…"));
    addCloud->setIcon(
        createModernIcon(QStringLiteral("cloud"), BlopTheme::textSecondary()));
    addCloud->setData(Qt::UserRole + 11, QStringLiteral("cloud"));
    addCloud->setData(Qt::UserRole + 5, QStringLiteral("clouds_add"));
    addCloud->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  }

  m_navSidebar->setCurrentRow(1);
  connect(m_navSidebar, &QListWidget::itemClicked, this,
          &MainWindow::onNavItemClicked);
  layout->addWidget(m_navSidebar, 1);
  updateSidebarBadges();

  // Tags — Drawboard-style collapsible section in the left nav.
  m_libraryTagsPanel = new LibraryTagsPanel(m_sidebarContainer);
  m_libraryTagsPanel->setSidebarMode(true);
  m_libraryTagsPanel->setAccentColor(m_currentAccentColor);
  connect(m_libraryTagsPanel, &LibraryTagsPanel::filterChanged, this,
          [this](const QStringList &) { applyLibraryFilters(); });
  connect(m_libraryTagsPanel, &LibraryTagsPanel::catalogChanged, this,
          [this]() {
            applyLibraryFilters();
            rebuildPageSettingsTags();
          });
  layout->addWidget(m_libraryTagsPanel, 0);

  // --- BOTTOM: quiet account + settings row ---
  QWidget *bottomBar = new QWidget(m_sidebarContainer);
  bottomBar->setObjectName("BottomBar");
#ifdef Q_OS_ANDROID
  bottomBar->setStyleSheet("QWidget#BottomBar { border-top: none; background: transparent; }");
#else
  bottomBar->setStyleSheet(BlopTheme::themed(
      "QWidget#BottomBar { border-top: 1px solid rgba(120,130,160,0.16); }"));
#endif

#ifdef Q_OS_ANDROID
  bottomBar->setFixedHeight(52);
#else
  bottomBar->setFixedHeight(52);
#endif

  QHBoxLayout *bottomLay = new QHBoxLayout(bottomBar);
#ifdef Q_OS_ANDROID
  bottomLay->setContentsMargins(10, 8, 10, 8);
  bottomLay->setSpacing(8);
#else
  bottomLay->setContentsMargins(12, 8, 12, 8);
  bottomLay->setSpacing(8);
#endif

  QString username =
      QSettings("Blop", "BlopApp").value("username", "Gast").toString();
  QString initial = username.isEmpty() ? "G" : username.left(1).toUpper();

  m_lblSidebarAvatar = new QLabel(initial, bottomBar);
#ifdef Q_OS_ANDROID
  m_lblSidebarAvatar->setFixedSize(30, 30);
#else
  m_lblSidebarAvatar->setFixedSize(30, 30);
#endif
  m_lblSidebarAvatar->setStyleSheet(
      "background: rgba(124,92,252,0.28);"
      "border-radius: 10px; color: white; font-weight: 700; font-size: 12px;");
  m_lblSidebarAvatar->setAlignment(Qt::AlignCenter);
  bottomLay->addWidget(m_lblSidebarAvatar);

  QVBoxLayout *userCol = new QVBoxLayout();
  userCol->setSpacing(1);
  m_lblSidebarUser =
      new QLabel(username.isEmpty() ? "Gast" : username, bottomBar);
#ifdef Q_OS_ANDROID
  m_lblSidebarUser->setStyleSheet(BlopTheme::themed(
      "font-size: 12px; font-weight: 600; color: #F4F2FF; "
      "background: transparent;"));
  m_lblSidebarUser->setMaximumWidth(118);
#else
  m_lblSidebarUser->setStyleSheet(BlopTheme::themed(
      "font-size: 12px; font-weight: 600; color: #F4F2FF; "
      "background: transparent;"));
  m_lblSidebarUser->setMaximumWidth(130);
#endif
  m_lblSidebarUser->setWordWrap(false);
  userCol->addWidget(m_lblSidebarUser);

  m_btnSidebarSettings = new QPushButton("Einstellungen", bottomBar);
  m_btnSidebarSettings->setFocusPolicy(Qt::NoFocus);
  m_btnSidebarSettings->setCursor(Qt::PointingHandCursor);
  m_btnSidebarSettings->setStyleSheet(BlopTheme::themed(
      "QPushButton { background: transparent; color: rgba(180,188,215,0.70); border: none; "
      "font-size: 11px; padding: 0; text-align: left; } "
      "QPushButton:hover { color: #C4B5FF; }"));
  connect(m_btnSidebarSettings, &QPushButton::clicked, this,
          &MainWindow::onOpenSettings);
  userCol->addWidget(m_btnSidebarSettings);

  bottomLay->addLayout(userCol);
  bottomLay->addStretch();

  layout->addWidget(bottomBar);

  m_sidebarContainer->setParent(this);
  {
    QRect r = sidebarPushContentRect();
    int h = r.height();
    if (h <= 0)
      h = qMax(100, height() - r.y());
    m_sidebarContainer->setGeometry(r.x(), r.y(), 0, h);
  }
  m_sidebarContainer->hide();

#ifdef Q_OS_ANDROID
  m_androidSidebarScrim = new QWidget(this);
  m_androidSidebarScrim->setObjectName(QStringLiteral("AndroidSidebarScrim"));
  m_androidSidebarScrim->setStyleSheet(
      QStringLiteral("background-color: rgba(0,0,0,0.45);"));
  m_androidSidebarScrim->hide();
  m_androidSidebarScrim->installEventFilter(this);
#endif
}

#ifdef Q_OS_ANDROID
void MainWindow::updateAndroidSidebarScrimGeometry() {
  if (!m_androidSidebarScrim || !m_centralContainer)
    return;
  QRect hostRect = m_centralContainer->geometry();
  const int topInset = UiScale::safeTopPx(this);
  const int bottomInset = UiScale::safeBottomPx(this);
  // v3.18.7: scrim must also clear the full chrome (status-bar inset
  // + chip row), not just the inset — otherwise it dims the chrome
  // and intercepts touches that should hit the home button / chips.
  const int chromeH = (m_androidHeader && m_androidHeader->isVisible())
                          ? m_androidHeader->height()
                          : topInset;
  const int topOffset = qMax(chromeH, topInset);
  hostRect.adjust(0, topOffset, 0, -bottomInset);
  m_androidSidebarScrim->setGeometry(hostRect);
}
#endif

void MainWindow::setLibraryRootFromSource(const QModelIndex &sourceIndex) {
  if (!m_fileListView || !m_fileModel)
    return;
  if (!sourceIndex.isValid())
    return;
  // QFileSystemModel often reports rowCount==0 until the directory is fetched.
  if (m_fileModel->canFetchMore(sourceIndex))
    m_fileModel->fetchMore(sourceIndex);
  if (m_libraryProxy) {
    if (m_fileListView->model() != m_libraryProxy)
      m_fileListView->setModel(m_libraryProxy);
    QModelIndex proxyRoot = m_libraryProxy->mapFromSource(sourceIndex);
    if (!proxyRoot.isValid()) {
      // Proxy may lag one filter refresh behind; retry once.
      m_libraryProxy->invalidate();
      proxyRoot = m_libraryProxy->mapFromSource(sourceIndex);
    }
    if (proxyRoot.isValid()) {
      m_fileListView->setRootIndex(proxyRoot);
      if (m_libraryProxy->canFetchMore(proxyRoot))
        m_libraryProxy->fetchMore(proxyRoot);
    } else
      qWarning() << "setLibraryRootFromSource: invalid proxy root for"
                 << m_fileModel->filePath(sourceIndex);
  } else {
    m_fileListView->setRootIndex(sourceIndex);
  }
  if (m_fileListView) {
    m_fileListView->show();
    m_fileListView->viewport()->update();
  }
  if (m_lblEmptyState)
    m_lblEmptyState->hide();
}

void MainWindow::navigateLibraryToPath(const QString &path) {
  if (!m_fileModel || !m_fileListView || path.isEmpty())
    return;
  const QFileInfo fi(path);
  if (!fi.isDir())
    return;
  // Opening a folder should always reveal its contents — Favorites/Recent/
  // Untagged filters would otherwise look like "navigation is broken".
  if (m_libraryOrgBar &&
      m_libraryOrgBar->smartView() != LibraryOrgBar::SmartView::All)
    m_libraryOrgBar->setSmartView(LibraryOrgBar::SmartView::All);
  const QString abs = fi.absoluteFilePath();
  m_pendingLibraryRootPath = abs;
  const QModelIndex fromSet = m_fileModel->setRootPath(abs);
  const QModelIndex idx =
      fromSet.isValid() ? fromSet : m_fileModel->index(abs);
  setLibraryRootFromSource(idx);
  updateOverviewBackButton();
  updateLibraryHeader();
  updateSidebarBadges();
  updateGrid();
}

void MainWindow::updateLibraryHeader() {
  if (!m_fileModel)
    return;
  QString folderPath = m_fileModel->rootPath();
  if (m_fileListView && m_libraryProxy) {
    const QModelIndex proxyRoot = m_fileListView->rootIndex();
    if (proxyRoot.isValid()) {
      const QModelIndex src = m_libraryProxy->mapToSource(proxyRoot);
      if (src.isValid())
        folderPath = m_fileModel->filePath(src);
    }
  }
  const QFileInfo fi(folderPath);
  const bool atRoot =
      QFileInfo(folderPath).canonicalFilePath() ==
          QFileInfo(m_rootPath).canonicalFilePath() ||
      folderPath == m_rootPath;
  if (m_lblLibraryTitle) {
    m_lblLibraryTitle->setText(atRoot ? QStringLiteral("Notizen")
                                      : fi.fileName());
  }
  if (m_lblLibrarySubtitle) {
    QDir dir(folderPath);
    const int count =
        dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).count();
    if (atRoot) {
      m_lblLibrarySubtitle->setText(
          QStringLiteral("Bibliothek · %1 Einträge").arg(count));
    } else {
      const QString rel = QDir(m_rootPath).relativeFilePath(folderPath);
      m_lblLibrarySubtitle->setText(
          QStringLiteral("%1 · %2 Einträge")
              .arg(rel.isEmpty() ? folderPath : rel)
              .arg(count));
    }
  }
}

void MainWindow::applyLibraryFilters() {
  // LibraryFilterProxy lives in an anonymous namespace (no Q_OBJECT), so
  // qobject_cast is illegal — the pointer is always our typed instance.
  auto *proxy = static_cast<LibraryFilterProxy *>(m_libraryProxy);
  if (!proxy)
    return;
  const QString search =
      m_overviewSearchBar ? m_overviewSearchBar->text() : QString();
  const QStringList tags =
      m_libraryTagsPanel ? m_libraryTagsPanel->selectedTags() : QStringList();
  proxy->setSearchText(search);
  proxy->setRequiredTags(tags);
  if (m_libraryOrgBar) {
    proxy->setSmartView(
        static_cast<LibraryFilterProxy::SmartView>(m_libraryOrgBar->smartView()));
    proxy->setSortMode(
        static_cast<LibraryFilterProxy::SortMode>(m_libraryOrgBar->sortMode()));
  }
  updateSidebarBadges();
  updateLibraryHeader();
}

QString MainWindow::currentEditorNotePath() const {
  if (!m_editorTabs)
    return {};
  QWidget *w = m_editorTabs->currentWidget();
  if (!w)
    return {};
  const QString prop = w->property("filePath").toString();
  if (!prop.isEmpty())
    return prop;
  if (auto *cv = w->findChild<CanvasView *>()) {
    const QString p = cv->property("filePath").toString();
    if (!p.isEmpty())
      return p;
  }
  return {};
}

void MainWindow::rebuildPageSettingsTags() {
  if (!m_tagsContainer || !m_tagsFlowLayout)
    return;

  while (QLayoutItem *child = m_tagsFlowLayout->takeAt(0)) {
    if (QWidget *w = child->widget())
      w->deleteLater();
    delete child;
  }

  const QString path = currentEditorNotePath();
  QStringList assigned = LibraryTagStore::tagsForPath(path);
  const QStringList catalog = LibraryTagStore::catalog();

  auto makeChip = [this](const QString &text, bool active) {
    auto *tag = new QPushButton(text, m_tagsContainer);
    tag->setCursor(Qt::PointingHandCursor);
    tag->setCheckable(true);
    tag->setChecked(active);
    tag->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 10px;"
        "  color: %3;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  padding: 5px 12px;"
        "}"
        "QPushButton:checked {"
        "  background: rgba(124,92,252,0.22);"
        "  border: 1px solid rgba(124,92,252,0.55);"
        "  color: #EDE9FF;"
        "}")
                           .arg(active ? QStringLiteral("rgba(124,92,252,0.22)")
                                       : QStringLiteral("rgba(255,255,255,0.05)"),
                                active ? QStringLiteral("rgba(124,92,252,0.55)")
                                       : QStringLiteral("rgba(255,255,255,0.14)"),
                                active ? QStringLiteral("#EDE9FF")
                                       : QStringLiteral("rgba(255,255,255,0.7)")));
    connect(tag, &QPushButton::toggled, this, [this, text](bool on) {
      const QString notePath = currentEditorNotePath();
      if (notePath.isEmpty())
        return;
      QStringList tags = LibraryTagStore::tagsForPath(notePath);
      if (on) {
        bool exists = false;
        for (const QString &t : tags) {
          if (t.compare(text, Qt::CaseInsensitive) == 0) {
            exists = true;
            break;
          }
        }
        if (!exists)
          tags.append(text);
      } else {
        QStringList kept;
        for (const QString &t : tags) {
          if (t.compare(text, Qt::CaseInsensitive) != 0)
            kept.append(t);
        }
        tags = kept;
      }
      LibraryTagStore::setTagsForPath(notePath, tags);
      if (m_libraryTagsPanel)
        m_libraryTagsPanel->reload();
      applyLibraryFilters();
      // Keep in-memory note tags in sync when possible.
      if (m_editorTabs) {
        if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
          if (Note *n = editor->note())
            n->tags = tags;
        }
      }
    });
    m_tagsFlowLayout->addWidget(tag);
  };

  QStringList shown = catalog;
  for (const QString &t : assigned) {
    bool inCat = false;
    for (const QString &c : shown) {
      if (c.compare(t, Qt::CaseInsensitive) == 0) {
        inCat = true;
        break;
      }
    }
    if (!inCat)
      shown.append(t);
  }
  if (shown.isEmpty()) {
    auto *empty = new QLabel(QStringLiteral("Noch keine Tags — rechts in der Bibliothek anlegen."),
                             m_tagsContainer);
    empty->setStyleSheet(
        QStringLiteral("color: rgba(255,255,255,0.45); font-size: 11px;"));
    empty->setWordWrap(true);
    m_tagsFlowLayout->addWidget(empty);
    return;
  }
  for (const QString &t : shown) {
    bool active = false;
    for (const QString &a : assigned) {
      if (a.compare(t, Qt::CaseInsensitive) == 0) {
        active = true;
        break;
      }
    }
    makeChip(t, active);
  }
}

void MainWindow::updateSidebarBadges() {
  QDir rootDir(m_rootPath);
  int rootCount =
      rootDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)
          .count();
  QString rootCountStr = QString::number(rootCount);
  for (int i = 0; i < m_navSidebar->count(); ++i) {
    QListWidgetItem *item = m_navSidebar->item(i);
    if (item->text() == QStringLiteral("Alle") ||
        item->text() == QStringLiteral("Blop Notizen")) {
      item->setData(Qt::UserRole + 2, rootCountStr);
    }
  }

  if (m_lblEmptyState && m_fileListView && m_fileModel) {
    const QModelIndex proxyRoot = m_fileListView->rootIndex();
    const int visible =
        m_libraryProxy ? m_libraryProxy->rowCount(proxyRoot)
                       : m_fileModel->rowCount(proxyRoot);
    QModelIndex sourceRoot = proxyRoot;
    if (m_libraryProxy && proxyRoot.isValid())
      sourceRoot = m_libraryProxy->mapToSource(proxyRoot);
    else if (m_libraryProxy && !proxyRoot.isValid())
      sourceRoot = m_fileModel->index(m_fileModel->rootPath());
    const int total = m_fileModel->rowCount(sourceRoot);
    if (visible == 0) {
      if (total > 0) {
        QString emptyHint = QStringLiteral(
            "Keine Treffer.\nAndere Suche, Smart-View oder Tags versuchen.");
        if (m_libraryOrgBar) {
          switch (m_libraryOrgBar->smartView()) {
          case LibraryOrgBar::SmartView::Favorites:
            emptyHint = QStringLiteral(
                "Noch keine Favoriten.\nMarkiere Notizen über das ⋯-Menü.");
            break;
          case LibraryOrgBar::SmartView::Recent:
            emptyHint = QStringLiteral(
                "Noch keine kürzlich geöffneten Notizen.");
            break;
          case LibraryOrgBar::SmartView::Untagged:
            emptyHint = QStringLiteral(
                "Alle Notizen haben Tags.\nOder wechsle zu „Alle“.");
            break;
          case LibraryOrgBar::SmartView::All:
          default:
            break;
          }
        }
        m_lblEmptyState->setText(emptyHint);
      } else {
        m_lblEmptyState->setText(QStringLiteral(
            "Noch keine Notizen.\nLeg mit + Notiz deine erste an."));
      }
      if (!m_lblEmptyState->isVisible()) {
        m_lblEmptyState->show();
        m_fileListView->hide();
      }
    } else {
      if (m_lblEmptyState->isVisible()) {
        m_lblEmptyState->hide();
        m_fileListView->show();
      }
    }
  }
}

void MainWindow::onNavItemClicked(QListWidgetItem *item) {
  if (!item)
    return;
  bool isHeader = item->data(Qt::UserRole + 1).toBool();
  if (isHeader)
    return;
  QString path = item->data(Qt::UserRole + 10).toString();
  bool isExpandable = item->data(Qt::UserRole + 6).toBool();

  if (!path.isEmpty() && QFileInfo(path).isDir()) {
    const bool alreadyHere =
        QFileInfo(m_fileModel->rootPath()).canonicalFilePath() ==
        QFileInfo(path).canonicalFilePath();
    navigateLibraryToPath(path);
    // Expand when collapsed. Second click on the already-open folder collapses.
    if (isExpandable) {
      const bool expanded = item->data(Qt::UserRole + 3).toBool();
      if (!expanded || alreadyHere)
        toggleFolderContent(item);
    }

#ifdef Q_OS_ANDROID
    onToggleSidebar();
#endif
    return;
  }

  if (!path.isEmpty() && QFileInfo(path).isFile()) {
    onFileDoubleClicked(m_fileModel->index(path));
    return;
  }

  QString name = item->text();
  if (name == QStringLiteral("Gerät")) {
    navigateLibraryToPath(QDir::rootPath());
#ifdef Q_OS_ANDROID
    onToggleSidebar();
#endif
    return;
  }

  const QString cloudRole = item->data(Qt::UserRole + 5).toString();
  if (cloudRole == QLatin1String("clouds_add")) {
    const QString label = BlopDialogs::promptText(
        this, QStringLiteral("Eigene Cloud"),
        QStringLiteral("Anzeigename:"), QStringLiteral("Meine Cloud"));
    if (label.isEmpty())
      return;
    const QString folder = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Sync-Ordner wählen"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    if (folder.isEmpty())
      return;
    QVector<CloudStorageEntry> entries = CloudStorageStore::load();
    CloudStorageEntry e;
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.name = label;
    e.type = QStringLiteral("custom");
    e.path = folder;
    entries.append(e);
    CloudStorageStore::save(entries);
    // Insert before the trailing "Eigene Cloud hinzufügen…" row.
    int insertAt = m_navSidebar->count();
    for (int i = 0; i < m_navSidebar->count(); ++i) {
      if (m_navSidebar->item(i)->data(Qt::UserRole + 5).toString() ==
          QLatin1String("clouds_add")) {
        insertAt = i;
        break;
      }
    }
    auto *navItem = new QListWidgetItem();
    navItem->setText(e.name);
    navItem->setIcon(createModernIcon(QStringLiteral("cloud"),
                                      BlopTheme::textSecondary()));
    navItem->setData(Qt::UserRole + 11, QStringLiteral("cloud"));
    navItem->setData(Qt::UserRole + 5, QStringLiteral("clouds_item"));
    navItem->setData(Qt::UserRole + 12, e.id);
    navItem->setData(Qt::UserRole + 13, e.type);
    navItem->setData(Qt::UserRole + 10, e.path);
    navItem->setToolTip(e.path);
    navItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_navSidebar->insertItem(insertAt, navItem);
    navigateLibraryToPath(folder);
#ifdef Q_OS_ANDROID
    onToggleSidebar();
#endif
    return;
  }

  if (cloudRole == QLatin1String("clouds_item")) {
    const QString id = item->data(Qt::UserRole + 12).toString();
    QString path = item->data(Qt::UserRole + 10).toString();
    if (path.isEmpty() || !QDir(path).exists()) {
      path = QFileDialog::getExistingDirectory(
          this,
          QStringLiteral("%1 — Sync-Ordner wählen").arg(item->text()),
          QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
      if (path.isEmpty())
        return;
      QVector<CloudStorageEntry> entries = CloudStorageStore::load();
      if (CloudStorageEntry *e = CloudStorageStore::findMutable(entries, id)) {
        e->path = path;
        CloudStorageStore::save(entries);
      }
      item->setData(Qt::UserRole + 10, path);
      item->setToolTip(path);
    }
    navigateLibraryToPath(path);
#ifdef Q_OS_ANDROID
    onToggleSidebar();
#endif
  }
}

void MainWindow::toggleFolderContent(QListWidgetItem *parentItem) {
  bool isExpanded = parentItem->data(Qt::UserRole + 3).toBool();
  QString parentPath = parentItem->data(Qt::UserRole + 10).toString();
  if (parentPath.isEmpty()) {
    if (parentItem->text() == QStringLiteral("Blop Notizen") ||
        parentItem->text() == QStringLiteral("Alle"))
      parentPath = m_rootPath;
    else
      return;
  }
  int currentDepth = parentItem->data(Qt::UserRole + 9).toInt();
  if (isExpanded) {
    int row = m_navSidebar->row(parentItem) + 1;
    while (row < m_navSidebar->count()) {
      QListWidgetItem *child = m_navSidebar->item(row);
      int childDepth = child->data(Qt::UserRole + 9).toInt();
      if (childDepth > currentDepth)
        delete m_navSidebar->takeItem(row);
      else
        break;
    }
    parentItem->setData(Qt::UserRole + 3, false);
  } else {
    QDir dir(parentPath);
    QFileInfoList list = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    int insertRow = m_navSidebar->row(parentItem) + 1;
    for (const QFileInfo &info : list) {
      QListWidgetItem *child = new QListWidgetItem();
      child->setText(info.fileName());
      child->setData(Qt::UserRole + 10, info.absoluteFilePath());
      child->setData(Qt::UserRole + 9, currentDepth + 1);
      child->setData(Qt::UserRole + 8, parentItem->text());
      if (info.isDir()) {
        child->setIcon(createModernIcon("folder", BlopTheme::textSecondary()));
        child->setData(Qt::UserRole + 6, true);
        child->setData(Qt::UserRole + 3, false);
      } else {
        QPixmap pix(64, 64);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(BlopTheme::textSecondary(), 2));
        p.drawRoundedRect(16, 10, 32, 44, 4, 4);
        p.drawLine(20, 18, 44, 18);
        p.drawLine(20, 26, 44, 26);
        child->setIcon(QIcon(pix));
      }
      m_navSidebar->insertItem(insertRow++, child);
    }
    parentItem->setData(Qt::UserRole + 3, true);
  }
}

void MainWindow::onNewPage() {
  auto createNote = [this](const QString &name, bool isInfinite,
                           const A4LayoutDialogResult &layoutResult) {
    QString safeName = name;
    safeName.replace("/", "_").replace("\\", "_");

    if (isInfinite) {
      // Legacy unendliche Leinwand (.blop -> CanvasView)
      QString path = m_fileModel->rootPath() + "/" + safeName + ".blop";
      QFile file(path);
      if (!file.open(QIODevice::WriteOnly)) {
        BlopDialogs::notify(
            this, QStringLiteral("Notiz erstellen"),
            QStringLiteral("Datei konnte nicht angelegt werden:\n%1")
                .arg(path));
        return;
      }
      QDataStream out(&file);
      out << (quint32)0xB10B0002;
      out << isInfinite;
      out << (int)0;
      file.close();
      onFileDoubleClicked(m_fileModel->index(path));
      if (CanvasView *cv = getCurrentCanvas()) {
        cv->setPageColor(layoutResult.paperColor.isValid()
                             ? layoutResult.paperColor
                             : UIStyles::PageBackground);
        PageStyle style = PageStyle::Squared;
        switch (layoutResult.backgroundType) {
        case 0:
          style = PageStyle::Blank;
          break;
        case 1:
          style = PageStyle::Lined;
          break;
        case 3:
          style = PageStyle::Dotted;
          break;
        default:
          style = PageStyle::Squared;
          break;
        }
        cv->setPageStyle(style);
        cv->setGridSize(40);
      }
      return;
    }

    // Modernes A4-Notizheft (.bnote -> MultiPageNoteView)
    QString path = m_fileModel->rootPath() + "/" + safeName + ".bnote";
    Note note;
    note.id = QUuid::createUuid().toString();
    note.title = name;
    NotePage p;
    p.paperColor = layoutResult.paperColor.isValid() ? layoutResult.paperColor
                                                      : UIStyles::PageBackground;
    p.backgroundType = qBound(0, layoutResult.backgroundType, 4);
    note.pages.append(p);
    if (!NoteManager::saveNote(note, path)) {
      BlopDialogs::notify(
          this, QStringLiteral("Notiz erstellen"),
          QStringLiteral("Notiz konnte nicht gespeichert werden:\n%1")
              .arg(path));
      return;
    }
    onFileDoubleClicked(m_fileModel->index(path));
  };

#ifdef Q_OS_ANDROID
  auto calcAndroidCardSize = [this](QWidget *host, int minW, int maxW, int minH,
                                    int maxH, qreal wRatio,
                                    qreal hRatio) -> QSize {
    const int hostW = host ? host->width() : width();
    const int hostH = host ? host->height() : height();
    const int w = qBound(UiScale::dp(minW),
                         int(qreal(qMax(1, hostW)) * wRatio), UiScale::dp(maxW));
    const int h = qBound(UiScale::dp(minH),
                         int(qreal(qMax(1, hostH)) * hRatio), UiScale::dp(maxH));
    return QSize(w, h);
  };

  // Android: avoid QDialog completely in this flow.
  auto *overlay = new QWidget(this);
  overlay->setAttribute(Qt::WA_DeleteOnClose, true);
  overlay->setObjectName(QStringLiteral("AndroidTransientOverlay"));
  overlay->setStyleSheet("background-color: rgba(0,0,0,150);");
  overlay->setGeometry(androidSafeOverlayRect(this));
  overlay->show();
  overlay->raise();

  auto *card = new QFrame(overlay);
  card->setStyleSheet(BlopTheme::themed(
      "QFrame { background-color: #1E1E1E; border: 1px solid #444; border-radius: 12px; }"
      "QLabel { color: #DDD; border: none; background: transparent; }"
      "QLineEdit { background: #252526; color: #F4F2FF; border: 1px solid #444; border-radius: 6px; padding: 8px; font-size: 14px; }"
      "QLineEdit:focus { border: 1px solid #5E5CE6; }"));
  const QSize noteCardSize =
      calcAndroidCardSize(overlay, 300, 460, 260, 420, 0.88, 0.62);
  card->setFixedSize(noteCardSize);
  card->move((overlay->width() - noteCardSize.width()) / 2,
             (overlay->height() - noteCardSize.height()) / 2);
  card->show();
  card->raise();

  auto *layout = new QVBoxLayout(card);
  layout->setContentsMargins(25, 25, 25, 25);
  layout->setSpacing(16);

  auto *title = new QLabel(QStringLiteral("Neue Notiz erstellen"), card);
  title->setStyleSheet(BlopTheme::themed(
      "font-size: 18px; font-weight: bold; color: #F4F2FF;"));
  layout->addWidget(title);

  auto *lblName = new QLabel(QStringLiteral("Name"), card);
  lblName->setStyleSheet(BlopTheme::themed(
      "font-size: 13px; color: #BBB; font-weight: bold;"));
  layout->addWidget(lblName);

  auto *nameInput = new QLineEdit(card);
  nameInput->setPlaceholderText(QStringLiteral("Meine Notiz"));
  nameInput->setFocus();
  layout->addWidget(nameInput);

  auto *lblFormat = new QLabel(QStringLiteral("Format"), card);
  lblFormat->setStyleSheet(BlopTheme::themed(
      "font-size: 13px; color: #BBB; font-weight: bold;"));
  layout->addWidget(lblFormat);

  auto *formatRow = new QHBoxLayout();
  auto mkBtn = [card](const QString &text, const QString &subtext) {
    auto *btn = new QPushButton(text + "\n" + subtext, card);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(UiScale::dp(70));
    btn->setStyleSheet(BlopTheme::themed(
        "QPushButton { background: #252526; color: #AAA; border: 1px solid #444; border-radius: 8px; text-align: left; padding: 10px; line-height: 1.2; font-size: 14px; }"
        "QPushButton:checked { background: #5E5CE6; color: white; border: 1px solid #5E5CE6; }"
        "QPushButton:hover:!checked { background: rgba(255,255,255,0.08); border-color: #555; }"));
    return btn;
  };
  auto *btnInfinite =
      mkBtn(QStringLiteral("Unendlich"), QStringLiteral("Freie Leinwand\n(Standard)"));
  auto *btnA4 =
      mkBtn(QStringLiteral("DIN A4"), QStringLiteral("Seitenbasiert\n(Druckoptimiert)"));
  btnInfinite->setChecked(true);
  auto *grp = new QButtonGroup(card);
  grp->setExclusive(true);
  grp->addButton(btnInfinite, 0);
  grp->addButton(btnA4, 1);
  formatRow->addWidget(btnInfinite);
  formatRow->addWidget(btnA4);
  layout->addLayout(formatRow);
  layout->addStretch();

  auto *actions = new QHBoxLayout();
  actions->setSpacing(UiScale::dp(10));
  auto *btnCancel = new QPushButton(QStringLiteral("Abbrechen"), card);
  btnCancel->setMinimumHeight(UiScale::dp(48));
  btnCancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  btnCancel->setStyleSheet(BlopTheme::themed(
      "QPushButton { background: #262237; color: #E0DBFF; border: 1px solid #3A3550; border-radius: 12px; font-weight: 700; font-size: 15px; padding: 10px 12px; }"
      "QPushButton:hover { background: #312C45; }"));
  auto *btnCreate = new QPushButton(QStringLiteral("Erstellen"), card);
  btnCreate->setMinimumHeight(UiScale::dp(48));
  btnCreate->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  btnCreate->setStyleSheet(
      "QPushButton { background: #5E5CE6; color: white; border: none; border-radius: 12px; font-weight: 700; font-size: 15px; padding: 10px 12px; }"
      "QPushButton:hover { background: #4b49c9; }");
  actions->addWidget(btnCancel);
  actions->addWidget(btnCreate);
  layout->addLayout(actions);

  connect(btnCancel, &QPushButton::clicked, overlay, &QWidget::close);
  connect(btnCreate, &QPushButton::clicked, this,
          [this, overlay, nameInput, btnInfinite, createNote]() {
            const QString name = nameInput->text().trimmed().isEmpty()
                                     ? QStringLiteral("Neue Notiz")
                                     : nameInput->text().trimmed();
            const bool isInfinite = btnInfinite->isChecked();
            const QString subtitle = isInfinite
                                         ? QStringLiteral("Lege Layout und Seitenfarbe fuer die unendliche Notiz fest.")
                                         : QStringLiteral("Lege Layout und Seitenfarbe fuer die neue A4-Notiz fest.");
            showA4LayoutOverlayAsync(
                this, QStringLiteral("Seitenlayout"), subtitle, 2,
                UIStyles::PageBackground,
                [this, overlay, name, isInfinite, createNote](
                    const A4LayoutDialogResult &layoutResult) {
                  if (layoutResult.accepted)
                    createNote(name, isInfinite, layoutResult);
                  if (overlay)
                    overlay->close();
                });
          });
  overlay->raise();
  card->raise();
  return;
#else
  NewNoteDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted)
    return;
  QString name = dlg.getNoteName();
  bool isInfinite = dlg.isInfiniteFormat();
  A4LayoutDialogResult layoutResult;
  const QString subtitle = isInfinite
      ? QStringLiteral("Lege Layout und Seitenfarbe fuer die unendliche Notiz fest.")
      : QStringLiteral("Lege Layout und Seitenfarbe fuer die neue A4-Notiz fest.");
  if (!showA4LayoutOverlay(this, QStringLiteral("Seitenlayout"), subtitle, 2,
                           UIStyles::PageBackground, &layoutResult) ||
      !layoutResult.accepted) {
    return;
  }
  createNote(name, isInfinite, layoutResult);
#endif
}

void MainWindow::onCreateFolder() {
#ifdef Q_OS_ANDROID
  auto calcAndroidCardSize = [this](QWidget *host, int minW, int maxW, int minH,
                                    int maxH, qreal wRatio,
                                    qreal hRatio) -> QSize {
    const int hostW = host ? host->width() : width();
    const int hostH = host ? host->height() : height();
    const int w = qBound(UiScale::dp(minW),
                         int(qreal(qMax(1, hostW)) * wRatio), UiScale::dp(maxW));
    const int h = qBound(UiScale::dp(minH),
                         int(qreal(qMax(1, hostH)) * hRatio), UiScale::dp(maxH));
    return QSize(w, h);
  };

  auto *overlay = new QWidget(this);
  overlay->setAttribute(Qt::WA_DeleteOnClose, true);
  overlay->setObjectName(QStringLiteral("AndroidTransientOverlay"));
  overlay->setStyleSheet("background-color: rgba(0,0,0,150);");
  overlay->setGeometry(androidSafeOverlayRect(this));
  overlay->show();
  overlay->raise();

  auto *card = new QFrame(overlay);
  card->setStyleSheet(BlopTheme::themed(
      "QFrame { background-color: #1E1E1E; border: 1px solid #444; border-radius: 12px; }"
      "QLabel { color: #DDD; border: none; background: transparent; }"
      "QLineEdit { background: #252526; color: #F4F2FF; border: 1px solid #444; border-radius: 6px; padding: 8px; font-size: 14px; }"
      "QLineEdit:focus { border: 1px solid #5E5CE6; }"));
  const QSize folderCardSize =
      calcAndroidCardSize(overlay, 280, 440, 170, 240, 0.86, 0.34);
  card->setFixedSize(folderCardSize);
  card->move((overlay->width() - folderCardSize.width()) / 2,
             (overlay->height() - folderCardSize.height()) / 2);
  card->show();
  card->raise();

  auto *layout = new QVBoxLayout(card);
  layout->setContentsMargins(20, 20, 20, 20);
  layout->setSpacing(12);
  auto *title = new QLabel(QStringLiteral("Neuer Ordner"), card);
  title->setStyleSheet(BlopTheme::themed(
      "font-size: 16px; font-weight: bold; color: #F4F2FF;"));
  layout->addWidget(title);
  auto *edit = new QLineEdit(card);
  edit->setPlaceholderText(QStringLiteral("Neuer Ordner"));
  edit->setFocus();
  layout->addWidget(edit);
  auto *actions = new QHBoxLayout();
  actions->setSpacing(UiScale::dp(10));
  auto *btnCancel = new QPushButton(QStringLiteral("Abbrechen"), card);
  btnCancel->setMinimumHeight(UiScale::dp(48));
  btnCancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  btnCancel->setStyleSheet(BlopTheme::themed(
      "QPushButton { background: #262237; color: #E0DBFF; border: 1px solid #3A3550; border-radius: 12px; font-weight: 700; font-size: 15px; padding: 10px 12px; }"
      "QPushButton:hover { background: #312C45; }"));
  auto *btnOk = new QPushButton(QStringLiteral("Erstellen"), card);
  btnOk->setMinimumHeight(UiScale::dp(48));
  btnOk->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  btnOk->setStyleSheet(
      "QPushButton { background: #5E5CE6; color: white; border: none; border-radius: 12px; padding: 10px 12px; font-weight: 700; font-size: 15px; }"
      "QPushButton:hover { background: #4b49c9; }");
  actions->addWidget(btnCancel);
  actions->addWidget(btnOk);
  layout->addLayout(actions);
  connect(btnCancel, &QPushButton::clicked, overlay, &QWidget::close);
  connect(btnOk, &QPushButton::clicked, this, [this, overlay, edit]() {
    const QString text = edit->text().trimmed();
    if (!text.isEmpty())
      m_fileModel->mkdir(m_fileModel->index(m_fileModel->rootPath()), text);
    if (overlay)
      overlay->close();
  });
  overlay->raise();
  card->raise();
  return;
#endif
  bool ok;
  QString text = BlopDialogs::promptText(this, QStringLiteral("Neuer Ordner"),
                                         QStringLiteral("Name:"),
                                         QStringLiteral("Neuer Ordner"));
  ok = !text.isEmpty();
  if (ok && !text.isEmpty()) {
    m_fileModel->mkdir(m_fileModel->index(m_fileModel->rootPath()), text);
  }
}

void MainWindow::performCopy(const QModelIndex &index) {
  QString srcPath = m_fileModel->filePath(index);
  QFileInfo info(srcPath);
  QString newName = info.baseName() + " Copy";
  if (!info.suffix().isEmpty())
    newName += "." + info.suffix();
  QString dstPath = info.dir().filePath(newName);
  int i = 1;
  while (QFile::exists(dstPath)) {
    newName = info.baseName() + QString(" Copy %1").arg(i++);
    if (!info.suffix().isEmpty())
      newName += "." + info.suffix();
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
  if (!dir.exists())
    return false;
  QDir().mkpath(dst);
  for (const QString &entry :
       dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
    QString srcItem = src + "/" + entry;
    QString dstItem = dst + "/" + entry;
    if (QFileInfo(srcItem).isDir()) {
      if (!copyRecursive(srcItem, dstItem))
        return false;
    } else {
      if (!QFile::copy(srcItem, dstItem))
        return false;
    }
  }
  return true;
}

void MainWindow::onNavigateUp() {
  if (!m_fileListView || !m_fileModel)
    return;
  QModelIndex current = m_fileListView->rootIndex();
  if (!current.isValid())
    return;
  QModelIndex parent = current.parent();
  QString parentPath = m_rootPath;
  if (m_libraryProxy) {
    const QModelIndex src = m_libraryProxy->mapToSource(
        parent.isValid() ? parent : current);
    if (src.isValid()) {
      if (parent.isValid())
        parentPath = m_fileModel->filePath(src);
      else
        parentPath = QFileInfo(m_fileModel->filePath(src)).absolutePath();
    }
  }
  // Never climb above the Blop library root via the overview back button.
  const QString rootCanon = QFileInfo(m_rootPath).canonicalFilePath();
  const QString parentCanon = QFileInfo(parentPath).canonicalFilePath();
  if (parentCanon.isEmpty() ||
      (!rootCanon.isEmpty() && !parentCanon.startsWith(rootCanon)))
    parentPath = m_rootPath;
  navigateLibraryToPath(parentPath);
}

void MainWindow::updateOverviewBackButton() {
  if (!btnBackOverview || !m_fileListView || !m_fileModel)
    return;
  QString folderPath = m_fileModel->rootPath();
  if (m_libraryProxy) {
    const QModelIndex proxyRoot = m_fileListView->rootIndex();
    if (proxyRoot.isValid()) {
      const QModelIndex src = m_libraryProxy->mapToSource(proxyRoot);
      if (src.isValid())
        folderPath = m_fileModel->filePath(src);
    }
  }
  const QString cur = QFileInfo(folderPath).canonicalFilePath();
  const QString root = QFileInfo(m_rootPath).canonicalFilePath();
  const bool canGoUp = !cur.isEmpty() && !root.isEmpty() && cur != root;
  btnBackOverview->setVisible(canGoUp);
  updateLibraryHeader();
}

void MainWindow::setupRightSidebar() {
  m_pageSettingsOverlay = new QWidget(m_editorCenterWidget);
  m_pageSettingsOverlay->setObjectName(QStringLiteral("PageSettingsOverlay"));
  m_pageSettingsOverlay->setAttribute(Qt::WA_StyledBackground, true);
  m_pageSettingsOverlay->setStyleSheet(
      QStringLiteral("QWidget#PageSettingsOverlay { background-color: %1; }")
          .arg(BlopTheme::scrimColor().name(QColor::HexArgb)));
  m_pageSettingsOverlay->hide();
  m_pageSettingsOverlay->installEventFilter(this);

  auto *outerLay = new QVBoxLayout(m_pageSettingsOverlay);
  outerLay->setContentsMargins(24, 32, 24, 32);
  outerLay->setSpacing(0);
  outerLay->addStretch(1);
  auto *midRow = new QHBoxLayout();
  midRow->addStretch(1);

  m_pageSettingsCard = new QWidget(m_pageSettingsOverlay);
  m_pageSettingsCard->setObjectName(QStringLiteral("PageSettingsCard"));
  m_pageSettingsCard->setFixedWidth(520);
  m_pageSettingsCard->setMaximumHeight(800);
  m_pageSettingsCard->setStyleSheet(
      QStringLiteral("QWidget#PageSettingsCard { background: %1; border: 1px solid %2; "
                     "border-radius: 18px; }")
          .arg(UIStyles::PageBackground.name(),
               QColor(124, 92, 252, 100).name(QColor::HexArgb)));

  QVBoxLayout *mainLayout = new QVBoxLayout(m_pageSettingsCard);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // =========================================================================
  // HEADER
  // =========================================================================
  QWidget *headerWidget = new QWidget(m_pageSettingsCard);
  headerWidget->setStyleSheet(
      "background: transparent;"
      "border-bottom: 1px solid rgba(255,255,255,0.07);");
  headerWidget->setFixedHeight(44);
  QHBoxLayout *header = new QHBoxLayout(headerWidget);
  header->setContentsMargins(16, 0, 8, 0);
  QLabel *sidebarTitle = new QLabel(QStringLiteral("Seite & Notiz"), headerWidget);
  sidebarTitle->setStyleSheet(
      "color: rgba(255,255,255,0.85); font-size: 13px; font-weight: 600;"
      "background: transparent; border: none;");
  header->addWidget(sidebarTitle);
  header->addStretch();
  ModernButton *closeBtn = new ModernButton(headerWidget);
  closeBtn->setIcon(createModernIcon("close", QColor("#888")));
  closeBtn->setFixedSize(28, 28);
  connect(closeBtn, &QAbstractButton::clicked, this, [this]() {
    setPageSettingsOverlayVisible(false);
  });
  header->addWidget(closeBtn);
  mainLayout->addWidget(headerWidget);

  // =========================================================================
  // NOTIZNAME
  // =========================================================================
  m_lblActiveNote = new QLabel("", m_pageSettingsCard);
  m_lblActiveNote->setStyleSheet(BlopTheme::themed(
      "color: rgba(255,255,255,0.80); font-size: 12px; font-weight: 600;"
      "padding: 12px 16px 8px 16px; background: transparent;"));
  m_lblActiveNote->setWordWrap(true);
  mainLayout->addWidget(m_lblActiveNote);

  // =========================================================================
  // TAB WIDGET (Optionen vs Tags)
  // =========================================================================
  QTabWidget *settingsTabs = new QTabWidget(m_pageSettingsCard);
  m_pageSettingsTabs = settingsTabs;
  mainLayout->addWidget(settingsTabs, 1);

  // -------------------------------------------------------------------------
  // TAB 1: OPTIONEN (Formatierung, Input, Profile)
  // -------------------------------------------------------------------------
  QWidget *tabOptions = new QWidget();
  m_pageSettingsTabOptions = tabOptions;
  QVBoxLayout *optLayoutMain = new QVBoxLayout(tabOptions);
  optLayoutMain->setContentsMargins(0, 0, 0, 0);

  QScrollArea *optScroll = new QScrollArea(tabOptions);
  optScroll->setWidgetResizable(true);
  optScroll->setFrameShape(QFrame::NoFrame);
  optScroll->setStyleSheet("QScrollArea { background: transparent; }");
  OverlayScrollIndicator::install(optScroll);

  QWidget *optContent = new QWidget();
  optContent->setStyleSheet("background: transparent;");
  QVBoxLayout *optLayout = new QVBoxLayout(optContent);
  optLayout->setContentsMargins(16, 12, 16, 20);
  optLayout->setSpacing(15);

  auto sectionLabel = [&](const QString &text, QWidget *sectionParent) -> QLabel * {
    QLabel *lbl = new QLabel(text, sectionParent);
    lbl->setStyleSheet(
        "color: rgba(255,255,255,0.40); font-size: 10px; font-weight: 700;"
        "letter-spacing: 0.5px; background: transparent;");
    return lbl;
  };

  // --- Seiten-Optionen ---
  optLayout->addWidget(sectionLabel(QStringLiteral("SEITE"), optContent));

  // Format (Infinite/A4) is fixed at note creation — hide the disabled
  // toggles that looked like unfinished controls.
  m_btnFormatInfinite = new QPushButton("Infinite", optContent);
  m_btnFormatInfinite->setCheckable(true);
  m_btnFormatInfinite->setEnabled(false);
  m_btnFormatInfinite->hide();
  m_btnFormatA4 = new QPushButton("A4", optContent);
  m_btnFormatA4->setCheckable(true);
  m_btnFormatA4->setEnabled(false);
  m_btnFormatA4->hide();
  QButtonGroup *grp = new QButtonGroup(this);
  grp->addButton(m_btnFormatInfinite);
  grp->addButton(m_btnFormatA4);
  grp->setExclusive(true);

  QLabel *lblLayout = new QLabel(QStringLiteral("Layout:"), optContent);
  lblLayout->setObjectName(QStringLiteral("pageSettingsLayoutLabel"));
  optLayout->addWidget(lblLayout);
  m_btnStyleBlank = new QPushButton("Blank");
  m_btnStyleLined = new QPushButton("Lined");
  m_btnStyleSquared = new QPushButton("Squared");
  m_btnStyleDotted = new QPushButton("Dotted");
  m_btnStyleBlank->setCheckable(true);
  m_btnStyleLined->setCheckable(true);
  m_btnStyleSquared->setCheckable(true);
  m_btnStyleDotted->setCheckable(true);
  QString btnStyleTemplate =
      "QPushButton { background: #333; color: white; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; } QPushButton:checked { background: "
      "%1; border: 1px solid %1; }";
  QString accentColorName = m_currentAccentColor.name();
  QString styleSheet = btnStyleTemplate.arg(accentColorName);
  m_btnStyleBlank->setStyleSheet(styleSheet);
  m_btnStyleLined->setStyleSheet(styleSheet);
  m_btnStyleSquared->setStyleSheet(styleSheet);
  m_btnStyleDotted->setStyleSheet(styleSheet);
  m_grpPageStyle = new QButtonGroup(this);
  m_grpPageStyle->addButton(m_btnStyleBlank, (int)PageStyle::Blank);
  m_grpPageStyle->addButton(m_btnStyleLined, (int)PageStyle::Lined);
  m_grpPageStyle->addButton(m_btnStyleSquared, (int)PageStyle::Squared);
  m_grpPageStyle->addButton(m_btnStyleDotted, (int)PageStyle::Dotted);
  m_grpPageStyle->setExclusive(true);
  connect(m_grpPageStyle,
          &QButtonGroup::buttonToggled,
          this, &MainWindow::onPageStyleButtonToggled);
  QWidget *styleBtnsContainer = new QWidget(optContent);
  styleBtnsContainer->setObjectName(QStringLiteral("pageSettingsStyleRow"));
  QHBoxLayout *styleBtnsLayout = new QHBoxLayout(styleBtnsContainer);
  styleBtnsLayout->setContentsMargins(0, 0, 0, 0);
  styleBtnsLayout->addWidget(m_btnStyleBlank);
  styleBtnsLayout->addWidget(m_btnStyleLined);
  styleBtnsLayout->addWidget(m_btnStyleSquared);
  styleBtnsLayout->addWidget(m_btnStyleDotted);
  optLayout->addWidget(styleBtnsContainer);

#ifndef Q_OS_ANDROID
  // --- Seitenleisten-Layout (hide / move) ---
  optLayout->addWidget(
      sectionLabel(QStringLiteral("SEITENLEISTE"), optContent));
  auto *cbLeftRail = new QCheckBox(
      QStringLiteral("Linke Werkzeugleiste anzeigen"), optContent);
  cbLeftRail->setObjectName(QStringLiteral("pageSettingsLeftRailVisible"));
  cbLeftRail->setChecked(m_noteLeftRailPrefVisible);
  cbLeftRail->setStyleSheet(
      QStringLiteral("QCheckBox { color: rgba(255,255,255,0.82); background: transparent; "
                     "spacing: 8px; font-size: 12px; }"
                     "QCheckBox::indicator { width: 16px; height: 16px; }"));
  connect(cbLeftRail, &QCheckBox::toggled, this, [this](bool on) {
    m_noteLeftRailPrefVisible = on;
    persistPageChromePrefs();
    applyPageChromePrefs();
  });
  optLayout->addWidget(cbLeftRail);

  auto *cbPages = new QCheckBox(
      QStringLiteral("Seiten-Manager anzeigen"), optContent);
  cbPages->setObjectName(QStringLiteral("pageSettingsPagesVisible"));
  cbPages->setChecked(m_pageThumbnailSidebar &&
                      !m_pageThumbnailSidebar->isCollapsed());
  cbPages->setStyleSheet(cbLeftRail->styleSheet());
  connect(cbPages, &QCheckBox::toggled, this, [this](bool on) {
    if (!m_pageThumbnailSidebar)
      return;
    if (on) {
      m_pageThumbnailSidebar->setCollapsed(false);
      m_pageThumbnailSidebar->show();
      m_pageThumbnailSidebar->rebuild();
      if (m_noteLeftRail)
        m_noteLeftRail->setPagesExpanded(true);
    } else {
      m_pageThumbnailSidebar->setCollapsed(true);
      m_pageThumbnailSidebar->hide();
      if (m_noteLeftRail)
        m_noteLeftRail->setPagesExpanded(false);
    }
    positionNoteChrome();
  });
  optLayout->addWidget(cbPages);

  auto *lblEdge = new QLabel(QStringLiteral("Position:"), optContent);
  lblEdge->setObjectName(QStringLiteral("pageSettingsRailEdgeLabel"));
  lblEdge->setStyleSheet(
      QStringLiteral("color: rgba(255,255,255,0.55); font-size: 11px; "
                     "background: transparent;"));
  optLayout->addWidget(lblEdge);
  auto *edgeRow = new QWidget(optContent);
  auto *edgeLay = new QHBoxLayout(edgeRow);
  edgeLay->setContentsMargins(0, 0, 0, 0);
  edgeLay->setSpacing(12);
  auto *rLeft = new QRadioButton(QStringLiteral("Links"), edgeRow);
  auto *rRight = new QRadioButton(QStringLiteral("Rechts"), edgeRow);
  const QString radioCss = QStringLiteral(
      "QRadioButton { color: rgba(255,255,255,0.85); background: transparent; "
      "font-size: 12px; spacing: 6px; }");
  rLeft->setStyleSheet(radioCss);
  rRight->setStyleSheet(radioCss);
  rLeft->setChecked(!m_pageRailOnRight);
  rRight->setChecked(m_pageRailOnRight);
  auto *edgeGroup = new QButtonGroup(edgeRow);
  edgeGroup->addButton(rLeft, 0);
  edgeGroup->addButton(rRight, 1);
  connect(edgeGroup, &QButtonGroup::idClicked, this, [this](int id) {
    m_pageRailOnRight = (id == 1);
    persistPageChromePrefs();
    positionNoteChrome();
  });
  edgeLay->addWidget(rLeft);
  edgeLay->addWidget(rRight);
  edgeLay->addStretch();
  optLayout->addWidget(edgeRow);
#endif

  QLabel *lblGrid = new QLabel("Grid Spacing (px):", optContent);
  lblGrid->setObjectName(QStringLiteral("pageSettingsGridLabel"));
  optLayout->addWidget(lblGrid);
  m_sliderGridSpacing = new QSlider(Qt::Horizontal);
  m_sliderGridSpacing->setObjectName(QStringLiteral("pageSettingsGridSlider"));
  m_sliderGridSpacing->setRange(10, 80);
  m_sliderGridSpacing->setValue(40);
  m_sliderGridSpacing->setStyleSheet(
      "QSlider::groove:horizontal { border: 1px solid #333; height: 6px; "
      "background: #121212; margin: 2px 0; border-radius: 3px; } "
      "QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid "
      "#5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; "
      "}");
  connect(m_sliderGridSpacing, &QSlider::valueChanged, this,
          &MainWindow::onPageGridSpacingSliderChanged);
  optLayout->addWidget(m_sliderGridSpacing);

  QLabel *lblPageColor = new QLabel("Page Color:", optContent);
  lblPageColor->setObjectName(QStringLiteral("pageSettingsColorLabel"));
  optLayout->addWidget(lblPageColor);
  m_btnColorWhite = new QPushButton("Light");
  m_btnColorWhite->setCheckable(true);
  m_btnColorWhite->setChecked(false);
  m_btnColorWhite->setCursor(Qt::PointingHandCursor);
  m_btnColorWhite->setStyleSheet(
      "QPushButton { background: #333; color: white; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; } QPushButton:checked { background: "
      "#5E5CE6; border: 1px solid #5E5CE6; }");
  connect(m_btnColorWhite, &QPushButton::clicked,
          [this]() { setPageColor(false); });
  m_btnColorDark = new QPushButton("Dark");
  m_btnColorDark->setCheckable(true);
  m_btnColorDark->setChecked(true);
  m_btnColorDark->setCursor(Qt::PointingHandCursor);
  m_btnColorDark->setStyleSheet(
      "QPushButton { background: #333; color: white; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; } QPushButton:checked { background: "
      "#5E5CE6; border: 1px solid #5E5CE6; }");
  connect(m_btnColorDark, &QPushButton::clicked,
          [this]() { setPageColor(true); });
  QButtonGroup *grpColor = new QButtonGroup(this);
  grpColor->addButton(m_btnColorWhite);
  grpColor->addButton(m_btnColorDark);
  grpColor->setExclusive(true);
  optLayout->addWidget(m_btnColorWhite);
  optLayout->addWidget(m_btnColorDark);

  optLayout->addWidget(new QLabel("Input Mode:", optContent));
  m_btnInputPen = new QPushButton("Pen Only\n(1 Finger scrolls)");
  m_btnInputPen->setCheckable(true);
  m_btnInputPen->setChecked(true);
  m_btnInputPen->setCursor(Qt::PointingHandCursor);
  m_btnInputPen->setStyleSheet(
      "QPushButton { background: #333; color: white; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; text-align: left; } "
      "QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; "
      "}");
  connect(m_btnInputPen, &QPushButton::clicked,
          [this]() { updateInputMode(true); });
  m_btnInputTouch = new QPushButton("Touch & Pen\n(2 Fingers scroll)");
  m_btnInputTouch->setCheckable(true);
  m_btnInputTouch->setCursor(Qt::PointingHandCursor);
  m_btnInputTouch->setStyleSheet(
      "QPushButton { background: #333; color: white; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; text-align: left; } "
      "QPushButton:checked { background: #5E5CE6; border: 1px solid #5E5CE6; "
      "}");
  connect(m_btnInputTouch, &QPushButton::clicked,
          [this]() { updateInputMode(false); });
  QButtonGroup *grpInput = new QButtonGroup(this);
  grpInput->addButton(m_btnInputPen);
  grpInput->addButton(m_btnInputTouch);
  grpInput->setExclusive(true);
  optLayout->addWidget(m_btnInputPen);
  optLayout->addWidget(m_btnInputTouch);

  // Toolbar-Style + Scale only apply to ModernToolbar (Radial/Normal switch
  // and continuous scaling). On Android phones we ship AndroidPhoneToolbar,
  // which is a fixed bottom-pill with no style or scale knobs - so we still
  // create the controls (preserving layout indices) but disable them.
  const bool phoneToolbarActive =
      qobject_cast<AndroidPhoneToolbar *>(m_floatingTools) != nullptr;
  QLabel *lblToolbarStyle = new QLabel("Toolbar Style:", optContent);
  optLayout->addWidget(lblToolbarStyle);
  m_comboToolbarStyle = new QComboBox();
  m_comboToolbarStyle->addItems({"Vertical", "Radial (Full)", "Radial (Half)"});
  m_comboToolbarStyle->setStyleSheet(
      "QComboBox { background: #333; color: white; border: 1px solid #444; "
      "padding: 5px; border-radius: 5px; } QComboBox::drop-down { border: 0px; "
      "} QComboBox QAbstractItemView { background: #333; color: white; "
      "selection-background-color: #5E5CE6; }");
  m_comboToolbarStyle->setCursor(Qt::PointingHandCursor);
  connect(m_comboToolbarStyle,
          &QComboBox::currentIndexChanged,
          [this](int index) {
            ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools);
            if (tb) {
              if (index == 0) {
                tb->setStyle(ModernToolbar::Normal);
                tb->applyDrawboardVerticalRail();
                if (m_radialFab)
                  m_radialFab->hide();
                positionNoteChrome();
              } else if (index == 1) {
                tb->setStyle(ModernToolbar::Radial);
                tb->setRadialType(ModernToolbar::FullCircle);
                if (m_radialFab)
                  m_radialFab->setVisible(true);
                positionNoteChrome();
              } else {
                tb->setStyle(ModernToolbar::Radial);
                tb->setRadialType(ModernToolbar::HalfEdge);
                if (m_radialFab)
                  m_radialFab->setVisible(true);
                positionNoteChrome();
              }
            }
          });
  if (phoneToolbarActive) {
    lblToolbarStyle->setEnabled(false);
    m_comboToolbarStyle->setEnabled(false);
    m_comboToolbarStyle->setToolTip(
        "Toolbar-Style ist auf Android Phones fest (Bottom-Pille).");
  }
#ifndef Q_OS_ANDROID
  // Desktop Drawboard locks the vertical Favorites rail — Radial/FAB are secondary.
  lblToolbarStyle->setEnabled(false);
  m_comboToolbarStyle->setEnabled(false);
  m_comboToolbarStyle->setCurrentIndex(0);
  m_comboToolbarStyle->setToolTip(
      QStringLiteral("Desktop nutzt die Drawboard-Favorites-Leiste (vertikal)."));
#endif
  optLayout->addWidget(m_comboToolbarStyle);

  optLayout->addWidget(new QLabel("UI Profile:", optContent));
  m_comboProfiles = new QComboBox();
  m_comboProfiles->setStyleSheet(m_comboToolbarStyle->styleSheet());
  m_comboProfiles->setCursor(Qt::PointingHandCursor);
  for (const auto &p : m_profileManager->profiles()) {
    m_comboProfiles->addItem(p.name, p.id);
  }
  m_comboProfiles->setCurrentText(m_currentProfile.name);
  connect(m_comboProfiles, &QComboBox::currentIndexChanged,
          [this](int) {
            QString id = m_comboProfiles->currentData().toString();
            m_profileManager->setCurrentProfile(id);
          });
  connect(m_profileManager, &UiProfileManager::listChanged, [this]() {
    m_comboProfiles->blockSignals(true);
    m_comboProfiles->clear();
    for (const auto &p : m_profileManager->profiles()) {
      m_comboProfiles->addItem(p.name, p.id);
    }
    int idx = m_comboProfiles->findData(m_currentProfile.id);
    if (idx != -1)
      m_comboProfiles->setCurrentIndex(idx);
    m_comboProfiles->blockSignals(false);
  });
  optLayout->addWidget(m_comboProfiles);

  QLabel *lblToolbarSize = new QLabel("Toolbar Size:", optContent);
  optLayout->addWidget(lblToolbarSize);
  m_sliderToolbarScale = new QSlider(Qt::Horizontal);
  m_sliderToolbarScale->setRange(50, 150);
  m_sliderToolbarScale->setValue(100);
  m_sliderToolbarScale->setStyleSheet(
      "QSlider::groove:horizontal { border: 1px solid #333; height: 6px; "
      "background: #121212; margin: 2px 0; border-radius: 3px; } "
      "QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid "
      "#5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; "
      "}");
  connect(m_sliderToolbarScale, &QSlider::valueChanged, [this](int val) {
    ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools);
    if (tb)
      tb->setScale(val / 100.0);
  });
  if (phoneToolbarActive) {
    lblToolbarSize->setEnabled(false);
    m_sliderToolbarScale->setEnabled(false);
    m_sliderToolbarScale->setToolTip(
        "Toolbar-Skala auf Android Phones fest (Bottom-Pille).");
  }
  optLayout->addWidget(m_sliderToolbarScale);

  optLayout->addStretch();
  optScroll->setWidget(optContent);
  optLayoutMain->addWidget(optScroll);
  QScroller::grabGesture(optScroll->viewport(), QScroller::LeftMouseButtonGesture);

  settingsTabs->addTab(tabOptions, "Optionen");


  // -------------------------------------------------------------------------
  // TAB 2: TAGS & META
  // -------------------------------------------------------------------------
  QWidget *tabTags = new QWidget();
  m_pageSettingsTabTags = tabTags;
  QVBoxLayout *tagsLayoutMain = new QVBoxLayout(tabTags);
  tagsLayoutMain->setContentsMargins(16, 16, 16, 20);
  tagsLayoutMain->setSpacing(16);

  // Tags Section — wired to LibraryTagStore (same catalog as library shelf).
  tagsLayoutMain->addWidget(sectionLabel(QStringLiteral("TAGS"), tabTags));
  
  m_tagsContainer = new QWidget(tabTags);
  m_tagsContainer->setStyleSheet("background: transparent;");
  m_tagsFlowLayout = new QHBoxLayout(m_tagsContainer);
  m_tagsFlowLayout->setContentsMargins(0, 0, 0, 0);
  m_tagsFlowLayout->setSpacing(6);
  m_tagsFlowLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  tagsLayoutMain->addWidget(m_tagsContainer);

  QPushButton *btnAddTag = new QPushButton("+ Neuer Tag", tabTags);
  btnAddTag->setFixedHeight(32);
  btnAddTag->setCursor(Qt::PointingHandCursor);
  btnAddTag->setStyleSheet(
      "QPushButton {"
      "  background: rgba(255,255,255,0.05);"
      "  border: 1px dashed rgba(255,255,255,0.15);"
      "  border-radius: 6px;"
      "  color: rgba(255,255,255,0.5);"
      "  font-size: 11px; font-weight: 500;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(255,255,255,0.08);"
      "  border-color: rgba(255,255,255,0.25);"
      "  color: rgba(255,255,255,0.8);"
      "}");
  connect(btnAddTag, &QPushButton::clicked, this, [this]() {
    const QString raw = BlopDialogs::promptText(
        this, QStringLiteral("Neuer Tag"), QStringLiteral("Tag-Name:"));
    if (raw.isEmpty())
      return;
    if (!LibraryTagStore::addTagToCatalog(raw))
      return;
    const QString path = currentEditorNotePath();
    if (!path.isEmpty()) {
      QStringList tags = LibraryTagStore::tagsForPath(path);
      const QString n = LibraryTagStore::normalize(raw);
      bool exists = false;
      for (const QString &t : tags) {
        if (t.compare(n, Qt::CaseInsensitive) == 0) {
          exists = true;
          break;
        }
      }
      if (!exists)
        tags.append(n);
      LibraryTagStore::setTagsForPath(path, tags);
    }
    if (m_libraryTagsPanel)
      m_libraryTagsPanel->reload();
    rebuildPageSettingsTags();
    applyLibraryFilters();
  });
  tagsLayoutMain->addWidget(btnAddTag);
  rebuildPageSettingsTags();

  // Meta Section
  tagsLayoutMain->addSpacing(12);
  tagsLayoutMain->addWidget(sectionLabel(QStringLiteral("NOTIZ INFO"), tabTags));

  auto makeMetaRow = [&](const QString &label, const QString &value) -> QLabel* {
    QWidget *row = new QWidget(tabTags);
    QHBoxLayout *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(6);
    QLabel *lbl = new QLabel(label, row);
    lbl->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 11px;");
    QLabel *val = new QLabel(value, row);
    val->setStyleSheet("color: rgba(255,255,255,0.85); font-size: 11px; font-weight: 500;");
    rowLay->addWidget(lbl);
    rowLay->addStretch();
    rowLay->addWidget(val);
    tagsLayoutMain->addWidget(row);
    return val;
  };
  m_lblMetaCreated  = makeMetaRow("Erstellt:", "—");
  m_lblMetaModified = makeMetaRow("Geändert:", "—");

  tagsLayoutMain->addStretch();
  settingsTabs->addTab(tabTags, "Tags");

  midRow->addWidget(m_pageSettingsCard, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  midRow->addStretch(1);
  outerLay->addLayout(midRow);
  outerLay->addStretch(1);

  // v3.18.5: apply theme-aware styles to all inner controls after the
  // sheet is fully constructed. Light/Dark switches re-run this same
  // method via applyThemeRefresh().
  refreshPageSettingsTheme();
}

void MainWindow::refreshPageSettingsTheme() {
  // v3.18.5: centralized re-skinning for the Page-Settings sheet.
  // The original setupRightSidebar() baked in hardcoded #333 / #444 /
  // #14121f surfaces that were dark even after a Light-mode switch.
  // Pulling everything through BlopTheme tokens here makes the sheet
  // respect the current theme on every refresh.
  if (!m_pageSettingsCard)
    return;
  const bool isDark = BlopTheme::instance().isDark();
  const QString surfaceBg = BlopTheme::surfaceBase().name(QColor::HexRgb);
  const QString surfaceMuted = BlopTheme::surfaceMuted().name(QColor::HexRgb);
  const QString textPrimary = BlopTheme::textPrimary().name();
  const QString textSecondary = BlopTheme::textSecondary().name();
  const QString textOnAccent = BlopTheme::textOnAccent().name();
  const QString border = BlopTheme::borderSubtle().name();
  const QString accent = m_currentAccentColor.name();
  const QString divider = isDark ? QStringLiteral("rgba(255,255,255,0.07)")
                                 : QStringLiteral("rgba(0,0,0,0.08)");
  const QString subtleDivider = isDark ? QStringLiteral("rgba(255,255,255,0.05)")
                                       : QStringLiteral("rgba(0,0,0,0.06)");
  const QString tabInactive = isDark ? QStringLiteral("rgba(255,255,255,0.45)")
                                     : QStringLiteral("rgba(0,0,0,0.55)");
  const QString tabHover = isDark ? QStringLiteral("rgba(255,255,255,0.8)")
                                  : QStringLiteral("rgba(0,0,0,0.85)");

  m_pageSettingsCard->setStyleSheet(QStringLiteral(
      "QWidget#PageSettingsCard { background: %1; border: 1px solid %2;"
      "  border-radius: 18px; }")
      .arg(surfaceBg, border));

  if (m_pageSettingsTabs) {
    m_pageSettingsTabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: none; border-top: 1px solid %1; background: %2; }"
        "QTabWidget > QWidget { background: %2; }"
        "QTabBar::tab { background: transparent; color: %3;"
        "  padding: 8px 16px; font-size: 11px; font-weight: 600; border: none; }"
        "QTabBar::tab:selected { color: %4; border-bottom: 2px solid %5; }"
        "QTabBar::tab:hover:!selected { color: %6; }")
        .arg(subtleDivider, surfaceBg, tabInactive, textPrimary, accent, tabHover));
  }
  if (m_pageSettingsTabOptions)
    m_pageSettingsTabOptions->setStyleSheet(QStringLiteral("background: %1;").arg(surfaceBg));
  if (m_pageSettingsTabTags)
    m_pageSettingsTabTags->setStyleSheet(QStringLiteral("background: %1;").arg(surfaceBg));

  if (m_lblActiveNote) {
    m_lblActiveNote->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; font-weight: 600;"
        "padding: 12px 16px 8px 16px; background: transparent;")
        .arg(textPrimary));
  }

  const QString segmentBtnQss = QStringLiteral(
      "QPushButton { background: %1; color: %2; border: 1px solid %3;"
      "  padding: 10px; border-radius: 5px; }"
      "QPushButton:disabled { color: %4; }"
      "QPushButton:checked { background: %5; color: %6; border: 1px solid %5; }")
      .arg(surfaceMuted, textPrimary, border, textSecondary, accent, textOnAccent);
  for (QPushButton *b : {m_btnFormatInfinite, m_btnFormatA4, m_btnStyleBlank,
                          m_btnStyleLined, m_btnStyleSquared, m_btnStyleDotted,
                          m_btnColorWhite, m_btnColorDark}) {
    if (b)
      b->setStyleSheet(segmentBtnQss);
  }
  const QString inputBtnQss = QStringLiteral(
      "QPushButton { background: %1; color: %2; border: 1px solid %3;"
      "  padding: 10px; border-radius: 5px; text-align: left; }"
      "QPushButton:checked { background: %4; color: %5; border: 1px solid %4; }")
      .arg(surfaceMuted, textPrimary, border, accent, textOnAccent);
  if (m_btnInputPen) m_btnInputPen->setStyleSheet(inputBtnQss);
  if (m_btnInputTouch) m_btnInputTouch->setStyleSheet(inputBtnQss);

  const QString comboQss = QStringLiteral(
      "QComboBox { background: %1; color: %2; border: 1px solid %3;"
      "  padding: 5px; border-radius: 5px; }"
      "QComboBox::drop-down { border: 0px; }"
      "QComboBox QAbstractItemView { background: %1; color: %2;"
      "  selection-background-color: %4; }")
      .arg(surfaceMuted, textPrimary, border, accent);
  if (m_comboToolbarStyle) m_comboToolbarStyle->setStyleSheet(comboQss);
  if (m_comboProfiles) m_comboProfiles->setStyleSheet(comboQss);

  const QString sliderQss = QStringLiteral(
      "QSlider::groove:horizontal { border: 1px solid %1; height: 6px;"
      "  background: %2; margin: 2px 0; border-radius: 3px; }"
      "QSlider::handle:horizontal { background: %3; border: 1px solid %3;"
      "  width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }")
      .arg(border, surfaceMuted, accent);
  if (m_sliderGridSpacing) m_sliderGridSpacing->setStyleSheet(sliderQss);
  if (m_sliderToolbarScale) m_sliderToolbarScale->setStyleSheet(sliderQss);

  // Section labels and "plain" labels inside the option scroll area.
  // Iterate over QLabel children to re-tint them in one pass.
  for (QLabel *lbl : m_pageSettingsCard->findChildren<QLabel *>()) {
    if (lbl == m_lblActiveNote)
      continue;
    const QString existing = lbl->styleSheet();
    if (existing.contains(QStringLiteral("font-weight: 700")) &&
        existing.contains(QStringLiteral("letter-spacing"))) {
      // Section caption — keep small + muted.
      lbl->setStyleSheet(QStringLiteral(
          "color: %1; font-size: 10px; font-weight: 700;"
          "letter-spacing: 0.5px; background: transparent;")
          .arg(textSecondary));
    } else if (!existing.isEmpty() && lbl->text().endsWith(QStringLiteral(":"))) {
      lbl->setStyleSheet(QStringLiteral(
          "color: %1; background: transparent;").arg(textPrimary));
    }
  }

  if (style()) {
    style()->unpolish(m_pageSettingsCard);
    style()->polish(m_pageSettingsCard);
  }
  m_pageSettingsCard->update();
}

void MainWindow::updateInputMode(bool penOnly) {
  m_penOnlyMode = penOnly;
  if (CanvasView *cv = getCurrentCanvas()) {
    cv->setPenMode(penOnly);
  } else if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
    if (editor->view())
      editor->view()->setPenOnlyMode(penOnly);
  }
}

void MainWindow::onPageStyleButtonToggled(QAbstractButton *button,
                                          bool checked) {
  if (!checked)
    return;
  PageStyle style = (PageStyle)m_grpPageStyle->id(button);
  if (CanvasView *cv = getCurrentCanvas()) {
    cv->setPageStyle(style);
    cv->setGridSize(m_sliderGridSpacing->value());
  }
}

void MainWindow::onPageGridSpacingSliderChanged(int value) {
  m_gridSpacingTimer->start();
}
void MainWindow::applyDelayedGridSpacing() {
  if (CanvasView *cv = getCurrentCanvas()) {
    cv->setGridSize(m_sliderGridSpacing->value());
  }
}

void MainWindow::animateSidebar(bool show) {
  QRect r = sidebarPushContentRect();
  int containerHeight = r.height();
  if (containerHeight <= 0)
    containerHeight = qMax(100, height() - r.y());

  const int fullW = effectiveSidebarWidthPx();

  // Cancel any in-flight fold so we never accumulate fighting animations.
  const int animGen = ++m_sidebarAnimGeneration;
  if (m_sidebarAnim) {
    QObject::disconnect(m_sidebarAnim, nullptr, this, nullptr);
    m_sidebarAnim->stop();
    m_sidebarAnim->deleteLater();
    m_sidebarAnim.clear();
  }

  if (m_sidebarContainer) {
    // Layout min-size otherwise blocks setGeometry from shrinking to 0 —
    // that was the "only folds a few pixels" bug.
    m_sidebarContainer->setMinimumSize(0, 0);
    m_sidebarContainer->setMaximumWidth(fullW);
    if (QLayout *lay = m_sidebarContainer->layout())
      lay->setSizeConstraint(QLayout::SetNoConstraint);
    m_sidebarContainer->setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
  }

  m_sidebarAnimating = true;

  if (show) {
    m_isSidebarOpen = true;
    m_sidebarContainer->setGeometry(r.x(), r.y(), 0, containerHeight);
    m_sidebarContainer->raise();
    m_sidebarContainer->show();
#ifdef Q_OS_ANDROID
    if (m_androidSidebarScrim) {
      updateAndroidSidebarScrimGeometry();
      m_androidSidebarScrim->show();
      m_androidSidebarScrim->raise();
    }
#endif
  } else {
    m_sidebarContainer->raise();
  }

  auto *anim = new QVariantAnimation(this);
  m_sidebarAnim = anim;
  anim->setDuration(BlopMotion::kEmphasis);
  anim->setEasingCurve(BlopMotion::kEaseStandard);
  const int startW = show ? 0
                          : (m_sidebarContainer ? m_sidebarContainer->width()
                                                : fullW);
  anim->setStartValue(startW);
  anim->setEndValue(show ? fullW : 0);
  connect(anim, &QVariantAnimation::valueChanged, this,
          [this, fullW](const QVariant &v) {
            const int w = qBound(0, v.toInt(), fullW);
            QRect r2 = sidebarPushContentRect();
            int h2 = r2.height();
            if (h2 <= 0)
              h2 = qMax(100, height() - r2.y());
#ifdef Q_OS_ANDROID
            if (m_androidSidebarScrim) {
              updateAndroidSidebarScrimGeometry();
              m_androidSidebarScrim->setVisible(w > 0);
              m_androidSidebarScrim->raise();
            }
#else
            if (m_desktopSidebarPushSpacer)
              m_desktopSidebarPushSpacer->setFixedWidth(w);
#endif
            if (m_sidebarContainer) {
              m_sidebarContainer->setMaximumWidth(qMax(w, 1));
              m_sidebarContainer->setFixedWidth(w);
              m_sidebarContainer->setGeometry(r2.x(), r2.y(), w, h2);
            }
#ifdef Q_OS_ANDROID
            if (m_androidSidebarScrim)
              m_androidSidebarScrim->raise();
            if (m_sidebarContainer)
              m_sidebarContainer->raise();
#endif
            // Keep docked toolbar fully visible while sidebar animates.
            if (m_floatingTools) {
              if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools)) {
                if (m_editorCenterWidget) {
                  const int h = phone->preferredHeightPx();
                  const int margin = UiScale::dp(8);
                  const int avail =
                      qMax(UiScale::dp(180), m_editorCenterWidget->width());
                  const int dockW = qMin(avail - 2 * margin, UiScale::dp(360));
                  const int y = qMax(0, m_editorCenterWidget->height() - h -
                                            UiScale::safeBottomPx(this) -
                                            UiScale::dp(8));
                  phone->setGeometry((avail - dockW) / 2, y, dockW, h);
                  phone->raise();
                }
              } else if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
                if (tb->isDockedMode() && m_editorCenterWidget) {
                  int idealW = tb->calculateMinLength();
                  idealW = qMax(220, int(idealW * 0.90));
                  const int availableW = qMax(240, m_editorCenterWidget->width());
                  const int dockW = qMin(idealW, availableW);
                  int dockX = qMax(0, (availableW - dockW) / 2);
                  if (dockX + dockW > m_editorCenterWidget->width())
                    dockX = m_editorCenterWidget->width() - dockW;
                  const int headerH = noteHeaderHeight();
                  tb->setTopBound(headerH);
                  tb->setGeometry(dockX, headerH, dockW, 48);
                  tb->raise();
                  syncPenPresetBarGeometry();
                }
              }
            }
          });
  connect(anim, &QVariantAnimation::finished, this, [this, show, fullW, animGen]() {
    if (animGen != m_sidebarAnimGeneration)
      return; // superseded by a newer toggle
    m_sidebarAnimating = false;
    m_sidebarAnim.clear();
    if (m_sidebarContainer) {
      m_sidebarContainer->setMaximumWidth(fullW);
      m_sidebarContainer->setMinimumWidth(0);
      // Clear fixed-width lock from the animation ticks.
      m_sidebarContainer->setMinimumSize(0, 0);
      m_sidebarContainer->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    if (show) {
      syncSidebarPushLayout();
#ifdef Q_OS_ANDROID
      if (m_androidSidebarScrim) {
        updateAndroidSidebarScrimGeometry();
        m_androidSidebarScrim->raise();
      }
      if (m_sidebarContainer)
        m_sidebarContainer->raise();
#endif
    } else {
      m_isSidebarOpen = false;
      if (m_sidebarContainer)
        m_sidebarContainer->hide();
#ifdef Q_OS_ANDROID
      if (m_androidSidebarScrim)
        m_androidSidebarScrim->hide();
#else
      if (m_desktopSidebarPushSpacer)
        m_desktopSidebarPushSpacer->setFixedWidth(0);
#endif
    }
    updateGrid();
    updateSidebarState();
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}
void MainWindow::onToggleSidebar() {
#ifdef Q_OS_ANDROID
  const bool inNotesMode = m_mainContentStack && m_mainContentStack->currentIndex() == 0;
  if (!inNotesMode)
    return;
  // Debounce: Android touch events can fire clicked() twice (press+release propagation).
  static QElapsedTimer sidebarToggleCooldown;
  if (sidebarToggleCooldown.isValid() && sidebarToggleCooldown.elapsed() < 400)
    return;
  sidebarToggleCooldown.start();
#endif
  // Heal visibility/flag desync — otherwise a "close" click can start an open
  // animation on an already-visible drawer (looks like a tiny fold twitch).
  if (m_sidebarContainer) {
    if (m_sidebarContainer->isVisible() && !m_isSidebarOpen)
      m_isSidebarOpen = true;
    else if (!m_sidebarContainer->isVisible() && m_isSidebarOpen)
      m_isSidebarOpen = false;
  }
  animateSidebar(!m_isSidebarOpen);
}
void MainWindow::updateSidebarState() {
  // When the Study WebView is active, we must not treat the UI as "editor"
  // even if the last Notes tab was an editor.
  bool inNotesMode = true;
  if (m_mainContentStack)
    inNotesMode = (m_mainContentStack->currentIndex() == 0);
  bool isEditor = inNotesMode && (m_rightStack->currentWidget() == m_editorContainer);
  // Used for small UI morph animation when entering note editing.
  const bool prevIsEditor = m_lastIsEditor;
  const bool shouldMorphTopButtons =
      isEditor && !prevIsEditor && !m_isSidebarOpen;
  if (m_noteBottomChrome) {
    // Phone UI: page/zoom live in thumbnails + pinch; undo lives in the
    // phone toolbar. Never show the desktop notch here.
    const bool phoneUi =
        qobject_cast<AndroidPhoneToolbar *>(m_floatingTools) != nullptr;
    m_noteBottomChrome->setVisible(isEditor && !phoneUi);
  }
  if (isEditor)
    updateNoteBottomChrome();
  if (m_floatingTools) {
    m_floatingTools->setVisible(isEditor);
  }
  if (m_penPresetBar) {
    m_penPresetBar->hide();
  }
  if (m_editorTitleControls) {
    m_editorTitleControls->setVisible(isEditor);
  }
#ifndef Q_OS_ANDROID
  // Modus (Notizen / Study / …) immer sichtbar — auch in der Notiz, damit man wechseln kann.
  if (m_btnMode)
    m_btnMode->setVisible(true);
  // „+“ für Web-Lesezeichen: in Study und in der Notiz-Übersicht, nicht während des Schreibens.
  if (m_btnAddWebBookmark)
    m_btnAddWebBookmark->setVisible((inNotesMode && !isEditor) || !inNotesMode);
  if (m_documentTabBar)
    m_documentTabBar->setVisible(inNotesMode);
  if (m_noteLeftRail)
    m_noteLeftRail->setVisible(inNotesMode && isEditor &&
                               m_noteLeftRailPrefVisible);
  if (m_noteLeftRail && m_noteLeftRail->isVisible())
    m_noteLeftRail->setPageFeaturesVisible(currentNoteView() != nullptr);
  // When the icon strip is hidden, still allow the pages panel if it was open.
  if (m_pageThumbnailSidebar && inNotesMode && isEditor) {
    if (!m_noteLeftRailPrefVisible && m_pageThumbnailSidebar->isCollapsed())
      m_pageThumbnailSidebar->hide();
  }

  // Lock Drawboard vertical Favorites rail whenever the note editor is active.
  if (isEditor) {
    if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
#ifndef Q_OS_ANDROID
      // Desktop: always Favorites rail — never Radial/FAB.
      if (tb->currentStyle() != ModernToolbar::Normal)
        tb->setStyle(ModernToolbar::Normal);
#endif
      if (tb->currentStyle() == ModernToolbar::Normal)
        tb->applyDrawboardVerticalRail();
    }
    if (m_toolPropertiesPanel)
      m_toolPropertiesPanel->setVisible(m_toolPropertiesVisible);
  } else if (m_toolPropertiesPanel) {
    m_toolPropertiesPanel->hide();
  }

  // FAB only for Radial toolbar style (Android / legacy).
  if (m_radialFab) {
    bool showFab = false;
#ifndef Q_OS_ANDROID
    Q_UNUSED(inNotesMode);
    Q_UNUSED(isEditor);
#else
    if (inNotesMode && isEditor) {
      if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools))
        showFab = (tb->currentStyle() == ModernToolbar::Radial);
    }
#endif
    m_radialFab->setVisible(showFab);
  }

  bool hasA4Pages = false;
  if (isEditor && m_editorTabs) {
    if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget()))
      hasA4Pages = (editor->view() != nullptr);
  }
  if (m_pageThumbnailSidebar) {
    const bool pagesWantedByRail =
        !m_noteLeftRail || !m_noteLeftRail->isVisible() ||
        m_noteLeftRail->pagesExpanded();
    const bool wantPages =
        inNotesMode && isEditor && hasA4Pages && pagesWantedByRail;
    // Respect an intentional collapse — never force-expand here (that left a
    // stubborn stub strip when the user tried to fold the pages rail).
    const bool showPages =
        wantPages && !m_pageThumbnailSidebar->isCollapsed();
    m_pageThumbnailSidebar->setVisible(showPages);
    if (showPages)
      m_pageThumbnailSidebar->rebuild();
  }
  if (isEditor) {
    // Keep library sidebar state as the user left it (hamburger toggles).
    positionNoteChrome();
  }
  // Title-bar search competes with document tabs while editing; overview has its own.
  if (m_titleSearchBar)
    m_titleSearchBar->setVisible(inNotesMode && !isEditor);
  {
    bool showNoteOverflow = false;
    if (isEditor && m_editorTabs) {
      // A4 NoteEditor and infinite CanvasView both get the title-bar ⋯.
      if (qobject_cast<NoteEditor *>(m_editorTabs->currentWidget()) ||
          getCurrentCanvas())
        showNoteOverflow = true;
    }
    if (m_btnEditorNoteOverflow)
      m_btnEditorNoteOverflow->setVisible(inNotesMode && showNoteOverflow);
    // Pages toggle lives on the left rail — hide redundant title-bar pill.
    if (m_btnTitleBarPageManager)
      m_btnTitleBarPageManager->setVisible(false);
  }
#endif
  
  if (m_isSidebarOpen) {
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->hide();
    // Keep Android top-bar buttons visible even when the left sidebar is open.
    // Continue below so isEditor/overview visibility logic is applied.
    m_lastIsEditor = isEditor;
  }
#ifdef Q_OS_ANDROID
  if (m_btnAndroidHome)
    m_btnAndroidHome->setVisible(inNotesMode);
  // Overview: floating menu next to welcome; Editor: compact hamburger in top bar
  if (!inNotesMode) {
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->hide();
    if (m_btnAndroidHome)
      m_btnAndroidHome->hide();
    if (m_btnAndroidToolbarMenu)
      m_btnAndroidToolbarMenu->hide();
    if (m_btnAndroidToolbarPageManager)
      m_btnAndroidToolbarPageManager->hide();
    if (m_btnAndroidToolbarExport)
      m_btnAndroidToolbarExport->hide();
    if (m_androidTopSearchBar) {
      m_androidTopSearchBar->clear();
      m_androidTopSearchBar->hide();
    }
    if (m_documentTabBar)
      m_documentTabBar->hide();
    if (m_pageThumbnailSidebar)
      m_pageThumbnailSidebar->setVisible(false);
    if (m_btnAndroidNotes)
      m_btnAndroidNotes->show();
    if (m_btnAndroidStudy)
      m_btnAndroidStudy->show();
    syncAndroidHeaderGeometry(this);
  } else if (isEditor) {
    if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
      // Avoid random floating state on Android.
      tb->setDockMode(true);
      tb->setDraggable(false);
    }
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->hide();
    if (m_btnAndroidToolbarMenu)
      m_btnAndroidToolbarMenu->setVisible(!m_isSidebarOpen);
    // Page manager lives in note overflow; keep header calm for document tabs.
    if (m_btnAndroidToolbarPageManager)
      m_btnAndroidToolbarPageManager->setVisible(false);
    if (m_btnAndroidToolbarExport)
      m_btnAndroidToolbarExport->setVisible(true);
    if (m_androidTopSearchBar)
      m_androidTopSearchBar->hide();
    // Editor: document tabs replace Notizen/Study pills as primary chrome.
    if (m_documentTabBar)
      m_documentTabBar->setVisible(true);
    if (m_btnAndroidNotes)
      m_btnAndroidNotes->hide();
    if (m_btnAndroidStudy)
      m_btnAndroidStudy->hide();
    if (m_btnAndroidAddWebBookmark)
      m_btnAndroidAddWebBookmark->hide();
    if (m_pageThumbnailSidebar)
      m_pageThumbnailSidebar->setVisible(currentNoteView() != nullptr);
    syncAndroidHeaderGeometry(this);

    if (shouldMorphTopButtons) {
      auto animateScale = [](ModernButton *btn) {
        if (!btn)
          return;
        btn->setButtonScale(0.70);
        btn->update();
        QPropertyAnimation *a =
            new QPropertyAnimation(btn, "buttonScale", btn);
        a->setDuration(BlopMotion::kStandard);
        a->setStartValue(0.70);
        a->setEndValue(1.0);
        a->setEasingCurve(BlopMotion::kEaseOvershoot);
        a->start(QAbstractAnimation::DeleteWhenStopped);
      };
      animateScale(m_btnAndroidToolbarExport);
    }
  } else {
    m_sidebarStrip->show();
    // Kein zweites Burger-Menü auf der Übersicht: Navigation nur über Top-Leiste + schmalen Strip.
    if (btnEditorMenu)
      btnEditorMenu->hide();
    // Ein Hamburger: in der Top-Leiste (nicht doppelt neben „Willkommen“).
    if (m_btnAndroidToolbarMenu)
      m_btnAndroidToolbarMenu->setVisible(!m_isSidebarOpen);
    if (m_btnAndroidToolbarPageManager)
      m_btnAndroidToolbarPageManager->setVisible(false);
    if (m_btnAndroidToolbarExport)
      m_btnAndroidToolbarExport->setVisible(false);
    if (m_androidTopSearchBar) {
      m_androidTopSearchBar->clear();
      m_androidTopSearchBar->hide();
    }
    if (m_documentTabBar)
      m_documentTabBar->hide();
    if (m_btnAndroidNotes)
      m_btnAndroidNotes->show();
    if (m_btnAndroidStudy)
      m_btnAndroidStudy->show();
    if (m_btnAndroidAddWebBookmark)
      m_btnAndroidAddWebBookmark->setVisible(true);
    if (m_pageThumbnailSidebar)
      m_pageThumbnailSidebar->setVisible(false);
    if (m_pageSettingsOverlay && m_pageSettingsOverlay->isVisible())
      setPageSettingsOverlayVisible(false);
    syncAndroidHeaderGeometry(this);
  }
#else
  if (isEditor) {
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->show();
    if (btnOverviewMenu)
      btnOverviewMenu->hide();
  } else {
    // Übersicht: Hamburger nur wenn Sidebar eingeklappt — sonst kein Re-Open.
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->hide();
    if (btnOverviewMenu)
      btnOverviewMenu->setVisible(!m_isSidebarOpen);
  }
#endif

  m_lastIsEditor = isEditor;
}

void MainWindow::setPageSettingsOverlayVisible(bool show) {
  // v3.17.0: route through BlopModal. On Android phones this gives us a
  // proper bottom-sheet (rounded top corners, drag-to-dismiss); on desktop
  // and tablets we get a centered card with backdrop fade-in. The legacy
  // m_pageSettingsOverlay scrim is no longer used -- BlopModal supplies
  // its own backdrop and outside-tap dismissal.
  if (!m_pageSettingsCard)
    return;

  if (show) {
    if (m_pageSettingsModal)
      return; // already presented
    syncPageSettingsPanelFromEditor();
    rebuildPageSettingsTags();
    // BlopModal will size the card via its layout; release the fixed-width
    // constraint that the legacy scrim used.
    m_pageSettingsCard->setMinimumWidth(0);
    m_pageSettingsCard->setMaximumWidth(QWIDGETSIZE_MAX);
    m_pageSettingsCard->setMaximumHeight(QWIDGETSIZE_MAX);
    // Strip the legacy outer card stylesheet -- BlopModal hosts the card
    // inside a themed surface, so a second border would look chunky.
    m_pageSettingsCard->setStyleSheet(
        QStringLiteral("QWidget#PageSettingsCard { background: transparent; }"));
    m_pageSettingsCard->show();
    m_pageSettingsModal = BlopModal::present(this, m_pageSettingsCard,
                                             BlopModal::Mode::Auto);
    if (m_pageSettingsModal) {
      m_pageSettingsModal->setPreferredCardWidth(520);
      connect(m_pageSettingsModal, &BlopModal::dismissed, this, [this]() {
        if (m_pageSettingsCard && m_pageSettingsOverlay) {
          // Re-parent the card back to its original home so the next show
          // finds it where setupRightSidebar() expects.
          m_pageSettingsCard->setParent(m_pageSettingsOverlay);
          m_pageSettingsCard->hide();
        }
        m_pageSettingsModal = nullptr;
      });
    }
  } else {
    if (m_pageSettingsModal) {
      m_pageSettingsModal->dismiss();
    }
  }
}

void MainWindow::syncPageSettingsPanelFromEditor() {
  if (!m_editorTabs)
    return;
  QWidget *current = m_editorTabs->currentWidget();
  CanvasView *cv = getCurrentCanvas();
  NoteEditor *editor = qobject_cast<NoteEditor *>(current);
  const bool canvasNote = (cv != nullptr);
  const bool a4Note = (editor != nullptr);

  // Canvas-only page chrome looks unfinished on A4 notes (no-op buttons).
  auto setNamedVisible = [&](const char *objectName) {
    if (!m_pageSettingsCard)
      return;
    if (QWidget *w = m_pageSettingsCard->findChild<QWidget *>(
            QString::fromLatin1(objectName)))
      w->setVisible(canvasNote);
  };
  setNamedVisible("pageSettingsLayoutLabel");
  setNamedVisible("pageSettingsStyleRow");
  setNamedVisible("pageSettingsGridLabel");
  setNamedVisible("pageSettingsGridSlider");
  setNamedVisible("pageSettingsColorLabel");
  if (m_btnColorWhite)
    m_btnColorWhite->setVisible(canvasNote);
  if (m_btnColorDark)
    m_btnColorDark->setVisible(canvasNote);

#ifndef Q_OS_ANDROID
  if (m_pageSettingsCard) {
    if (auto *cb = m_pageSettingsCard->findChild<QCheckBox *>(
            QStringLiteral("pageSettingsLeftRailVisible"))) {
      QSignalBlocker b(cb);
      cb->setChecked(m_noteLeftRailPrefVisible);
    }
    if (auto *cb = m_pageSettingsCard->findChild<QCheckBox *>(
            QStringLiteral("pageSettingsPagesVisible"))) {
      QSignalBlocker b(cb);
      cb->setChecked(m_pageThumbnailSidebar &&
                     !m_pageThumbnailSidebar->isCollapsed() &&
                     m_pageThumbnailSidebar->isVisible());
    }
  }
#endif

  if (cv) {
    bool inf = cv->isInfinite();
    m_btnFormatInfinite->setChecked(inf);
    m_btnFormatA4->setChecked(!inf);
    m_btnInputPen->setChecked(m_penOnlyMode);
    m_btnInputTouch->setChecked(!m_penOnlyMode);
    bool isDark = (cv->pageColor() == UIStyles::SceneBackground);
    m_btnColorDark->setChecked(isDark);
    m_btnColorWhite->setChecked(!isDark);
    int styleId = (int)cv->pageStyle();
    QAbstractButton *styleBtn = m_grpPageStyle->button(styleId);
    if (styleBtn)
      styleBtn->setChecked(true);
    m_sliderGridSpacing->blockSignals(true);
    m_sliderGridSpacing->setValue(cv->gridSize());
    m_sliderGridSpacing->blockSignals(false);
  } else if (editor) {
    m_btnFormatInfinite->setChecked(false);
    m_btnFormatA4->setChecked(true);
    m_btnInputPen->setChecked(m_penOnlyMode);
    m_btnInputTouch->setChecked(!m_penOnlyMode);
    Q_UNUSED(a4Note);
  }
  ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools);
  if (tb) {
    if (tb->currentStyle() == ModernToolbar::Normal) {
      m_comboToolbarStyle->setCurrentIndex(0);
    } else {
      if (tb->radialType() == ModernToolbar::FullCircle)
        m_comboToolbarStyle->setCurrentIndex(1);
      else
        m_comboToolbarStyle->setCurrentIndex(2);
    }
    if (m_sliderToolbarScale)
      m_sliderToolbarScale->setValue(tb->scale() * 100);
  }
}

void MainWindow::onEditorNoteOverflowMenu() {
  if (!m_editorTabs)
    return;

#ifdef Q_OS_ANDROID
  QWidget *anchor = m_btnAndroidToolbarExport;
#else
  QWidget *anchor = m_btnEditorNoteOverflow;
#endif
  if (!anchor)
    return;

  if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
    editor->showOverflowMenuFromAnchor(anchor);
    return;
  }

  // Infinite CanvasView notes: previously Android header ⋯ was a dead button
  // because only NoteEditor was handled. Offer the working in-window actions.
  CanvasView *cv = getCurrentCanvas();
  if (!cv)
    return;

  const QPoint globalPos =
      anchor->mapToGlobal(QPoint(anchor->width() / 2, anchor->height()));
  QList<BlopInWindowMenu::Item> items;
  items.append({QStringLiteral("Optionen & Tags…"), QIcon(),
                [this]() { setPageSettingsOverlayVisible(true); }});
  items.append({QString(), QIcon(), {}, false, true});
  items.append({QStringLiteral("An Breite anpassen"), QIcon(),
                [this, cv]() {
                  if (cv) {
                    cv->fitToWidth();
                    updateNoteBottomChrome();
                  }
                }});
  items.append({QStringLiteral("Ganze Seite / Inhalt"), QIcon(),
                [this, cv]() {
                  if (cv) {
                    cv->fitPage();
                    updateNoteBottomChrome();
                  }
                }});
#ifdef Q_OS_ANDROID
  items.append({QString(), QIcon(), {}, false, true});
  items.append({QStringLiteral("Als PDF exportieren"), QIcon(),
                [this, cv]() {
                  if (!cv)
                    return;
                  const QString dir = QStandardPaths::writableLocation(
                      QStandardPaths::DocumentsLocation);
                  QDir().mkpath(dir);
                  const QString path =
                      dir +
                      QStringLiteral("/Blop_%1.pdf")
                          .arg(QDateTime::currentDateTime().toString(
                              QStringLiteral("yyyyMMdd_HHmmss")));
                  const bool ok = cv->exportToPDF(path);
                  BlopDialogs::notify(
                      this,
                      ok ? QStringLiteral("Exportiert")
                         : QStringLiteral("Fehler"),
                      ok ? QStringLiteral("PDF gespeichert:\n%1").arg(path)
                         : QStringLiteral("PDF-Export fehlgeschlagen."));
                }});
  items.append({QStringLiteral("Als Bild exportieren"), QIcon(),
                [this, cv]() {
                  if (!cv)
                    return;
                  const QString dir = QStandardPaths::writableLocation(
                      QStandardPaths::PicturesLocation);
                  QDir().mkpath(dir);
                  const QString path =
                      dir +
                      QStringLiteral("/Blop_%1.png")
                          .arg(QDateTime::currentDateTime().toString(
                              QStringLiteral("yyyyMMdd_HHmmss")));
                  const bool ok = cv->exportToImage(path);
                  BlopDialogs::notify(
                      this,
                      ok ? QStringLiteral("Exportiert")
                         : QStringLiteral("Fehler"),
                      ok ? QStringLiteral("Bild gespeichert:\n%1").arg(path)
                         : QStringLiteral("Bild-Export fehlgeschlagen."));
                }});
  items.append({QStringLiteral("PDF importieren"), QIcon(), [this, cv]() {
                  if (!cv)
                    return;
                  AndroidContentPicker::instance().pickOpen(
                      {QStringLiteral("application/pdf")},
                      [this, cv](const QString &localPath) {
                        if (localPath.isEmpty() || !cv)
                          return;
                        const bool ok = cv->importPdfIntoCanvas(localPath);
                        BlopDialogs::notify(
                            this,
                            ok ? QStringLiteral("Importiert")
                               : QStringLiteral("Fehler"),
                            ok ? QStringLiteral(
                                     "PDF wurde in die unendliche Seite "
                                     "eingefügt.")
                               : QStringLiteral(
                                     "PDF konnte nicht importiert werden."));
                      });
                }});
  BlopInWindowMenu::show(this, globalPos, items);
#else
  // Desktop infinite: same export/options actions as the floating ⋯.
  items.append({QString(), QIcon(), {}, false, true});
  items.append({QStringLiteral("Als PDF exportieren"), QIcon(),
                [this, cv]() {
                  if (!cv)
                    return;
                  const QString path = QFileDialog::getSaveFileName(
                      this, QStringLiteral("Als PDF exportieren"),
                      QStandardPaths::writableLocation(
                          QStandardPaths::DocumentsLocation) +
                          QStringLiteral("/Blop.pdf"),
                      QStringLiteral("PDF Dokument (*.pdf)"));
                  if (path.isEmpty())
                    return;
                  const bool ok = cv->exportToPDF(path);
                  BlopDialogs::notify(
                      this,
                      ok ? QStringLiteral("Exportiert")
                         : QStringLiteral("Fehler"),
                      ok ? QStringLiteral("PDF gespeichert:\n%1").arg(path)
                         : QStringLiteral("PDF-Export fehlgeschlagen."));
                }});
  items.append({QStringLiteral("Als Bild exportieren"), QIcon(),
                [this, cv]() {
                  if (!cv)
                    return;
                  const QString path = QFileDialog::getSaveFileName(
                      this, QStringLiteral("Als Bild exportieren"),
                      QStandardPaths::writableLocation(
                          QStandardPaths::PicturesLocation) +
                          QStringLiteral("/Blop.png"),
                      QStringLiteral("PNG Bild (*.png)"));
                  if (path.isEmpty())
                    return;
                  const bool ok = cv->exportToImage(path);
                  BlopDialogs::notify(
                      this,
                      ok ? QStringLiteral("Exportiert")
                         : QStringLiteral("Fehler"),
                      ok ? QStringLiteral("Bild gespeichert:\n%1").arg(path)
                         : QStringLiteral("Bild-Export fehlgeschlagen."));
                }});
  BlopInWindowMenu::show(this, globalPos, items);
#endif
}

void MainWindow::onTogglePageManager() {
#ifndef Q_OS_ANDROID
  // Desktop: toggle the floating page-thumbnail rail completely (no stub).
  if (m_pageThumbnailSidebar && m_noteLeftRail) {
    const bool on = !m_noteLeftRail->pagesExpanded();
    m_noteLeftRail->setPagesExpanded(on);
    if (on) {
      m_pageThumbnailSidebar->setCollapsed(false);
      m_pageThumbnailSidebar->show();
      m_pageThumbnailSidebar->rebuild();
    } else {
      m_pageThumbnailSidebar->setCollapsed(true);
      m_pageThumbnailSidebar->hide();
    }
    positionNoteChrome();
    return;
  }
  if (m_pageThumbnailSidebar && m_pageThumbnailSidebar->isVisible()) {
    m_pageThumbnailSidebar->setCollapsed(true);
    m_pageThumbnailSidebar->hide();
    positionNoteChrome();
    return;
  }
#endif
  if (!m_pageManager)
    return;
  if (m_pageManager->isVisible()) {
    m_pageManager->hide();
  } else {
    if (m_isSidebarOpen)
      animateSidebar(false);
    m_pageManager->raise();
    QWidget *current = m_editorTabs->currentWidget();
    MultiPageNoteView *mpv = nullptr;
    if (NoteEditor *editor = qobject_cast<NoteEditor *>(current)) {
      mpv = editor->view();
    } else {
      mpv = qobject_cast<MultiPageNoteView *>(current);
    }
    if (mpv) {
      m_pageManager->setNoteView(mpv);
      // v3.16.1: scrollToPage now takes a `bool animate=true` default arg, so
      // the bare member-function pointer no longer matches the int-only signal
      // signature. Wrap in a lambda that explicitly passes animate=true.
      connect(m_pageManager, &PageManager::pageSelected, mpv,
              [mpv](int idx) { mpv->scrollToPage(idx, true); },
              Qt::UniqueConnection);
      QObject::disconnect(m_pageManager, &PageManager::pageOrderChanged, nullptr,
                          nullptr);
      connect(m_pageManager, &PageManager::pageOrderChanged, [mpv, this]() {
        if (mpv->onSaveRequested && mpv->note())
          mpv->onSaveRequested(mpv->note());
      });
      m_pageManager->show();
      m_pageManager->refreshThumbnails();
    } else {
#ifdef Q_OS_ANDROID
      showAndroidToast(this,
                       QStringLiteral("Seitenmanager nur für A4-Notizen"));
#else
      QMessageBox::information(
          this, "Nicht verfügbar",
          "Der Seitenmanager ist nur für A4-Notizen verfügbar.");
#endif
    }
  }
}

void MainWindow::openNotePath(const QString &absolutePath) {
  if (absolutePath.isEmpty() || !QFile::exists(absolutePath)) {
    qWarning() << "openNotePath: missing file" << absolutePath;
    return;
  }
  if (!m_fileModel) {
    qWarning() << "openNotePath: file model not ready";
    return;
  }
  // Ensure Notes mode + overview root can resolve the index.
  if (m_modeSelector && m_modeSelector->currentIndex() != 0)
    m_modeSelector->setCurrentIndex(0);
  const QModelIndex idx = m_fileModel->index(absolutePath);
  if (!idx.isValid()) {
    // File may live outside current root — still open via temporary index.
    m_fileModel->setRootPath(QFileInfo(absolutePath).absolutePath());
  }
  const QModelIndex openIdx = m_fileModel->index(absolutePath);
  if (openIdx.isValid())
    onFileDoubleClicked(openIdx);
  else
    qWarning() << "openNotePath: cannot resolve model index for" << absolutePath;
}

void MainWindow::onFileDoubleClicked(const QModelIndex &index) {
  BlopDiag::recordUiAction(QStringLiteral("open_note"));
  if (m_fileModel->isDir(index)) {
    navigateLibraryToPath(m_fileModel->filePath(index));
  } else {
    QString path = m_fileModel->filePath(index);
    LibraryOrgStore::touchRecent(path);
    QString fileName = index.data().toString();
    bool isBinary = false;
    {
      QFile f(path);
      if (f.open(QIODevice::ReadOnly)) {
        QDataStream in(&f);
        quint32 magic;
        in >> magic;
        if (magic == 0xB10B0001 || magic == 0xB10B0002)
          isBinary = true;
        f.close();
      }
    }

    if (isBinary) {
      CanvasView *canvas = new CanvasView(this);

      // WICHTIG: Manager verbinden
      if (m_toolManager)
        canvas->setToolManager(m_toolManager);

      canvas->setPenColor(m_penColor);
      canvas->setPenWidth(m_penWidth);
      canvas->setPenMode(m_penOnlyMode);
      canvas->setFilePath(path);
      canvas->loadFromFile();
      connect(canvas, &CanvasView::contentModified, this,
              &MainWindow::onContentModified);

      // Wrapper widget so we can overlay a 3-dot action button on top of the canvas
      QWidget *canvasWrapper = new QWidget(this);
      canvasWrapper->setContentsMargins(0,0,0,0);
      canvasWrapper->setStyleSheet("background-color: #0D0B14;");
      QVBoxLayout *cwLay = new QVBoxLayout(canvasWrapper);
      cwLay->setContentsMargins(0,0,0,0);
      cwLay->setSpacing(0);
      cwLay->addWidget(canvas);

#ifndef Q_OS_ANDROID
      // Floating 3-dot note action button (top-right, pinned via Move).
      // Desktop-only: on Android the same actions are exposed via
      // m_btnAndroidToolbarExport (header `more_pill`) which calls into
      // MainWindow::onEditorNoteOverflowMenu -> NoteEditor::showOverflowMenuFromAnchor.
      // Keeping the canvas-floating button on Android would (a) duplicate the
      // header `more_pill` visually and (b) crash because the click handler
      // below uses menu->exec / QFileDialog / QMessageBox / QInputDialog,
      // which are all top-level widgets on Android's single-window surface.
      QPushButton *noteBtn = new QPushButton("\u22EF", canvasWrapper);
      noteBtn->setFixedSize(36, 36);
      noteBtn->setCursor(Qt::PointingHandCursor);
      noteBtn->setToolTip("Notiz-Aktionen");
      noteBtn->setAttribute(Qt::WA_TranslucentBackground);
      noteBtn->setStyleSheet(
          "QPushButton {"
          "  background: rgba(30,30,50,0.80);"
          "  border: 1px solid rgba(108,92,231,0.55);"
          "  border-radius: 8px; color: #D8D5FF;"
          "  font-size: 18px; font-weight: 700;"
          "}"
          "QPushButton:hover {"
          "  background: rgba(108,92,231,0.90);"
          "  color: white;"
          "}");
      noteBtn->raise();
      // Position it once the wrapper is laid out
      QObject::connect(canvas, &QGraphicsView::destroyed, noteBtn, [noteBtn](){
          noteBtn->deleteLater();
      });
      // We use a real-time resize connection through an event filter lambda wrapper:
      auto *posUpdater = new QObject(canvasWrapper);
      QObject::connect(canvasWrapper, &QWidget::destroyed, posUpdater, [posUpdater](){
          posUpdater->deleteLater();
      });
      // Position on first show via single-shot timer
      QTimer::singleShot(0, noteBtn, [noteBtn, canvasWrapper](){
          noteBtn->move(canvasWrapper->width() - noteBtn->width() - 12, 12);
          noteBtn->raise();
      });
      QString capPath = path;
      connect(noteBtn, &QPushButton::clicked, this, [this, canvas, capPath](){
          QMenu *menu = new QMenu(this);
          menu->setAttribute(Qt::WA_DeleteOnClose);
          menu->setStyleSheet(
              "QMenu { background: #1E1E2E; border: 1px solid #6C5CE7;"
              "  border-radius: 8px; padding: 6px; }"
              "QMenu::item { color: #D8D5FF; padding: 10px 20px;"
              "  border-radius: 5px; font-size: 13px; }"
              "QMenu::item:selected { background: #6C5CE7; color: white; }");
          auto *actLayout = menu->addAction("Seitenlayout...");
          auto *actOptions = menu->addAction("Optionen & Tags...");
          menu->addSeparator();
          auto *actPdf = menu->addAction("\U00002714  Als PDF exportieren");
          auto *actImg = menu->addAction("\U0001F5BC  Als Bild exportieren");
          menu->addSeparator();
          auto *actImportPdf = menu->addAction("PDF importieren");
          auto *actShareUser = menu->addAction("Mit Username teilen...");
          auto *actCreateLink = menu->addAction("Share-Link erstellen...");
          auto *actImportLink = menu->addAction("Datei aus Link importieren...");
          auto *chosen = menu->exec(QCursor::pos());
          QFileInfo fi(capPath);
          if (chosen == actLayout || chosen == actOptions) {
              setPageSettingsOverlayVisible(true);
          } else if (chosen == actPdf) {
              QString out = QFileDialog::getSaveFileName(this, "Als PDF exportieren",
                  fi.baseName() + ".pdf", "PDF (*.pdf)");
              if (!out.isEmpty()) {
                  bool ok = canvas->exportToPDF(out);
                  if (ok) BlopDialogs::notify(this, QStringLiteral("Exportiert"),
                                              QStringLiteral("PDF gespeichert!"));
                  else    BlopDialogs::notify(this, QStringLiteral("Fehler"),
                                              QStringLiteral("PDF fehlgeschlagen."));
              }
          } else if (chosen == actImg) {
              QString out = QFileDialog::getSaveFileName(this, "Als Bild exportieren",
                  fi.baseName() + ".png", "Bilder (*.png *.jpg)");
              if (!out.isEmpty()) {
                  bool ok = canvas->exportToImage(out);
                  if (ok) BlopDialogs::notify(this, QStringLiteral("Exportiert"),
                                              QStringLiteral("Bild gespeichert!"));
                  else    BlopDialogs::notify(this, QStringLiteral("Fehler"),
                                              QStringLiteral("Bild fehlgeschlagen."));
              }
          } else if (chosen == actImportPdf) {
              QString in = QFileDialog::getOpenFileName(this, "PDF importieren",
                  QString(), "PDF (*.pdf)");
              if (!in.isEmpty()) {
                  bool ok = canvas->importPdfIntoCanvas(in);
                  if (ok) BlopDialogs::notify(this, QStringLiteral("Importiert"),
                                              QStringLiteral("PDF wurde in die unendliche Seite eingefügt."));
                  else    BlopDialogs::notify(this, QStringLiteral("Fehler"),
                                              QStringLiteral("PDF konnte nicht importiert werden."));
              }
          } else if (chosen == actShareUser) {
              const QString username =
                  QSettings("Blop", "BlopApp").value("username").toString().trimmed();
              if (username.isEmpty()) {
                  BlopDialogs::notify(this, QStringLiteral("Nicht angemeldet"),
                                      QStringLiteral("Bitte zuerst in Blop Study anmelden."));
                  return;
              }
              const QString localPath = capPath;
              QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
              if (fileId.isEmpty()) {
                  fileId = BlopDialogs::promptText(
                      this, QStringLiteral("Cloud-Datei-ID"),
                      QStringLiteral("Datei-ID im Blop-Study-Cloudspeicher:"),
                      fi.baseName()).trimmed();
                  if (fileId.isEmpty()) return;
              } else {
                  BlopDialogs::notify(this, QStringLiteral("Cloud-Datei erkannt"),
                                      QStringLiteral("Automatisch erkannt: %1").arg(fileId));
              }
              const QString target = BlopDialogs::promptText(
                  this, QStringLiteral("Zielnutzer"),
                  QStringLiteral("Username des Empfängers:"), QString()).trimmed();
              if (target.isEmpty()) return;
              const QString message = BlopDialogs::promptText(
                  this, QStringLiteral("Nachricht (optional)"),
                  QStringLiteral("Begleitnachricht:"), QString());

              QNetworkRequest req(QUrl(kBlopStudyUrl + "/api/shares/username"));
              req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
              QJsonObject payload{
                  {"username", username},
                  {"file_id", fileId},
                  {"target_username", target},
                  {"message", message},
              };
              QNetworkReply *reply = m_netManager->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
              QEventLoop loop;
              connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
              loop.exec();
              const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
              const QByteArray raw = reply->readAll();
              if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
                  BlopDialogs::notify(this, QStringLiteral("Teilen fehlgeschlagen"),
                                      QStringLiteral("Serverantwort (%1):\n%2")
                                          .arg(status).arg(QString::fromUtf8(raw)));
              } else {
                  BlopDialogs::notify(this, QStringLiteral("Request gesendet"),
                                      QStringLiteral("Die Freigabeanfrage wurde an den Zielnutzer gesendet."));
              }
              reply->deleteLater();
          } else if (chosen == actCreateLink) {
              const QString username =
                  QSettings("Blop", "BlopApp").value("username").toString().trimmed();
              if (username.isEmpty()) {
                  BlopDialogs::notify(this, QStringLiteral("Nicht angemeldet"),
                                      QStringLiteral("Bitte zuerst in Blop Study anmelden."));
                  return;
              }
              const QString localPath = capPath;
              QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
              if (fileId.isEmpty()) {
                  fileId = BlopDialogs::promptText(
                      this, QStringLiteral("Cloud-Datei-ID"),
                      QStringLiteral("Datei-ID im Blop-Study-Cloudspeicher:"),
                      fi.baseName()).trimmed();
                  if (fileId.isEmpty()) return;
              } else {
                  BlopDialogs::notify(this, QStringLiteral("Cloud-Datei erkannt"),
                                      QStringLiteral("Automatisch erkannt: %1").arg(fileId));
              }
              bool ok = false;
              const int expiresDays = BlopDialogs::promptInt(
                  this, QStringLiteral("Gültigkeit"),
                  QStringLiteral("Link gültig für (Tage):"), 7, 1, 30, &ok);
              if (!ok) return;
              const int maxUses = BlopDialogs::promptInt(
                  this, QStringLiteral("Nutzungslimit"),
                  QStringLiteral("Maximale Nutzungen:"), 1, 1, 100, &ok);
              if (!ok) return;

              QNetworkRequest req(QUrl(kBlopStudyUrl + "/api/shares/link"));
              req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
              QJsonObject payload{
                  {"username", username},
                  {"file_id", fileId},
                  {"expires_in_days", expiresDays},
                  {"max_uses", maxUses},
              };
              QNetworkReply *reply = m_netManager->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
              QEventLoop loop;
              connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
              loop.exec();
              const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
              const QByteArray raw = reply->readAll();
              if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
                  BlopDialogs::notify(this, QStringLiteral("Link fehlgeschlagen"),
                                      QStringLiteral("Serverantwort (%1):\n%2")
                                          .arg(status).arg(QString::fromUtf8(raw)));
                  reply->deleteLater();
                  return;
              }
              const QJsonDocument doc = QJsonDocument::fromJson(raw);
              const QString link = doc.object().value("url").toString();
              if (!link.isEmpty())
                  QGuiApplication::clipboard()->setText(link);
              BlopDialogs::notify(this, QStringLiteral("Link erstellt"),
                                  link.isEmpty()
                                      ? QStringLiteral("Share-Link wurde erstellt.")
                                      : QStringLiteral("Share-Link wurde erstellt und kopiert:\n%1").arg(link));
              reply->deleteLater();
          } else if (chosen == actImportLink) {
              const QString username =
                  QSettings("Blop", "BlopApp").value("username").toString().trimmed();
              if (username.isEmpty()) {
                  BlopDialogs::notify(this, QStringLiteral("Nicht angemeldet"),
                                      QStringLiteral("Bitte zuerst in Blop Study anmelden."));
                  return;
              }
              QString linkOrToken = BlopDialogs::promptText(
                  this, QStringLiteral("Share-Link"),
                  QStringLiteral("Share-Link oder Token einfügen:"), QString()).trimmed();
              if (linkOrToken.isEmpty()) return;
              if (linkOrToken.contains(QStringLiteral("/"))) {
                  const QString marker = QStringLiteral("/share/");
                  const int pos = linkOrToken.lastIndexOf(marker);
                  if (pos >= 0) linkOrToken = linkOrToken.mid(pos + marker.size());
                  else linkOrToken = linkOrToken.section('/', -1).trimmed();
              }
              const QString targetFolderId = chooseCloudFolderId(this, m_netManager, username);
              if (targetFolderId.isEmpty()) return;

              const QString encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(linkOrToken));
              QNetworkRequest req(QUrl(kBlopStudyUrl + "/api/shares/link/" + encodedToken + "/import"));
              req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
              QJsonObject payload{
                  {"username", username},
                  {"folder_id", targetFolderId},
              };
              QNetworkReply *reply = m_netManager->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
              QEventLoop loop;
              connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
              loop.exec();
              const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
              const QByteArray raw = reply->readAll();
              if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
                  BlopDialogs::notify(this, QStringLiteral("Import fehlgeschlagen"),
                                      QStringLiteral("Serverantwort (%1):\n%2")
                                          .arg(status).arg(QString::fromUtf8(raw)));
              } else {
                  BlopDialogs::notify(this, QStringLiteral("Import erfolgreich"),
                                      QStringLiteral("Die geteilte Datei wurde in dein Konto importiert."));
              }
              reply->deleteLater();
          }
      });
#endif // !Q_OS_ANDROID

      m_editorTabs->addTab(canvasWrapper, fileName);
      m_editorTabs->setCurrentWidget(canvasWrapper);
      addNoteTab(QFileInfo(fileName).baseName());
    } else {
      QFileInfo fi(path);
      if (fi.suffix().toLower() == "md" || fi.suffix().toLower() == "txt") {
        MarkdownEditor *mdEditor = new MarkdownEditor(this);
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
          mdEditor->setText(QString::fromUtf8(f.readAll()));
          f.close();
        }
        mdEditor->onSaveRequested = [path](const QString &text) {
          QFile f(path);
          if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(text.toUtf8());
            f.close();
          }
        };
        m_editorTabs->addTab(mdEditor, fileName);
        m_editorTabs->setCurrentWidget(mdEditor);
        addNoteTab(QFileInfo(fileName).baseName());
      } else {
        Note note;
        if (NoteManager::loadNote(path, note)) {
          // Prefer tags embedded in the note file; fall back to library store.
          if (note.tags.isEmpty())
            note.tags = LibraryTagStore::tagsForPath(path);
          else
            LibraryTagStore::setTagsForPath(path, note.tags);
          NoteEditor *editor = new NoteEditor(this);
          editor->setProperty("filePath", path);
          Note *heapNote = new Note(note);
          editor->setNote(heapNote);
          if (editor->view()) {
            editor->view()->setPenOnlyMode(m_penOnlyMode);
            editor->view()->setProperty("viewStateKey", path);
            // Restore after layout settles so page scroll targets are valid.
            QTimer::singleShot(0, editor->view(), [v = editor->view(), path]() {
              if (v)
                v->restoreViewState(path);
            });
          }
          editor->onSaveRequested = [this, path, editor](Note *n) {
            if (!n) return;
            LibraryTagStore::setTagsForPath(path, n->tags);
            m_pendingA4SaveNote = n;
            m_pendingA4SavePath = path;
            const bool force = editor->property("forceSave").toBool();
            editor->setProperty("forceSave", false);
            if (force) {
              if (m_a4SaveDebounce) m_a4SaveDebounce->stop();
              Note copy = *n;
              const QString p = path;
              m_noteManager.saveNoteAsync(copy, p, [p](bool ok) {
                if (!ok) qWarning() << "A4 async save failed" << p;
              });
            } else {
              if (m_a4SaveDebounce) m_a4SaveDebounce->start();
            }
          };
          editor->onOpenNoteOptionsRequested = [this]() {
            if (!m_pageSettingsOverlay)
              return;
            if (!m_pageSettingsOverlay->isVisible())
              setPageSettingsOverlayVisible(true);
          };
          editor->onOpenPageManagerRequested = [this]() {
            onTogglePageManager();
          };
          m_editorTabs->addTab(editor, fileName);
          m_editorTabs->setCurrentWidget(editor);
          addNoteTab(QFileInfo(fileName).baseName());
        } else {
          BlopDialogs::notify(
              this, QStringLiteral("Notiz öffnen"),
              QStringLiteral("Datei konnte nicht geladen werden:\n%1")
                  .arg(path));
          return;
        }
      }
    }
    if (m_rightStack) {
      const int editorIdx = m_rightStack->indexOf(m_editorContainer);
#ifdef Q_OS_ANDROID
      m_rightStack->setCurrentIndex(editorIdx);
#else
      crossfadeStackTo(m_rightStack, editorIdx);
#endif
    }
    if (m_documentTabBar)
      m_documentTabBar->setNoteChromeMode(true);
    applyNoteChromeTheme();
    setActiveTool(m_activeToolType);
    updateSidebarState();
  }
}

void MainWindow::flushPendingA4Save() {
  if (!m_pendingA4SaveNote || m_pendingA4SavePath.isEmpty()) return;
  if (m_a4SaveDebounce) m_a4SaveDebounce->stop();
  Note copy = *m_pendingA4SaveNote;
  const QString p = m_pendingA4SavePath;
  m_pendingA4SaveNote = nullptr;
  m_pendingA4SavePath.clear();
  m_noteManager.saveNoteAsync(copy, p, [p](bool ok) {
    if (!ok) qWarning() << "A4 flush save failed" << p;
  });
}

void MainWindow::onBackToOverview() {
  if (auto *view = currentNoteView())
    view->persistViewState(view->property("viewStateKey").toString());
  flushPendingA4Save();
  if (m_documentTabBar)
    m_documentTabBar->setNoteChromeMode(false);
  refreshNoteTitleChrome(false);
  if (m_rightStack) {
    const int overviewIdx = m_rightStack->indexOf(m_overviewContainer);
#ifdef Q_OS_ANDROID
    m_rightStack->setCurrentIndex(overviewIdx);
#else
    crossfadeStackTo(m_rightStack, overviewIdx);
#endif
  }
  updateSidebarState();
#ifdef Q_OS_ANDROID
  const auto transientOverlays = findChildren<QWidget *>(
      QStringLiteral("AndroidTransientOverlay"), Qt::FindDirectChildrenOnly);
  for (QWidget *overlay : transientOverlays) {
    if (overlay && overlay->isVisible())
      overlay->close();
  }
  syncAndroidHeaderGeometry(this);
  if (m_androidHeader)
    m_androidHeader->raise();
#endif
}
void MainWindow::showContextMenu(const QPoint &globalPos,
                                 const QModelIndex &index) {
  BlopDiag::recordUiAction(QStringLiteral("ctx_menu_show"));
  if (!index.isValid())
    return;

  // Library view may hand us a proxy index — always normalize to the
  // QFileSystemModel source index before path lookups / open / rename.
  QModelIndex sourceIndex = index;
  if (m_libraryProxy && index.model() == m_libraryProxy)
    sourceIndex = m_libraryProxy->mapToSource(index);
  if (!sourceIndex.isValid())
    return;

  // Shared with desktop (exec) and Android (in-window menu). Lambdas capture a
  // QPersistentModelIndex so action bodies stay safe if the QFileSystemModel
  // refreshes its internal nodes between menu-open and the user picking an
  // item (the file watcher can fire at any time on Android).
  const QPersistentModelIndex persistent(sourceIndex);

  // Cloud share action lambdas – shared between the desktop QMenu and the
  // Android BlopInWindowMenu so the logic lives in exactly one place.
  auto doShareUser = [this, persistent]() {
    if (!persistent.isValid()) return;
    const QString username =
        QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
            .value(QStringLiteral("username")).toString().trimmed();
    if (username.isEmpty()) {
      BlopDialogs::notify(this, QStringLiteral("Nicht angemeldet"),
                          QStringLiteral("Bitte zuerst in Blop Study anmelden."));
      return;
    }
    const QString localPath = m_fileModel->filePath(QModelIndex(persistent));
    QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
    if (fileId.isEmpty()) {
      fileId = BlopDialogs::promptText(
                   this, QStringLiteral("Cloud-Datei-ID"),
                   QStringLiteral("Datei-ID im Blop-Study-Cloudspeicher:"),
                   QFileInfo(localPath).baseName()).trimmed();
      if (fileId.isEmpty()) return;
    } else {
      BlopDialogs::notify(this, QStringLiteral("Cloud-Datei erkannt"),
                          QStringLiteral("Automatisch erkannt: %1").arg(fileId));
    }
    const QString target = BlopDialogs::promptText(
        this, QStringLiteral("Zielnutzer"),
        QStringLiteral("Username des Empfängers:"), QString()).trimmed();
    if (target.isEmpty()) return;
    const QString message = BlopDialogs::promptText(
        this, QStringLiteral("Nachricht (optional)"),
        QStringLiteral("Begleitnachricht:"), QString());
    QUrl url(kBlopStudyUrl + "/api/shares/username");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload{
        {QStringLiteral("username"), username},
        {QStringLiteral("file_id"), fileId},
        {QStringLiteral("target_username"), target},
        {QStringLiteral("message"), message},
    };
    QNetworkReply *reply = m_netManager->post(
        req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
      BlopDialogs::notify(this, QStringLiteral("Teilen fehlgeschlagen"),
                          QStringLiteral("Serverantwort (%1):\n%2")
                              .arg(status).arg(QString::fromUtf8(raw)));
    } else {
      BlopDialogs::notify(this, QStringLiteral("Request gesendet"),
                          QStringLiteral("Die Freigabeanfrage wurde an den Zielnutzer gesendet."));
    }
    reply->deleteLater();
  };

  auto doCreateLink = [this, persistent]() {
    if (!persistent.isValid()) return;
    const QString username =
        QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
            .value(QStringLiteral("username")).toString().trimmed();
    if (username.isEmpty()) {
      BlopDialogs::notify(this, QStringLiteral("Nicht angemeldet"),
                          QStringLiteral("Bitte zuerst in Blop Study anmelden."));
      return;
    }
    const QString localPath = m_fileModel->filePath(QModelIndex(persistent));
    QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
    if (fileId.isEmpty()) {
      fileId = BlopDialogs::promptText(
                   this, QStringLiteral("Cloud-Datei-ID"),
                   QStringLiteral("Datei-ID im Blop-Study-Cloudspeicher:"),
                   QFileInfo(localPath).baseName()).trimmed();
      if (fileId.isEmpty()) return;
    } else {
      BlopDialogs::notify(this, QStringLiteral("Cloud-Datei erkannt"),
                          QStringLiteral("Automatisch erkannt: %1").arg(fileId));
    }
    bool ok = false;
    const int expiresDays = BlopDialogs::promptInt(
        this, QStringLiteral("Gültigkeit"),
        QStringLiteral("Link gültig für (Tage):"), 7, 1, 30, &ok);
    if (!ok) return;
    const int maxUses = BlopDialogs::promptInt(
        this, QStringLiteral("Nutzungslimit"),
        QStringLiteral("Maximale Nutzungen:"), 1, 1, 100, &ok);
    if (!ok) return;
    QUrl url(kBlopStudyUrl + "/api/shares/link");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload{
        {QStringLiteral("username"), username},
        {QStringLiteral("file_id"), fileId},
        {QStringLiteral("expires_in_days"), expiresDays},
        {QStringLiteral("max_uses"), maxUses},
    };
    QNetworkReply *reply = m_netManager->post(
        req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
      BlopDialogs::notify(this, QStringLiteral("Link fehlgeschlagen"),
                          QStringLiteral("Serverantwort (%1):\n%2")
                              .arg(status).arg(QString::fromUtf8(raw)));
      reply->deleteLater();
      return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    const QString link = doc.object().value(QStringLiteral("url")).toString();
    if (!link.isEmpty())
      QGuiApplication::clipboard()->setText(link);
    BlopDialogs::notify(this, QStringLiteral("Link erstellt"),
                        link.isEmpty()
                            ? QStringLiteral("Share-Link wurde erstellt.")
                            : QStringLiteral("Share-Link wurde erstellt und kopiert:\n%1").arg(link));
    reply->deleteLater();
  };

  auto doImportLink = [this]() {
    const QString username =
        QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
            .value(QStringLiteral("username")).toString().trimmed();
    if (username.isEmpty()) {
      BlopDialogs::notify(this, QStringLiteral("Nicht angemeldet"),
                          QStringLiteral("Bitte zuerst in Blop Study anmelden."));
      return;
    }
    QString linkOrToken = BlopDialogs::promptText(
        this, QStringLiteral("Share-Link"),
        QStringLiteral("Share-Link oder Token einfügen:"), QString()).trimmed();
    if (linkOrToken.isEmpty()) return;
    if (linkOrToken.contains(QStringLiteral("/"))) {
      const QString marker = QStringLiteral("/share/");
      const int pos = linkOrToken.lastIndexOf(marker);
      if (pos >= 0)
        linkOrToken = linkOrToken.mid(pos + marker.size());
      else
        linkOrToken = linkOrToken.section('/', -1).trimmed();
    }
    const QString targetFolderId = chooseCloudFolderId(this, m_netManager, username);
    if (targetFolderId.isEmpty()) return;
    const QString encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(linkOrToken));
    QUrl url(kBlopStudyUrl + "/api/shares/link/" + encodedToken + "/import");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload{
        {QStringLiteral("username"), username},
        {QStringLiteral("folder_id"), targetFolderId},
    };
    QNetworkReply *reply = m_netManager->post(
        req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
      BlopDialogs::notify(this, QStringLiteral("Import fehlgeschlagen"),
                          QStringLiteral("Serverantwort (%1):\n%2")
                              .arg(status).arg(QString::fromUtf8(raw)));
    } else {
      BlopDialogs::notify(this, QStringLiteral("Import erfolgreich"),
                          QStringLiteral("Die geteilte Datei wurde in dein Konto importiert."));
    }
    reply->deleteLater();
  };

  const auto populateMenu = [this, persistent, doShareUser, doCreateLink, doImportLink](QMenu *menu) {
    menu->addAction(QStringLiteral("Öffnen"), [this, persistent]() {
      if (!persistent.isValid()) return;
      onFileDoubleClicked(QModelIndex(persistent));
    });
    menu->addAction(QStringLiteral("Umbenennen"), [this, persistent]() {
      if (!persistent.isValid()) return;
      startRename(QModelIndex(persistent));
    });

    const QString path = m_fileModel->filePath(QModelIndex(persistent));
    const bool isDir = m_fileModel->isDir(QModelIndex(persistent));
    if (!isDir) {
      const bool fav = LibraryOrgStore::isFavorite(path);
      menu->addAction(fav ? QStringLiteral("Aus Favoriten entfernen")
                          : QStringLiteral("Zu Favoriten"),
                      [this, persistent, path, fav]() {
                        if (!persistent.isValid()) return;
                        LibraryOrgStore::setFavorite(path, !fav);
                        if (m_fileListView)
                          m_fileListView->viewport()->update();
                        applyLibraryFilters();
                      });

      QMenu *colorMenu = menu->addMenu(QStringLiteral("Farb-Label"));
      const LibraryOrgStore::ColorLabel current =
          LibraryOrgStore::colorLabel(path);
      auto addColor = [&](LibraryOrgStore::ColorLabel label) {
        QAction *a = colorMenu->addAction(LibraryOrgStore::labelDisplayName(label));
        a->setCheckable(true);
        a->setChecked(current == label);
        QObject::connect(a, &QAction::triggered, this,
                         [this, persistent, path, label]() {
                           if (!persistent.isValid()) return;
                           LibraryOrgStore::setColorLabel(path, label);
                           if (m_fileListView)
                             m_fileListView->viewport()->update();
                         });
      };
      addColor(LibraryOrgStore::ColorLabel::None);
      addColor(LibraryOrgStore::ColorLabel::Rose);
      addColor(LibraryOrgStore::ColorLabel::Amber);
      addColor(LibraryOrgStore::ColorLabel::Lime);
      addColor(LibraryOrgStore::ColorLabel::Sky);
      addColor(LibraryOrgStore::ColorLabel::Violet);
      addColor(LibraryOrgStore::ColorLabel::Slate);

      menu->addAction(QStringLiteral("Tags zuweisen\u2026"), [this, path]() {
        QStringList catalog = LibraryTagStore::catalog();
        if (catalog.isEmpty()) {
          LibraryTagStore::addTagToCatalog(QStringLiteral("Projekt"));
          LibraryTagStore::addTagToCatalog(QStringLiteral("Entwurf"));
          catalog = LibraryTagStore::catalog();
        }
        const QStringList current = LibraryTagStore::tagsForPath(path);

        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("Tags zuweisen"));
        dlg.setModal(true);
        dlg.setMinimumWidth(UiScale::dp(280));
        auto *lay = new QVBoxLayout(&dlg);
        auto *hint = new QLabel(
            QStringLiteral("Hake Tags an, um sie an diese Notiz zu hängen."),
            &dlg);
        hint->setWordWrap(true);
        lay->addWidget(hint);
        QList<QCheckBox *> boxes;
        for (const QString &tag : catalog) {
          auto *cb = new QCheckBox(tag, &dlg);
          for (const QString &c : current) {
            if (c.compare(tag, Qt::CaseInsensitive) == 0) {
              cb->setChecked(true);
              break;
            }
          }
          boxes.append(cb);
          lay->addWidget(cb);
        }
        auto *bbox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        bbox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Fertig"));
        bbox->button(QDialogButtonBox::Cancel)
            ->setText(QStringLiteral("Abbrechen"));
        lay->addWidget(bbox);
        QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg,
                         &QDialog::accept);
        QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg,
                         &QDialog::reject);
        if (dlg.exec() != QDialog::Accepted)
          return;
        QStringList next;
        for (QCheckBox *cb : boxes) {
          if (cb && cb->isChecked())
            next.append(cb->text());
        }
        LibraryTagStore::setTagsForPath(path, next);
        if (m_libraryTagsPanel)
          m_libraryTagsPanel->reload();
        applyLibraryFilters();
        if (m_fileListView)
          m_fileListView->viewport()->update();
      });
    }

    menu->addSeparator();
    menu->addAction(QStringLiteral("Mit Username teilen\u2026"), doShareUser);
    menu->addAction(QStringLiteral("Share-Link erstellen\u2026"), doCreateLink);
    menu->addAction(QStringLiteral("Datei aus Link importieren\u2026"), doImportLink);
    menu->addAction(QStringLiteral("Löschen"), [this, persistent]() {
      if (!persistent.isValid()) return;
      if (!BlopDialogs::confirm(
              this, QStringLiteral("Notiz löschen"),
              QStringLiteral("Diese Notiz wirklich löschen? Das kann nicht "
                             "rückgängig gemacht werden."),
              QStringLiteral("Löschen"), QStringLiteral("Abbrechen")))
        return;
      m_fileModel->remove(QModelIndex(persistent));
    });
  };

#ifdef Q_OS_ANDROID
  // QMenu allocates a top-level QWindow whose backing-store creation calls
  // QOpenGLContext::makeCurrent. When QtAndroidAccessibility holds the
  // EGL surface lock (TalkBack & friends on Android 16) Qt's deadlock
  // protector aborts the process. We confirmed this in v3.16.7 with a
  // tombstone showing this exact stack. Use the in-window QFrame menu
  // replacement instead -- no top-level QWindow, no EGL allocation.
  QList<BlopInWindowMenu::Item> items;
  items.append({QStringLiteral("Öffnen"), QIcon(),
                [this, persistent]() {
                  if (!persistent.isValid()) return;
                  onFileDoubleClicked(QModelIndex(persistent));
                }, false, false});
  items.append({QStringLiteral("Umbenennen"), QIcon(),
                [this, persistent]() {
                  if (!persistent.isValid()) return;
                  startRename(QModelIndex(persistent));
                }, false, false});
  {
    const QString path = m_fileModel->filePath(QModelIndex(persistent));
    if (!m_fileModel->isDir(QModelIndex(persistent))) {
      const bool fav = LibraryOrgStore::isFavorite(path);
      items.append({fav ? QStringLiteral("Aus Favoriten entfernen")
                        : QStringLiteral("Zu Favoriten"),
                    QIcon(),
                    [this, persistent, path, fav]() {
                      if (!persistent.isValid()) return;
                      LibraryOrgStore::setFavorite(path, !fav);
                      if (m_fileListView)
                        m_fileListView->viewport()->update();
                      applyLibraryFilters();
                    },
                    false, false});
      items.append({QStringLiteral("Tags zuweisen\u2026"), QIcon(),
                    [this, path]() {
                      QStringList catalog = LibraryTagStore::catalog();
                      if (catalog.isEmpty()) {
                        LibraryTagStore::addTagToCatalog(QStringLiteral("Projekt"));
                        LibraryTagStore::addTagToCatalog(QStringLiteral("Entwurf"));
                        catalog = LibraryTagStore::catalog();
                      }
                      const QStringList current = LibraryTagStore::tagsForPath(path);
                      QStringList next = current;
                      // Simple cycle attach: if empty catalog selection, prompt via dialog.
                      QDialog dlg(this);
                      dlg.setWindowTitle(QStringLiteral("Tags zuweisen"));
                      auto *lay = new QVBoxLayout(&dlg);
                      QList<QCheckBox *> boxes;
                      for (const QString &tag : catalog) {
                        auto *cb = new QCheckBox(tag, &dlg);
                        for (const QString &c : current) {
                          if (c.compare(tag, Qt::CaseInsensitive) == 0) {
                            cb->setChecked(true);
                            break;
                          }
                        }
                        boxes.append(cb);
                        lay->addWidget(cb);
                      }
                      auto *bbox = new QDialogButtonBox(
                          QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                      lay->addWidget(bbox);
                      QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg,
                                       &QDialog::accept);
                      QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg,
                                       &QDialog::reject);
                      if (dlg.exec() != QDialog::Accepted)
                        return;
                      next.clear();
                      for (QCheckBox *cb : boxes) {
                        if (cb && cb->isChecked())
                          next.append(cb->text());
                      }
                      LibraryTagStore::setTagsForPath(path, next);
                      if (m_libraryTagsPanel)
                        m_libraryTagsPanel->reload();
                      applyLibraryFilters();
                      if (m_fileListView)
                        m_fileListView->viewport()->update();
                    },
                    false, false});
    }
  }
  items.append({QString(), QIcon(), {}, false, true});
  items.append({QStringLiteral("Mit Username teilen\u2026"), QIcon(), doShareUser, false, false});
  items.append({QStringLiteral("Share-Link erstellen\u2026"), QIcon(), doCreateLink, false, false});
  items.append({QStringLiteral("Datei aus Link importieren\u2026"), QIcon(), doImportLink, false, false});
  items.append({QString(), QIcon(), {}, false, true});
  items.append({QStringLiteral("Löschen"), QIcon(),
                [this, persistent]() {
                  if (!persistent.isValid()) return;
                  if (!BlopDialogs::confirm(
                          this, QStringLiteral("Notiz löschen"),
                          QStringLiteral(
                              "Diese Notiz wirklich löschen? Das kann nicht "
                              "rückgängig gemacht werden."),
                          QStringLiteral("Löschen"),
                          QStringLiteral("Abbrechen")))
                    return;
                  m_fileModel->remove(QModelIndex(persistent));
                }, true, false});
  BlopInWindowMenu::show(this, globalPos, items);
#else
  QMenu menu(this);
  populateMenu(&menu);
  menu.setStyleSheet(blopWebMenuStyleSheet());
  menu.exec(globalPos);
#endif
}

void MainWindow::startRename(const QModelIndex &index) {
  m_indexToRename = index;
  showRenameOverlay(index.data().toString());
}
void MainWindow::showRenameOverlay(const QString &currentName) {
  auto *form = new QWidget();
  form->setObjectName(QStringLiteral("RenameForm"));
  auto *lay = new QVBoxLayout(form);
  lay->setContentsMargins(20, 18, 20, 16);
  lay->setSpacing(12);

  auto *title = new QLabel(QStringLiteral("Umbenennen"), form);
  title->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: %1; font-size: 16px; font-weight: 700; background: transparent;")
                                             .arg(BlopTheme::textPrimary().name())));
  lay->addWidget(title);

  auto *edit = new QLineEdit(currentName, form);
  edit->setObjectName(QStringLiteral("RenameInput"));
  edit->setFixedHeight(UiScale::dp(40));
  edit->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QLineEdit {"
      "  background: %1; color: %2; border: 1px solid %3;"
      "  border-radius: 12px; padding: 0 12px; font-size: 14px;"
      "}"
      "QLineEdit:focus { border: 1px solid %4; }")
                                            .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
                                                 BlopTheme::textPrimary().name(QColor::HexRgb),
                                                 BlopTheme::borderSubtle().name(QColor::HexRgb),
                                                 m_currentAccentColor.name(QColor::HexRgb))));
  lay->addWidget(edit);

  auto *row = new QHBoxLayout();
  row->setSpacing(8);
  row->addStretch(1);
  auto *btnCancel = new QPushButton(QStringLiteral("Abbrechen"), form);
  btnCancel->setCursor(Qt::PointingHandCursor);
  btnCancel->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QPushButton { background: transparent; color: %1; border: 1px solid %2;"
      "  border-radius: 10px; padding: 8px 14px; font-size: 12px; }"
      "QPushButton:hover { color: %3; border-color: %4; }")
                                                 .arg(BlopTheme::textSecondary().name(),
                                                      BlopTheme::borderSubtle().name(QColor::HexRgb),
                                                      BlopTheme::textPrimary().name(),
                                                      m_currentAccentColor.name(QColor::HexRgb))));
  auto *btnOk = new QPushButton(QStringLiteral("Speichern"), form);
  btnOk->setCursor(Qt::PointingHandCursor);
  btnOk->setDefault(true);
  btnOk->setStyleSheet(BlopTheme::primaryButtonQss());
  row->addWidget(btnCancel);
  row->addWidget(btnOk);
  lay->addLayout(row);

  BlopModal *modal = BlopModal::present(this, form, BlopModal::Mode::Auto,
                                        QStringLiteral("Umbenennen"));
  if (!modal)
    return;
  modal->setPreferredCardWidth(420);

  auto commit = [this, edit, modal]() {
    if (!edit || !m_indexToRename.isValid()) {
      if (modal)
        modal->dismiss();
      return;
    }
    const QString newName = edit->text().trimmed();
    if (!newName.isEmpty()) {
      const QString oldPath = m_fileModel->filePath(m_indexToRename);
      if (!m_fileModel->setData(m_indexToRename, newName, Qt::EditRole)) {
        BlopDialogs::notify(
            this, QStringLiteral("Umbenennen"),
            QStringLiteral("Umbenennen fehlgeschlagen. Name evtl. ungültig "
                           "oder bereits vergeben."));
        return;
      }
      const QString newPath =
          QFileInfo(oldPath).absolutePath() + QLatin1Char('/') + newName;
      LibraryTagStore::remapPath(oldPath, newPath);
      LibraryOrgStore::remapPath(oldPath, newPath);
      applyLibraryFilters();
    }
    m_indexToRename = QModelIndex();
    if (modal)
      modal->dismiss();
  };

  connect(btnOk, &QPushButton::clicked, form, commit);
  connect(edit, &QLineEdit::returnPressed, form, commit);
  connect(btnCancel, &QPushButton::clicked, modal, &BlopModal::dismiss);
  QTimer::singleShot(0, edit, [edit]() {
    if (edit) {
      edit->setFocus(Qt::OtherFocusReason);
      edit->selectAll();
    }
  });
}

void MainWindow::finishRename() {
  // Legacy Enter-handler kept for callers; rename now commits via BlopModal.
  if (m_indexToRename.isValid() && m_renameInput) {
    const QString newName = m_renameInput->text().trimmed();
    if (!newName.isEmpty()) {
      if (!m_fileModel->setData(m_indexToRename, newName, Qt::EditRole)) {
        BlopDialogs::notify(
            this, QStringLiteral("Umbenennen"),
            QStringLiteral("Umbenennen fehlgeschlagen. Name evtl. ungültig "
                           "oder bereits vergeben."));
        return;
      }
    }
    m_indexToRename = QModelIndex();
  }
  if (m_renameOverlay)
    m_renameOverlay->hide();
}

void MainWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
#ifdef Q_OS_WIN
  // Keep one Blop chrome bar only: never re-enable WS_CAPTION (that draws the
  // native "Blop" title strip above our custom bar). Still request min/max /
  // thick frame so taskbar minimize and edge-resize work while frameless.
  if (HWND hwnd = reinterpret_cast<HWND>(winId())) {
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~WS_CAPTION;
    style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  }
#endif
#ifdef Q_OS_ANDROID
  applyAndroidImmersiveUi();
  syncAndroidHeaderGeometry(this);
  if (m_androidHeader)
    m_androidHeader->raise();
  if (m_authNavigationLocked && m_mainContentStack &&
      m_mainContentStack->currentIndex() == 1) {
    QTimer::singleShot(100, this, [this]() {
      if (!m_authNavigationLocked)
        return;
      if (!m_mainContentStack || m_mainContentStack->currentIndex() != 1)
        return;
      setAndroidStudyBootOverlayVisible(true);
      if (m_studyQQuickView && m_studyQQuickView->rootObject()) {
        QObject *root = m_studyQQuickView->rootObject();
        // Ensure tabActive is true before requestSurfaceActivation — the QML
        // property now defaults to false and requestSurfaceActivation returns
        // early when tabActive == false.
        root->setProperty("tabActive", true);
        QMetaObject::invokeMethod(
            root, "requestSurfaceActivation", Qt::QueuedConnection,
            Q_ARG(QVariant, QVariant(QStringLiteral("showEvent"))));
        QMetaObject::invokeMethod(root, "ensureStudyLoaded",
                                  Qt::QueuedConnection);
      }
    });
  }
#endif
#if defined(BLOP_HAS_WEBENGINE) && !defined(Q_OS_ANDROID)
  if (m_studyWebView) {
    m_studyWebView->updateGeometry();
    m_studyWebView->raise();
  }
#endif
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  updateGrid();
  int fabSize = 56;
  int bottomOffset = 80;
#ifdef Q_OS_ANDROID
  applyAndroidImmersiveUi();
  fabSize = FAB_SIZE_ANDROID;
  bottomOffset = FAB_DISTANCE_FROM_BOTTOM;
  syncAndroidHeaderGeometry(this);
  syncSidebarPushLayout();
  if (m_studyVBoxLayout)
    m_studyVBoxLayout->setContentsMargins(0, 0, 0, 0);
  // Re-clamp embedded Study container to the (possibly rotated) screen width
  // so the Bookmark sheet anchored to parent.left/right stays inside.
  if (m_studyWindowContainer)
    m_studyWindowContainer->setMaximumWidth(
        UiScale::androidScreenWidthPx(this));
  if (m_androidSidebarScrim && m_androidSidebarScrim->isVisible())
    updateAndroidSidebarScrimGeometry();
  if (m_androidOAuthOverlay && m_androidOAuthOverlay->isVisible())
    m_androidOAuthOverlay->setGeometry(androidSafeOverlayRect(this));
  syncAndroidStudyBootOverlayGeometry();
  if (m_androidStudyBootOverlay && m_androidStudyBootOverlay->isVisible())
    m_androidStudyBootOverlay->raise();
  if (m_androidHeader && m_mainContentStack && m_mainContentStack->currentIndex() == 0)
    m_androidHeader->raise();
#else
  if (isTouchMode())
    bottomOffset = 100;
  syncSidebarPushLayout();
#endif
  if (m_editorTabs && m_floatingTools) {
    if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools)) {
      if (m_editorCenterWidget) {
        const int h = phone->preferredHeightPx();
        const int margin = UiScale::dp(8);
        const int avail =
            qMax(UiScale::dp(180), m_editorCenterWidget->width());
        const int w = qMin(avail - 2 * margin, UiScale::dp(360));
        const int y = qMax(0, m_editorCenterWidget->height() - h -
                                  UiScale::safeBottomPx(this) -
                                  UiScale::dp(8));
        phone->setGeometry((avail - w) / 2, y, w, h);
        phone->raise();
      }
    } else if (ModernToolbar *tb =
                   qobject_cast<ModernToolbar *>(m_floatingTools)) {
      tb->setTopBound(noteHeaderHeight());
      tb->requestAdaptiveReflow();
      // Let the toolbar govern its own dragged docking position seamlessly.
    }
  }
  if (m_pageManager && m_pageManager->isVisible()) {
    if (QWidget *pw = m_pageManager->parentWidget()) {
      m_pageManager->setGeometry(0, 0, pw->width(), pw->height());
    }
  }
}

void MainWindow::onOpenSettings() {
  // Close sidebar so modal overlays receive mouse/touch reliably.
  if (m_isSidebarOpen)
    onToggleSidebar();

  SettingsDialog dlg(m_profileManager, this);
  ModernToolbar *toolbar = qobject_cast<ModernToolbar *>(m_floatingTools);
  if (toolbar) {
    bool isRad = (toolbar->currentStyle() == ModernToolbar::Radial);
    bool isHalf = (toolbar->radialType() == ModernToolbar::HalfEdge);
    dlg.setToolbarConfig(isRad, isHalf);
  }
  connect(&dlg, &SettingsDialog::accentColorChanged, this,
          &MainWindow::updateTheme);
  connect(&dlg, &SettingsDialog::toolbarStyleChanged,
          [this, toolbar](bool radial) {
#ifndef Q_OS_ANDROID
            // Desktop Drawboard: keep Favorites rail; ignore Radial.
            Q_UNUSED(radial);
            if (toolbar) {
              toolbar->setStyle(ModernToolbar::Normal);
              toolbar->applyDrawboardVerticalRail();
            }
            if (m_radialFab)
              m_radialFab->hide();
            positionNoteChrome();
#else
            if (toolbar)
              toolbar->setStyle(radial ? ModernToolbar::Radial
                                       : ModernToolbar::Normal);
            if (m_radialFab)
              m_radialFab->setVisible(radial);
#endif
          });
  connect(&dlg, &SettingsDialog::logoutRequested, this, [this]() {
    QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    st.remove(QStringLiteral("session_id"));
    st.remove(QStringLiteral("username"));
    st.sync();
    updateSidebarUser(QString());
    const QString clearJs = QStringLiteral(
        "localStorage.removeItem('session_id');"
        "localStorage.removeItem('username');"
        "window.location.href = '/login';");
#ifdef Q_OS_ANDROID
    emit injectToken(clearJs);
#else
#ifdef BLOP_HAS_WEBENGINE
    if (m_studyWebView && m_studyWebView->page())
      m_studyWebView->page()->runJavaScript(clearJs);
#endif
#endif
  });

  // Desktop/Windows: full-height side sheet so Settings is not stuck in a
  // tiny centered card. Preferred width ~640dp scales with the window.
  // Android phones keep the default BottomSheet via Mode::Auto.
#ifndef Q_OS_ANDROID
  int res = BlopModal::execBlocking(this, &dlg, BlopModal::Mode::SideSheet,
                                    UiScale::dp(640));
#else
  int res = BlopModal::execBlocking(this, &dlg);
#endif
  if (res == SettingsDialog::EditProfileCode) {
    QString id = dlg.profileIdToEdit();
    UiProfile p = m_profileManager->profileById(id);
    UiProfile original = p;
    ProfileEditorDialog editor(p, this);
    connect(&editor, &ProfileEditorDialog::previewRequested, this,
            &MainWindow::applyProfile);
    if (BlopModal::execBlocking(this, &editor) == QDialog::Accepted) {
      m_profileManager->updateProfile(editor.getProfile(), true);
      applyProfile(editor.getProfile());
    } else {
      m_profileManager->updateProfile(original, true);
      applyProfile(m_profileManager->currentProfile());
    }
  }
}

void MainWindow::setPageColor(bool dark) {
  if (CanvasView *cv = getCurrentCanvas()) {
    cv->setPageColor(dark ? UIStyles::SceneBackground
                          : UIStyles::PageBackground);
  }
}

void MainWindow::setActiveTool(CanvasView::ToolType tool) {
  m_activeToolType = tool;

  // FIX: Mapping von CanvasView::ToolType zu ToolMode
  ToolMode tm = ToolMode::Pen;

  switch (tool) {
  case CanvasView::ToolType::Eraser:
    tm = ToolMode::Eraser;
    break;
  case CanvasView::ToolType::Lasso:
    tm = ToolMode::Lasso;
    break;
  case CanvasView::ToolType::Highlighter:
    tm = ToolMode::Highlighter;
    break;
  case CanvasView::ToolType::Select:
    tm = ToolMode::Lasso;
    break; // Select -> Lasso/Hand fallback
  case CanvasView::ToolType::Ruler:
    tm = ToolMode::Ruler;
    break;
  case CanvasView::ToolType::Image:
    tm = ToolMode::Image;
    break;
  case CanvasView::ToolType::Shape:
    tm = ToolMode::Shape;
    break;
  case CanvasView::ToolType::Text:
    tm = ToolMode::Text;
    break;
  case CanvasView::ToolType::Pencil:
    tm = ToolMode::Pencil;
    break;
  case CanvasView::ToolType::StickyNote:
    tm = ToolMode::StickyNote;
    break;
  case CanvasView::ToolType::Hand:
    tm = ToolMode::Hand;
    break;
  case CanvasView::ToolType::Pen:
  default:
    tm = ToolMode::Pen;
    break;
  }

  if (m_toolManager) {
    m_toolManager->selectTool(tm);
  }

  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
    tb->setToolMode(tm);
  }
  if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools)) {
    phone->setToolMode(tm);
  }
  QWidget *current = m_editorTabs ? m_editorTabs->currentWidget() : nullptr;
  // Direct or inside-wrapper
  if (auto *cv = qobject_cast<CanvasView *>(current)) {
    cv->setTool(tool);
  } else if (current) {
    if (auto *cv = current->findChild<CanvasView *>()) {
      cv->setTool(tool);
    } else if (auto *editor = qobject_cast<NoteEditor *>(current)) {
      if (editor->view())
        editor->view()->setToolMode(tm);
    }
  }
}

int MainWindow::noteHeaderHeight() const {
  return (m_noteHeader && m_noteHeader->isVisible()) ? m_noteHeader->height() : 0;
}

int MainWindow::noteChromeThickness() const { return UiScale::dp(48); }

int MainWindow::noteBottomChromeHeight() const {
  // Bottom-edge clearance for Favorites rail / presets.
  // Phone UI uses AndroidPhoneToolbar instead — report that clearance.
  if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools)) {
    if (!phone->isVisible())
      return 0;
    return phone->preferredHeightPx() + UiScale::dp(16) +
           UiScale::safeBottomPx(const_cast<MainWindow *>(this));
  }
  if (!(m_noteBottomChrome && m_noteBottomChrome->isVisible()))
    return 0;
  if (m_noteChromeEdge != NoteChromeEdge::Bottom)
    return 0;
  return m_noteBottomChrome->height();
}

int MainWindow::noteChromeClearanceTop() const {
  if (!(m_noteBottomChrome && m_noteBottomChrome->isVisible()))
    return 0;
  if (m_noteChromeEdge != NoteChromeEdge::Top)
    return 0;
  return m_noteBottomChrome->height();
}

int MainWindow::noteChromeClearanceLeft() const {
  if (!(m_noteBottomChrome && m_noteBottomChrome->isVisible()))
    return 0;
  if (m_noteChromeEdge != NoteChromeEdge::Left)
    return 0;
  return m_noteBottomChrome->width();
}

int MainWindow::noteChromeClearanceRight() const {
  if (!(m_noteBottomChrome && m_noteBottomChrome->isVisible()))
    return 0;
  if (m_noteChromeEdge != NoteChromeEdge::Right)
    return 0;
  return m_noteBottomChrome->width();
}

QRect MainWindow::noteChromeContentRect() const {
  if (!m_editorCenterWidget)
    return {};
  const int W = m_editorCenterWidget->width();
  const int H = m_editorCenterWidget->height();
  int left = 0;
  int top = noteHeaderHeight();
  int right = W;
  int bottom = H;
#ifndef Q_OS_ANDROID
  const bool pagesOnRight = pageRailOnRight();
  if (!pagesOnRight) {
    if (m_noteLeftRail && m_noteLeftRail->isVisible())
      left += m_noteLeftRail->preferredWidth();
    if (m_pageThumbnailSidebar && m_pageThumbnailSidebar->isVisible() &&
        !m_pageThumbnailSidebar->isCollapsed())
      left += m_pageThumbnailSidebar->width();
  }
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
    if (tb->isVisible() && tb->isDrawboardVerticalRail()) {
      const int railW = tb->preferredRailWidth();
      if (tb->isRailDockedLeft())
        left += railW;
      else
        right -= railW;
    }
  }
  if (pagesOnRight) {
    if (m_pageThumbnailSidebar && m_pageThumbnailSidebar->isVisible() &&
        !m_pageThumbnailSidebar->isCollapsed())
      right -= m_pageThumbnailSidebar->width();
    if (m_noteLeftRail && m_noteLeftRail->isVisible())
      right -= m_noteLeftRail->preferredWidth();
  }
#endif
  return QRect(QPoint(left, top), QPoint(qMax(left, right - 1), qMax(top, bottom - 1)));
}

NoteChromeEdge MainWindow::nearestNoteChromeEdge(const QPoint &posInParent) const {
  const QRect content = noteChromeContentRect();
  if (!content.isValid())
    return NoteChromeEdge::Bottom;
  const int dTop = qAbs(posInParent.y() - content.top());
  const int dBottom = qAbs(content.bottom() - posInParent.y());
  const int dLeft = qAbs(posInParent.x() - content.left());
  const int dRight = qAbs(content.right() - posInParent.x());
  const int best = qMin(qMin(dTop, dBottom), qMin(dLeft, dRight));
  if (best == dTop)
    return NoteChromeEdge::Top;
  if (best == dBottom)
    return NoteChromeEdge::Bottom;
  if (best == dLeft)
    return NoteChromeEdge::Left;
  return NoteChromeEdge::Right;
}

void MainWindow::persistNoteChromeEdge() const {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/noteChromeEdge"), int(m_noteChromeEdge));
}

void MainWindow::loadPageChromePrefs() {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  m_noteLeftRailPrefVisible =
      s.value(QStringLiteral("ui/noteLeftRailVisible"), true).toBool();
  m_pageRailOnRight =
      s.value(QStringLiteral("ui/pageRailEdge"), 0).toInt() == 1;
}

void MainWindow::persistPageChromePrefs() const {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/noteLeftRailVisible"), m_noteLeftRailPrefVisible);
  s.setValue(QStringLiteral("ui/pageRailEdge"), m_pageRailOnRight ? 1 : 0);
}

bool MainWindow::noteLeftChromeVisible() const {
  return m_noteLeftRailPrefVisible;
}

bool MainWindow::pageRailOnRight() const { return m_pageRailOnRight; }

void MainWindow::applyPageChromePrefs() {
#ifndef Q_OS_ANDROID
  // Re-apply visibility from prefs when in the note editor.
  updateSidebarState();
  positionNoteChrome();
#endif
}

void MainWindow::setNoteChromeEdge(NoteChromeEdge edge) {
  m_noteChromeEdge = edge;
  persistNoteChromeEdge();
  applyNoteChromeLayoutOrientation();
  refreshNoteChromeStyle();
  positionNoteChrome();
}

void MainWindow::applyNoteChromeLayoutOrientation() {
  if (!m_noteChromeLayout || !m_noteBottomChrome)
    return;
  const bool horiz = noteChromeEdgeIsHorizontal(m_noteChromeEdge);
  m_noteChromeLayout->setDirection(horiz ? QBoxLayout::LeftToRight
                                         : QBoxLayout::TopToBottom);
  if (horiz) {
    m_noteChromeLayout->setContentsMargins(UiScale::dp(8), UiScale::dp(4),
                                           UiScale::dp(8), UiScale::dp(4));
  } else {
    m_noteChromeLayout->setContentsMargins(UiScale::dp(4), UiScale::dp(8),
                                           UiScale::dp(4), UiScale::dp(8));
  }
  if (m_btnNoteChromeGrip) {
    m_btnNoteChromeGrip->setFixedSize(horiz ? UiScale::dp(28) : UiScale::dp(36),
                                      horiz ? UiScale::dp(36) : UiScale::dp(28));
  }
  const auto sizeSep = [&](QFrame *sep) {
    if (!sep)
      return;
    if (horiz)
      sep->setFixedSize(UiScale::dp(1), UiScale::dp(22));
    else
      sep->setFixedSize(UiScale::dp(22), UiScale::dp(1));
  };
  sizeSep(m_noteChromeSep1);
  sizeSep(m_noteChromeSep2);
  if (m_lblNotePage) {
    if (horiz) {
      m_lblNotePage->setMinimumWidth(UiScale::dp(64));
      m_lblNotePage->setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
      m_lblNotePage->setMinimumWidth(0);
      m_lblNotePage->setMaximumWidth(UiScale::dp(40));
      m_lblNotePage->setWordWrap(true);
    }
  }
  if (m_lblNoteZoom) {
    if (horiz) {
      m_lblNoteZoom->setMinimumWidth(UiScale::dp(48));
      m_lblNoteZoom->setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
      m_lblNoteZoom->setMinimumWidth(0);
      m_lblNoteZoom->setMaximumWidth(UiScale::dp(40));
    }
  }
}

void MainWindow::refreshNoteChromeStyle() {
  if (!m_noteBottomChrome)
    return;
  // Flush edge notch — rounded free corners, square flush edge.
  const int rad = UiScale::dp(16);
  QString radiusCss;
  QString borderCss;
  switch (m_noteChromeEdge) {
  case NoteChromeEdge::Top:
    radiusCss = QStringLiteral(
        "border-top-left-radius: 0px; border-top-right-radius: 0px;"
        "border-bottom-left-radius: %1px; border-bottom-right-radius: %1px;")
                     .arg(rad);
    borderCss = QStringLiteral(
        "border: 1px solid %1; border-top: none;");
    break;
  case NoteChromeEdge::Bottom:
    radiusCss = QStringLiteral(
        "border-top-left-radius: %1px; border-top-right-radius: %1px;"
        "border-bottom-left-radius: 0px; border-bottom-right-radius: 0px;")
                     .arg(rad);
    borderCss = QStringLiteral(
        "border: 1px solid %1; border-bottom: none;");
    break;
  case NoteChromeEdge::Left:
    radiusCss = QStringLiteral(
        "border-top-left-radius: 0px; border-bottom-left-radius: 0px;"
        "border-top-right-radius: %1px; border-bottom-right-radius: %1px;")
                     .arg(rad);
    borderCss = QStringLiteral(
        "border: 1px solid %1; border-left: none;");
    break;
  case NoteChromeEdge::Right:
    radiusCss = QStringLiteral(
        "border-top-right-radius: 0px; border-bottom-right-radius: 0px;"
        "border-top-left-radius: %1px; border-bottom-left-radius: %1px;")
                     .arg(rad);
    borderCss = QStringLiteral(
        "border: 1px solid %1; border-right: none;");
    break;
  }
  const QString borderResolved =
      borderCss.arg(NoteChrome::notchBorder().name(QColor::HexRgb));
  m_noteBottomChrome->setGraphicsEffect(nullptr);
  m_noteBottomChrome->setStyleSheet(QStringLiteral(
      "QWidget#NoteBottomChrome {"
      "  background: %1;"
      "  %2"
      "  %3"
      "}"
      "QPushButton {"
      "  background: transparent; color: %4;"
      "  border: none; border-radius: 8px;"
      "  font-size: 12px; font-weight: 600; min-width: 32px; min-height: 32px;"
      "  padding: 0 4px;"
      "}"
      "QPushButton:hover { background: rgba(127,127,127,0.16); color: %5; }"
      "QPushButton:disabled { color: %4; }"
      "QFrame#NoteChromeSep { background: %6; border: none; }"
      "QLabel { background: transparent; color: %4;"
      "  font-size: 12px; font-weight: 600; }")
                                        .arg(NoteChrome::toolbarFill().name(
                                                 QColor::HexRgb),
                                             borderResolved, radiusCss,
                                             NoteChrome::textSecondary().name(
                                                 QColor::HexRgb),
                                             NoteChrome::textPrimary().name(
                                                 QColor::HexRgb),
                                             NoteChrome::borderSoft().name(
                                                 QColor::HexRgb)));
}

void MainWindow::showNoteChromeEdgeMenu(const QPoint &globalPos) {
  QMenu menu(this);
  auto addEdge = [&](const QString &label, NoteChromeEdge edge) {
    QAction *a = menu.addAction(label);
    a->setCheckable(true);
    a->setChecked(m_noteChromeEdge == edge);
    connect(a, &QAction::triggered, this, [this, edge]() { setNoteChromeEdge(edge); });
  };
  addEdge(QStringLiteral("Oben andocken"), NoteChromeEdge::Top);
  addEdge(QStringLiteral("Unten andocken"), NoteChromeEdge::Bottom);
  addEdge(QStringLiteral("Links andocken"), NoteChromeEdge::Left);
  addEdge(QStringLiteral("Rechts andocken"), NoteChromeEdge::Right);
  menu.exec(globalPos);
}

void MainWindow::positionDrawboardToolbar() {
  auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools);
  if (!tb || !m_editorCenterWidget)
    return;
  if (tb->currentStyle() != ModernToolbar::Normal)
    return;
  // Don't fight the user while they drag/snap the Favorites rail.
  if (tb->isDragging())
    return;

  const int W = m_editorCenterWidget->width();
  const int H = m_editorCenterWidget->height();

  // Desktop Drawboard: vertical Favorites rail, full page height into the corner.
  tb->applyDrawboardVerticalRail();
  const int railW = tb->preferredRailWidth();
  const int topY = noteHeaderHeight();
  // Own the bottom corner — no clearance for the utilities notch.
  const int h = qMax(UiScale::dp(200), H - topY);
  int x = 0;
  if (tb->isRailDockedLeft()) {
    // Flush to the window's left edge; page chrome stacks to the right of it.
    x = 0;
  } else {
    int rightInset = 0;
#ifndef Q_OS_ANDROID
    if (pageRailOnRight()) {
      if (m_noteLeftRail && m_noteLeftRail->isVisible())
        rightInset += m_noteLeftRail->preferredWidth();
      if (m_pageThumbnailSidebar && m_pageThumbnailSidebar->isVisible() &&
          !m_pageThumbnailSidebar->isCollapsed())
        rightInset += m_pageThumbnailSidebar->width();
    }
#endif
    x = qBound(0, W - railW - rightInset, W - railW);
  }
  const int y = topY;
  tb->setMinimumSize(0, 0);
  tb->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  tb->setFixedWidth(railW);
  tb->setGeometry(x, y, railW, h);
  tb->raise();
}

void MainWindow::positionNoteChrome() {
  if (!m_editorCenterWidget)
    return;

  const int margin = UiScale::dp(12);
  const int W = m_editorCenterWidget->width();
  const int H = m_editorCenterWidget->height();

#ifndef Q_OS_ANDROID
  int leftX = 0;
  int rightX = W;
  const bool pagesOnRight = pageRailOnRight();
  const bool thumbsExpanded =
      m_pageThumbnailSidebar && m_pageThumbnailSidebar->isVisible() &&
      !m_pageThumbnailSidebar->isCollapsed();
  const bool thumbsVisible =
      m_pageThumbnailSidebar && m_pageThumbnailSidebar->isVisible();

  // Favorites rail may occupy left or right — reserve space first when pages
  // share that edge so stacks don't overlap.
  int favLeft = 0;
  int favRight = 0;
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
    if (tb->isVisible() && tb->isDrawboardVerticalRail()) {
      if (tb->isRailDockedLeft())
        favLeft = tb->preferredRailWidth();
      else
        favRight = tb->preferredRailWidth();
    }
  }

  if (!pagesOnRight) {
    leftX = favLeft;
    if (m_noteLeftRail && m_noteLeftRail->isVisible()) {
      const int railW = m_noteLeftRail->preferredWidth();
      m_noteLeftRail->setGeometry(leftX, 0, railW, H);
      m_noteLeftRail->setThumbsAdjacent(thumbsExpanded);
      m_noteLeftRail->raise();
      leftX += railW;
    } else if (m_noteLeftRail) {
      m_noteLeftRail->setThumbsAdjacent(false);
    }

    if (thumbsExpanded) {
      const int thumbW = m_pageThumbnailSidebar->width();
      m_pageThumbnailSidebar->setGeometry(leftX, 0, thumbW, H);
      m_pageThumbnailSidebar->raise();
      leftX += thumbW;
    } else if (thumbsVisible && m_pageThumbnailSidebar->isCollapsed()) {
      // Fully folded: no stub strip. Pages button on the left rail re-opens it.
      m_pageThumbnailSidebar->setGeometry(0, 0, 0, H);
    }
  } else {
    // Pages chrome on the right (inside of Favorites when Favorites is right).
    rightX = W - favRight;
    if (m_noteLeftRail && m_noteLeftRail->isVisible()) {
      const int railW = m_noteLeftRail->preferredWidth();
      rightX -= railW;
      m_noteLeftRail->setGeometry(rightX, 0, railW, H);
      m_noteLeftRail->setThumbsAdjacent(thumbsExpanded);
      m_noteLeftRail->raise();
    } else if (m_noteLeftRail) {
      m_noteLeftRail->setThumbsAdjacent(false);
    }
    if (thumbsExpanded) {
      const int thumbW = m_pageThumbnailSidebar->width();
      rightX -= thumbW;
      m_pageThumbnailSidebar->setGeometry(rightX, 0, thumbW, H);
      m_pageThumbnailSidebar->raise();
    } else if (thumbsVisible && m_pageThumbnailSidebar->isCollapsed()) {
      m_pageThumbnailSidebar->setGeometry(0, 0, 0, H);
    }
    leftX = favLeft;
  }

  // Tool options float as a card beside the Favorites rail (not a full-height
  // sidebar dock). The rail stays edge-flush into the corner.
  if (m_toolPropertiesPanel && m_toolPropertiesVisible) {
    m_toolPropertiesPanel->show();
    m_toolPropertiesPanel->syncFromToolManager();
    const int propsW = m_toolPropertiesPanel->preferredWidth();
    const int propsH = m_toolPropertiesPanel->preferredHeight();
    int railW = UiScale::dp(64);
    bool railLeft = false;
    if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
      railW = tb->preferredRailWidth();
      railLeft = tb->isRailDockedLeft();
    }
    const int bottomH = noteBottomChromeHeight();
    const int topClear = noteChromeClearanceTop();
    int x = 0;
    if (railLeft) {
      x = leftX + margin + noteChromeClearanceLeft();
    } else {
      x = qMax(leftX + margin + noteChromeClearanceLeft(),
               W - railW - propsW - margin - noteChromeClearanceRight());
      if (pagesOnRight)
        x = qMin(x, rightX - propsW - margin);
    }
    const int y = noteHeaderHeight() + margin + topClear;
    const int maxH = qMax(UiScale::dp(200), H - y - bottomH - margin);
    // Clamp height; ToolPropertiesPanel scrolls internally so controls never
    // stack on top of each other when the card is shorter than its content.
    m_toolPropertiesPanel->setGeometry(x, y, propsW, qMin(propsH, maxH));
    m_toolPropertiesPanel->raise();
  } else if (m_toolPropertiesPanel) {
    m_toolPropertiesPanel->hide();
  }
#else
  const int leftX = 0;
  const int rightInset = 0;
  Q_UNUSED(rightInset);
#endif

  if (m_noteBottomChrome && m_noteBottomChrome->isVisible() &&
      !m_noteChromeDragging) {
    // Flush edge notch — content-sized, centered on the free edge (not a
    // full-width wall). Favorites rail owns the bottom corner separately.
    const QRect content = noteChromeContentRect();
    const int thick = noteChromeThickness();
    const bool a4 = (currentNoteView() != nullptr);
    m_noteBottomChrome->setMinimumSize(0, 0);
    m_noteBottomChrome->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    const QSize hint = m_noteBottomChrome->sizeHint();
    QRect geo;
    switch (m_noteChromeEdge) {
    case NoteChromeEdge::Top: {
      const int pillW =
          qBound(UiScale::dp(200),
                qMax(hint.width(), UiScale::dp(a4 ? 320 : 240)),
                content.width());
      const int x = content.left() + qMax(0, (content.width() - pillW) / 2);
      geo = QRect(x, content.top(), pillW, thick);
      break;
    }
    case NoteChromeEdge::Bottom: {
      const int pillW =
          qBound(UiScale::dp(200),
                qMax(hint.width(), UiScale::dp(a4 ? 320 : 240)),
                content.width());
      const int x = content.left() + qMax(0, (content.width() - pillW) / 2);
      geo = QRect(x, content.bottom() - thick + 1, pillW, thick);
      break;
    }
    case NoteChromeEdge::Left: {
      const int pillH =
          qBound(UiScale::dp(160),
                qMax(hint.height(), UiScale::dp(a4 ? 280 : 200)),
                content.height());
      const int y = content.top() + qMax(0, (content.height() - pillH) / 2);
      geo = QRect(content.left(), y, thick, pillH);
      break;
    }
    case NoteChromeEdge::Right: {
      const int pillH =
          qBound(UiScale::dp(160),
                qMax(hint.height(), UiScale::dp(a4 ? 280 : 200)),
                content.height());
      const int y = content.top() + qMax(0, (content.height() - pillH) / 2);
      geo = QRect(content.right() - thick + 1, y, thick, pillH);
      break;
    }
    }
    m_noteBottomChrome->setGeometry(geo);
    m_noteBottomChrome->raise();
  }

  positionDrawboardToolbar();
  if (m_floatingTools)
    m_floatingTools->raise();
  // Utilities notch sits under the Favorites rail at the corner so the rail
  // owns the bottom-right (or bottom-left) flush edge.
  if (m_noteBottomChrome && m_noteBottomChrome->isVisible())
    m_noteBottomChrome->raise();
  if (m_floatingTools &&
      qobject_cast<ModernToolbar *>(m_floatingTools) &&
      qobject_cast<ModernToolbar *>(m_floatingTools)->isDrawboardVerticalRail())
    m_floatingTools->raise();
  syncPenPresetBarGeometry();

#ifndef Q_OS_ANDROID
  if (m_radialFab && m_radialFab->isVisible() && !m_radialFab->isExpanded()) {
    const int fab = m_radialFab->collapsedSize();
    const int x = qMax(leftX + margin, W - margin - fab);
    const int y = qMax(margin, H - noteBottomChromeHeight() - fab - margin);
    if (m_radialFab->x() <= 0 && m_radialFab->y() <= 0)
      m_radialFab->move(x, y);
    else {
      const int nx = qBound(0, m_radialFab->x(), W - fab);
      const int ny = qBound(0, m_radialFab->y(), H - fab);
      m_radialFab->move(nx, ny);
    }
    m_radialFab->raise();
  }
  if (m_allPagesOverlay && m_allPagesOverlay->isVisible()) {
    m_allPagesOverlay->setGeometry(0, 0, W, H);
    m_allPagesOverlay->raise();
  }
#endif
}

void MainWindow::updateNoteBottomChrome() {
  if (!m_noteBottomChrome)
    return;
  // Android phone / simulate: single bottom bar is AndroidPhoneToolbar.
  if (qobject_cast<AndroidPhoneToolbar *>(m_floatingTools)) {
    m_noteBottomChrome->hide();
    return;
  }
  MultiPageNoteView *view = nullptr;
  CanvasView *canvas = nullptr;
  if (m_editorTabs) {
    QWidget *w = m_editorTabs->currentWidget();
    if (auto *editor = qobject_cast<NoteEditor *>(w))
      view = editor->view();
    else if (auto *cv = qobject_cast<CanvasView *>(w))
      canvas = cv;
    else if (w)
      canvas = w->findChild<CanvasView *>();
  }
  if (!view && !canvas) {
    m_noteBottomChrome->hide();
    return;
  }
  m_noteBottomChrome->show();

  // Page controls only for A4 MultiPage notes — infinite canvas hides them.
  const bool a4 = (view != nullptr);
  if (m_btnNotePagePrev)
    m_btnNotePagePrev->setVisible(a4);
  if (m_btnNotePageNext)
    m_btnNotePageNext->setVisible(a4);
  if (m_lblNotePage)
    m_lblNotePage->setVisible(a4);

  if (view) {
    const int pages = qMax(1, view->pageCount());
    const int cur = qBound(1, view->currentPageIndex() + 1, pages);
    const bool bm = view->isPageBookmarked(view->currentPageIndex());
    if (m_lblNotePage) {
      const bool horiz = noteChromeEdgeIsHorizontal(m_noteChromeEdge);
      if (horiz) {
        m_lblNotePage->setText(
            QStringLiteral("%1%2 of %3")
                .arg(bm ? QStringLiteral("★ ") : QString())
                .arg(cur)
                .arg(pages));
      } else {
        m_lblNotePage->setText(
            QStringLiteral("%1%2/%3")
                .arg(bm ? QStringLiteral("★") : QString())
                .arg(cur)
                .arg(pages));
      }
    }
    if (m_lblNoteZoom)
      m_lblNoteZoom->setText(
          QStringLiteral("%1%").arg(qRound(view->zoomFactor() * 100.0)));
    if (m_btnNotePagePrev)
      m_btnNotePagePrev->setEnabled(cur > 1);
    if (m_btnNotePageNext)
      m_btnNotePageNext->setEnabled(cur < pages);
  } else if (canvas) {
    if (m_lblNoteZoom)
      m_lblNoteZoom->setText(
          QStringLiteral("%1%").arg(qRound(canvas->transform().m11() * 100.0)));
  }
  positionNoteChrome();
}

void MainWindow::syncPenPresetBarGeometry() {
  if (!m_penPresetBar || !m_editorCenterWidget)
    return;
  auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools);
  // Show PenPreset chips in Drawboard when an ink tool is active — presets
  // were previously only visible in docked horizontal mode (looked like dead UI).
  const ToolMode mode = ToolManager::instance().activeToolMode();
  const bool inkTool = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                        mode == ToolMode::Highlighter);
  const bool show =
      m_floatingTools && m_floatingTools->isVisible() && tb && inkTool &&
      (tb->isDrawboardVerticalRail() ||
       (tb->currentStyle() == ModernToolbar::Normal && tb->isDockedMode()));
  if (!show) {
    m_penPresetBar->hide();
    return;
  }
  const int h = m_penPresetBar->preferredHeightPx();
  int x = 0;
  int y = 0;
  int w = 0;
  if (tb->isDrawboardVerticalRail()) {
    // Sit above the bottom utilities notch, beside the Favorites rail.
    const int margin = UiScale::dp(16);
    const int bottomH = noteBottomChromeHeight();
    const int rightClear = noteChromeClearanceRight();
    const int leftClear = noteChromeClearanceLeft();
    w = qMin(UiScale::dp(280),
             qMax(UiScale::dp(160), m_editorCenterWidget->width() -
                                        tb->width() - rightClear - leftClear -
                                        margin * 3));
    if (tb->isRailDockedLeft()) {
      x = tb->x() + tb->width() + margin;
    } else {
      x = m_editorCenterWidget->width() - tb->width() - rightClear - w - margin;
    }
    y = qMax(noteChromeClearanceTop() + margin,
             m_editorCenterWidget->height() - bottomH - h - margin);
  } else {
    x = m_floatingTools->x();
    y = m_floatingTools->y() + m_floatingTools->height() + UiScale::dp(6);
    w = m_floatingTools->width();
  }
  m_penPresetBar->setGeometry(x, y, w, h);
  m_penPresetBar->show();
  m_penPresetBar->raise();
  const auto &cfg = ToolManager::instance().config();
  m_penPresetBar->syncActive(mode, cfg.penColor, cfg.penWidth);
}

MultiPageNoteView *MainWindow::currentNoteView() const {
  if (!m_editorTabs)
    return nullptr;
  if (auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget()))
    return editor->view();
  return nullptr;
}

void MainWindow::applyNoteChromeTheme() {
#ifndef Q_OS_ANDROID
  if (m_editorCenterWidget) {
    m_editorCenterWidget->setStyleSheet(
        QStringLiteral("QWidget { background: %1; }")
            .arg(NoteChrome::canvasBg().name(QColor::HexRgb)));
  }
  if (m_noteLeftRail) {
    m_noteLeftRail->setAccentColor(NoteChrome::accent());
    refreshNoteLeftRailIcons();
    m_noteLeftRail->update();
  }
  if (m_pageThumbnailSidebar) {
    m_pageThumbnailSidebar->setAccentColor(NoteChrome::accent());
    m_pageThumbnailSidebar->rebuild();
  }
  if (m_toolPropertiesPanel) {
    m_toolPropertiesPanel->setAccentColor(NoteChrome::accent());
    m_toolPropertiesPanel->syncFromToolManager();
  }
  if (auto *view = currentNoteView())
    view->applyNoteChrome();
  if (m_allPagesOverlay)
    m_allPagesOverlay->setAccentColor(NoteChrome::accent());
  if (m_penPresetBar)
    m_penPresetBar->setAccentColor(NoteChrome::accent());
  if (m_noteBottomChrome) {
    applyNoteChromeLayoutOrientation();
    refreshNoteChromeStyle();
    refreshNoteBottomChromeIcons();
  }
  if (m_documentTabBar)
    m_documentTabBar->setNoteChromeMode(true);
  refreshNoteTitleChrome(true);
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
    tb->setAccentColor(NoteChrome::accent());
    tb->update();
  }
  positionNoteChrome();
#endif
}

void MainWindow::refreshNoteBottomChromeIcons() {
#ifndef Q_OS_ANDROID
  const QColor ic = NoteChrome::textSecondary();
  const QSize sz(UiScale::dp(18), UiScale::dp(18));
  auto apply = [&](QPushButton *btn, const QString &name) {
    if (!btn)
      return;
    btn->setText(QString());
    btn->setIcon(createModernIcon(name, ic));
    btn->setIconSize(sz);
  };
  apply(m_btnNoteUndo, QStringLiteral("undo"));
  apply(m_btnNoteRedo, QStringLiteral("redo"));
  apply(m_btnNotePagePrev, QStringLiteral("chevron_left"));
  apply(m_btnNotePageNext, QStringLiteral("chevron_right"));
  apply(m_btnNoteZoomOut, QStringLiteral("zoom_out"));
  apply(m_btnNoteZoomIn, QStringLiteral("zoom_in"));
  apply(m_btnNoteFitWidth, QStringLiteral("fit_width"));
  apply(m_btnNoteFitPage, QStringLiteral("fit_page"));
  apply(m_btnNoteChromeGrip, QStringLiteral("more_vert"));
#endif
}

void MainWindow::refreshNoteLeftRailIcons() {
#ifndef Q_OS_ANDROID
  if (!m_noteLeftRail)
    return;
  const QColor ic = NoteChrome::textSecondary();
  QColor icDim = ic;
  icDim.setAlphaF(NoteChrome::isDark() ? 0.72 : 0.55);
  const QColor icActive = NoteChrome::textPrimary();
  auto setRailIcon = [&](const QString &id, const QString &glyph,
                         bool active = false) {
    m_noteLeftRail->setIcon(id, createModernIcon(glyph, active ? icActive : icDim));
  };
  setRailIcon(QStringLiteral("pages"), QStringLiteral("pages"),
              m_noteLeftRail->pagesExpanded());
  setRailIcon(QStringLiteral("allpages"), QStringLiteral("pages"));
  setRailIcon(QStringLiteral("bookmarks"), QStringLiteral("bookmark"));
  setRailIcon(QStringLiteral("history"), QStringLiteral("history"));
  setRailIcon(QStringLiteral("search"), QStringLiteral("search"));
  setRailIcon(QStringLiteral("props"), QStringLiteral("palette"),
              m_toolPropertiesVisible);
  setRailIcon(QStringLiteral("theme"), QStringLiteral("palette"));
#endif
}

void MainWindow::refreshNoteTitleChrome(bool noteChrome) {
#ifndef Q_OS_ANDROID
  const QString hoverGray =
      QStringLiteral("background: rgba(127,127,127,0.18);");
  const QString hoverPurple =
      QStringLiteral("background: rgba(124,92,252,0.18);");

  if (m_titleBarWidget) {
    if (noteChrome) {
      m_titleBarWidget->setStyleSheet(QStringLiteral(
          "background: %1; border-bottom: 1px solid %2;")
                                          .arg(NoteChrome::toolbarFill().name(
                                                   QColor::HexRgb),
                                               NoteChrome::borderSoft().name(
                                                   QColor::HexRgb)));
    } else {
      m_titleBarWidget->setStyleSheet(BlopTheme::themed(
          "background-color: #0B0912; border-bottom: 1px solid "
          "rgba(120,130,160,0.12);"));
    }
  }

  if (btnEditorMenu) {
    if (noteChrome) {
      btnEditorMenu->setIcon(
          createModernIcon(QStringLiteral("menu"), NoteChrome::textSecondary()));
      btnEditorMenu->setStyleSheet(QStringLiteral(
          "QToolButton { background: transparent; border: none; border-radius: 10px; }"
          "QToolButton:hover { %1 }")
                                       .arg(hoverGray));
    } else {
      btnEditorMenu->setIcon(
          createModernIcon(QStringLiteral("menu"), BlopTheme::textSecondary()));
      btnEditorMenu->setStyleSheet(QStringLiteral(
          "QToolButton { background: transparent; border: none; border-radius: 10px; }"
          "QToolButton:hover { background: %1; }")
                                       .arg(BlopTheme::surfaceMuted().name(QColor::HexArgb)));
    }
  }

  if (m_lblBrand) {
    m_lblBrand->setStyleSheet(
        noteChrome
            ? QStringLiteral(
                  "color: %1; font-size: 17px; font-weight: 800;"
                  "letter-spacing: 0.4px; background: transparent; border: none;")
                  .arg(NoteChrome::textPrimary().name(QColor::HexRgb))
            : QStringLiteral(
                  "color: %1; font-size: 17px; font-weight: 800;"
                  "letter-spacing: 0.4px; background: transparent; border: none;")
                  .arg(BlopTheme::textPrimary().name(QColor::HexRgb)));
  }

  if (m_titleBarSep) {
    m_titleBarSep->setStyleSheet(
        noteChrome
            ? QStringLiteral("background: %1; border: none;")
                  .arg(NoteChrome::borderSoft().name(QColor::HexRgb))
            : QStringLiteral("background: %1; border: none;")
                  .arg(BlopTheme::borderSubtle().name(QColor::HexArgb)));
  }

  if (m_btnMode) {
    if (noteChrome) {
      m_btnMode->setStyleSheet(QStringLiteral(
          "QPushButton {"
          "  background: %1; border: 1px solid %2; border-radius: 10px;"
          "  color: %3; font-size: 12px; font-weight: 650; padding: 0 14px;"
          "}"
          "QPushButton:hover { %4 border-color: %5; color: %6; }")
                                   .arg(NoteChrome::panelBg().name(QColor::HexRgb),
                                        NoteChrome::borderSoft().name(QColor::HexRgb),
                                        NoteChrome::textSecondary().name(QColor::HexRgb),
                                        hoverGray,
                                        NoteChrome::accent().name(QColor::HexRgb),
                                        NoteChrome::textPrimary().name(QColor::HexRgb)));
    } else {
      m_btnMode->setStyleSheet(QStringLiteral(
          "QPushButton {"
          "  background: %1;"
          "  border: 1px solid %2;"
          "  border-radius: 11px;"
          "  color: %3;"
          "  font-size: 12px; font-weight: 650;"
          "  padding: 0 14px;"
          "}"
          "QPushButton:hover {"
          "  background: %4;"
          "  border-color: %5;"
          "  color: %6;"
          "}")
                                   .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
                                        BlopTheme::borderDefault().name(QColor::HexRgb),
                                        BlopTheme::textSecondary().name(QColor::HexRgb),
                                        BlopTheme::surfaceElevated().name(QColor::HexRgb),
                                        BlopTheme::accentPrimary().name(QColor::HexRgb),
                                        BlopTheme::textPrimary().name(QColor::HexRgb)));
    }
  }

  if (m_btnAddWebBookmark) {
    if (noteChrome) {
      m_btnAddWebBookmark->setStyleSheet(QStringLiteral(
          "QPushButton {"
          "  background: %1; border: none; border-radius: 8px;"
          "  color: %2; font-size: 18px; font-weight: 600;"
          "}"
          "QPushButton:hover { %3 }")
                                             .arg(NoteChrome::panelBg().name(
                                                      QColor::HexRgb),
                                                  NoteChrome::textSecondary().name(
                                                      QColor::HexRgb),
                                                  hoverGray));
    } else {
      m_btnAddWebBookmark->setStyleSheet(QStringLiteral(
          "QPushButton {"
          "  background: rgba(255,255,255,0.06);"
          "  border: none; border-radius: 8px;"
          "  color: rgba(200,196,255,0.95);"
          "  font-size: 18px; font-weight: 600;"
          "}"
          "QPushButton:hover { background: rgba(124,92,252,0.22); }"));
    }
  }

  if (m_btnNewTab) {
    if (noteChrome) {
      m_btnNewTab->setIcon(
          createModernIcon(QStringLiteral("add"), NoteChrome::textSecondary()));
      m_btnNewTab->setIconSize(QSize(UiScale::dp(18), UiScale::dp(18)));
      m_btnNewTab->setStyleSheet(QStringLiteral(
          "QPushButton {"
          "  background: transparent; border: none; border-radius: 8px;"
          "}"
          "QPushButton:hover { %1 }")
                                     .arg(hoverGray));
    } else {
      m_btnNewTab->setIcon(
          createModernIcon(QStringLiteral("add"), QColor(200, 190, 255, 220)));
      m_btnNewTab->setIconSize(QSize(18, 18));
      m_btnNewTab->setStyleSheet(QStringLiteral(
          "QPushButton {"
          "  background: transparent; border: none; border-radius: 8px;"
          "}"
          "QPushButton:hover { %1 }")
                                     .arg(hoverPurple));
    }
  }

  if (m_titleSearchBar) {
    if (noteChrome) {
      m_titleSearchBar->setStyleSheet(QStringLiteral(
          "QLineEdit {"
          "  background: %1;"
          "  border: 1px solid %2;"
          "  border-radius: 10px;"
          "  color: %3; font-size: 12px;"
          "  padding: 0 14px;"
          "}"
          "QLineEdit:focus {"
          "  background: %1;"
          "  border: 1px solid %4;"
          "}"
          "QLineEdit::placeholder { color: %5; }")
                                          .arg(NoteChrome::panelBg().name(
                                                   QColor::HexRgb),
                                               NoteChrome::borderSoft().name(
                                                   QColor::HexRgb),
                                               NoteChrome::textPrimary().name(
                                                   QColor::HexRgb),
                                               NoteChrome::accent().name(
                                                   QColor::HexRgb),
                                               NoteChrome::textSecondary().name(
                                                   QColor::HexRgb)));
    } else {
      m_titleSearchBar->setStyleSheet(QStringLiteral(
          "QLineEdit {"
          "  background: rgba(255,255,255,0.05);"
          "  border: 1px solid rgba(120,130,160,0.16);"
          "  border-radius: 11px;"
          "  color: #D8D5FF; font-size: 12px;"
          "  padding: 0 14px;"
          "}"
          "QLineEdit:focus {"
          "  background: rgba(124,92,252,0.10);"
          "  border: 1px solid rgba(124,92,252,0.48);"
          "}"
          "QLineEdit::placeholder { color: rgba(255,255,255,0.32); }"));
    }
  }

  if (m_btnEditorNoteOverflow) {
    const QColor ic = noteChrome ? NoteChrome::textSecondary()
                                 : QColor(QStringLiteral("#C8CDDC"));
    m_btnEditorNoteOverflow->setIcon(
        createModernIcon(QStringLiteral("more_pill"), ic));
    m_btnEditorNoteOverflow->setStyleSheet(
        noteChrome
            ? QStringLiteral(
                  "QToolButton { background: transparent; border: none; "
                  "border-radius: 8px; }"
                  "QToolButton:hover { %1 }")
                  .arg(hoverGray)
            : QStringLiteral(
                  "QToolButton { background: transparent; border: none; "
                  "border-radius: 8px; }"
                  "QToolButton:hover { background: rgba(124,92,252,0.22); }"));
  }

  if (m_btnTitleBarPageManager) {
    const QColor ic = noteChrome ? NoteChrome::textSecondary()
                                 : QColor(QStringLiteral("#C8CDDC"));
    m_btnTitleBarPageManager->setIcon(
        createModernIcon(QStringLiteral("pages_pill"), ic));
    m_btnTitleBarPageManager->setStyleSheet(
        noteChrome
            ? QStringLiteral(
                  "QToolButton { background: transparent; border: none; "
                  "border-radius: 8px; padding: 0; }"
                  "QToolButton:hover { %1 }"
                  "QToolButton:pressed { background: rgba(127,127,127,0.28); }")
                  .arg(hoverGray)
            : QStringLiteral(
                  "QToolButton { background: transparent; border: none; "
                  "border-radius: 8px; padding: 0; }"
                  "QToolButton:hover { background: rgba(124,92,252,0.22); }"
                  "QToolButton:pressed { background: rgba(124,92,252,0.32); }"));
  }

  auto styleWinBtn = [&](QPushButton *btn, bool isClose) {
    if (!btn)
      return;
    if (noteChrome) {
      const QString fg = NoteChrome::textSecondary().name(QColor::HexRgb);
      const QString fgHover = NoteChrome::textPrimary().name(QColor::HexRgb);
      btn->setStyleSheet(
          isClose
              ? QStringLiteral(
                    "QPushButton { background: transparent; border: none;"
                    "  color: %1; font-size: 14px; font-weight: 400; }"
                    "QPushButton:hover { background: #E81123; color: white; }")
                    .arg(fg)
              : QStringLiteral(
                    "QPushButton { background: transparent; border: none;"
                    "  color: %1; font-size: 14px; font-weight: 400; }"
                    "QPushButton:hover { %2 color: %3; }")
                    .arg(fg, hoverGray, fgHover));
    } else {
      const QString fg = BlopTheme::textSecondary().name(QColor::HexRgb);
      const QString fgHover = BlopTheme::textPrimary().name(QColor::HexRgb);
      const QString hover =
          QStringLiteral("background: %1;")
              .arg(BlopTheme::surfaceMuted().name(QColor::HexArgb));
      btn->setStyleSheet(
          isClose
              ? QStringLiteral(
                    "QPushButton { background: transparent; border: none;"
                    "  color: %1; font-size: 14px; font-weight: 400; }"
                    "QPushButton:hover { background: #E81123; color: white; }")
                    .arg(fg)
              : QStringLiteral(
                    "QPushButton { background: transparent; border: none;"
                    "  color: %1; font-size: 14px; font-weight: 400; }"
                    "QPushButton:hover { %2 color: %3; }")
                    .arg(fg, hover, fgHover));
    }
  };
  styleWinBtn(m_btnWinMin, false);
  styleWinBtn(m_btnWinMax, false);
  styleWinBtn(m_btnWinClose, true);
#endif
}

void MainWindow::showNoteBookmarksMenu() {
  auto *view = currentNoteView();
  if (!view || !view->note())
    return;
  QList<BlopInWindowMenu::Item> items;
  const QList<int> bms = view->note()->bookmarkedPageIndices();
  if (bms.isEmpty()) {
    items.push_back(
        {QStringLiteral("Keine Lesezeichen"), QIcon(), []() {}, false, false});
  } else {
    for (int idx : bms) {
      const QString title = view->pageTitle(idx);
      const bool here = (idx == view->currentPageIndex());
      items.push_back(
          {QStringLiteral("%1%2 — %3")
               .arg(here ? QStringLiteral("★ ") : QString())
               .arg(idx + 1)
               .arg(title.isEmpty() ? QStringLiteral("Seite") : title),
           QIcon(),
           [this, idx]() {
             if (auto *v = currentNoteView()) {
               v->scrollToPage(idx, true);
               updateNoteBottomChrome();
             }
           }});
    }
    BlopInWindowMenu::Item sepBm;
    sepBm.separator = true;
    items.push_back(sepBm);
    items.push_back(
        {QStringLiteral("Nächstes Lesezeichen"), QIcon(),
         [this, bms]() {
           auto *v = currentNoteView();
           if (!v || bms.isEmpty())
             return;
           const int cur = v->currentPageIndex();
           int next = -1;
           for (int idx : bms) {
             if (idx > cur) {
               next = idx;
               break;
             }
           }
           if (next < 0)
             next = bms.first(); // wrap
           v->scrollToPage(next, true);
           updateNoteBottomChrome();
         }});
  }
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);
  const int cur = view->currentPageIndex();
  items.push_back({view->isPageBookmarked(cur)
                       ? QStringLiteral("Lesezeichen hier entfernen")
                       : QStringLiteral("Aktuelle Seite markieren"),
                   QIcon(),
                   [this, cur]() {
                     if (auto *v = currentNoteView()) {
                       v->togglePageBookmark(cur);
                       if (m_pageThumbnailSidebar)
                         m_pageThumbnailSidebar->rebuild();
                       updateNoteBottomChrome();
                     }
                   }});
  // Rename current page for clearer bookmark labels.
  items.push_back(
      {QStringLiteral("Seite umbenennen…"), QIcon(),
       [this, cur]() {
         auto *v = currentNoteView();
         if (!v || !v->note())
           return;
         bool ok = false;
         const QString oldTitle = v->pageTitle(cur);
         const QString title = QInputDialog::getText(
             this, QStringLiteral("Seite umbenennen"),
             QStringLiteral("Titel für Seite %1:").arg(cur + 1),
             QLineEdit::Normal,
             oldTitle.isEmpty() ? QStringLiteral("Seite %1").arg(cur + 1)
                                : oldTitle,
             &ok);
         if (!ok)
           return;
         if (cur >= 0 && cur < v->note()->pages.size()) {
           v->note()->pages[cur].title = title.trimmed();
           if (m_pageThumbnailSidebar)
             m_pageThumbnailSidebar->rebuild();
           updateNoteBottomChrome();
         }
       }});
  QPoint anchor(UiScale::dp(60), UiScale::dp(80));
  if (m_noteLeftRail)
    anchor = m_noteLeftRail->mapToGlobal(
        QPoint(m_noteLeftRail->width(), UiScale::dp(80)));
  BlopInWindowMenu::show(this, anchor, items);
}

void MainWindow::showNoteHistoryMenu() {
  auto *view = currentNoteView();
  if (!view)
    return;
  QList<BlopInWindowMenu::Item> items;
  items.push_back(
      {QStringLiteral("Rückgängig verfügbar: %1").arg(view->undoDepth()),
       QIcon(),
       [this]() {
         if (auto *v = currentNoteView())
           v->undo();
         updateNoteBottomChrome();
       },
       false, false});
  items.push_back(
      {QStringLiteral("Wiederholen verfügbar: %1").arg(view->redoDepth()),
       QIcon(),
       [this]() {
         if (auto *v = currentNoteView())
           v->redo();
         updateNoteBottomChrome();
       },
       false, false});
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);
  for (int i = 0; i < view->pageCount(); ++i) {
    const int strokes = view->strokeCountOnPage(i);
    items.push_back(
        {QStringLiteral("Zur Seite %1 · %2 Striche")
             .arg(i + 1)
             .arg(strokes),
         QIcon(),
         [this, i]() {
           if (auto *v = currentNoteView()) {
             v->scrollToPage(i, true);
             updateNoteBottomChrome();
           }
         }});
  }
  QPoint anchor(UiScale::dp(60), UiScale::dp(120));
  if (m_noteLeftRail)
    anchor = m_noteLeftRail->mapToGlobal(
        QPoint(m_noteLeftRail->width(), UiScale::dp(120)));
  BlopInWindowMenu::show(this, anchor, items);
}

void MainWindow::showNoteExportMenu(QWidget *anchor) {
  QPoint globalPos;
  if (anchor) {
    globalPos =
        anchor->mapToGlobal(QPoint(anchor->width(), UiScale::dp(40)));
  } else {
    globalPos = QCursor::pos();
  }

  if (auto *editor = m_editorTabs
                         ? qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())
                         : nullptr) {
    editor->showOverflowMenuFromAnchor(anchor ? anchor
                                              : m_btnEditorNoteOverflow);
    return;
  }

  CanvasView *cv = getCurrentCanvas();
  if (!cv)
    return;

  QList<BlopInWindowMenu::Item> items;
  items.append({QStringLiteral("Als PDF exportieren"), QIcon(),
                [this, cv]() {
                  if (!cv)
                    return;
                  const QString path = QFileDialog::getSaveFileName(
                      this, QStringLiteral("Als PDF exportieren"),
                      QStandardPaths::writableLocation(
                          QStandardPaths::DocumentsLocation) +
                          QStringLiteral("/Blop.pdf"),
                      QStringLiteral("PDF Dokument (*.pdf)"));
                  if (path.isEmpty())
                    return;
                  const bool ok = cv->exportToPDF(path);
                  BlopDialogs::notify(
                      this,
                      ok ? QStringLiteral("Exportiert")
                         : QStringLiteral("Fehler"),
                      ok ? QStringLiteral("PDF gespeichert:\n%1").arg(path)
                         : QStringLiteral("PDF-Export fehlgeschlagen."));
                }});
  items.append({QStringLiteral("Als Bild exportieren"), QIcon(),
                [this, cv]() {
                  if (!cv)
                    return;
                  const QString path = QFileDialog::getSaveFileName(
                      this, QStringLiteral("Als Bild exportieren"),
                      QStandardPaths::writableLocation(
                          QStandardPaths::PicturesLocation) +
                          QStringLiteral("/Blop.png"),
                      QStringLiteral("PNG Bild (*.png)"));
                  if (path.isEmpty())
                    return;
                  const bool ok = cv->exportToImage(path);
                  BlopDialogs::notify(
                      this,
                      ok ? QStringLiteral("Exportiert")
                         : QStringLiteral("Fehler"),
                      ok ? QStringLiteral("Bild gespeichert:\n%1").arg(path)
                         : QStringLiteral("Bild-Export fehlgeschlagen."));
                }});
  BlopInWindowMenu::show(this, globalPos, items);
}

void MainWindow::switchToSelectTool() { onToolSelect(); }
void MainWindow::onToolSelect() { setActiveTool(CanvasView::ToolType::Select); }
void MainWindow::onToolPen() {
  if (m_activeToolType == CanvasView::ToolType::Pen) {
  } else {
    setActiveTool(CanvasView::ToolType::Pen);
  }
}
void MainWindow::onToolEraser() { setActiveTool(CanvasView::ToolType::Eraser); }
void MainWindow::onToolLasso() { setActiveTool(CanvasView::ToolType::Lasso); }
void MainWindow::onUndo() {
  if (CanvasView *cv = getCurrentCanvas()) {
    cv->undo();
    return;
  }
  QWidget *cur = m_editorTabs ? m_editorTabs->currentWidget() : nullptr;
  if (auto *ed = qobject_cast<NoteEditor *>(cur)) {
    if (MultiPageNoteView *v = ed->view())
      v->undo();
  }
}
void MainWindow::onRedo() {
  if (CanvasView *cv = getCurrentCanvas()) {
    cv->redo();
    return;
  }
  QWidget *cur = m_editorTabs ? m_editorTabs->currentWidget() : nullptr;
  if (auto *ed = qobject_cast<NoteEditor *>(cur)) {
    if (MultiPageNoteView *v = ed->view())
      v->redo();
  }
}

void MainWindow::onItemDropped(const QModelIndex &sourceIndex,
                               const QModelIndex &targetIndex) {
  if (!sourceIndex.isValid() || !targetIndex.isValid() || !m_fileModel)
    return;

  QModelIndex src = sourceIndex;
  QModelIndex dst = targetIndex;
  if (m_libraryProxy) {
    if (sourceIndex.model() == m_libraryProxy)
      src = m_libraryProxy->mapToSource(sourceIndex);
    if (targetIndex.model() == m_libraryProxy)
      dst = m_libraryProxy->mapToSource(targetIndex);
  }
  if (!src.isValid() || !dst.isValid())
    return;

  // We only support dropping INTO folders. If the target is not a dir, do
  // nothing, or we might want to drop it exactly in the target's parent
  // directory if it's a file.
  QString targetPath = m_fileModel->filePath(dst);
  QFileInfo targetInfo(targetPath);

  if (!targetInfo.isDir()) {
    targetPath =
        targetInfo
            .absolutePath(); // Drop into the same folder as the target file
  }

  QString sourcePath = m_fileModel->filePath(src);
  QFileInfo sourceInfo(sourcePath);

  if (sourcePath == targetPath)
    return; // Dropping into its current directory
  if (targetPath.startsWith(sourcePath + "/"))
    return; // Cannot drop a folder into itself

  QString newPath = targetPath + "/" + sourceInfo.fileName();

  if (QFile::exists(newPath)) {
    QMessageBox::warning(
        this, "Move failed",
        "A file or folder with this name already exists in the destination.");
    return;
  }

  // Attempt to move file/folder locally. The backend handles synchronization or
  // we handle it via network requests? NOTE: In the local version of the app,
  // renaming the file locally suffices if the path updates.
  if (sourceInfo.isDir()) {
    QDir dir;
    if (!dir.rename(sourcePath, newPath)) {
      QMessageBox::warning(this, "Move failed",
                           "Failed to move the directory.");
      return;
    }
  } else {
    if (!QFile::rename(sourcePath, newPath)) {
      QMessageBox::warning(this, "Move failed", "Failed to move the file.");
      return;
    }
  }
  LibraryTagStore::remapPath(sourcePath, newPath);
  LibraryOrgStore::remapPath(sourcePath, newPath);
  applyLibraryFilters();
}

void MainWindow::onTabChanged(int index) {
  // Persist view state of the note we're leaving.
  if (auto *prev = currentNoteView())
    prev->persistViewState(prev->property("viewStateKey").toString());

  if (m_documentTabBar) {
    if (index < 0)
      m_documentTabBar->setHomeActive(true);
    else
      m_documentTabBar->setCurrentIndex(index);
  }

  QWidget *current = m_editorTabs->currentWidget();
  NoteEditor *editor = qobject_cast<NoteEditor *>(current);
  if (m_pageThumbnailSidebar) {
    if (editor && editor->view()) {
      m_pageThumbnailSidebar->setNoteView(editor->view());
      m_pageThumbnailSidebar->onCurrentPageChanged(
          editor->view()->currentPageIndex());
    } else {
      m_pageThumbnailSidebar->setNoteView(nullptr);
    }
  }


  // WICHTIG: ToolManager auf neuen View aktualisieren
  CanvasView *cv = getCurrentCanvas();
  if (cv && m_toolManager) {
    cv->setToolManager(m_toolManager);
  }

  setActiveTool(m_activeToolType);

  // --- Aktive Notiz in die rechte Sidebar + Header schreiben ---
  QString noteTitle;
  if (index >= 0)
    noteTitle = m_editorTabs->tabText(index);

  if (m_lblActiveNote)
    m_lblActiveNote->setText(noteTitle);

  rebuildPageSettingsTags();

  if (m_noteHeader) {
#ifdef Q_OS_ANDROID
    // Phone: document tabs already show the note title; page index lives in
    // the thumbnail strip. Drop the duplicate header band that stacked with
    // the phone toolbar clutter.
    if (UiScale::isAndroidPhoneUi(this)) {
      m_noteHeader->hide();
    } else if (index >= 0 && editor && editor->view()) {
      m_noteHeader->show();
      m_lblNoteHeaderTitle->setText(noteTitle);
      const int pageCount = editor->view()->pageCount();
      const int currentPage = qMax(0, editor->view()->currentPageIndex() + 1);
      m_lblNoteHeaderMeta->setText(
          QStringLiteral("Seite %1 / %2").arg(currentPage).arg(pageCount));
    } else {
      m_noteHeader->hide();
    }
#else
    m_noteHeader->hide();
#endif
  }
  updateNoteBottomChrome();
#ifndef Q_OS_ANDROID
  positionNoteChrome();
#endif
  if (m_editorCenterWidget)
    QCoreApplication::postEvent(
        m_editorCenterWidget,
        new QResizeEvent(m_editorCenterWidget->size(), m_editorCenterWidget->size()));

  // Metadaten aus der Datei lesen (Erstellt / Geändert) — Canvas + A4 NoteEditor.
  {
    QString filePath = currentEditorNotePath();
    if (filePath.isEmpty() && cv)
      filePath = cv->property("filePath").toString();
    if (!filePath.isEmpty()) {
      QFileInfo fi(filePath);
      if (fi.exists()) {
        if (m_lblMetaCreated)
          m_lblMetaCreated->setText(fi.birthTime().toString("dd.MM.yyyy"));
        if (m_lblMetaModified) {
          qint64 secsAgo = fi.lastModified().secsTo(QDateTime::currentDateTime());
          QString relTime;
          if (secsAgo < 60)
            relTime = "Gerade eben";
          else if (secsAgo < 3600)
            relTime = QString("Vor %1 Min.").arg(secsAgo / 60);
          else if (secsAgo < 86400)
            relTime = QString("Vor %1 Std.").arg(secsAgo / 3600);
          else
            relTime = fi.lastModified().toString("dd.MM.yyyy");
          m_lblMetaModified->setText(relTime);
        }
      }
    } else {
      if (m_lblMetaCreated)
        m_lblMetaCreated->setText(QStringLiteral("—"));
      if (m_lblMetaModified)
        m_lblMetaModified->setText(QStringLiteral("—"));
    }
  }

  // Seiten-Overlay schließen, wenn keine Notiz geöffnet ist
  if (index < 0 && m_pageSettingsOverlay && m_pageSettingsOverlay->isVisible())
    setPageSettingsOverlayVisible(false);

#ifdef Q_OS_ANDROID
  if (m_pageSettingsOverlay && m_pageSettingsOverlay->isVisible())
    setPageSettingsOverlayVisible(false);
#endif

  if (editor == nullptr && m_pageManager && m_pageManager->isVisible()) {
    m_pageManager->hide();
  }
  updateSidebarState();
}


void MainWindow::restoreWindowState() {
  QSettings settings("Blop", "BlopApp");

#ifdef Q_OS_ANDROID
  // Android must always use fullscreen metrics from the current device.
  // Restoring desktop-like window geometry/state can shift the whole UI and
  // produce clipped top/bottom areas after updates or device changes.
  showFullScreen();
  return;
#endif

  if (settings.contains("geometry") && settings.contains("windowState")) {
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    show(); // Important: Actually display the window if we didn't call showMaximized/showFullScreen!
  } else {
    // Default fallback on first run
#ifdef Q_OS_ANDROID
    showFullScreen();
#else
    showMaximized();
#endif
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
#ifndef Q_OS_ANDROID
  QSettings settings("Blop", "BlopApp");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
#endif
  QMainWindow::closeEvent(event);
}

bool MainWindow::showAuthOverlay(const QUrl &url) {
#ifdef Q_OS_ANDROID
  // Google blocks OAuth inside embedded WebViews (disallowed_useragent). For
  // the Android PKCE flow we open the auth URL in a Chrome Custom Tab, which
  // runs in the same task and redirects back via
  // com.googleusercontent.apps.<client>:/oauth2redirect
  // (BlopActivity -> BlopOAuthBridge -> GoogleAuthManager).
  dismissAndroidOAuthOverlay();
  qInfo() << "showAuthOverlay: opening auth URL via Chrome Custom Tab";
  bool opened = false;
  QJniEnvironment env;
  if (env.isValid()) {
    QJniObject urlObj = QJniObject::fromString(url.toString());
    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopOAuthBridge", "openAuthUrl",
        "(Ljava/lang/String;)V", urlObj.object<jstring>());
    opened = true;
  } else {
    qWarning() << "showAuthOverlay: JNI environment not available, falling back to QDesktopServices";
    opened = QDesktopServices::openUrl(url);
  }
  if (!opened) {
    qCritical() << "showAuthOverlay: failed to open auth URL"
                << url
                << "— no browser installed OR Android 11+ <queries> blocks it.";
    // v3.18.6: release the PKCE in-progress lock immediately so the user
    // can retry without having to wait 60 s for the stale-timeout.
    GoogleAuthManager::instance().cancelPendingLogin();
    // v3.18.6: surface the failure to the user instead of silently doing
    // nothing. Use BlopModal::execBlocking so we never spawn a top-level
    // QWindow that would trip the EGL deadlock path (see settings crash
    // fix in v3.18.5).
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Anmeldung fehlgeschlagen"));
    auto *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(20, 18, 20, 16);
    lay->setSpacing(12);
    auto *lbl = new QLabel(
        tr("Es konnte kein Browser geöffnet werden. Stelle sicher, dass"
           " Chrome oder ein anderer Browser installiert ist und versuche"
           " es erneut."),
        &dlg);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "color: %1; background: transparent;")
        .arg(BlopTheme::textPrimary().name())));
    lay->addWidget(lbl);
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    auto *ok = new QPushButton(tr("OK"), &dlg);
    ok->setStyleSheet(BlopTheme::primaryButtonQss());
    ok->setDefault(true);
    btnRow->addWidget(ok);
    lay->addLayout(btnRow);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    BlopModal::execBlocking(this, &dlg);
  }
  return opened;
#else
  // Desktop: system browser; redirect to http://127.0.0.1:8080/ is handled by
  // QOAuthHttpServerReplyHandler.
  return QDesktopServices::openUrl(url);
#endif
}

#ifdef Q_OS_ANDROID
void MainWindow::resetOAuthTimer() {
    // Kept for ABI/call-site compatibility. Deep-link success no longer clears
    // in-flight here — MainWindow waits for idToken / authenticationFailed so
    // a mid-exchange unlock cannot leave the UI inconsistent.
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        MainWindow *mainWin = qobject_cast<MainWindow*>(widget);
        if (mainWin) {
            qInfo() << "MainWindow: resetOAuthTimer (no-op while verifying)";
            return;
        }
    }
    qWarning() << "MainWindow: resetOAuthTimer called but no MainWindow instance found";
}
#endif
