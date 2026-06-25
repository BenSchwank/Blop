#include "mainwindow.h"
#include "UIStyles.h"
#include "canvasview.h"
#include "moderntoolbar.h"
#include "androidphonetoolbar.h"
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
#include "uiscale.h"
#include "tools/ToolManager.h"
#include "googleauthmanager.h"
#ifdef Q_OS_ANDROID
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
#include "tools/WritingTools.h" // Enthält PenTool, PencilTool, HighlighterTool
// -------------------------------------

#include <QApplication>
#include <QColor>
#include <QMessageBox>
#include <QButtonGroup>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QQmlContext>
#include <QEvent>
#include <QFile>
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
#include <QVariant>
#include <QVariantAnimation>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
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
static const int MARGIN_OVERVIEW = 30;

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
  foldersUrl.setQuery(query);
  const QByteArray raw = getSync(nam, foldersUrl, &status, &err);
  if (err != QNetworkReply::NoError || status < 200 || status >= 300) {
    QMessageBox::warning(parent, "Ordner laden fehlgeschlagen",
                         QString("Serverantwort (%1):\n%2")
                             .arg(status)
                             .arg(QString::fromUtf8(raw)));
    return QString();
  }
  const QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isArray()) {
    QMessageBox::warning(parent, "Ordner laden fehlgeschlagen",
                         "Unerwartete Antwort vom Server.");
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
    QMessageBox::information(parent, "Keine Ordner",
                             "Es wurden keine Cloud-Ordner gefunden.");
    return QString();
  }
  bool ok = false;
  const QString chosen = QInputDialog::getItem(
      parent, "Cloud-Zielordner", "Wähle den Zielordner:", labels, 0, false, &ok);
  if (!ok || chosen.isEmpty())
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
  if (fallbackId.isEmpty()) {
    QMessageBox::information(parent, "Datei nicht gefunden",
                             "Keine passende Cloud-Datei wurde automatisch gefunden.");
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
      // SYSTEM_UI_FLAG_LAYOUT_STABLE | LAYOUT_HIDE_NAVIGATION | LAYOUT_FULLSCREEN |
      // HIDE_NAVIGATION | FULLSCREEN | IMMERSIVE_STICKY
      const jint uiFlags = 0x00000100 | 0x00000200 | 0x00000400 | 0x00000002 |
                           0x00000004 | 0x00001000;
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
    // Restore normal height when not in login mode
    const int androidHeaderTotalH = UiScale::dp(56) + UiScale::safeTopPx(window);
    chrome->setFixedHeight(androidHeaderTotalH);
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
  const int totalHeight = headerHeight + clampedInset + topExtra;

  chrome->setFixedHeight(totalHeight);
  inner->setFixedHeight(totalHeight);
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
  return QStyledItemDelegate::sizeHint(option, index);
}
void ModernItemDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  QRect rect = option.rect.adjusted(4, 4, -4, -4);

  if (option.state & QStyle::State_MouseOver) {
    painter->translate(rect.center());
    painter->scale(1.02, 1.02);
    painter->translate(-rect.center());
  }

  // Blop Notes Redesign (Etappe 3) Item Background
  QColor bgColor = QColor("#161423"); // Modern Card background
  if (option.state & QStyle::State_Selected)
    bgColor = m_window->currentAccentColor().darker(150);
  else if (option.state & QStyle::State_MouseOver)
    bgColor = QColor("#1C1A29"); // Hover background

  painter->setBrush(bgColor);
  painter->setPen(Qt::NoPen);
  painter->drawRoundedRect(rect, 16, 16);

  QString fileName = index.data(Qt::DisplayRole).toString();
  QIcon icon;
  bool isFolder = index.data(Qt::UserRole + 1).toBool(); // Assuming UserRole+1 identifies folders if set by model
  
  if (fileName.endsWith(".bnote", Qt::CaseInsensitive)) {
      icon = m_window->createModernIcon("note_bnote", QColor("#A0A0C8"));
  } else if (fileName.endsWith(".blop", Qt::CaseInsensitive)) {
      icon = m_window->createModernIcon("note_blop", m_window->currentAccentColor());
  } else {
      icon = index.data(Qt::DecorationRole).value<QIcon>();
      if (icon.isNull())
          icon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
  }
  QString text = index.data(Qt::DisplayRole).toString();
  painter->setPen(QColor("#e0e0e0"));

  bool isWideList = rect.width() > (rect.height() * 1.5);

  // Aggressive shrink (0.65x) on desktop felt cramped on phones, where the
  // tiles are larger and the icon should look prominent. Use 0.95x on
  // Android so the glyph fills the touch target the user actually sees.
#ifdef Q_OS_ANDROID
  const double iconShrink = 0.95;
  // QListView in IconMode can collapse to a single column when the model
  // contains very few items, which then triggers the "wide list" code path
  // (tiny icon + text on the right). On Android we always want the proper
  // square-tile rendering, so force the grid branch unconditionally.
  isWideList = false;
#else
  const double iconShrink = 0.65;
#endif

  if (isWideList) {
    int iconDim = rect.height() - 20;
    if (iconDim < 16)
      iconDim = 16;
    iconDim = qMax(16, (int)(iconDim * iconShrink));
    QRect iconRect(rect.left() + 12, rect.center().y() - iconDim / 2, iconDim,
                   iconDim);
    icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);
    QRect textRect = rect;
    textRect.setLeft(iconRect.right() + 15);
    textRect.setRight(rect.right() - 40);
    QFont f = painter->font();
    f.setBold(true);
    f.setPointSize(FONT_SIZE_BASE);
    painter->setFont(f);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight,
                                                        textRect.width()));
  } else {
    int textH = 30;
    int maxIconH = rect.height() - textH - 10;
    int maxIconW = rect.width() - 20;
    int iconDim = qMin(maxIconW, maxIconH);
    iconDim = qMax(18, (int)(iconDim * iconShrink));
#ifdef Q_OS_ANDROID
    // Hard floor: even on cramped tile-rect calculations the glyph should
    // never be smaller than dp(80) so the user can actually recognise the
    // note/folder icon. The tile itself will grow as needed via updateGrid.
    iconDim = qMax(iconDim, qMin(maxIconW, qMin(maxIconH, UiScale::dp(80))));
#endif

    int contentHeight = iconDim + textH + 5;
    int startY = rect.top() + (rect.height() - contentHeight) / 2;
    QRect iconRect(rect.center().x() - iconDim / 2, startY, iconDim, iconDim);

    icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::On);

    if (textH > 0) {
      QRect textRect(rect.left() + 4, iconRect.bottom() + 5, rect.width() - 8,
                     textH);
      QFont f = painter->font();
      f.setPointSize(FONT_SIZE_BASE - 2);
      painter->setFont(f);
      QTextOption opt;
      opt.setAlignment(Qt::AlignCenter);
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
  const int pillW = 30;
  const int pillH = 20;
#endif
  QRect menuRect(rect.right() - pillW - 4, rect.top() + 4, pillW, pillH);
  if (isWideList)
    menuRect.moveTop(rect.center().y() - pillH / 2);
  menuIcon.paint(painter, menuRect, Qt::AlignCenter);

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

  setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

#ifdef Q_OS_ANDROID
  QFont f = this->font();
  f.setPointSize(FONT_SIZE_BASE);
  this->setFont(f);
#endif
  qDebug() << "MainWindow: Konstruktor start";

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
    m_authNavigationLocked = false;
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
#ifdef Q_OS_ANDROID
      qWarning() << "Google Login Fehler:" << error;
#else
      QMessageBox::warning(this, "Google Login Fehler", "Der Login über Google ist fehlgeschlagen:\n" + error);
#endif
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
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools))
    tb->setAccentColor(m_currentAccentColor);
  if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools))
    phone->setAccentColor(m_currentAccentColor);

  // PageManager: re-skin scrim + panel using the new tokens. The panel
  // QSS was set once in the ctor; this method is the documented post-
  // theme-change refresh hook.
  if (m_pageManager) {
    m_pageManager->applyThemeRefresh();
  }
  // v3.18.5: re-skin every control inside the Page-Settings sheet so
  // theme toggles take effect on already-constructed widgets.
  refreshPageSettingsTheme();

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
    QString currentVer = localVersion;
    QMessageBox *box = new QMessageBox(this);
    box->setWindowTitle("Update verfügbar");
    box->setText(QString("<b>Blop %1 ist verfügbar!</b><br>"
                         "<small>Deine Version: %2</small>")
                     .arg("v" + tagName, "v" + currentVer));
    box->setInformativeText(
        "Möchtest du die neue Version jetzt herunterladen?");
    box->setStandardButtons(QMessageBox::Yes | QMessageBox::Ignore);
    box->setDefaultButton(QMessageBox::Yes);
    box->button(QMessageBox::Yes)->setText("Jetzt herunterladen");
    box->button(QMessageBox::Ignore)->setText("Später");
    box->setIcon(QMessageBox::Information);
    box->setStyleSheet(
        "QMessageBox { background-color: #1e1e1e; color: white; }"
        "QLabel { color: #e0e0e0; }"
        "QPushButton { background: #5E5CE6; color: white; border-radius: 6px; "
        "padding: 6px 18px; border: none; } "
        "QPushButton:hover { background: #7D7AFF; } "
        "QPushButton[text='Später'] { background: #333; }");

    if (box->exec() == QMessageBox::Yes) {
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
              QMessageBox::warning(this, "Fehler", "Lokale Datei konnte nicht erstellt werden.");
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
                      QMessageBox::warning(this, "Fehler", "Fehler beim Download des Updates:\n" + dlReply->errorString());
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
    box->deleteLater();
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
  m_titleBarWidget->setFixedHeight(50); // Unified single-row layout
  m_titleBarWidget->setStyleSheet(
      "background: #1A1B2E;" // Solid elevated navy background
      "border-bottom: 1px solid rgba(255,255,255,0.06);");

  QHBoxLayout *mainLayout = new QHBoxLayout(m_titleBarWidget);
  mainLayout->setContentsMargins(10, 0, 0, 0);
  mainLayout->setSpacing(6);

  // ── Hamburger ─────────────────────────────────────────────────────────────
  btnEditorMenu = new ModernButton(m_titleBarWidget);
  btnEditorMenu->setIcon(createModernIcon("menu", QColor("#A0A0C8")));
  btnEditorMenu->setFixedSize(36, 36);
  btnEditorMenu->setToolTip("Navigation");
  btnEditorMenu->setStyleSheet(
      "QToolButton {"
      "  background: transparent; border: none; border-radius: 8px;"
      "}"
      "QToolButton:hover {"
      "  background: rgba(255,255,255,0.08);"
      "}");
  connect(btnEditorMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  mainLayout->addWidget(btnEditorMenu);
  mainLayout->addSpacing(4);

  // ── Blop Brand ────────────────────────────────────────────────────────────
  QLabel *lblBrand = new QLabel("Blop", m_titleBarWidget);
  lblBrand->setStyleSheet(
      "color: #E6E4FF; font-size: 15px; font-weight: 700;"
      "letter-spacing: 1px; background: transparent; border: none;");
  mainLayout->addWidget(lblBrand);

  // ── Separator ─────────────────────────────────────────────────────────────
  QFrame *sep = new QFrame(m_titleBarWidget);
  sep->setFrameShape(QFrame::VLine);
  sep->setFixedSize(1, 18);
  sep->setStyleSheet("background: rgba(255,255,255,0.12); border: none;");
  mainLayout->addSpacing(10);
  mainLayout->addWidget(sep);
  mainLayout->addSpacing(10);

  // ── Top Navigation Container ───────────────────────────────────────────────
  m_topNavControls = new QWidget(m_titleBarWidget);
  m_topNavControls->setStyleSheet("background: transparent; border: none;");
  QHBoxLayout *navLayout = new QHBoxLayout(m_topNavControls);
  navLayout->setContentsMargins(0, 0, 0, 0);
  navLayout->setSpacing(6);

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
  btnMode->setFixedHeight(32);
  btnMode->setCursor(Qt::PointingHandCursor);
  btnMode->setStyleSheet(
      "QPushButton {"
      "  background: rgba(255,255,255,0.06);"
      "  border: none;"
      "  border-radius: 8px;"
      "  color: rgba(200,196,255,0.85);"
      "  font-size: 12px; font-weight: 600;"
      "  padding: 0 14px;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.18);"
      "  color: rgba(220,216,255,1.0);"
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
  // Direkt links (nach Modus / +): Home, offene Notizen, neuer Tab — nicht hinter der Suche.
  QWidget *tabsContainer = new QWidget(m_topNavControls);
  m_noteTabsChrome = tabsContainer;
  tabsContainer->setStyleSheet("background: transparent; border: none;");
  m_tabBarLayout = new QHBoxLayout(tabsContainer);
  m_tabBarLayout->setContentsMargins(0, 0, 0, 0);
  m_tabBarLayout->setSpacing(4);

  // Tab Styles (größere Schrift + Icons — besser lesbar in der Titelleiste)
  const int kTabIconPx = 22;
  auto tabActiveStyle = []() -> QString {
    return "QPushButton {"
           "  background: rgba(124,92,252,0.25);"
           "  border: none;"
           "  border-radius: 8px;"
           "  color: #E8E4FF; font-size: 14px; font-weight: 600;"
           "  padding: 0 16px;"
           "}"
           "QPushButton:hover { background: rgba(124,92,252,0.35); }";
  };
  auto tabInactiveStyle = []() -> QString {
    return "QPushButton {"
           "  background: transparent;"
           "  border: none;"
           "  border-radius: 8px;"
           "  color: rgba(255,255,255,0.45); font-size: 14px; font-weight: 500;"
           "  padding: 0 14px;"
           "}"
           "QPushButton:hover {"
           "  background: rgba(255,255,255,0.08);"
           "  color: rgba(255,255,255,0.85);"
           "}";
  };

  // Home Tab
  m_btnHomeTab = new QPushButton(tabsContainer);
  m_btnHomeTab->setIcon(
      createModernIcon(QStringLiteral("home"), QColor(QStringLiteral("#E8E4FF"))));
  m_btnHomeTab->setText(QStringLiteral(" Home"));
  m_btnHomeTab->setIconSize(QSize(kTabIconPx, kTabIconPx));
  m_btnHomeTab->setFixedHeight(36);
  m_btnHomeTab->setCursor(Qt::PointingHandCursor);
  m_btnHomeTab->setStyleSheet(tabActiveStyle());
  connect(m_btnHomeTab, &QPushButton::clicked, this,
          [this, tabActiveStyle, tabInactiveStyle]() {
            m_activeTabIndex = -1;
            m_btnHomeTab->setIcon(createModernIcon(
                QStringLiteral("home"), QColor(QStringLiteral("#E8E4FF"))));
            m_btnHomeTab->setStyleSheet(tabActiveStyle());
            for (auto *btn : m_noteTabButtons)
              btn->setStyleSheet(tabInactiveStyle());
            onBackToOverview();
          });
  m_tabBarLayout->addWidget(m_btnHomeTab);

  // "+ Tab" Button
  QPushButton *btnNewTab = new QPushButton("+", tabsContainer);
  btnNewTab->setFixedSize(36, 36);
  btnNewTab->setCursor(Qt::PointingHandCursor);
  btnNewTab->setToolTip("Neue Notiz öffnen");
  btnNewTab->setStyleSheet(
      "QPushButton {"
      "  background: transparent;"
      "  border: none;"
      "  border-radius: 8px;"
      "  color: rgba(255,255,255,0.45); font-size: 20px; font-weight: 500;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.18);"
      "  color: rgba(200,190,255,0.95);"
      "}");
  connect(btnNewTab, &QPushButton::clicked, this,
          &MainWindow::onShowNewTabPopup);
  m_tabBarLayout->addWidget(btnNewTab);

  navLayout->addWidget(tabsContainer);
  navLayout->addSpacing(10);
  // Rest der Leiste nach rechts: Suche und Aktions-Icons
  navLayout->addStretch(1);

  // ── Suchleiste ─────────────────────────────────────────────────────────────
  m_titleSearchBar = new QLineEdit(m_topNavControls);
  m_titleSearchBar->setPlaceholderText(
      QStringLiteral("Notizen durchsuchen..."));
  m_titleSearchBar->setFixedHeight(32);
  m_titleSearchBar->setFixedWidth(200);
  m_titleSearchBar->setStyleSheet(
      "QLineEdit {"
      "  background: rgba(255,255,255,0.06);"
      "  border: none;"
      "  border-radius: 8px;"
      "  color: #D8D5FF; font-size: 12px;"
      "  padding: 0 14px;"
      "}"
      "QLineEdit:focus {"
      "  background: rgba(124,92,252,0.12);"
      "  border: 1px solid rgba(124,92,252,0.50);"
      "}"
      "QLineEdit::placeholder { color: rgba(255,255,255,0.35); }");
  navLayout->addWidget(m_titleSearchBar);
  navLayout->addSpacing(8);

  // Tags & Seiten-Optionen nur noch über Notiz-Menü (⋯) → „Optionen & Tags…“
  // Höhe = ROW_HEIGHT_ITEM (wie Sidebar-Nav-Zeilen „All / Blop Notes / …“).
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
}


// Window Dragging Implementation
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
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
    } else if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
      if (tb->isDockedMode()) {
        int idealW = tb->calculateMinLength();
        // Slightly shorter docked toolbar at full width.
        idealW = qMax(220, int(idealW * 0.90));
        // Toolbar coordinates are relative to m_editorCenterWidget; no extra inset needed.
        const int availableW = qMax(240, m_editorCenterWidget->width());
        const int dockW = qMin(idealW, availableW);
        int dockX = qMax(0, (availableW - dockW) / 2);
        if (dockX + dockW > m_editorCenterWidget->width())
          dockX = m_editorCenterWidget->width() - dockW;
        tb->setGeometry(dockX, 0, dockW, 48);
      } else {
        int xPos = (m_editorCenterWidget->width() - tb->width()) / 2;
        int yPos = qMax(10, tb->y()); // don't push it out of bounds
        tb->move(xPos, yPos);
      }
      tb->raise();
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
    // v119 perf: QEvent::Move fires on EVERY pixel of a drag and was
    // re-evaluating the dock condition + (when triggered) running an
    // animation 60+ times per second. Gate on a small y-threshold so
    // we only re-evaluate when the toolbar's y actually crossed a
    // meaningful boundary.
    const int newY = m_floatingTools->y();
    if (qAbs(newY - m_lastDockCheckY) >= 8) {
      m_lastDockCheckY = newY;
      // Re-dock when floating toolbar is dragged to the top (y <= 10)
      if (newY <= 10) {
        if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
          tb->setDockMode(true);
        }
      }
    }
  }

  return QMainWindow::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// addNoteTab / closeNoteTab  (work with the editorHeaderWidget tab bar)
// ---------------------------------------------------------------------------
void MainWindow::addNoteTab(const QString &title) {
  if (!m_tabBarLayout || !m_btnHomeTab)
    return;

  // Deactivate Home tab visually (inaktiver Stil)
  m_btnHomeTab->setIcon(createModernIcon(
      QStringLiteral("home"), QColor(255, 255, 255, 130)));
  m_btnHomeTab->setStyleSheet(
      "QPushButton {"
      "  background: rgba(255,255,255,0.04);"
      "  border: 1px solid rgba(255,255,255,0.08);"
      "  color: rgba(255,255,255,0.45); font-size: 14px; font-weight: 500;"
      "  padding: 0 14px; border-radius: 14px;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.10);"
      "  border: 1px solid rgba(124,92,252,0.25);"
      "  color: rgba(255,255,255,0.80);"
      "}");

  int tabIndex = m_noteTabButtons.size();

  // Container widget: [icon+title][×] – Pill mit größeren Klickflächen
  QWidget *tabWidget = new QWidget;
  tabWidget->setFixedHeight(34);
  tabWidget->setStyleSheet(
      "QWidget {"
      "  background-color: rgba(124,92,252,0.22);"
      "  border: 1px solid rgba(124,92,252,0.55);"
      "  border-radius: 16px;"
      "}");
  QHBoxLayout *tabLay = new QHBoxLayout(tabWidget);
  tabLay->setContentsMargins(8, 0, 6, 0);
  tabLay->setSpacing(4);

  QPushButton *tabBtn = new QPushButton(title, tabWidget);
  tabBtn->setIcon(
      createModernIcon(QStringLiteral("note_bnote"), QColor(QStringLiteral("#E8E8FF"))));
  tabBtn->setIconSize(QSize(22, 22));
  tabBtn->setFixedHeight(26);
  tabBtn->setCursor(Qt::PointingHandCursor);
  tabBtn->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  border: none; color: #E8E8FF; font-size: 14px; font-weight: 600;"
      "  padding: 0 2px 0 4px;"
      "}");
  m_noteTabButtons.append(tabBtn);
  tabLay->addWidget(tabBtn);

  QPushButton *closeBtn = new QPushButton("\u2715", tabWidget);
  closeBtn->setFixedSize(24, 24);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  border: none; color: rgba(255,255,255,0.50); font-size: 12px;"
      "  border-radius: 12px;"
      "}"
      "QPushButton:hover { background-color: rgba(255,255,255,0.15); color: white; }");
  connect(closeBtn, &QPushButton::clicked, this,
          [this, tabIndex]() { closeNoteTab(tabIndex); });
  tabLay->addWidget(closeBtn);

  int insertIdx = m_tabBarLayout->indexOf(m_btnHomeTab) + 1;
  m_tabBarLayout->insertWidget(insertIdx + tabIndex, tabWidget);
  m_activeTabIndex = tabIndex;
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
  if (!m_tabBarLayout || index < 0 || index >= m_noteTabButtons.size())
    return;

  QWidget *tabWidget = m_noteTabButtons.at(index)->parentWidget();
  m_tabBarLayout->removeWidget(tabWidget);
  tabWidget->deleteLater();
  m_noteTabButtons.removeAt(index);

  if (m_noteTabButtons.isEmpty() && m_btnHomeTab) {
    m_activeTabIndex = -1;
    m_btnHomeTab->setIcon(createModernIcon(
        QStringLiteral("home"), QColor(QStringLiteral("#E8E4FF"))));
    // Aktiver Stil wieder herstellen (neue Farben)
    m_btnHomeTab->setStyleSheet(
        "QPushButton {"
        "  background: rgba(124,92,252,0.22);"
        "  border: 1px solid rgba(124,92,252,0.55);"
        "  color: #DDD8FF; font-size: 14px; font-weight: 600;"
        "  padding: 0 14px; border-radius: 14px;"
        "}"
        "QPushButton:hover { background: rgba(124,92,252,0.30); }");
  }
}


void MainWindow::onWinMinimize() { showMinimized(); }
void MainWindow::onWinClose() { close(); }
void MainWindow::onWinMaximize() {
  if (isMaximized())
    showNormal();
  else
    showMaximized();
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
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
  MSG *msg = static_cast<MSG *>(message);
  if (msg->message == WM_NCHITTEST) {
    if (isMaximized()) return false;
    
    long x = GET_X_LPARAM(msg->lParam);
    long y = GET_Y_LPARAM(msg->lParam);
    QPoint pos = mapFromGlobal(QPoint(x, y));
    
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

    // Titelbereich: Differenziere zwischen Drag-Bereich und interaktiven Widgets
    if (m_titleBarWidget && m_titleBarWidget->isVisible()) {
      int titleBarBottom = m_titleBarWidget->geometry().bottom();
      if (pos.y() <= titleBarBottom) {
        // Prüfe ob ein interaktives Kind-Widget unter der Maus liegt
        QWidget *child = m_titleBarWidget->childAt(
            m_titleBarWidget->mapFromGlobal(QPoint(x, y)));
        if (child && child != m_titleBarWidget) {
          // Button, ComboBox, etc. → Qt bekommt den Klick
          *result = HTCLIENT;
        } else {
          // Leerer Titelbereich → Fenster drag
          *result = HTCAPTION;
        }
        return true;
      }
    }

    // Restlicher Client-Bereich → Klicks normal durchlassen
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
  if (auto *tb = qobject_cast<ModernToolbar *>(m_floatingTools))
    tb->setAccentColor(m_currentAccentColor);
  if (auto *phone = qobject_cast<AndroidPhoneToolbar *>(m_floatingTools))
    phone->setAccentColor(m_currentAccentColor);

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
        "background-color: #0F111A; border-right: 1px solid rgba(255,255,255,0.06);"));
  if (m_sidebarStrip)
    m_sidebarStrip->setStyleSheet(BlopTheme::themed(
        "background-color: #0F111A; border-right: 1px solid rgba(255,255,255,0.06);"));
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

  if (m_titleBarWidget)
    m_titleBarWidget->setStyleSheet(BlopTheme::themed(
        "background-color: #0D0B14; border-bottom: 1px solid #201E2E;"));

  // Overview controls follow current accent color while keeping dark base style.
  if (m_overviewContainer) {
    m_overviewContainer->setStyleSheet(BlopTheme::themed(
        QString(
            "QLineEdit#overviewSearchBar {"
            "  background-color: #1A1829; color: white;"
            "  border: 1px solid %1; border-radius: 22px;"
            "  min-height: 44px; max-height: 44px;"
            "  padding: 0 20px; font-size: 13px;"
            "}"
            "QLineEdit#overviewSearchBar:focus { border: 1px solid %2; }"
            "QPushButton#overviewBtnNewNote {"
            "  background-color: %1; color: white; border-radius: 20px;"
            "  padding: 0 24px; font-weight: bold; font-size: 13px; border: none;"
            "}"
            "QPushButton#overviewBtnNewNote:hover { background-color: %2; }"
            "QPushButton#overviewBtnNewFolder {"
            "  background-color: transparent; color: white; border-radius: 20px;"
            "  padding: 0 24px; font-weight: bold; font-size: 13px;"
            "  border: 1px solid rgba(255,255,255,0.28);"
            "}"
            "QPushButton#overviewBtnNewFolder:hover {"
            "  background-color: rgba(255,255,255,0.05); border-color: %1; }")
            .arg(c, c_light)));
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
    const int r = qMax(1, m_lblSidebarAvatar->width() / 2);
    m_lblSidebarAvatar->setStyleSheet(
        QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %1,stop:1 %2); "
                "border-radius: %3px; color: white; font-weight: bold; font-size: 12px;")
            .arg(c, c_light)
            .arg(r));
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

#ifdef Q_OS_ANDROID
  // Match Windows desktop grid: same profile-driven tile size, square cells, spacing
  int screenWidth = 0;
  if (m_fileListView->viewport())
    screenWidth = m_fileListView->viewport()->width();
  if (screenWidth <= 0)
    screenWidth = m_fileListView->width();
  if (screenWidth <= 0)
    screenWidth = QApplication::primaryScreen()->availableGeometry().width();

  // Floor the iconSize to a touch-friendly value on Android. The desktop
  // default of 100px renders embarrassingly small on phones - bump to dp(140)
  // so each tile is comfortably tappable.
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
  // Hard minimum so a single-column fallback (small viewport, few items)
  // still produces a visibly-sized tile instead of a tiny pill.
  itemWidth = qMax(itemWidth, UiScale::dp(120));

  // Force a perfectly square tile: itemHeight == itemWidth. Combined with
  // the matching gridSize this prevents Qt from stretching items horizontally
  // when only one column fits, which previously triggered the "wide list"
  // delegate branch with shrunken icons.
  const int itemHeight = itemWidth;

  m_fileListView->setItemSize(QSize(itemWidth, itemHeight));
  m_fileListView->setIconSize(QSize(itemWidth, itemWidth));
  m_fileListView->setGridSize(QSize(itemWidth + spacing, itemHeight + spacing));
  m_fileListView->setUniformItemSizes(true);

#else
  int s = m_currentProfile.iconSize;
  if (s <= 20)
    s = 20;
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
#endif
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
    p.drawRect(14, 20, 36, 28);
    p.drawLine(14, 20, 24, 12);
    p.drawLine(24, 12, 50, 20);
  } else if (name == "cloud") {
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
    // A4 Portrait with lines
    p.drawRect(16, 12, 32, 40);
    p.setPen(QPen(color, 2));
    p.drawLine(22, 22, 42, 22);
    p.drawLine(22, 30, 42, 30);
    p.drawLine(22, 38, 42, 38);
  } else if (name == "note_blop") {
    // Infinite Square with grid
    p.drawRect(14, 14, 36, 36);
    p.setPen(QPen(color, 1, Qt::DotLine));
    p.drawLine(26, 14, 26, 50);
    p.drawLine(38, 14, 38, 50);
    p.drawLine(14, 26, 50, 26);
    p.drawLine(14, 38, 50, 38);
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

  {
    QVBoxLayout *chromeLay = new QVBoxLayout(androidTopChrome);
    chromeLay->setContentsMargins(0, 0, 0, 0);
    chromeLay->setSpacing(0);
    chromeLay->addWidget(androidHeader);
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
  overviewLayout->setContentsMargins(MARGIN_OVERVIEW, MARGIN_OVERVIEW,
                                     MARGIN_OVERVIEW, MARGIN_OVERVIEW);
#endif

  QHBoxLayout *overviewTopRow = new QHBoxLayout();
  btnOverviewMenu = new ModernButton(this);
  btnOverviewMenu->setIcon(createModernIcon("menu", Qt::white));
  connect(btnOverviewMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
#ifdef Q_OS_ANDROID
  // Android already has the hamburger in the androidHeader bar — hide duplicate
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

  // --- NEW HEADER BEREICH (Blop Notes Redesign Etappe 3) ---
  QVBoxLayout *headerLayout = new QVBoxLayout();

#ifdef Q_OS_ANDROID
  // ── Android: compact welcome page (phone + tablet friendly). Buttons stay
  //    side-by-side, sized to comfortable Material touch targets (~40 dp), and
  //    the entire welcome card centres + caps at dp(560) on tablets.
  headerLayout->setContentsMargins(UiScale::dp(10), UiScale::dp(16),
                                   UiScale::dp(10), UiScale::dp(24));
  headerLayout->setSpacing(UiScale::dp(12));

  btnEditorMenu = new ModernButton(m_overviewContainer);
  btnEditorMenu->setIcon(createModernIcon("menu", Qt::white));
  btnEditorMenu->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btnEditorMenu->setCursor(Qt::PointingHandCursor);
  btnEditorMenu->setStyleSheet(
      "QToolButton { background: transparent; border: none; border-radius: 8px; }"
      "QToolButton:pressed { background-color: rgba(255,255,255,0.08); }");
  connect(btnEditorMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  headerLayout->addWidget(btnEditorMenu, 0, Qt::AlignLeft);

  QLabel *lblWelcome = new QLabel("Willkommen zurück!", m_overviewContainer);
  lblWelcome->setStyleSheet(BlopTheme::themed(
      QStringLiteral("color: #F4F5FB; font-size: 22px; font-weight: 700;"
                     " font-family: 'Roboto', 'Segoe UI', sans-serif;"
                     " letter-spacing: -0.2px;")));
  headerLayout->addWidget(lblWelcome);

  QLineEdit *searchBar = new QLineEdit(m_overviewContainer);
  searchBar->setObjectName("overviewSearchBar");
  searchBar->setPlaceholderText("Notizen durchsuchen...");
  searchBar->setFrame(false);
  searchBar->setAttribute(Qt::WA_StyledBackground, true);
  searchBar->setFixedHeight(UiScale::dp(40));
  searchBar->setStyleSheet(BlopTheme::themed(
      "QLineEdit {"
      "  background-color: #1A1829;"
      "  color: #F4F5FB;"
      "  border: 1px solid #201E2E;"
      "  border-radius: 20px;"
      "  padding: 0 14px;"
      "  font-size: 12px;"
      "}"
      "QLineEdit:focus {"
      "  border: 1px solid #5E5CE6;"
      "}"
  ));

  QPushButton *btnNewNote = new QPushButton("Neue Notiz", m_overviewContainer);
  btnNewNote->setObjectName("overviewBtnNewNote");
  btnNewNote->setFixedHeight(UiScale::dp(40));
  btnNewNote->setCursor(Qt::PointingHandCursor);
  btnNewNote->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  btnNewNote->setStyleSheet(
      "QPushButton {"
      "  background-color: #5E5CE6;"
      "  color: white;"
      "  border-radius: 20px;"
      "  padding: 0 14px;"
      "  font-weight: 600;"
      "  font-size: 12px;"
      "  border: none;"
      "}"
      "QPushButton:pressed { background-color: #4F4DCF; }"
  );
  connect(btnNewNote, &QPushButton::clicked, this, &MainWindow::onNewPage);
  BlopRipple::attachPressFeedback(btnNewNote, 0.94);

  QPushButton *btnNewFolder = new QPushButton("Neuer Ordner", m_overviewContainer);
  btnNewFolder->setObjectName("overviewBtnNewFolder");
  btnNewFolder->setFixedHeight(UiScale::dp(40));
  btnNewFolder->setCursor(Qt::PointingHandCursor);
  btnNewFolder->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  btnNewFolder->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  color: white;"
      "  border-radius: 20px;"
      "  padding: 0 14px;"
      "  font-weight: 600;"
      "  font-size: 12px;"
      "  border: 1px solid rgba(255,255,255,0.18);"
      "}"
      "QPushButton:pressed { background-color: rgba(255,255,255,0.06); }"
  );
  connect(btnNewFolder, &QPushButton::clicked, this, &MainWindow::onCreateFolder);
  BlopRipple::attachPressFeedback(btnNewFolder, 0.94);

  // Cap individual elements to the device viewport so nothing spills.
  const int welcomeContentW = UiScale::androidContentWidthPx(this);
  searchBar->setMaximumWidth(welcomeContentW);
  btnNewNote->setMaximumWidth(welcomeContentW);
  btnNewFolder->setMaximumWidth(welcomeContentW);

  // Always horizontal action row - phones get smaller buttons, never wrap.
  QVBoxLayout *searchBlock = new QVBoxLayout();
  searchBlock->setSpacing(UiScale::dp(10));
  searchBlock->addWidget(searchBar);
  QHBoxLayout *actionsRow = new QHBoxLayout();
  actionsRow->setSpacing(UiScale::dp(8));
  actionsRow->addWidget(btnNewNote, 1);
  actionsRow->addWidget(btnNewFolder, 1);
  searchBlock->addLayout(actionsRow);

  // On wide screens (tablets / landscape) cap the welcome card at dp(560) and
  // centre it horizontally so the buttons don't stretch ridiculously wide.
  const int welcomeCardMaxW = UiScale::dp(560);
  if (welcomeContentW > welcomeCardMaxW) {
    QWidget *cardWrap = new QWidget(m_overviewContainer);
    cardWrap->setMaximumWidth(welcomeCardMaxW);
    cardWrap->setLayout(searchBlock);
    QHBoxLayout *centerRow = new QHBoxLayout();
    centerRow->setContentsMargins(0, 0, 0, 0);
    centerRow->addStretch(1);
    centerRow->addWidget(cardWrap, 0);
    centerRow->addStretch(1);
    headerLayout->addLayout(centerRow);
  } else {
    headerLayout->addLayout(searchBlock);
  }

#else
  // ── Desktop/Windows: Original layout unchanged ───────────────────────────
  headerLayout->setContentsMargins(10, 20, 10, 30);
  headerLayout->setSpacing(15);

  QLabel *lblWelcome = new QLabel("Willkommen zurück!", m_overviewContainer);
  lblWelcome->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: #F4F5FB; font-size: 28px; font-weight: bold; font-family: 'Segoe UI';")));
  headerLayout->addWidget(lblWelcome);

  QHBoxLayout *searchActionLayout = new QHBoxLayout();
  searchActionLayout->setSpacing(15);

  QLineEdit *searchBar = new QLineEdit(m_overviewContainer);
  searchBar->setObjectName("overviewSearchBar");
  searchBar->setPlaceholderText("Notizen durchsuchen...");
  searchBar->setFrame(false);
  searchBar->setAttribute(Qt::WA_StyledBackground, true);
  searchBar->setFixedHeight(44);
  searchBar->setStyleSheet(BlopTheme::themed(
      "QLineEdit {"
      "  background-color: #1A1829;"
      "  color: #F4F5FB;"
      "  border: 1px solid #201E2E;"
      "  border-radius: 22px;"
      "  padding: 0 20px;"
      "  font-size: 14px;"
      "}"
      "QLineEdit:focus {"
      "  border: 1px solid #5E5CE6;"
      "}"
  ));
  searchActionLayout->addWidget(searchBar, 1); // stretcht aus

  QPushButton *btnNewNote = new QPushButton("Neue Notiz", m_overviewContainer);
  btnNewNote->setObjectName("overviewBtnNewNote");
  btnNewNote->setFixedHeight(44);
  btnNewNote->setCursor(Qt::PointingHandCursor);
  btnNewNote->setStyleSheet(
      "QPushButton {"
      "  background-color: #5E5CE6;"
      "  color: white;"
      "  border-radius: 22px;"
      "  padding: 0 24px;"
      "  font-weight: bold;"
      "  font-size: 14px;"
      "  border: none;"
      "}"
      "QPushButton:hover { background-color: #7D7AFF; }"
  );
  connect(btnNewNote, &QPushButton::clicked, this, &MainWindow::onNewPage);
  BlopRipple::attachPressFeedback(btnNewNote, 0.94);
  searchActionLayout->addWidget(btnNewNote);

  QPushButton *btnNewFolder = new QPushButton("Neuer Ordner", m_overviewContainer);
  btnNewFolder->setObjectName("overviewBtnNewFolder");
  btnNewFolder->setFixedHeight(44);
  btnNewFolder->setCursor(Qt::PointingHandCursor);
  btnNewFolder->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  color: white;"
      "  border-radius: 22px;"
      "  padding: 0 24px;"
      "  font-weight: bold;"
      "  font-size: 14px;"
      "  border: 1px solid #333;"
      "}"
      "QPushButton:hover { background-color: rgba(255,255,255,0.05); border-color: #555; }"
  );
  connect(btnNewFolder, &QPushButton::clicked, this, &MainWindow::onCreateFolder);
  BlopRipple::attachPressFeedback(btnNewFolder, 0.94);
  searchActionLayout->addWidget(btnNewFolder);

  headerLayout->addLayout(searchActionLayout);
#endif

  overviewLayout->addLayout(headerLayout);
  // --------------------------------------------------------

  m_fileListView = new FreeGridView(this);
  m_fileListView->setModel(m_fileModel);
  m_fileListView->setRootIndex(m_fileModel->index(m_rootPath));
  m_fileListView->setSpacing(20);
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
  // Dateien: ein Tap / ein Klick öffnet (Maus & Touch). Ordner: weiter Doppelklick,
  // damit versehentliches „Reingehen“ seltener ist. Kurze Entprellung verhindert
  // doppeltes Öffnen bei schnellem Doppelklick auf dieselbe Notiz.
  connect(m_fileListView, &QListView::clicked, this,
          [this](const QModelIndex &index) {
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
            if (!m_fileModel || !index.isValid())
              return;
            if (m_fileModel->isDir(index))
              return;
            static QElapsedTimer debounce;
            static QModelIndex lastIdx;
            if (lastIdx == index && debounce.isValid() &&
                debounce.elapsed() < 450)
              return;
            lastIdx = index;
            debounce.restart();
            onFileDoubleClicked(index);
          });
  connect(m_fileListView, &QListView::doubleClicked, this,
          [this](const QModelIndex &index) {
            if (m_fileModel && index.isValid() && m_fileModel->isDir(index))
              onFileDoubleClicked(index);
          });
  connect(m_fileListView, &FreeGridView::itemDropped, this,
          &MainWindow::onItemDropped);
  m_fileListView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_fileListView, &QWidget::customContextMenuRequested,
          [this](const QPoint &pos) {
            QModelIndex index = m_fileListView->indexAt(pos);
            if (index.isValid())
              showContextMenu(m_fileListView->mapToGlobal(pos), index);
          });
  overviewLayout->addWidget(m_fileListView);

  m_lblEmptyState = new QLabel(
      "Noch keine Notizen oder Ordner hier.\nKlicke auf das '+' um loszulegen.",
      m_overviewContainer);
  m_lblEmptyState->setAlignment(Qt::AlignCenter);
  m_lblEmptyState->setStyleSheet(
      "color: #777; font-size: 16px; font-weight: bold;");
  m_lblEmptyState->hide();
  overviewLayout->addWidget(m_lblEmptyState);



  m_editorContainer = new QWidget(this);
  // KEIN installEventFilter - damit Klicks auf Buttons korrekt weitergeleitet werden!
  
  // Das primäre Editor-Layout
  QHBoxLayout *editorMainLayout = new QHBoxLayout(m_editorContainer);
  editorMainLayout->setContentsMargins(0, 0, 0, 0);
  editorMainLayout->setSpacing(0);

  m_editorCenterWidget = new QWidget(m_editorContainer);
  QVBoxLayout *centerLayout = new QVBoxLayout(m_editorCenterWidget);
  centerLayout->setContentsMargins(0, 0, 0, 0);
  centerLayout->setSpacing(0);

  // ── Floating Toolbar ─────────────────────────────────────────────────────
  ModernToolbar *topToolbar = nullptr;
  AndroidPhoneToolbar *phoneToolbar = nullptr;
#ifdef Q_OS_ANDROID
  const bool usePhoneToolbar = !UiScale::isAndroidTablet(this);
#else
  const bool usePhoneToolbar = false;
#endif
  if (usePhoneToolbar) {
    phoneToolbar = new AndroidPhoneToolbar(m_editorCenterWidget);
    phoneToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    phoneToolbar->resize(UiScale::dp(360), UiScale::dp(44));
    m_floatingTools = phoneToolbar;
  } else {
    topToolbar = new ModernToolbar(m_editorCenterWidget);
    topToolbar->setOrientation(ModernToolbar::Horizontal);
#ifdef Q_OS_ANDROID
    // Android Tablet: keep toolbar behavior deterministic (fixed/docked).
    topToolbar->setDraggable(false);
#else
    topToolbar->setDraggable(true);
#endif
    topToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    int idealW = topToolbar->calculateMinLength();
    topToolbar->resize(idealW, 52);
    m_floatingTools = topToolbar;
    topToolbar->setDockMode(true);
  }
  m_floatingTools->raise();

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
    m_editorTabs->removeTab(index);
    if (m_editorTabs->count() == 0)
      onBackToOverview();
  });
  connect(m_editorTabs, &QTabWidget::currentChanged, this,
          &MainWindow::onTabChanged);
  centerLayout->addWidget(m_editorTabs);

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
        if (auto *view = activeEditor->findChild<MultiPageNoteView *>())
          view->toggleRuler(rulerActive);
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
    connect(topToolbar, &ModernToolbar::toolChanged, this, onToolModeChanged);
    connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
            [this, topToolbar, onToolModeChanged](AbstractTool *tool) {
              if (tool && topToolbar->toolMode() != tool->mode()) {
                // Ensure the topToolbar's own state updates, so icon highlights
                // and double-click radial menus actually trigger.
                topToolbar->setToolMode(tool->mode());
                onToolModeChanged(tool->mode());
              }
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
  editorMainLayout->addWidget(m_editorCenterWidget);
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
          << "studyQQuickView=" << (m_studyQQuickView != nullptr);
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
      if (m_googleLoginInFlightSinceMs > 0 &&
          (nowMs - m_googleLoginInFlightSinceMs) > 15000) {
        qWarning() << "Google login in-flight guard was stale, unlocking retry";
        m_googleLoginInFlight = false;
        m_googleLoginInFlightSinceMs = 0;
        m_authNavigationLocked = false;
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
        m_authNavigationLocked = false;
        return;
      }
#endif
      m_googleLoginInFlight = true;
      m_googleLoginInFlightSinceMs = QDateTime::currentMSecsSinceEpoch();
      m_authNavigationLocked = true;
      GoogleAuthManager::instance().login();

      // Failsafe: if no browser handoff/response arrives, release lock to avoid dead button.
      QTimer::singleShot(12000, this, [this]() {
        if (!m_googleLoginInFlight)
          return;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_googleLoginInFlightSinceMs > 0 &&
            (nowMs - m_googleLoginInFlightSinceMs) >= 11500) {
          qWarning() << "Google login timeout before browser handoff; unlocking.";
          m_googleLoginInFlight = false;
          m_googleLoginInFlightSinceMs = 0;
          m_authNavigationLocked = false;
          emit oauthFailed(QStringLiteral("google_login_start_timeout"));
        }
      });
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
    if (m_androidHeader)
      m_androidHeader->setVisible(false);
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
  m_sidebarContainer->setMaximumWidth(SIDEBAR_WIDTH);
#ifdef Q_OS_ANDROID
  // On Android, give the sidebar a solid background and enable styled background
  // so touch events are fully absorbed and don't pass through to the main content.
  m_sidebarContainer->setAttribute(Qt::WA_StyledBackground, true);
  m_sidebarContainer->setStyleSheet(
      BlopTheme::themed("background-color: #0F111A;"));
#endif

  QVBoxLayout *layout = new QVBoxLayout(m_sidebarContainer);
#ifdef Q_OS_ANDROID
  layout->setContentsMargins(UiScale::dp(6), 0, UiScale::dp(6), 0);
#else
  layout->setContentsMargins(4, 0, 4, 0);
#endif

  layout->setSpacing(0);

  // --- HEADER: Blop Study style ---
  QWidget *header = new QWidget(m_sidebarContainer);
#ifdef Q_OS_ANDROID
  header->setFixedHeight(UiScale::dp(52));
#else
  header->setFixedHeight(62);
#endif
#ifdef Q_OS_ANDROID
  header->setStyleSheet("border-bottom: none; background: transparent;");
#else
  header->setStyleSheet(BlopTheme::themed("border-bottom: 1px solid #333;"));
#endif
  QHBoxLayout *headerLay = new QHBoxLayout(header);
#ifdef Q_OS_ANDROID
  headerLay->setContentsMargins(UiScale::dp(12), UiScale::dp(10), UiScale::dp(12),
                                UiScale::dp(10));
  headerLay->setSpacing(UiScale::dp(8));
#else
  headerLay->setContentsMargins(12, 10, 12, 10);
  headerLay->setSpacing(8);
#endif

  // Logo box (oval, accent color)
  QLabel *lblLogo = new QLabel(header);
#ifdef Q_OS_ANDROID
  lblLogo->setFixedSize(UiScale::dp(32), UiScale::dp(32));
#else
  lblLogo->setFixedSize(38, 38);
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
        "background-color: #5E5CE6; border-radius: 8px; color: white; "
        "font-weight: bold; font-size: 12px;");
    lblLogo->setText("B");
  }
  headerLay->addWidget(lblLogo);

  QVBoxLayout *titleCol = new QVBoxLayout();
  titleCol->setSpacing(2);
  QLabel *lblTitle = new QLabel("Blop", header);
#ifdef Q_OS_ANDROID
  lblTitle->setStyleSheet(BlopTheme::themed(
      "font-size: 15px; font-weight: bold; color: #F4F2FF; "
      "background: transparent; border: none;"));
#else
  lblTitle->setStyleSheet(BlopTheme::themed(
      "font-size: 16px; font-weight: bold; color: #F4F2FF; "
      "background: transparent; border: none;"));
#endif
  QLabel *lblSub = new QLabel("Notiz-App", header);
#ifdef Q_OS_ANDROID
  lblSub->setStyleSheet(BlopTheme::themed(
      "font-size: 9px; color: #888; background: transparent; border: none;"));
#else
  lblSub->setStyleSheet(BlopTheme::themed(
      "font-size: 10px; color: #888; background: transparent; border: none;"));
#endif
  titleCol->addWidget(lblTitle);
  titleCol->addWidget(lblSub);

  headerLay->addLayout(titleCol);
  headerLay->addStretch();

  m_closeSidebarBtn = new QPushButton("«", header);
  m_closeSidebarBtn->setFixedSize(24, 24);
  m_closeSidebarBtn->setFocusPolicy(Qt::NoFocus);
  m_closeSidebarBtn->setCursor(Qt::PointingHandCursor);
  m_closeSidebarBtn->setStyleSheet(BlopTheme::themed(
      "QPushButton { background: transparent; color: #888; border: none; "
      "font-size: 16px; outline: none; } QPushButton:hover { color: #F4F2FF; "
      "background: rgba(255,255,255,0.10); outline: none; "
      "border-radius: 4px; }"));
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
      if (name == "All" || name == "Blop Notes") {
        item->setData(Qt::UserRole + 2, rootCountStr);
        item->setData(Qt::UserRole + 10, m_rootPath);
      }
    }
    item->setData(Qt::UserRole + 1, isHeader);
    if (isHeader) {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      if (name == "Clouds")
        item->setData(Qt::UserRole + 4, "clouds_header");
    } else {
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      if (name == "Blop Notes" || name == "All") {
        item->setData(Qt::UserRole + 6, true);
      }
      if (name == "Google Drive" || name == "OneDrive" || name == "Dropbox") {
        item->setData(Qt::UserRole + 5, "clouds_item");
      }
    }
  };

  addItem("All", "folder");
  addItem("Blop Notes", "folder");
  addItem("Device Files", "device");
  addItem("Shared by me", "share");
  addItem("Shared with me", "share");
  addItem("Clouds", "", true);
  addItem("Google Drive", "drive");
  addItem("OneDrive", "onedrive");
  addItem("Dropbox", "dropbox");
  m_navSidebar->setCurrentRow(1);
  connect(m_navSidebar, &QListWidget::itemClicked, this,
          &MainWindow::onNavItemClicked);
  layout->addWidget(m_navSidebar);
  updateSidebarBadges();

  // --- BOTTOM: User Account Section (Blop Study style) ---
  QWidget *bottomBar = new QWidget(m_sidebarContainer);
  bottomBar->setObjectName("BottomBar");
#ifdef Q_OS_ANDROID
  bottomBar->setStyleSheet("QWidget#BottomBar { border-top: none; background: transparent; }");
#else
  bottomBar->setStyleSheet(BlopTheme::themed(
      "QWidget#BottomBar { border-top: 1px solid #201E2E; }"));
#endif

#ifdef Q_OS_ANDROID
  bottomBar->setFixedHeight(62);
#else
  bottomBar->setFixedHeight(66);
#endif

  QHBoxLayout *bottomLay = new QHBoxLayout(bottomBar);
#ifdef Q_OS_ANDROID
  bottomLay->setContentsMargins(10, 7, 10, 9);
  bottomLay->setSpacing(8);
#else
  bottomLay->setContentsMargins(12, 8, 12, 10);
  bottomLay->setSpacing(8);
#endif

  // Read username from QSettings (saved by Blop Study web app via localStorage)
  QString username =
      QSettings("Blop", "BlopApp").value("username", "Gast").toString();
  QString initial = username.isEmpty() ? "G" : username.left(1).toUpper();

  // Avatar circle
  m_lblSidebarAvatar = new QLabel(initial, bottomBar);
#ifdef Q_OS_ANDROID
  m_lblSidebarAvatar->setFixedSize(32, 32);
  m_lblSidebarAvatar->setStyleSheet(
      "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #5E5CE6,stop:1 "
      "#7D7AFF); "
      "border-radius: 16px; color: white; font-weight: bold; font-size: 12px;");
#else
  m_lblSidebarAvatar->setFixedSize(36, 36);
  m_lblSidebarAvatar->setStyleSheet(
      "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #5E5CE6,stop:1 "
      "#7D7AFF); "
      "border-radius: 18px; color: white; font-weight: bold; font-size: 14px;");
#endif
  m_lblSidebarAvatar->setAlignment(Qt::AlignCenter);
  bottomLay->addWidget(m_lblSidebarAvatar);

  // Username + settings link
  QVBoxLayout *userCol = new QVBoxLayout();
  userCol->setSpacing(1);
  m_lblSidebarUser =
      new QLabel(username.isEmpty() ? "Gast" : username, bottomBar);
#ifdef Q_OS_ANDROID
  m_lblSidebarUser->setStyleSheet(BlopTheme::themed(
      "font-size: 11px; font-weight: 600; color: #F4F2FF; "
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
      "QPushButton { background: transparent; color: #888; border: none; "
      "font-size: 10px; padding: 0; text-align: left; } "
      "QPushButton:hover { color: #5E5CE6; }"));
  connect(m_btnSidebarSettings, &QPushButton::clicked, this,
          &MainWindow::onOpenSettings);
  userCol->addWidget(m_btnSidebarSettings);

  QString verStr = QString::fromUtf8(BLOP_VERSION);
  if (verStr.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
    verStr = verStr.mid(1);
  QLabel *lblVersion = new QLabel(QStringLiteral("v") + verStr, bottomBar);
  lblVersion->setStyleSheet(BlopTheme::themed(
      "font-size: 10px; color: #555; background: transparent; border: none;"));
  userCol->addWidget(lblVersion);

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

void MainWindow::updateSidebarBadges() {
  QDir rootDir(m_rootPath);
  int rootCount =
      rootDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)
          .count();
  QString rootCountStr = QString::number(rootCount);
  for (int i = 0; i < m_navSidebar->count(); ++i) {
    QListWidgetItem *item = m_navSidebar->item(i);
    if (item->text() == "All" || item->text() == "Blop Notes") {
      item->setData(Qt::UserRole + 2, rootCountStr);
    }
  }

  if (m_lblEmptyState && m_fileListView && m_fileModel) {
    if (m_fileModel->rowCount(m_fileListView->rootIndex()) == 0) {
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
  if (isHeader) {
    if (item->text() == "Clouds")
      toggleSection(item);
    return;
  }
  QString path = item->data(Qt::UserRole + 10).toString();
  bool isExpandable = item->data(Qt::UserRole + 6).toBool();

  if (!path.isEmpty() && QFileInfo(path).isDir()) {
    m_fileModel->setRootPath(path);
    m_fileListView->setModel(m_fileModel);
    m_fileListView->setRootIndex(m_fileModel->index(path));
    updateOverviewBackButton();
    if (isExpandable)
      toggleFolderContent(item);

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
  if (name == "Device Files") {
    m_fileModel->setRootPath(QDir::rootPath());
    m_fileListView->setModel(m_fileModel);
    m_fileListView->setRootIndex(QModelIndex());
#ifdef Q_OS_ANDROID
    onToggleSidebar();
#endif
  } else if (name == "Google Drive" || name == "OneDrive" ||
             name == "Dropbox") {
    m_fileListView->setRootIndex(QModelIndex());
    QMessageBox::information(this, "Cloud Integration",
                             "Cloud integration coming soon.");
  }
}

void MainWindow::toggleSection(QListWidgetItem *headerItem) {
  bool isCollapsed = headerItem->data(Qt::UserRole + 3).toBool();
  bool newState = !isCollapsed;
  headerItem->setData(Qt::UserRole + 3, newState);
  for (int i = 0; i < m_navSidebar->count(); ++i) {
    QListWidgetItem *item = m_navSidebar->item(i);
    if (item->data(Qt::UserRole + 5).toString() == "clouds_item")
      item->setHidden(newState);
  }
}

void MainWindow::toggleFolderContent(QListWidgetItem *parentItem) {
  bool isExpanded = parentItem->data(Qt::UserRole + 3).toBool();
  QString parentPath = parentItem->data(Qt::UserRole + 10).toString();
  if (parentPath.isEmpty()) {
    if (parentItem->text() == "Blop Notes" || parentItem->text() == "All")
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
      if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out << (quint32)0xB10B0002;
        out << isInfinite;
        out << (int)0;
        file.close();
      }
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
    NoteManager::saveNote(note, path);
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
  QString text = QInputDialog::getText(this, QStringLiteral("Neuer Ordner"),
                                       QStringLiteral("Name:"),
                                       QLineEdit::Normal,
                                       QStringLiteral("Neuer Ordner"), &ok);
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
  m_pageSettingsOverlay = new QWidget(m_editorCenterWidget);
  m_pageSettingsOverlay->setObjectName(QStringLiteral("PageSettingsOverlay"));
  m_pageSettingsOverlay->setAttribute(Qt::WA_StyledBackground, true);
  m_pageSettingsOverlay->setStyleSheet(
      QStringLiteral("QWidget#PageSettingsOverlay { background-color: rgba(8, 6, 18, "
                     "210); }"));
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

  m_btnFormatInfinite = new QPushButton("Infinite");
  m_btnFormatInfinite->setCheckable(true);
  m_btnFormatInfinite->setEnabled(false);
  m_btnFormatInfinite->setStyleSheet(
      "QPushButton { background: #333; color: #888; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; } QPushButton:checked { background: "
      "#5E5CE6; color: white; border: 1px solid #5E5CE6; }");
  m_btnFormatA4 = new QPushButton("A4");
  m_btnFormatA4->setCheckable(true);
  m_btnFormatA4->setEnabled(false);
  m_btnFormatA4->setStyleSheet(
      "QPushButton { background: #333; color: #888; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; } QPushButton:checked { background: "
      "#5E5CE6; color: white; border: 1px solid #5E5CE6; }");
  QButtonGroup *grp = new QButtonGroup(this);
  grp->addButton(m_btnFormatInfinite);
  grp->addButton(m_btnFormatA4);
  grp->setExclusive(true);
  optLayout->addWidget(m_btnFormatInfinite);
  optLayout->addWidget(m_btnFormatA4);

  optLayout->addWidget(new QLabel("Layout:", optContent));
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
  QHBoxLayout *styleBtnsLayout = new QHBoxLayout(styleBtnsContainer);
  styleBtnsLayout->setContentsMargins(0, 0, 0, 0);
  styleBtnsLayout->addWidget(m_btnStyleBlank);
  styleBtnsLayout->addWidget(m_btnStyleLined);
  styleBtnsLayout->addWidget(m_btnStyleSquared);
  styleBtnsLayout->addWidget(m_btnStyleDotted);
  optLayout->addWidget(styleBtnsContainer);

  optLayout->addWidget(new QLabel("Grid Spacing (px):", optContent));
  m_sliderGridSpacing = new QSlider(Qt::Horizontal);
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

  optLayout->addWidget(new QLabel("Page Color:", optContent));
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
              if (index == 0)
                tb->setStyle(ModernToolbar::Normal);
              else if (index == 1) {
                tb->setStyle(ModernToolbar::Radial);
                tb->setRadialType(ModernToolbar::FullCircle);
              } else {
                tb->setStyle(ModernToolbar::Radial);
                tb->setRadialType(ModernToolbar::HalfEdge);
              }
            }
          });
  if (phoneToolbarActive) {
    lblToolbarStyle->setEnabled(false);
    m_comboToolbarStyle->setEnabled(false);
    m_comboToolbarStyle->setToolTip(
        "Toolbar-Style ist auf Android Phones fest (Bottom-Pille).");
  }
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

  // Tags Section
  tagsLayoutMain->addWidget(sectionLabel(QStringLiteral("QUICK-TAGS"), tabTags));
  
  m_tagsContainer = new QWidget(tabTags);
  m_tagsContainer->setStyleSheet("background: transparent;");
  m_tagsFlowLayout = new QHBoxLayout(m_tagsContainer);
  m_tagsFlowLayout->setContentsMargins(0, 0, 0, 0);
  m_tagsFlowLayout->setSpacing(6);
  m_tagsFlowLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

  auto addTag = [&](const QString &text) {
    QLabel *tag = new QLabel(text, m_tagsContainer);
    tag->setStyleSheet(
        "QLabel {"
        "  background: rgba(124,92,252,0.15);"
        "  border: 1px solid rgba(124,92,252,0.30);"
        "  border-radius: 6px;"
        "  color: #D8D5FF;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "  padding: 4px 10px;"
        "}");
    tag->setCursor(Qt::PointingHandCursor);
    m_tagsFlowLayout->addWidget(tag);
  };
  addTag("[[Projekt]]");
  addTag("[[Entwurf]]");
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
  tagsLayoutMain->addWidget(btnAddTag);

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
  m_lblMetaModified = makeMetaRow("Geändert:", "Vor 5 Min.");

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

  QVariantAnimation *anim = new QVariantAnimation(this);
  anim->setDuration(BlopMotion::kEmphasis);
  anim->setEasingCurve(BlopMotion::kEaseStandard);
  anim->setStartValue(show ? 0 : fullW);
  anim->setEndValue(show ? fullW : 0);
  connect(anim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant &v) {
            const int w = v.toInt();
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
            if (m_sidebarContainer)
              m_sidebarContainer->setGeometry(r2.x(), r2.y(), w, h2);
#ifdef Q_OS_ANDROID
            if (m_androidSidebarScrim)
              m_androidSidebarScrim->raise();
            if (m_sidebarContainer)
              m_sidebarContainer->raise();
            // v3.18.7: m_androidHeader->raise() removed here — chrome
            // is a child of m_centralContainer, drawer is a sibling of
            // m_centralContainer, so raise() cannot lift the chrome
            // above the drawer. Now obsolete because the drawer's
            // geometry no longer overlaps the chrome (see
            // sidebarPushContentRect()).
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
                  // Same centering rule during sidebar animation.
                  int dockX = qMax(0, (availableW - dockW) / 2);
                  if (dockX + dockW > m_editorCenterWidget->width())
                    dockX = m_editorCenterWidget->width() - dockW;
                  tb->setGeometry(dockX, 0, dockW, 48);
                  tb->raise();
                }
              }
            }
          });
  connect(anim, &QVariantAnimation::finished, this, [this, show]() {
    if (show) {
      syncSidebarPushLayout();
#ifdef Q_OS_ANDROID
      if (m_androidSidebarScrim) {
        updateAndroidSidebarScrimGeometry();
        m_androidSidebarScrim->raise();
      }
      if (m_sidebarContainer)
        m_sidebarContainer->raise();
      // v3.18.7: see note above — chrome cannot be raised over the
      // drawer (different parents), and after sidebarPushContentRect
      // no longer maps the drawer onto the chrome rect, it doesn't
      // need to be.
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
  // v3.18.7: legacy m_androidHeader->raise() chains removed — they
  // were attempts to lift the chrome above the drawer, but Qt's
  // sibling z-order rules prevent that (chrome is inside
  // m_centralContainer, drawer is its sibling at the MainWindow
  // level). The drawer now starts BELOW the chrome row instead, so
  // there is no overlap to fight.
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
  if (m_floatingTools) {
    m_floatingTools->setVisible(isEditor);
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
  if (m_noteTabsChrome)
    m_noteTabsChrome->setVisible(inNotesMode);
  if (m_titleSearchBar)
    m_titleSearchBar->setVisible(inNotesMode);
  {
    bool showNoteOverflow = false;
    if (isEditor && m_editorTabs) {
      if (qobject_cast<NoteEditor *>(m_editorTabs->currentWidget()))
        showNoteOverflow = true;
    }
    if (m_btnEditorNoteOverflow)
      m_btnEditorNoteOverflow->setVisible(inNotesMode && showNoteOverflow);
    bool showPageManagerBtn = false;
    if (isEditor && m_editorTabs) {
      if (auto *editor =
              qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
        if (editor->view())
          showPageManagerBtn = true;
      }
    }
    if (m_btnTitleBarPageManager)
      m_btnTitleBarPageManager->setVisible(inNotesMode && showPageManagerBtn);
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
    if (m_btnAndroidToolbarPageManager) {
      // Always keep page settings entry point reachable in Notes mode.
      m_btnAndroidToolbarPageManager->setVisible(true);
    }
    if (m_btnAndroidToolbarExport)
      m_btnAndroidToolbarExport->setVisible(true);
    if (m_androidTopSearchBar) {
      // Only show the inline search bar on real tablets (>= 600 dp wide via
      // a DPI-robust raw-pixel check). Phones — even ones whose Qt HighDPI
      // mode reports >dp(420) of usable width — would still crowd the
      // right-side action buttons off the screen. Phone users navigate via
      // the Notizen/Study pills directly.
      if (UiScale::isAndroidTablet(this))
        m_androidTopSearchBar->show();
      else
        m_androidTopSearchBar->hide();
    }

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
      animateScale(m_btnAndroidToolbarPageManager);
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
    if (m_pageSettingsOverlay && m_pageSettingsOverlay->isVisible())
      setPageSettingsOverlayVisible(false);
  }
#else
  if (isEditor) {
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->show();
  } else {
    // Übersicht: nur ein Hamburger — in overviewTopRow (btnOverviewMenu). Der schmale
    // Strip links davon war redundant (zweites identisches Menü neben der Sidebar).
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->hide();
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
  auto *editor = qobject_cast<NoteEditor *>(m_editorTabs->currentWidget());
  if (!editor)
    return;
#ifdef Q_OS_ANDROID
  QWidget *anchor = m_btnAndroidToolbarExport;
#else
  QWidget *anchor = m_btnEditorNoteOverflow;
#endif
  editor->showOverflowMenuFromAnchor(anchor);
}

void MainWindow::onTogglePageManager() {
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

void MainWindow::onFileDoubleClicked(const QModelIndex &index) {
  BlopDiag::recordUiAction(QStringLiteral("open_note"));
  if (m_fileModel->isDir(index)) {
    m_fileListView->setRootIndex(index);
    m_fileModel->setRootPath(m_fileModel->filePath(index));
  } else {
    QString path = m_fileModel->filePath(index);
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
                  if (ok) QMessageBox::information(this, "Exportiert", "PDF gespeichert!");
                  else    QMessageBox::warning(this, "Fehler", "PDF fehlgeschlagen.");
              }
          } else if (chosen == actImg) {
              QString out = QFileDialog::getSaveFileName(this, "Als Bild exportieren",
                  fi.baseName() + ".png", "Bilder (*.png *.jpg)");
              if (!out.isEmpty()) {
                  bool ok = canvas->exportToImage(out);
                  if (ok) QMessageBox::information(this, "Exportiert", "Bild gespeichert!");
                  else    QMessageBox::warning(this, "Fehler", "Bild fehlgeschlagen.");
              }
          } else if (chosen == actImportPdf) {
              QString in = QFileDialog::getOpenFileName(this, "PDF importieren",
                  QString(), "PDF (*.pdf)");
              if (!in.isEmpty()) {
                  bool ok = canvas->importPdfIntoCanvas(in);
                  if (ok) QMessageBox::information(this, "Importiert", "PDF wurde in die unendliche Seite eingefuegt.");
                  else    QMessageBox::warning(this, "Fehler", "PDF konnte nicht importiert werden.");
              }
          } else if (chosen == actShareUser) {
              const QString username =
                  QSettings("Blop", "BlopApp").value("username").toString().trimmed();
              if (username.isEmpty()) {
                  QMessageBox::warning(this, "Nicht angemeldet", "Bitte zuerst in Blop Study anmelden.");
                  return;
              }
              const QString localPath = capPath;
              QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
              bool ok = false;
              if (fileId.isEmpty()) {
                  fileId = QInputDialog::getText(
                      this, "Cloud-Datei-ID", "Datei-ID im Blop-Study-Cloudspeicher:",
                      QLineEdit::Normal, fi.baseName(), &ok).trimmed();
                  if (!ok || fileId.isEmpty()) return;
              } else {
                  QMessageBox::information(this, "Cloud-Datei erkannt",
                                           QString("Automatisch erkannt: %1").arg(fileId));
              }
              const QString target = QInputDialog::getText(
                  this, "Zielnutzer", "Username des Empfängers:",
                  QLineEdit::Normal, "", &ok).trimmed();
              if (!ok || target.isEmpty()) return;
              const QString message = QInputDialog::getText(
                  this, "Nachricht (optional)", "Begleitnachricht:",
                  QLineEdit::Normal, "", &ok);
              if (!ok) return;

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
                  QMessageBox::warning(this, "Teilen fehlgeschlagen",
                                       QString("Serverantwort (%1):\n%2").arg(status).arg(QString::fromUtf8(raw)));
              } else {
                  QMessageBox::information(this, "Request gesendet",
                                           "Die Freigabeanfrage wurde an den Zielnutzer gesendet.");
              }
              reply->deleteLater();
          } else if (chosen == actCreateLink) {
              const QString username =
                  QSettings("Blop", "BlopApp").value("username").toString().trimmed();
              if (username.isEmpty()) {
                  QMessageBox::warning(this, "Nicht angemeldet", "Bitte zuerst in Blop Study anmelden.");
                  return;
              }
              const QString localPath = capPath;
              QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
              bool ok = false;
              if (fileId.isEmpty()) {
                  fileId = QInputDialog::getText(
                      this, "Cloud-Datei-ID", "Datei-ID im Blop-Study-Cloudspeicher:",
                      QLineEdit::Normal, fi.baseName(), &ok).trimmed();
                  if (!ok || fileId.isEmpty()) return;
              } else {
                  QMessageBox::information(this, "Cloud-Datei erkannt",
                                           QString("Automatisch erkannt: %1").arg(fileId));
              }
              const int expiresDays = QInputDialog::getInt(this, "Gueltigkeit", "Link gueltig fuer (Tage):", 7, 1, 30, 1, &ok);
              if (!ok) return;
              const int maxUses = QInputDialog::getInt(this, "Nutzungslimit", "Maximale Nutzungen:", 1, 1, 100, 1, &ok);
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
                  QMessageBox::warning(this, "Link fehlgeschlagen",
                                       QString("Serverantwort (%1):\n%2").arg(status).arg(QString::fromUtf8(raw)));
                  reply->deleteLater();
                  return;
              }
              const QJsonDocument doc = QJsonDocument::fromJson(raw);
              const QString link = doc.object().value("url").toString();
              if (!link.isEmpty())
                  QGuiApplication::clipboard()->setText(link);
              QMessageBox::information(this, "Link erstellt",
                                       link.isEmpty() ? "Share-Link wurde erstellt."
                                                      : QString("Share-Link wurde erstellt und kopiert:\n%1").arg(link));
              reply->deleteLater();
          } else if (chosen == actImportLink) {
              const QString username =
                  QSettings("Blop", "BlopApp").value("username").toString().trimmed();
              if (username.isEmpty()) {
                  QMessageBox::warning(this, "Nicht angemeldet", "Bitte zuerst in Blop Study anmelden.");
                  return;
              }
              bool ok = false;
              QString linkOrToken = QInputDialog::getText(
                  this, "Share-Link", "Share-Link oder Token einfügen:",
                  QLineEdit::Normal, "", &ok).trimmed();
              if (!ok || linkOrToken.isEmpty()) return;
              if (linkOrToken.contains("/")) {
                  const QString marker = "/share/";
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
                  QMessageBox::warning(this, "Import fehlgeschlagen",
                                       QString("Serverantwort (%1):\n%2").arg(status).arg(QString::fromUtf8(raw)));
              } else {
                  QMessageBox::information(this, "Import erfolgreich",
                                           "Die geteilte Datei wurde in dein Konto importiert.");
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
          NoteEditor *editor = new NoteEditor(this);
          Note *heapNote = new Note(note);
          editor->setNote(heapNote);
          if (editor->view())
            editor->view()->setPenOnlyMode(m_penOnlyMode);
          editor->onSaveRequested = [path](Note *n) {
            NoteManager::saveNote(*n, path);
          };
          editor->onOpenNoteOptionsRequested = [this]() {
            if (!m_pageSettingsOverlay)
              return;
            if (!m_pageSettingsOverlay->isVisible())
              setPageSettingsOverlayVisible(true);
          };
          m_editorTabs->addTab(editor, fileName);
          m_editorTabs->setCurrentWidget(editor);
          addNoteTab(QFileInfo(fileName).baseName());
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
    setActiveTool(m_activeToolType);
    updateSidebarState();
  }
}

void MainWindow::onBackToOverview() {
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

  // Shared with Windows (exec) and Android (popup). Lambdas capture a
  // QPersistentModelIndex so action bodies stay safe if the QFileSystemModel
  // refreshes its internal nodes between menu-open and the user picking an
  // item (the file watcher can fire at any time on Android).
  const QPersistentModelIndex persistent(index);
  const auto populateMenu = [this, persistent](QMenu *menu) {
  menu->addAction(QStringLiteral("Öffnen"), [this, persistent]() {
    if (!persistent.isValid())
      return;
    onFileDoubleClicked(QModelIndex(persistent));
  });
  menu->addAction(QStringLiteral("Umbenennen"), [this, persistent]() {
    if (!persistent.isValid())
      return;
    startRename(QModelIndex(persistent));
  });
  menu->addSeparator();
#ifndef Q_OS_ANDROID
  // Cloud-share actions use QInputDialog / QMessageBox / nested QEventLoop -
  // all top-level on Android's single-window surface (same crash family as
  // QMenu::exec). Hide these items on Android until we have in-window
  // equivalents; Android users have the same workflows via Blop Study.
  menu->addAction("Mit Username teilen...", [this, persistent]() {
    if (!persistent.isValid())
      return;
    const QString username =
        QSettings("Blop", "BlopApp").value("username").toString().trimmed();
    if (username.isEmpty()) {
      QMessageBox::warning(this, "Nicht angemeldet",
                           "Bitte zuerst in Blop Study anmelden.");
      return;
    }
    const QString localPath = m_fileModel->filePath(QModelIndex(persistent));
    QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
    bool ok = false;
    if (fileId.isEmpty()) {
      fileId = QInputDialog::getText(
                   this, "Cloud-Datei-ID", "Datei-ID im Blop-Study-Cloudspeicher:",
                   QLineEdit::Normal, QFileInfo(localPath).baseName(), &ok)
                   .trimmed();
      if (!ok || fileId.isEmpty())
        return;
    } else {
      QMessageBox::information(this, "Cloud-Datei erkannt",
                               QString("Automatisch erkannt: %1").arg(fileId));
    }
    if (fileId.isEmpty())
      return;
    const QString target = QInputDialog::getText(
        this, "Zielnutzer", "Username des Empfängers:", QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || target.isEmpty())
      return;
    const QString message = QInputDialog::getText(
        this, "Nachricht (optional)", "Begleitnachricht:", QLineEdit::Normal, "", &ok);
    if (!ok)
      return;

    QUrl url(kBlopStudyUrl + "/api/shares/username");
    QNetworkRequest req(url);
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
      QMessageBox::warning(this, "Teilen fehlgeschlagen",
                           QString("Serverantwort (%1):\n%2").arg(status).arg(QString::fromUtf8(raw)));
    } else {
      QMessageBox::information(this, "Request gesendet",
                               "Die Freigabeanfrage wurde an den Zielnutzer gesendet.");
    }
    reply->deleteLater();
  });
  menu->addAction("Share-Link erstellen...", [this, persistent]() {
    if (!persistent.isValid())
      return;
    const QString username =
        QSettings("Blop", "BlopApp").value("username").toString().trimmed();
    if (username.isEmpty()) {
      QMessageBox::warning(this, "Nicht angemeldet",
                           "Bitte zuerst in Blop Study anmelden.");
      return;
    }
    const QString localPath = m_fileModel->filePath(QModelIndex(persistent));
    QString fileId = resolveCloudFileId(this, m_netManager, username, localPath);
    bool ok = false;
    if (fileId.isEmpty()) {
      fileId = QInputDialog::getText(
                   this, "Cloud-Datei-ID", "Datei-ID im Blop-Study-Cloudspeicher:",
                   QLineEdit::Normal, QFileInfo(localPath).baseName(), &ok)
                   .trimmed();
      if (!ok || fileId.isEmpty())
        return;
    } else {
      QMessageBox::information(this, "Cloud-Datei erkannt",
                               QString("Automatisch erkannt: %1").arg(fileId));
    }
    if (fileId.isEmpty())
      return;
    const int expiresDays = QInputDialog::getInt(this, "Gültigkeit",
                                                 "Link gültig für (Tage):", 7, 1, 30, 1, &ok);
    if (!ok)
      return;
    const int maxUses = QInputDialog::getInt(this, "Nutzungslimit",
                                             "Maximale Nutzungen:", 1, 1, 100, 1, &ok);
    if (!ok)
      return;

    QUrl url(kBlopStudyUrl + "/api/shares/link");
    QNetworkRequest req(url);
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
      QMessageBox::warning(this, "Link fehlgeschlagen",
                           QString("Serverantwort (%1):\n%2").arg(status).arg(QString::fromUtf8(raw)));
      reply->deleteLater();
      return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    const QString link = doc.object().value("url").toString();
    if (!link.isEmpty())
      QGuiApplication::clipboard()->setText(link);
    QMessageBox::information(this, "Link erstellt",
                             link.isEmpty() ? "Share-Link wurde erstellt."
                                            : QString("Share-Link wurde erstellt und kopiert:\n%1").arg(link));
    reply->deleteLater();
  });
  menu->addAction("Datei aus Link importieren...", [this]() {
    const QString username =
        QSettings("Blop", "BlopApp").value("username").toString().trimmed();
    if (username.isEmpty()) {
      QMessageBox::warning(this, "Nicht angemeldet",
                           "Bitte zuerst in Blop Study anmelden.");
      return;
    }
    bool ok = false;
    QString linkOrToken = QInputDialog::getText(
        this, "Share-Link", "Share-Link oder Token einfügen:", QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || linkOrToken.isEmpty())
      return;
    if (linkOrToken.contains("/")) {
      const QString marker = "/share/";
      const int pos = linkOrToken.lastIndexOf(marker);
      if (pos >= 0)
        linkOrToken = linkOrToken.mid(pos + marker.size());
      else
        linkOrToken = linkOrToken.section('/', -1).trimmed();
    }
    const QString targetFolderId = chooseCloudFolderId(this, m_netManager, username);
    if (targetFolderId.isEmpty())
      return;

    const QString encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(linkOrToken));
    QUrl url(kBlopStudyUrl + "/api/shares/link/" + encodedToken + "/import");
    QNetworkRequest req(url);
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
      QMessageBox::warning(this, "Import fehlgeschlagen",
                           QString("Serverantwort (%1):\n%2").arg(status).arg(QString::fromUtf8(raw)));
    } else {
      QMessageBox::information(this, "Import erfolgreich",
                               "Die geteilte Datei wurde in dein Konto importiert.");
    }
    reply->deleteLater();
  });
#endif // !Q_OS_ANDROID
  menu->addAction(QStringLiteral("Löschen"), [this, persistent]() {
    if (!persistent.isValid())
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
                },
                false, false});
  items.append({QStringLiteral("Umbenennen"), QIcon(),
                [this, persistent]() {
                  if (!persistent.isValid()) return;
                  startRename(QModelIndex(persistent));
                },
                false, false});
  items.append({QString(), QIcon(), {}, false, true});
  items.append({QStringLiteral("Löschen"), QIcon(),
                [this, persistent]() {
                  if (!persistent.isValid()) return;
                  m_fileModel->remove(QModelIndex(persistent));
                },
                false, false});
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
  if (!m_renameOverlay) {
    m_renameOverlay = new QWidget(this);
    m_renameOverlay->resize(size());
    m_renameOverlay->setStyleSheet("background-color: rgba(0,0,0,200);");
    m_renameInput = new QLineEdit(m_renameOverlay);
    m_renameInput->setFixedSize(300, 40);
    m_renameInput->setStyleSheet(
        "QLineEdit { background: #333; color: white; border: 1px solid #555; "
        "font-size: 16px; padding: 5px; }");
    connect(m_renameInput, &QLineEdit::returnPressed, this,
            &MainWindow::finishRename);
  }
  m_renameInput->move(width() / 2 - 150, height() / 2 - 20);
  m_renameInput->setText(currentName);
  m_renameOverlay->show();
  m_renameInput->setFocus();
}
void MainWindow::finishRename() {
  if (m_renameOverlay && m_indexToRename.isValid()) {
    QString newName = m_renameInput->text();
    if (!newName.isEmpty()) {
      m_fileModel->setData(m_indexToRename, newName, Qt::EditRole);
    }
    m_renameOverlay->hide();
  }
}

void MainWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
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
  if (m_renameOverlay) {
    m_renameOverlay->resize(size());
    if (m_renameInput)
      m_renameInput->move(width() / 2 - 150, height() / 2 - 20);
  }
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
      tb->setTopBound(0);
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
            if (toolbar)
              toolbar->setStyle(radial ? ModernToolbar::Radial
                                       : ModernToolbar::Normal);
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

  // v3.18.5: route through BlopModal to avoid the Qt 6.10 Android EGL
  // deadlock that any top-level QWindow (raw QDialog::exec) triggers.
  // execBlocking embeds the dialog into our in-window modal and supplies
  // its own backdrop, so the legacy rgba scrim is no longer needed.
  int res = BlopModal::execBlocking(this, &dlg);
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
  if (!sourceIndex.isValid() || !targetIndex.isValid())
    return;

  // We only support dropping INTO folders. If the target is not a dir, do
  // nothing, or we might want to drop it exactly in the target's parent
  // directory if it's a file.
  QString targetPath = m_fileModel->filePath(targetIndex);
  QFileInfo targetInfo(targetPath);

  if (!targetInfo.isDir()) {
    targetPath =
        targetInfo
            .absolutePath(); // Drop into the same folder as the target file
  }

  QString sourcePath = m_fileModel->filePath(sourceIndex);
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
}

void MainWindow::onTabChanged(int index) {
  QWidget *current = m_editorTabs->currentWidget();
  NoteEditor *editor = qobject_cast<NoteEditor *>(current);

  // WICHTIG: ToolManager auf neuen View aktualisieren
  CanvasView *cv = getCurrentCanvas();
  if (cv && m_toolManager) {
    cv->setToolManager(m_toolManager);
  }

  setActiveTool(m_activeToolType);

  // --- Aktive Notiz in die rechte Sidebar schreiben ---
  QString noteTitle;
  if (index >= 0)
    noteTitle = m_editorTabs->tabText(index);

  if (m_lblActiveNote)
    m_lblActiveNote->setText(noteTitle);

  // Metadaten aus der Datei lesen (Erstellt / Geändert)
  if (cv && m_fileModel) {
    // Try to find the note file path from the canvas
    QString filePath = cv->property("filePath").toString();
    if (!filePath.isEmpty()) {
      QFileInfo fi(filePath);
      if (fi.exists()) {
        if (m_lblMetaCreated)
          m_lblMetaCreated->setText(
              fi.birthTime().toString("dd.MM.yyyy"));
        if (m_lblMetaModified) {
          qint64 secsAgo = fi.lastModified().secsTo(QDateTime::currentDateTime());
          QString relTime;
          if (secsAgo < 60)       relTime = "Gerade eben";
          else if (secsAgo < 3600) relTime = QString("Vor %1 Min.").arg(secsAgo / 60);
          else if (secsAgo < 86400) relTime = QString("Vor %1 Std.").arg(secsAgo / 3600);
          else relTime = fi.lastModified().toString("dd.MM.yyyy");
          m_lblMetaModified->setText(relTime);
        }
      }
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
  // runs in the same task and reliably redirects back to the app via the
  // com.benschwank.blop:/oauth2redirect deep link handled by
  // BlopActivity -> BlopOAuthBridge -> GoogleAuthManager.
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
    // Find the main window instance and reset the OAuth timer
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        MainWindow *mainWin = qobject_cast<MainWindow*>(widget);
        if (mainWin) {
            qInfo() << "MainWindow: resetOAuthTimer called, resetting OAuth timer";
            mainWin->m_googleLoginInFlight = false;
            mainWin->m_googleLoginInFlightSinceMs = 0;
            return;
        }
    }
    qWarning() << "MainWindow: resetOAuthTimer called but no MainWindow instance found";
}
#endif
