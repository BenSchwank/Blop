#include "mainwindow.h"
#include "UIStyles.h"
#include "canvasview.h"
#include "moderntoolbar.h"
#include "newnotedialog.h"
#include "profileeditordialog.h"
#include "settingsdialog.h"

// --- WICHTIGE ZUSÄTZLICHE INCLUDES ---
#include "Note.h"
#include "ToolMode.h"
#include "markdowneditor.h"
#include "multipagenoteview.h"
#include "noteeditor.h"
#include "notemanager.h"
#include "pagemanager.h"
#include "tools/ToolManager.h"
#include "googleauthmanager.h"

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
#include <QDesktopServices>
#include <QDir>
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
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProgressDialog>
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
#include <utility>
#include <QCloseEvent>
#include <QShowEvent>

#ifdef Q_OS_ANDROID
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QSslSocket>
#include <QTemporaryFile>
#include <QWidget>
#include <QElapsedTimer>
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
#define BLOP_VERSION_STR "3.13.5.6"
#endif
static const char *BLOP_VERSION = BLOP_VERSION_STR;

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

  if (isHeader) {
    painter->setPen(QColor("#888888"));
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
    painter->setBrush(QColor("#888888"));
    painter->setPen(Qt::NoPen);
    painter->drawPath(arrow);
    QRect textRect = option.rect.adjusted(30, 0, -15, 0);
    painter->setPen(QColor("#888888"));
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    QFontMetrics fm(f);
    int textW = fm.horizontalAdvance(text);
    int lineX = textRect.left() + textW + 10;
    int lineY = option.rect.center().y();
    painter->setPen(QColor("#333333"));
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
      painter->setBrush(QColor("#2A2640"));
      painter->setPen(Qt::NoPen);
      painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
    } else if (hover) {
      painter->setBrush(QColor(255, 255, 255, 10));
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
      painter->setBrush(selected ? Qt::white : QColor("#aaaaaa"));
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
      painter->setOpacity(0.7);
      icon.paint(painter, iconRect, Qt::AlignCenter, mode, QIcon::Off);
      painter->setOpacity(1.0);
    }

    QRect textRect = rect;
    textRect.setLeft(iconRect.right() + 15);
    textRect.setRight(rect.right() - 40);
    painter->setPen(selected ? m_window->currentAccentColor() : QColor(220, 220, 220));

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
      painter->setPen(selected ? Qt::white : m_window->currentAccentColor());
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

  if (isWideList) {
    int iconDim = rect.height() - 20;
    if (iconDim < 16)
      iconDim = 16;
    // Visually shrink the icon inside the touch cell.
    iconDim = qMax(16, (int)(iconDim * 0.65));
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
    // Visually shrink the icon inside the touch cell.
    iconDim = qMax(18, (int)(iconDim * 0.65));

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

  QIcon menuIcon = m_window->createModernIcon("more_vert", Qt::gray);
  int dotSize = 24;
  QRect menuRect(rect.right() - dotSize - 4, rect.top() + 4, dotSize, dotSize);
  if (isWideList)
    menuRect.moveTop(rect.center().y() - dotSize / 2);
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
  m_anim->stop();
  m_anim->setEndValue(1.2);
  m_anim->start();
  QToolButton::enterEvent(event);
}
void ModernButton::leaveEvent(QEvent *event) {
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
  int iconS = iconSize().width();
  p.translate(w / 2, h / 2);
  p.scale(m_scale, m_scale);
  p.translate(-iconS / 2, -iconS / 2);
  QIcon::Mode mode = isChecked() ? QIcon::Active : QIcon::Normal;
  ic.paint(&p, 0, 0, iconS, iconS, Qt::AlignCenter, mode);
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
  btnEditorMenu->show();
  m_mainSplitter->setHandleWidth(0);
#else
  qDebug() << "MainWindow: updateSidebarState davor";
  updateSidebarState();
  qDebug() << "MainWindow: updateSidebarState danach";
#endif

  // Initial mode: guest → Study, logged-in → Notes (setCurrentIndex emits onModeChanged)
  QString savedUser = QSettings("Blop", "BlopApp").value("username").toString();
  updateSidebarUser(savedUser);
  // Start unfolded in Notes for logged-in users.
  if (!savedUser.trimmed().isEmpty()) {
    animateSidebar(true);
  }
#if defined(Q_OS_WIN) && !defined(QT_DEBUG)
  // On Windows release, always show a visible login helper at startup when logged out.
  if (savedUser.trimmed().isEmpty()) {
    QTimer::singleShot(250, this, [this]() { showWindowsStudyNoticeOnce(); });
  }
#endif

  // Connect GoogleAuthManager's browser prompts to custom overlay logic
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::requireBrowser,
          this, [this](const QUrl &url) {
#ifdef Q_OS_ANDROID
              // Check TLS availability before opening browser, warn user if broken
              if (!QSslSocket::supportsSsl()) {
                  QMessageBox::critical(this, "TLS Fehler",
                      "HTTPS wird nicht unterstützt!\n\n"
                      "OpenSSL wurde nicht gefunden. "
                      "Der Google-Login kann nicht funktionieren.\n\n"
                      "TLS-Version: " + QSslSocket::sslLibraryVersionString());
                  return;
              }
#endif
              showAuthOverlay(url);
          });
          
  // Close the overlay when login completes successfully
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::authenticated, this, [this]() {
      if (m_authOverlay) {
          m_authOverlay->accept();
          m_authOverlay->deleteLater();
          m_authOverlay = nullptr;
      }
  });

  // Verify the Google access token via our own backend and inject session into the WebView
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::idTokenReceived, this, [this](const QString &token) {
      qDebug() << "Received Google token in MainWindow, posting to backend for verification...";

      QNetworkAccessManager *nam = new QNetworkAccessManager(this);
      QNetworkRequest req(QUrl("https://blop-study.com/api/auth/google/verify"));
      req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

      QJsonObject body;
      body["token"] = token;
      QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);

      QNetworkReply *reply = nam->post(req, postData);
      connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
          reply->deleteLater();
          nam->deleteLater();
          if (reply->error() != QNetworkReply::NoError) {
              qWarning() << "Backend Google verify failed:" << reply->errorString();
              return;
          }
          QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
          if (doc.isNull() || !doc.isObject()) return;
          QJsonObject obj = doc.object();
          QString sessionId = obj.value("session_id").toString();
          QString username  = obj.value("username").toString();
          if (sessionId.isEmpty() || username.isEmpty()) {
              qWarning() << "Backend returned empty session_id or username!";
              return;
          }
          qDebug() << "Backend verified Google login! username:" << username;

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
          if (QWebEngineView *wv = m_studyContainer->findChild<QWebEngineView *>()) {
            if (wv->page())
              wv->page()->runJavaScript(js);
          }
#endif
      });
  });

  // Warn user if OAuth fails locally or via server
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::authenticationFailed, this, [this](const QString& error) {
      QMessageBox::warning(this, "Google Login Fehler", "Der Login über Google ist fehlgeschlagen:\n" + error);
  });

  QTimer::singleShot(100, this, &MainWindow::updateGrid);
  // Delay update check by 5s so the web view has time to render before the
  // modal blocks
#ifndef Q_OS_ANDROID
  QTimer::singleShot(5000, this, &MainWindow::checkForUpdates);
#endif
}
MainWindow::~MainWindow() {}

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

    // Parse tag name, e.g. "v3.5.3.9" → "3.5.3.9"
    QString tagName = root.value("tag_name").toString();
    if (tagName.startsWith('v') || tagName.startsWith('V'))
      tagName = tagName.mid(1);

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
    QList<int> local = parseVersion(QString(BLOP_VERSION));

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
    QString currentVer = QString(BLOP_VERSION);
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
  m_modeSelector->addItem("Notizen");
  m_modeSelector->addItem("Study");
  m_modeSelector->setFixedSize(0, 0);
  m_modeSelector->setMaximumSize(0, 0);
  connect(m_modeSelector, &QComboBox::currentIndexChanged,
          this, &MainWindow::onModeChanged);

  QPushButton *btnMode = new QPushButton("Notizen  ▾", m_topNavControls);
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
            btnMode->setText(m_modeSelector->itemText(idx) + "  ▾");
          });
  m_btnMode = btnMode;
  // QComboBox stays hidden; Qt may refuse showPopup() when visible==false — use QMenu instead.
  m_modeSelector->hide();
  connect(btnMode, &QPushButton::clicked, this, [this, btnMode]() {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #1E1E2E; border: 1px solid rgba(255,255,255,0.12); "
        "border-radius: 8px; padding: 4px; }"
        "QMenu::item { color: #E0DEFF; padding: 8px 28px; }"
        "QMenu::item:selected { background: rgba(124,92,252,0.35); }");
    QAction *aNotes = menu.addAction(tr("Notizen"));
    QAction *aStudy = menu.addAction(tr("Study"));
    QAction *chosen =
        menu.exec(btnMode->mapToGlobal(QPoint(0, btnMode->height())));
    if (!m_modeSelector)
      return;
    if (chosen == aNotes)
      m_modeSelector->setCurrentIndex(0);
    else if (chosen == aStudy)
      m_modeSelector->setCurrentIndex(1);
  });
  navLayout->addWidget(btnMode);
  navLayout->addWidget(m_modeSelector);
  navLayout->addSpacing(12);

  // ── TABS ──────────────────────────────────────────────────────────────────
  // Container for dynamic tabs
  QWidget *tabsContainer = new QWidget(m_topNavControls);
  tabsContainer->setStyleSheet("background: transparent; border: none;");
  m_tabBarLayout = new QHBoxLayout(tabsContainer);
  m_tabBarLayout->setContentsMargins(0, 0, 0, 0);
  m_tabBarLayout->setSpacing(4);
  navLayout->addWidget(tabsContainer);

  // Tab Styles
  auto tabActiveStyle = []() -> QString {
    return "QPushButton {"
           "  background: rgba(124,92,252,0.25);"
           "  border: none;"
           "  border-radius: 8px;"
           "  color: #E8E4FF; font-size: 12px; font-weight: 600;"
           "  padding: 0 16px;"
           "}"
           "QPushButton:hover { background: rgba(124,92,252,0.35); }";
  };
  auto tabInactiveStyle = []() -> QString {
    return "QPushButton {"
           "  background: transparent;"
           "  border: none;"
           "  border-radius: 8px;"
           "  color: rgba(255,255,255,0.45); font-size: 12px; font-weight: 500;"
           "  padding: 0 14px;"
           "}"
           "QPushButton:hover {"
           "  background: rgba(255,255,255,0.08);"
           "  color: rgba(255,255,255,0.85);"
           "}";
  };

  // Home Tab
  m_btnHomeTab = new QPushButton(tabsContainer);
  m_btnHomeTab->setText("⌂  Home");
  m_btnHomeTab->setFixedHeight(32);
  m_btnHomeTab->setCursor(Qt::PointingHandCursor);
  m_btnHomeTab->setStyleSheet(tabActiveStyle());
  connect(m_btnHomeTab, &QPushButton::clicked, this,
          [this, tabActiveStyle, tabInactiveStyle]() {
            m_activeTabIndex = -1;
            m_btnHomeTab->setStyleSheet(tabActiveStyle());
            for (auto *btn : m_noteTabButtons)
              btn->setStyleSheet(tabInactiveStyle());
            onBackToOverview();
          });
  m_tabBarLayout->addWidget(m_btnHomeTab);

  // "+ Tab" Button
  QPushButton *btnNewTab = new QPushButton("+", tabsContainer);
  btnNewTab->setFixedSize(32, 32);
  btnNewTab->setCursor(Qt::PointingHandCursor);
  btnNewTab->setToolTip("Neue Notiz öffnen");
  btnNewTab->setStyleSheet(
      "QPushButton {"
      "  background: transparent;"
      "  border: none;"
      "  border-radius: 8px;"
      "  color: rgba(255,255,255,0.45); font-size: 16px; font-weight: 400;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.18);"
      "  color: rgba(200,190,255,0.95);"
      "}");
  connect(btnNewTab, &QPushButton::clicked, this,
          &MainWindow::onShowNewTabPopup);
  m_tabBarLayout->addWidget(btnNewTab);

  navLayout->addStretch(); // Push everything else to the right

  // ── Suchleiste ─────────────────────────────────────────────────────────────
  m_titleSearchBar = new QLineEdit(m_topNavControls);
  m_titleSearchBar->setPlaceholderText("🔍  Suchen...");
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

  // ── Einstellungen (Tags & Optionen) ────────────────────────────────────────
  btnEditorSettings = new ModernButton(m_topNavControls);
  btnEditorSettings->setIcon(createModernIcon("settings", QColor("#A0A0C8")));
  btnEditorSettings->setFixedSize(32, 32);
  btnEditorSettings->setToolTip("Tags & Optionen");
  btnEditorSettings->setStyleSheet(
      "QToolButton {"
      "  background: transparent; border: none; border-radius: 8px;"
      "}"
      "QToolButton:hover {"
      "  background: rgba(124,92,252,0.18);"
      "}");
  connect(btnEditorSettings, &QAbstractButton::clicked, this,
          &MainWindow::onToggleRightSidebar);
  navLayout->addWidget(btnEditorSettings);
  navLayout->addSpacing(4);

  // ── Teilen ─────────────────────────────────────────────────────────────────
  m_btnPages = new ModernButton(m_topNavControls);
  m_btnPages->setFixedHeight(32);
  m_btnPages->setText("  Teilen");
  m_btnPages->setIcon(createModernIcon("share", Qt::white));
  m_btnPages->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_btnPages->setStyleSheet(
      "QToolButton {"
      "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
      "    stop:0 #7C5CFC, stop:1 #5E3DCC);"
      "  border: none; border-radius: 8px;"
      "  color: white; font-size: 12px; font-weight: 600;"
      "  padding: 0 16px 0 10px;"
      "}"
      "QToolButton:hover {"
      "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
      "    stop:0 #9B79FF, stop:1 #7050EE);"
      "}");
  connect(m_btnPages, &QAbstractButton::clicked, this,
          &MainWindow::onShareClicked);
  navLayout->addWidget(m_btnPages);

  // === INJECT TOP NAV CONTAINER ===
  mainLayout->addWidget(m_topNavControls, 1); // Expand to fill middle space
  mainLayout->addSpacing(16); // Gap before window controls

  // ── Window Controls ────────────────────────────────────────────────────────
  struct WinBtn { QPushButton **ptr; QString sym; bool isClose; };
  QList<WinBtn> winBtns = {
    { &m_btnWinMin,   "\u2212", false },  // −
    { &m_btnWinMax,   "\u25A1", false },  // □
    { &m_btnWinClose, "\u00D7", true  },  // ×
  };
  for (auto &wb : winBtns) {
    *wb.ptr = new QPushButton(wb.sym, m_titleBarWidget);
    (*wb.ptr)->setFixedSize(46, 50); // Full height of titlebar
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
  connect(m_btnWinMin,   &QPushButton::clicked, this, &MainWindow::onWinMinimize);
  connect(m_btnWinMax,   &QPushButton::clicked, this, &MainWindow::onWinMaximize);
  connect(m_btnWinClose, &QPushButton::clicked, this, &MainWindow::onWinClose);
}


// Window Dragging Implementation
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_editorCenterWidget && event->type() == QEvent::Resize && m_floatingTools) {
    if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
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
  } else if (obj == m_floatingTools && event->type() == QEvent::Move) {
    // Re-dock when floating toolbar is dragged to the top (y <= 10)
    if (m_floatingTools->y() <= 10) {
      if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
        tb->setDockMode(true);
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
  m_btnHomeTab->setStyleSheet(
      "QPushButton {"
      "  background: rgba(255,255,255,0.04);"
      "  border: 1px solid rgba(255,255,255,0.08);"
      "  color: rgba(255,255,255,0.45); font-size: 12px; font-weight: 500;"
      "  padding: 0 14px; border-radius: 14px;"
      "}"
      "QPushButton:hover {"
      "  background: rgba(124,92,252,0.10);"
      "  border: 1px solid rgba(124,92,252,0.25);"
      "  color: rgba(255,255,255,0.80);"
      "}");

  int tabIndex = m_noteTabButtons.size();

  // Container widget: [title btn][× btn] – kompakte Pill (Höhe 30px)
  QWidget *tabWidget = new QWidget;
  tabWidget->setFixedHeight(30);
  tabWidget->setStyleSheet(
      "QWidget {"
      "  background-color: rgba(124,92,252,0.22);"
      "  border: 1px solid rgba(124,92,252,0.55);"
      "  border-radius: 14px;"
      "}");
  QHBoxLayout *tabLay = new QHBoxLayout(tabWidget);
  tabLay->setContentsMargins(6, 0, 4, 0);
  tabLay->setSpacing(2);

  QPushButton *tabBtn = new QPushButton(title, tabWidget);
  tabBtn->setFixedHeight(22);
  tabBtn->setCursor(Qt::PointingHandCursor);
  tabBtn->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  border: none; color: #E8E8FF; font-size: 12px; font-weight: 600;"
      "  padding: 0 2px 0 6px;"
      "}");
  m_noteTabButtons.append(tabBtn);
  tabLay->addWidget(tabBtn);

  QPushButton *closeBtn = new QPushButton("\u2715", tabWidget);
  closeBtn->setFixedSize(20, 20);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  border: none; color: rgba(255,255,255,0.50); font-size: 10px;"
      "  border-radius: 10px;"
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

  overlay->setFixedSize(450, 550);
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
    // Aktiver Stil wieder herstellen (neue Farben)
    m_btnHomeTab->setStyleSheet(
        "QPushButton {"
        "  background: rgba(124,92,252,0.22);"
        "  border: 1px solid rgba(124,92,252,0.55);"
        "  color: #DDD8FF; font-size: 12px; font-weight: 600;"
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
void MainWindow::onToggleFloatingTools() {}

void MainWindow::applyTheme() {
  QString c = m_currentAccentColor.name();
  QString c_light = m_currentAccentColor.lighter(130).name();
#ifdef Q_OS_ANDROID
  const QString c_scrollPress = m_currentAccentColor.darker(118).name();
  const char *sbW = "12px";
  const char *sbH = "12px";
#endif
  if (m_fileListView)
    m_fileListView->setAccentColor(m_currentAccentColor);

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
  this->setStyleSheet(style);

  if (m_sidebarContainer)
    m_sidebarContainer->setStyleSheet(
        "background-color: #0F111A; border-right: 1px solid rgba(255,255,255,0.06);");
  if (m_sidebarStrip)
    m_sidebarStrip->setStyleSheet(
        "background-color: #0F111A; border-right: 1px solid rgba(255,255,255,0.06);");
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

  if (m_rightSidebar)
    m_rightSidebar->setStyleSheet(
        "background-color: #0D0B14; border-left: 1px solid #201E2E;");

  if (m_titleBarWidget)
    m_titleBarWidget->setStyleSheet(
        "background-color: #0D0B14; border-bottom: 1px solid #201E2E;");

  // Overview controls follow current accent color while keeping dark base style.
  if (m_overviewContainer) {
    m_overviewContainer->setStyleSheet(
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
            .arg(c, c_light));
  }

  // Accent propagation for frequently used controls/widgets.
  if (m_btnPages) {
    m_btnPages->setStyleSheet(
        QString(
            "QToolButton {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "    stop:0 %1, stop:1 %2);"
            "  border: none; border-radius: 8px;"
            "  color: white; font-size: 12px; font-weight: 600;"
            "  padding: 0 16px 0 10px;"
            "}"
            "QToolButton:hover {"
            "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "    stop:0 %3, stop:1 %1);"
            "}")
            .arg(c_light, c, m_currentAccentColor.lighter(150).name()));
  }
  if (m_btnSidebarSettings) {
    m_btnSidebarSettings->setStyleSheet(
        QString(
            "QPushButton { background: transparent; color: #888; border: none; "
            "font-size: 10px; padding: 0; text-align: left; } "
            "QPushButton:hover { color: %1; }")
            .arg(c));
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
  if (m_btnAndroidToolbarMenu) {
    m_btnAndroidToolbarMenu->setStyleSheet(
        QString(
            "QToolButton { background: transparent; border: none; "
            "border-radius: 12px; padding: 0; }"
            "QToolButton:pressed { background-color: %1; }")
            .arg(m_currentAccentColor.darker(130).name()));
    m_btnAndroidToolbarMenu->setIcon(
        createModernIcon("menu", m_currentAccentColor));
  }
  if (m_btnAndroidToolbarSettings) {
    m_btnAndroidToolbarSettings->setStyleSheet(
        QString(
            "QToolButton { background: transparent; border: none; "
            "border-radius: 12px; padding: 0; }"
            "QToolButton:pressed { background-color: %1; }")
            .arg(m_currentAccentColor.darker(130).name()));
    m_btnAndroidToolbarSettings->setIcon(
        createModernIcon("settings", m_currentAccentColor));
  }
  if (m_btnAndroidToolbarExport) {
    m_btnAndroidToolbarExport->setIcon(
        createModernIcon("more_vert", m_currentAccentColor));
  }
  if (m_btnAndroidToolbarPageManager) {
    m_btnAndroidToolbarPageManager->setIcon(
        createModernIcon("pages", m_currentAccentColor));
  }
  if (m_mainContentStack)
    applyAndroidTabStyles(m_mainContentStack->currentIndex());
#endif
}void MainWindow::updateTheme(QColor accentColor) {
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
  int screenWidth = m_fileListView->width();
  if (screenWidth <= 0)
    screenWidth = QApplication::primaryScreen()->availableGeometry().width();

  int s = m_currentProfile.iconSize;
  if (s <= 20)
    s = 100;

  int spacing = m_currentProfile.gridSpacing;
  if (spacing <= 0)
    spacing = 20;

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

  const int itemHeight = itemWidth;

  m_fileListView->setItemSize(QSize(itemWidth, itemHeight));
  m_fileListView->setIconSize(QSize(itemWidth, itemWidth));
  m_fileListView->setGridSize(QSize(itemWidth + spacing, itemHeight + spacing));

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
  int size = 64;
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
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
    // Vertical 3-dot menu
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(30, 16, 4, 4);
    p.drawEllipse(30, 30, 4, 4);
    p.drawEllipse(30, 44, 4, 4);
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
  } else if (name == "menu") {
    p.drawLine(12, 20, 52, 20);
    p.drawLine(12, 32, 52, 32);
    p.drawLine(12, 44, 52, 44);
  } else if (name == "settings") {
    // Gear-like settings icon
    const QPointF c(32, 32);
    const qreal outerR = 20;
    const qreal innerR = 14;
    const qreal toothR = 23;
    // Teeth
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < 8; ++i) {
      const double a = (i * pi) / 4.0;
      const double cs = qCos(a);
      const double sn = qSin(a);
      QPointF p1(c.x() + cs * toothR, c.y() + sn * toothR);
      QPointF p2(c.x() + cs * outerR, c.y() + sn * outerR);
      p.drawLine(p1, p2);
    }
    // Ring
    p.drawEllipse(c, outerR, outerR);
    // Inner circle
    p.drawEllipse(c, innerR, innerR);
    // Cross spokes
    p.drawLine(QPointF(c.x() - 9, c.y()), QPointF(c.x() + 9, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - 9), QPointF(c.x(), c.y() + 9));
    // Center dot
    p.drawEllipse(QPointF(c.x(), c.y()), 3.5, 3.5);
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
    // Stacked pages / A4 icon
    p.drawRoundedRect(14, 16, 34, 34, 5, 5); // back
    p.drawRoundedRect(20, 12, 34, 34, 5, 5); // front offset
    // Lines
    p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(28, 26, 44, 26);
    p.drawLine(28, 34, 46, 34);
    p.drawLine(28, 42, 42, 42);
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
  // Keep content above Android system/nav bar area.
  mainLayout->setContentsMargins(0, 0, 0, 22);
#else
  mainLayout->setContentsMargins(0, 0, 0, 0);
#endif
  mainLayout->setSpacing(0);

#ifdef Q_OS_ANDROID
  // Native QQuickView/WebView can draw above normal widgets in the central layout
  // (Android SurfaceView z-order). Put tabs in QMainWindow's top tool bar so they
  // stay above the embedded native surface — changing onModeChanged alone won't fix that.
  QToolBar *topBar = new QToolBar(this);
  m_androidHeader = topBar;
  topBar->setObjectName("AndroidTopBar");
  topBar->setMovable(false);
  topBar->setFloatable(false);
  topBar->setAllowedAreas(Qt::TopToolBarArea);
  topBar->setContentsMargins(0, 0, 0, 0);
  topBar->setStyleSheet(
      "QToolBar { background-color: #0F111A; border: none; "
      "spacing: 0; padding: 0px; }"
      "QToolButton { background: transparent; border: none; }");
  topBar->setFixedHeight(48);

  QWidget *androidHeader = new QWidget(topBar);
  androidHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  androidHeader->setFixedHeight(48);
  androidHeader->setStyleSheet("background-color: #0F111A;");
  QHBoxLayout *headerLay = new QHBoxLayout(androidHeader);
  headerLay->setContentsMargins(10, 2, 10, 2);
  headerLay->setSpacing(8);

  // Hamburger only while a note is open (overview uses floating menu next to welcome)
  m_btnAndroidToolbarMenu = new ModernButton(androidHeader);
  m_btnAndroidToolbarMenu->setIcon(createModernIcon("menu", QColor("#A0A0C8")));
  m_btnAndroidToolbarMenu->setFixedSize(22, 22);
  m_btnAndroidToolbarMenu->setIconSize(QSize(14, 14));
  m_btnAndroidToolbarMenu->setStyleSheet(
      "QToolButton {"
      "  background: transparent; border: none; border-radius: 8px;"
      "}"
  );
  m_btnAndroidToolbarMenu->hide();
  connect(m_btnAndroidToolbarMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  headerLay->addWidget(m_btnAndroidToolbarMenu);

  QLabel *lblLogoIcon = new QLabel(androidHeader);
  QPixmap blopLogo(":/assets/logo.jpg");
  if (!blopLogo.isNull()) {
    lblLogoIcon->setPixmap(
        blopLogo.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    // Fallback if resource is missing on a build target.
    lblLogoIcon->setPixmap(createModernIcon("note_blop", QColor("#A8A6FF")).pixmap(30, 30));
  }
  lblLogoIcon->setFixedSize(30, 30);
  lblLogoIcon->setStyleSheet("background: transparent; border: none;");
  headerLay->addWidget(lblLogoIcon);
  headerLay->addSpacing(6);

  QLabel *lblLogo = new QLabel("Blop", androidHeader);
  lblLogo->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  lblLogo->setStyleSheet(
      "color: white; font-weight: bold; font-size: 17px; margin: 0px; padding: 0px; border: none;");
  headerLay->addWidget(lblLogo);
  headerLay->addSpacing(4);

  // Keep m_modeSelector QComboBox as the logic controller (hidden, 0x0)
  // Use visible tab buttons instead — QComboBox popups dismiss immediately on Android touch
  m_modeSelector = new QComboBox(androidHeader);
  m_modeSelector->addItem("Notizen");
  m_modeSelector->addItem("Study");
  m_modeSelector->setFixedSize(0, 0);
  m_modeSelector->setMaximumSize(0, 0);
  // Desktop: combo drives onModeChanged. Android: only explicit onModeChanged (tabs +
  // updateSidebarUser) — otherwise setCurrentIndex fires onModeChanged twice per tap
  // and updateSidebarState() can leave the UI stuck in Notes.
#ifndef Q_OS_ANDROID
  connect(m_modeSelector, &QComboBox::currentIndexChanged,
          this, &MainWindow::onModeChanged);
#endif

  // Visible tab-style toggle buttons
  QString tabActiveStyle =
      "QPushButton { background: #5E5CE6; color: white; border-radius: 13px;"
      "  padding: 3px 12px; font-weight: bold; font-size: 12px; border: none; }";
  QString tabInactiveStyle =
      "QPushButton { background: transparent; color: #AAA; border-radius: 13px;"
      "  padding: 3px 12px; font-weight: bold; font-size: 12px;"
      "  border: 1px solid #333; }"
      "QPushButton:pressed { background: rgba(94,92,230,0.2); }";

  m_btnAndroidNotes = new QPushButton("Notizen", androidHeader);
  m_btnAndroidNotes->setFixedHeight(28);
  m_btnAndroidNotes->setCursor(Qt::PointingHandCursor);
  m_btnAndroidNotes->setStyleSheet(tabActiveStyle); // default: notes active

  m_btnAndroidStudy = new QPushButton("Study", androidHeader);
  m_btnAndroidStudy->setFixedHeight(28);
  m_btnAndroidStudy->setCursor(Qt::PointingHandCursor);
  m_btnAndroidStudy->setStyleSheet(tabInactiveStyle);

  connect(m_btnAndroidNotes, &QPushButton::clicked, this, [this]() {
    QSignalBlocker b(m_modeSelector);
    m_modeSelector->setCurrentIndex(0);
    onModeChanged(0);
  });
  connect(m_btnAndroidStudy, &QPushButton::clicked, this, [this]() {
    QSignalBlocker b(m_modeSelector);
    m_modeSelector->setCurrentIndex(1);
    onModeChanged(1);
  });

  headerLay->addWidget(m_btnAndroidNotes);
  headerLay->addWidget(m_btnAndroidStudy);

  headerLay->addWidget(m_modeSelector); // hidden 0x0, logic only

  // Centered search bar (Android top bar), visually similar to overview search.
  headerLay->addStretch(); // tabs stay left; icons stay right

  m_btnAndroidToolbarSettings = new ModernButton(androidHeader);
  m_btnAndroidToolbarSettings->setIcon(
      createModernIcon("settings", m_currentAccentColor));
  m_btnAndroidToolbarSettings->setFixedSize(22, 22);
  m_btnAndroidToolbarSettings->setIconSize(QSize(14, 14));
  m_btnAndroidToolbarSettings->setStyleSheet(
      "QToolButton {"
      "  background: transparent; border: none; border-radius: 8px;"
      "}"
      "QToolButton:pressed { background: rgba(124,92,252,0.20); }");
  m_btnAndroidToolbarSettings->hide();
  connect(m_btnAndroidToolbarSettings, &QAbstractButton::clicked, this,
          &MainWindow::onToggleRightSidebar);
  headerLay->addWidget(m_btnAndroidToolbarSettings);

#ifdef Q_OS_ANDROID
  // Page manager (only for A4 notes) - orange button.
  m_btnAndroidToolbarPageManager = new ModernButton(androidHeader);
  m_btnAndroidToolbarPageManager->setIcon(
      createModernIcon("pages", m_currentAccentColor));
  m_btnAndroidToolbarPageManager->setFixedSize(22, 22);
  m_btnAndroidToolbarPageManager->setIconSize(QSize(14, 14));
  m_btnAndroidToolbarPageManager->setStyleSheet(
      "QToolButton { background: rgba(255,140,0,0.10); border: none; border-radius: 8px; }"
      "QToolButton:pressed { background: rgba(255,140,0,0.22); }");
  m_btnAndroidToolbarPageManager->hide();
  connect(m_btnAndroidToolbarPageManager, &QAbstractButton::clicked, this,
          &MainWindow::onTogglePageManager);
  headerLay->addWidget(m_btnAndroidToolbarPageManager);
#endif
#ifdef Q_OS_ANDROID
  // Put the export (3-dot) button next to the settings icon.
  if (m_btnAndroidToolbarExport)
    headerLay->addWidget(m_btnAndroidToolbarExport);
#endif

  topBar->addWidget(androidHeader);
  addToolBar(Qt::TopToolBarArea, topBar);

  // Status bar colors must run on Android's *UI* thread — JNI from Qt's thread
  // triggers CalledFromWrongThreadException and can break touch/layout (see logcat).
  QTimer::singleShot(300, this, []() {
      QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative",
            "activity",
            "()Landroid/app/Activity;");
        if (activity.isValid()) {
          QJniObject window =
              activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
          if (window.isValid()) {
            window.callMethod<void>("addFlags", "(I)V", 0x80000000);
            window.callMethod<void>("setStatusBarColor", "(I)V", 0xFF0D0B14);
            window.callMethod<void>("setNavigationBarColor", "(I)V", 0xFF0D0B14);
          }
        }
      });
  });

#else
  if (m_titleBarWidget) {
    mainLayout->addWidget(m_titleBarWidget);
  }

  // ── Single ModernToolbar instance replaces the docked bar ────────────────
  // (m_floatingTools initialized in setupRightSidebar/setupFloatingToolbar)
#endif

  m_mainContentStack = new QStackedWidget(this);

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
  m_overviewContainer->setStyleSheet("background-color: #0D0D12;");
#endif
  // KEIN installEventFilter - damit Klicks auf Buttons korrekt weitergeleitet werden!
  QVBoxLayout *overviewLayout = new QVBoxLayout(m_overviewContainer);
#ifdef Q_OS_ANDROID
  // Align with Windows overview: same horizontal breathing room; bottom inset for nav/FAB
  overviewLayout->setContentsMargins(MARGIN_OVERVIEW + 4, 20,
                                     MARGIN_OVERVIEW + 4, 110);
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
  // ── Android: same structure & styles as Windows (welcome → search row) ────
  headerLayout->setContentsMargins(10, 20, 10, 30);
  headerLayout->setSpacing(15);

  // Hamburger top-left, title below — matches Windows reference layout
  btnEditorMenu = new ModernButton(m_overviewContainer);
  btnEditorMenu->setIcon(createModernIcon("menu", Qt::white));
  btnEditorMenu->setFixedSize(40, 40);
  btnEditorMenu->setCursor(Qt::PointingHandCursor);
  btnEditorMenu->setStyleSheet(
      "QToolButton { background: transparent; border: none; border-radius: 8px; }"
      "QToolButton:pressed { background-color: rgba(255,255,255,0.08); }");
  connect(btnEditorMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  headerLayout->addWidget(btnEditorMenu, 0, Qt::AlignLeft);

  QLabel *lblWelcome = new QLabel("Willkommen zurück!", m_overviewContainer);
  lblWelcome->setStyleSheet(
      "color: white; font-size: 28px; font-weight: bold; font-family: 'Segoe UI';");
  headerLayout->addWidget(lblWelcome);

  QHBoxLayout *searchActionLayout = new QHBoxLayout();
  searchActionLayout->setSpacing(15);

  QLineEdit *searchBar = new QLineEdit(m_overviewContainer);
  searchBar->setObjectName("overviewSearchBar");
  searchBar->setPlaceholderText("Notizen durchsuchen...");
  searchBar->setFrame(false);
  searchBar->setAttribute(Qt::WA_StyledBackground, true);
  searchBar->setFixedHeight(44);
  searchBar->setStyleSheet(
      "QLineEdit {"
      "  background-color: #1A1829;"
      "  color: white;"
      "  border: 1px solid #201E2E;"
      "  border-radius: 22px;"
      "  padding: 0 20px;"
      "  font-size: 13px;"
      "}"
      "QLineEdit:focus {"
      "  border: 1px solid #5E5CE6;"
      "}"
  );
  searchActionLayout->addWidget(searchBar, 1);

  QPushButton *btnNewNote = new QPushButton("Neue Notiz", m_overviewContainer);
  btnNewNote->setObjectName("overviewBtnNewNote");
  btnNewNote->setFixedHeight(40);
  btnNewNote->setCursor(Qt::PointingHandCursor);
  btnNewNote->setStyleSheet(
      "QPushButton {"
      "  background-color: #5E5CE6;"
      "  color: white;"
      "  border-radius: 20px;"
      "  padding: 0 24px;"
      "  font-weight: bold;"
      "  font-size: 13px;"
      "  border: none;"
      "}"
      "QPushButton:hover { background-color: #7D7AFF; }"
  );
  connect(btnNewNote, &QPushButton::clicked, this, &MainWindow::onNewPage);
  searchActionLayout->addWidget(btnNewNote);

  QPushButton *btnNewFolder = new QPushButton("Neuer Ordner", m_overviewContainer);
  btnNewFolder->setObjectName("overviewBtnNewFolder");
  btnNewFolder->setFixedHeight(40);
  btnNewFolder->setCursor(Qt::PointingHandCursor);
  btnNewFolder->setStyleSheet(
      "QPushButton {"
      "  background-color: transparent;"
      "  color: white;"
      "  border-radius: 20px;"
      "  padding: 0 24px;"
      "  font-weight: bold;"
      "  font-size: 13px;"
      "  border: 1px solid #333;"
      "}"
      "QPushButton:hover { background-color: rgba(255,255,255,0.05); border-color: #555; }"
  );
  connect(btnNewFolder, &QPushButton::clicked, this, &MainWindow::onCreateFolder);
  searchActionLayout->addWidget(btnNewFolder);

  headerLayout->addLayout(searchActionLayout);

#else
  // ── Desktop/Windows: Original layout unchanged ───────────────────────────
  headerLayout->setContentsMargins(10, 20, 10, 30);
  headerLayout->setSpacing(15);

  QLabel *lblWelcome = new QLabel("Willkommen zurück!", m_overviewContainer);
  lblWelcome->setStyleSheet("color: white; font-size: 28px; font-weight: bold; font-family: 'Segoe UI';");
  headerLayout->addWidget(lblWelcome);

  QHBoxLayout *searchActionLayout = new QHBoxLayout();
  searchActionLayout->setSpacing(15);

  QLineEdit *searchBar = new QLineEdit(m_overviewContainer);
  searchBar->setObjectName("overviewSearchBar");
  searchBar->setPlaceholderText("Notizen durchsuchen...");
  searchBar->setFrame(false);
  searchBar->setAttribute(Qt::WA_StyledBackground, true);
  searchBar->setFixedHeight(44);
  searchBar->setStyleSheet(
      "QLineEdit {"
      "  background-color: #1A1829;"
      "  color: white;"
      "  border: 1px solid #201E2E;"
      "  border-radius: 22px;"
      "  padding: 0 20px;"
      "  font-size: 14px;"
      "}"
      "QLineEdit:focus {"
      "  border: 1px solid #5E5CE6;"
      "}"
  );
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
  m_fileListView->setItemDelegate(new ModernItemDelegate(this));
  // On Android, use TouchGesture; LeftMouseButtonGesture can cause "stuck" interactions
  // and mis-detected taps when using finger input.
#ifdef Q_OS_ANDROID
  if (m_fileListView->viewport())
    m_fileListView->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
  QScroller::grabGesture(m_fileListView, QScroller::TouchGesture);
#else
  QScroller::grabGesture(m_fileListView, QScroller::LeftMouseButtonGesture);
#endif
  connect(m_fileListView, &QListView::doubleClicked, this,
          &MainWindow::onFileDoubleClicked);
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
  ModernToolbar *topToolbar = new ModernToolbar(m_editorCenterWidget);
  topToolbar->setOrientation(ModernToolbar::Horizontal);
  topToolbar->setDraggable(true);
  topToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  int idealW = topToolbar->calculateMinLength();
  topToolbar->resize(idealW, 52);
  m_floatingTools = topToolbar;
  topToolbar->setDockMode(true);
  topToolbar->raise();

  // Install event filter to center the floating tools automatically on resize
  m_editorCenterWidget->installEventFilter(this);


  m_editorTabs = new QTabWidget(this);
  m_editorTabs->setDocumentMode(true);
  m_editorTabs->setTabsClosable(true);
  m_editorTabs->tabBar()->hide(); // We use the custom header tab bar instead
  // Style the tab bar to match the dark UI
  m_editorTabs->setStyleSheet(
      "QTabBar::tab {"
      "  background: transparent;"
      "  color: #888;"
      "  padding: 6px 16px;"
      "  border: none;"
      "  font-size: 12px;"
      "}"
      "QTabBar::tab:selected {"
      "  color: white;"
      "  border-bottom: 2px solid #5E5CE6;"
      "}"
      "QTabBar::tab:hover { color: #ccc; }"
      "QTabWidget::pane { border: none; }");
  connect(m_editorTabs, &QTabWidget::tabCloseRequested, [this](int index) {
    m_editorTabs->removeTab(index);
    if (m_editorTabs->count() == 0)
      onBackToOverview();
  });
  connect(m_editorTabs, &QTabWidget::currentChanged, this,
          &MainWindow::onTabChanged);
  centerLayout->addWidget(m_editorTabs);

  connect(topToolbar, &ModernToolbar::toolChanged,
          [this](ToolMode m) { 
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
          });

  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this, topToolbar](AbstractTool *tool) {
            if (tool && topToolbar->toolMode() != tool->mode()) {
              // Ensure the topToolbar's own state updates, so icon highlights
              // and double-click radial menus actually trigger.
              topToolbar->setToolMode(tool->mode());

              CanvasView::ToolType type = CanvasView::ToolType::Pen;
              switch (tool->mode()) {
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
#ifdef Q_OS_ANDROID
  connect(topToolbar, &ModernToolbar::backToOverviewRequested, this,
          &MainWindow::onBackToOverview);
#endif

  connect(topToolbar, &ModernToolbar::penConfigChanged,
          [this](QColor c, int w) {
            m_penColor = c;
            m_penWidth = w;
            CanvasView *cv = getCurrentCanvas();
            if (cv) {
              cv->setPenColor(c);
              cv->setPenWidth(w);
            }
          });
  connect(topToolbar, &ModernToolbar::scaleChanged, [this](qreal newScale) {
    if (m_sliderToolbarScale) {
      m_sliderToolbarScale->blockSignals(true);
      m_sliderToolbarScale->setValue(newScale * 100);
      m_sliderToolbarScale->blockSignals(false);
    }
  });
  editorMainLayout->addWidget(m_editorCenterWidget);
  setupRightSidebar();
  qDebug() << "setupUi() nach setupRightSidebar";
#ifdef Q_OS_ANDROID
  m_pageManager = new PageManager(this);
#else
  m_pageManager = new PageManager(m_editorCenterWidget);
#endif
  m_pageManager->hide();
  editorMainLayout->addWidget(m_rightSidebar);

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
  mainLayout->addWidget(m_mainContentStack);
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

void MainWindow::setupWebBrowser() {
  m_studyContainer = new QWidget(this);
  // Dark background so it's not pitch-black while WebEngine loads
  m_studyContainer->setStyleSheet("background-color: #1e1e1e;");
  QVBoxLayout *layout = new QVBoxLayout(m_studyContainer);
  layout->setContentsMargins(0, 0, 0, 0);
#ifdef Q_OS_ANDROID
  m_studyVBoxLayout = layout;
#endif

#ifdef Q_OS_ANDROID
  // IMPORTANT: QtWebView on Android uses a native Android WebView.
  // It renders correctly with a real QQuickWindow (QQuickView), but can appear
  // blank/gray when embedded via QQuickWidget.
  QQuickView *view = new QQuickView();
  m_studyQQuickView = view;
  view->setResizeMode(QQuickView::SizeRootObjectToView);

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

  QWidget *container = QWidget::createWindowContainer(view, m_studyContainer);
  container->setObjectName("StudyQuickContainer");
  m_studyWindowContainer = container;
  // Touch/focus hardening for Android
  container->setAttribute(Qt::WA_AcceptTouchEvents, true);
  container->setAttribute(Qt::WA_NativeWindow, true);
  container->setFocusPolicy(Qt::StrongFocus);
  container->setFocus(Qt::OtherFocusReason);
  container->raise();
  layout->addWidget(container);

#else
#ifdef BLOP_HAS_WEBENGINE
  const QString kStudyUrl(QStringLiteral("https://blop-six.vercel.app"));

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
    fallback->setStyleSheet("background: #1e1e1e;");
    QVBoxLayout *fallbackLayout = new QVBoxLayout(fallback);
    fallbackLayout->setContentsMargins(28, 28, 28, 28);
    fallbackLayout->setSpacing(14);

    QLabel *title = new QLabel(tr("Anmeldung"), fallback);
    title->setStyleSheet("color: #E8E4FF; font-size: 22px; font-weight: 700;");
    fallbackLayout->addWidget(title, 0, Qt::AlignLeft);

    QLabel *info = new QLabel(
        tr("Die eingebettete Study-Ansicht bleibt auf manchen Windows-Systemen schwarz.\n"
           "Bitte melde dich hier per Browser an. Danach wechselt Blop automatisch zu den Notizen."),
        fallback);
    info->setWordWrap(true);
    info->setStyleSheet("color: #C8C4E8; font-size: 13px;");
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
    btnBrowser->setStyleSheet(
        "QPushButton { background: #2d2b42; color: #E8E4FF; border: 1px solid rgba(124,92,252,0.45); "
        "border-radius: 8px; padding: 0 16px; font-weight: 600; font-size: 13px; }"
        "QPushButton:hover { background: #3a3754; }");
    connect(btnBrowser, &QPushButton::clicked, this,
            [kStudyUrl]() { QDesktopServices::openUrl(QUrl(kStudyUrl)); });
    fallbackLayout->addWidget(btnBrowser, 0, Qt::AlignLeft);

    fallbackLayout->addStretch(1);
    layout->addWidget(fallback, 1);
    return;
  }
#endif

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

  {
    QString weRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                     + QStringLiteral("/BlopWebEngine");
    QDir().mkpath(weRoot);
    QWebEngineProfile *pf = customPage->profile();
    pf->setPersistentStoragePath(weRoot + QStringLiteral("/storage"));
    pf->setCachePath(weRoot + QStringLiteral("/cache"));
    pf->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
  }

  layout->addWidget(view, 1);

  // Renderer/GPU crash leaves a black view; reload recovers (common in release-only setups).
  connect(customPage, &QWebEnginePage::renderProcessTerminated, m_studyContainer,
          [view](QWebEnginePage::RenderProcessTerminationStatus status, int exitCode) {
            Q_UNUSED(exitCode);
            if (status != QWebEnginePage::RenderProcessTerminationStatus::NormalTerminationStatus)
              QTimer::singleShot(400, view, [view]() { view->reload(); });
          });

  // Defer first navigation until after the window is shown so the native surface exists
  // (helps some Windows + frameless + WebEngine combinations that stay black).
  QTimer::singleShot(250, view, [view, kStudyUrl]() { view->load(QUrl(kStudyUrl)); });

  // --- SSO Bridge: Poll localStorage to support SPA Routing ---
  // In Next.js, navigating to Dashboard after login doesn't trigger
  // loadFinished, so we must poll localStorage to detect login state changes.
  QTimer *ssoTimer = new QTimer(m_studyContainer);
  connect(ssoTimer, &QTimer::timeout, this, [this, view]() {
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
              QString currentUser = QSettings("Blop", "BlopApp").value("username").toString();
              if (resStr != currentUser) {
                  updateSidebarUser(resStr);
              }
          } else {
              // Keep native UI in sync when web session/logged-in state was cleared.
              QString currentUser =
                  QSettings("Blop", "BlopApp").value("username").toString();
              if (!currentUser.isEmpty())
                updateSidebarUser("");
          }
        });
  });
  ssoTimer->start(1000); // Check every second

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
  QString c = m_currentAccentColor.name();
  QString c_light = m_currentAccentColor.lighter(130).name();
  const QString tabActive =
      QString("QPushButton { background: %1; color: white; border-radius: 13px;"
              "  padding: 3px 12px; font-weight: bold; font-size: 12px; border: none; }")
          .arg(c);
  const QString tabInactive =
      QString("QPushButton { background: transparent; color: #AAA; border-radius: 13px;"
              "  padding: 3px 12px; font-weight: bold; font-size: 12px;"
              "  border: 1px solid #333; }"
              "QPushButton:pressed { background: %1; border-color: %2; }")
          .arg(m_currentAccentColor.darker(170).name(), c_light);
  m_btnAndroidNotes->setStyleSheet(index == 0 ? tabActive : tabInactive);
  m_btnAndroidStudy->setStyleSheet(index == 1 ? tabActive : tabInactive);
}
#endif

void MainWindow::onModeChanged(int index) {
#ifdef Q_OS_ANDROID
  // Native WebView layer: hide/disable when leaving Study (no removeWidget — that
  // broke re-entering Study on some devices).
  if (index == 0) {
    if (m_studyWindowContainer) {
      m_studyWindowContainer->setEnabled(false);
      m_studyWindowContainer->clearFocus();
      m_studyWindowContainer->hide();
    }
    if (m_studyQQuickView)
      m_studyQQuickView->hide();
  }
#endif
  if (m_mainContentStack) {
    m_mainContentStack->setCurrentIndex(index);
#ifdef Q_OS_ANDROID
    if (m_androidHeader)
      m_androidHeader->raise();
#endif
  }
#ifdef Q_OS_ANDROID
  if (index == 1 && m_studyWindowContainer && m_studyVBoxLayout) {
    m_studyWindowContainer->setEnabled(true);
    if (m_studyVBoxLayout->indexOf(m_studyWindowContainer) < 0)
      m_studyVBoxLayout->addWidget(m_studyWindowContainer);
    m_studyWindowContainer->show();
    if (m_studyQQuickView)
      m_studyQQuickView->show();
  }
  if (m_mainContentStack) {
    if (QWidget *cur = m_mainContentStack->currentWidget()) {
      cur->raise();
      cur->setEnabled(true);
      cur->setFocus(Qt::OtherFocusReason);
    }
  }
  if (m_androidHeader)
    m_androidHeader->raise();
#endif

#ifdef Q_OS_ANDROID
  applyAndroidTabStyles(index);
#endif

#ifndef Q_OS_ANDROID
  if (m_titleBarWidget)
    m_titleBarWidget->raise();
#endif

#if defined(Q_OS_WIN) && !defined(QT_DEBUG)
  if (index == 1)
    showWindowsStudyNoticeOnce();
#endif

  // Ensure toolbar/mode selector visibility is correct for the selected mode.
  // Without this, the UI can remain in an "editor" state while the Study
  // WebView is shown, which may block switching back.
  updateSidebarState();
}

void MainWindow::showWindowsStudyNoticeOnce() {
#if defined(Q_OS_WIN) && !defined(QT_DEBUG)
  if (m_windowsStudyNoticeShown)
    return;
  m_windowsStudyNoticeShown = true;

  QDialog *dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose, true);
  dlg->setWindowTitle(tr("Anmeldung"));
  dlg->setModal(false);
  dlg->resize(560, 230);
  dlg->setStyleSheet("QDialog { background: #1e1e1e; color: #E8E4FF; }");

  QVBoxLayout *lay = new QVBoxLayout(dlg);
  lay->setContentsMargins(18, 18, 18, 18);
  lay->setSpacing(10);

  QLabel *title = new QLabel(tr("Anmeldung"), dlg);
  title->setStyleSheet("font-size: 18px; font-weight: 700; color: #E8E4FF;");
  lay->addWidget(title);

  QLabel *info = new QLabel(
      tr("Bitte melde dich an, um Blop zu starten.\n"
         "Wenn die eingebettete Ansicht schwarz bleibt, nutze Anmeldung im Browser."),
      dlg);
  info->setWordWrap(true);
  info->setStyleSheet("font-size: 13px; color: #C8C4E8;");
  lay->addWidget(info);

  QHBoxLayout *btnRow = new QHBoxLayout();
  btnRow->setSpacing(8);

  QPushButton *btnGoogle = new QPushButton(tr("Mit Google anmelden"), dlg);
  btnGoogle->setCursor(Qt::PointingHandCursor);
  btnGoogle->setMinimumHeight(36);
  btnGoogle->setStyleSheet(
      "QPushButton { background: #4285F4; color: white; border: none; border-radius: 8px; "
      "padding: 0 14px; font-weight: 700; }"
      "QPushButton:hover { background: #3367d6; }");
  QObject::connect(btnGoogle, &QPushButton::clicked, this, [this, dlg]() {
    requestGoogleLogin();
    dlg->close();
  });
  btnRow->addWidget(btnGoogle);

  QPushButton *btnBrowser = new QPushButton(tr("Study im Browser öffnen"), dlg);
  btnBrowser->setCursor(Qt::PointingHandCursor);
  btnBrowser->setMinimumHeight(36);
  btnBrowser->setStyleSheet(
      "QPushButton { background: #2d2b42; color: #E8E4FF; border: 1px solid rgba(124,92,252,0.45); "
      "border-radius: 8px; padding: 0 14px; font-weight: 600; }"
      "QPushButton:hover { background: #3a3754; }");
  QObject::connect(btnBrowser, &QPushButton::clicked, dlg, [dlg]() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://blop-six.vercel.app")));
    dlg->close();
  });
  btnRow->addWidget(btnBrowser);

  QPushButton *btnRegister = new QPushButton(tr("Registrieren"), dlg);
  btnRegister->setCursor(Qt::PointingHandCursor);
  btnRegister->setMinimumHeight(36);
  btnRegister->setStyleSheet(
      "QPushButton { background: #2d2b42; color: #E8E4FF; border: 1px solid rgba(124,92,252,0.45); "
      "border-radius: 8px; padding: 0 14px; font-weight: 600; }"
      "QPushButton:hover { background: #3a3754; }");
  QObject::connect(btnRegister, &QPushButton::clicked, dlg, [dlg]() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://blop-six.vercel.app/login")));
    dlg->close();
  });
  btnRow->addWidget(btnRegister);

  QPushButton *btnContinue = new QPushButton(tr("Weiter"), dlg);
  btnContinue->setCursor(Qt::PointingHandCursor);
  btnContinue->setMinimumHeight(36);
  btnContinue->setStyleSheet(
      "QPushButton { background: #3a3754; color: #E8E4FF; border: 1px solid #4d4a6c; "
      "border-radius: 8px; padding: 0 14px; font-weight: 600; }"
      "QPushButton:hover { background: #474367; }");
  QObject::connect(btnContinue, &QPushButton::clicked, dlg, &QDialog::close);
  btnRow->addWidget(btnContinue);

  lay->addLayout(btnRow);
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
#endif
}

void MainWindow::requestGoogleLogin() {
    GoogleAuthManager::instance().login();
}

void MainWindow::onSessionCheck(const QString &sessionData) {
    if (!sessionData.isEmpty() && sessionData != "null") {
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
    // Logged in: Switch to Blop Notes mode
    if (m_topNavControls) m_topNavControls->show();
    
    if (m_modeSelector) {
#ifdef Q_OS_ANDROID
      QSignalBlocker b(m_modeSelector);
#endif
      m_modeSelector->setCurrentIndex(0); // Switch to Notes mode
    }
#ifdef Q_OS_ANDROID
    onModeChanged(0);
    if (m_btnAndroidNotes) {
      m_btnAndroidNotes->setVisible(true);
      m_btnAndroidNotes->setEnabled(true);
    }
    if (m_btnAndroidStudy) {
      m_btnAndroidStudy->setVisible(true);
      m_btnAndroidStudy->setEnabled(true);
    }
#endif
    if (btnStripMenu)
      btnStripMenu->show();
    if (btnEditorMenu)
        btnEditorMenu->show(); // Show the Android Header menu when logged in

    // Ensure sidebar is closed (not double-visible)
    if (m_isSidebarOpen) {
      onToggleSidebar();
    }
  } else {
    // Logged out: Switch back to Study/Login web view
    if (m_topNavControls) m_topNavControls->hide();

    if (m_modeSelector) {
#ifdef Q_OS_ANDROID
      QSignalBlocker b(m_modeSelector);
#endif
      m_modeSelector->setCurrentIndex(1); // Force back to web login
      m_modeSelector->hide(); // Hide the selector completely to prevent bypass
    }
#ifdef Q_OS_ANDROID
    onModeChanged(1);
    // On auth screen the mode switch must not be usable.
    if (m_btnAndroidNotes) {
      m_btnAndroidNotes->setVisible(false);
      m_btnAndroidNotes->setEnabled(false);
    }
    if (m_btnAndroidStudy) {
      m_btnAndroidStudy->setVisible(false);
      m_btnAndroidStudy->setEnabled(false);
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

QRect MainWindow::sidebarPushContentRect() const {
  if (!m_centralContainer)
    return QRect(0, 0, SIDEBAR_WIDTH, height());
  const QPoint tl = m_centralContainer->mapTo(const_cast<MainWindow *>(this),
                                               QPoint(0, 0));
  int y = tl.y();
  int h = m_centralContainer->height();
#ifdef Q_OS_ANDROID
  // Avoid overlapping with Android bottom system/nav area.
  h -= 22;
#endif
#ifndef Q_OS_ANDROID
  if (m_titleBarWidget && m_titleBarWidget->isVisible()) {
    const int th = m_titleBarWidget->height();
    y = m_centralContainer->mapTo(const_cast<MainWindow *>(this), QPoint(0, th)).y();
    h -= th;
  }
#endif
  return QRect(tl.x(), y, SIDEBAR_WIDTH, h);
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
            qobject_cast<QVBoxLayout *>(m_centralContainer->layout()))
      mainLayout->setContentsMargins(0, 0, 0, 0);
#else
    if (m_desktopSidebarPushSpacer)
      m_desktopSidebarPushSpacer->setFixedWidth(0);
#endif
    return;
  }
  const int w = m_sidebarContainer->width();
#ifdef Q_OS_ANDROID
  if (QVBoxLayout *mainLayout =
          qobject_cast<QVBoxLayout *>(m_centralContainer->layout()))
    mainLayout->setContentsMargins(w, 0, 0, 0);
#else
  if (m_desktopSidebarPushSpacer)
    m_desktopSidebarPushSpacer->setFixedWidth(w);
#endif
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
  m_sidebarContainer->setStyleSheet("background-color: #0F111A;");
#endif

  QVBoxLayout *layout = new QVBoxLayout(m_sidebarContainer);
#ifdef Q_OS_ANDROID
  layout->setContentsMargins(6, 0, 6, 0);
#else
  layout->setContentsMargins(4, 0, 4, 0);
#endif

  layout->setSpacing(0);

  // --- HEADER: Blop Study style ---
  QWidget *header = new QWidget(m_sidebarContainer);
#ifdef Q_OS_ANDROID
  header->setFixedHeight(52);
#else
  header->setFixedHeight(62);
#endif
#ifdef Q_OS_ANDROID
  header->setStyleSheet("border-bottom: none; background: transparent;");
#else
  header->setStyleSheet("border-bottom: 1px solid #333;");
#endif
  QHBoxLayout *headerLay = new QHBoxLayout(header);
#ifdef Q_OS_ANDROID
  headerLay->setContentsMargins(12, 10, 12, 10);
  headerLay->setSpacing(8);
#else
  headerLay->setContentsMargins(12, 10, 12, 10);
  headerLay->setSpacing(8);
#endif

  // Logo box (oval, accent color)
  QLabel *lblLogo = new QLabel(header);
#ifdef Q_OS_ANDROID
  lblLogo->setFixedSize(32, 32);
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
  lblTitle->setStyleSheet("font-size: 15px; font-weight: bold; color: white; "
                          "background: transparent; border: none;");
#else
  lblTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: white; "
                          "background: transparent; border: none;");
#endif
  QLabel *lblSub = new QLabel("Notiz-App", header);
#ifdef Q_OS_ANDROID
  lblSub->setStyleSheet(
      "font-size: 9px; color: #888; background: transparent; border: none;");
#else
  lblSub->setStyleSheet(
      "font-size: 10px; color: #888; background: transparent; border: none;");
#endif
  titleCol->addWidget(lblTitle);
  titleCol->addWidget(lblSub);

  headerLay->addLayout(titleCol);
  headerLay->addStretch();

  m_closeSidebarBtn = new QPushButton("«", header);
  m_closeSidebarBtn->setFixedSize(24, 24);
  m_closeSidebarBtn->setFocusPolicy(Qt::NoFocus);
  m_closeSidebarBtn->setCursor(Qt::PointingHandCursor);
  m_closeSidebarBtn->setStyleSheet(
      "QPushButton { background: transparent; color: #888; border: none; "
      "font-size: 16px; outline: none; } QPushButton:hover { color: white; background: #333; "
      "outline: none; "
      "border-radius: 4px; }");
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
      item->setIcon(createModernIcon(icon, QColor(200, 200, 200)));
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
  bottomBar->setStyleSheet("QWidget#BottomBar { border-top: 1px solid #201E2E; }");
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
  m_lblSidebarUser->setStyleSheet(
      "font-size: 11px; font-weight: 600; color: white; "
      "background: transparent;");
  m_lblSidebarUser->setMaximumWidth(118);
#else
  m_lblSidebarUser->setStyleSheet(
      "font-size: 12px; font-weight: 600; color: white; "
      "background: transparent;");
  m_lblSidebarUser->setMaximumWidth(130);
#endif
  m_lblSidebarUser->setWordWrap(false);
  userCol->addWidget(m_lblSidebarUser);

  m_btnSidebarSettings = new QPushButton("Einstellungen", bottomBar);
  m_btnSidebarSettings->setFocusPolicy(Qt::NoFocus);
  m_btnSidebarSettings->setCursor(Qt::PointingHandCursor);
  m_btnSidebarSettings->setStyleSheet(
      "QPushButton { background: transparent; color: #888; border: none; "
      "font-size: 10px; padding: 0; text-align: left; } "
      "QPushButton:hover { color: #5E5CE6; }");
  connect(m_btnSidebarSettings, &QPushButton::clicked, this,
          &MainWindow::onOpenSettings);
  userCol->addWidget(m_btnSidebarSettings);

  QLabel *lblVersion = new QLabel("v" + QString(BLOP_VERSION), bottomBar);
  lblVersion->setStyleSheet(
      "font-size: 10px; color: #555; background: transparent; border: none;");
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
}

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
        child->setIcon(createModernIcon("folder", QColor(180, 180, 180)));
        child->setData(Qt::UserRole + 6, true);
        child->setData(Qt::UserRole + 3, false);
      } else {
        QPixmap pix(64, 64);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(180, 180, 180), 2));
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
  NewNoteDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    QString name = dlg.getNoteName();
    bool isInfinite = dlg.isInfiniteFormat();
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
    } else {
      // Modernes A4-Notizheft (.bnote -> MultiPageNoteView)
      QString path = m_fileModel->rootPath() + "/" + safeName + ".bnote";
      Note note;
      note.id = QUuid::createUuid().toString();
      note.title = name;
      note.pages.append(NotePage());
      NoteManager::saveNote(note, path);
      onFileDoubleClicked(m_fileModel->index(path));
    }
  }
}

void MainWindow::onCreateFolder() {
  bool ok;
  QString text = QInputDialog::getText(
      this, "New Folder", "Name:", QLineEdit::Normal, "New Folder", &ok);
  if (ok && !text.isEmpty()) {
    m_fileModel->mkdir(m_fileModel->index(m_fileModel->rootPath()), text);
  }
}

void MainWindow::onSidebarContextMenu(const QPoint &pos) {}

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
  m_rightSidebar = new QWidget(this);
  // Start fully collapsed; open only via explicit toggle.
  m_rightSidebar->setMinimumWidth(0);
  m_rightSidebar->setMaximumWidth(0);
  m_rightSidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  m_rightSidebar->setStyleSheet(
      "background-color: #0D0B14; border-left: 1px solid #201E2E;");
  m_rightSidebar->hide();

  QVBoxLayout *mainLayout = new QVBoxLayout(m_rightSidebar);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // =========================================================================
  // HEADER – Sidebar-Titel & Schließen-Button
  // =========================================================================
  QWidget *headerWidget = new QWidget(m_rightSidebar);
  headerWidget->setStyleSheet(
      "background: transparent;"
      "border-bottom: 1px solid rgba(255,255,255,0.07);");
  headerWidget->setFixedHeight(44);
  QHBoxLayout *header = new QHBoxLayout(headerWidget);
  header->setContentsMargins(16, 0, 8, 0);
  QLabel *sidebarTitle = new QLabel("Einstellungen", headerWidget);
  sidebarTitle->setStyleSheet(
      "color: rgba(255,255,255,0.85); font-size: 13px; font-weight: 600;"
      "background: transparent; border: none;");
  header->addWidget(sidebarTitle);
  header->addStretch();
  ModernButton *closeBtn = new ModernButton(headerWidget);
  closeBtn->setIcon(createModernIcon("close", QColor("#888")));
  closeBtn->setFixedSize(28, 28);
  connect(closeBtn, &QAbstractButton::clicked, this,
          &MainWindow::onToggleRightSidebar);
  header->addWidget(closeBtn);
  mainLayout->addWidget(headerWidget);

  // =========================================================================
  // NOTIZNAME
  // =========================================================================
  m_lblActiveNote = new QLabel("", m_rightSidebar);
  m_lblActiveNote->setStyleSheet(
      "color: rgba(255,255,255,0.80); font-size: 12px; font-weight: 600;"
      "padding: 12px 16px 8px 16px; background: transparent;");
  m_lblActiveNote->setWordWrap(true);
  mainLayout->addWidget(m_lblActiveNote);

  // =========================================================================
  // TAB WIDGET (Optionen vs Tags)
  // =========================================================================
  QTabWidget *settingsTabs = new QTabWidget(m_rightSidebar);
  settingsTabs->setStyleSheet(
      "QTabWidget::pane { border: none; border-top: 1px solid rgba(255,255,255,0.05); background: #0D0B14; }"
      "QTabWidget > QWidget { background: #0D0B14; }"
      "QTabBar::tab {"
      "  background: transparent;"
      "  color: rgba(255,255,255,0.45);"
      "  padding: 8px 16px;"
      "  font-size: 11px; font-weight: 600;"
      "  border: none;"
      "}"
      "QTabBar::tab:selected {"
      "  color: #DDD8FF;"
      "  border-bottom: 2px solid #7C5CFC;"
      "}"
      "QTabBar::tab:hover:!selected {"
      "  color: rgba(255,255,255,0.8);"
      "}");
  mainLayout->addWidget(settingsTabs);

  // -------------------------------------------------------------------------
  // TAB 1: OPTIONEN (Formatierung, Input, Profile)
  // -------------------------------------------------------------------------
  QWidget *tabOptions = new QWidget();
  tabOptions->setStyleSheet("background: #0D0B14;");
  QVBoxLayout *optLayoutMain = new QVBoxLayout(tabOptions);
  optLayoutMain->setContentsMargins(0, 0, 0, 0);

  QScrollArea *optScroll = new QScrollArea(tabOptions);
  optScroll->setWidgetResizable(true);
  optScroll->setFrameShape(QFrame::NoFrame);
  optScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  optScroll->setStyleSheet(
      "QScrollArea { background: transparent; } QScrollBar { width: 4px; "
      "background: transparent; } QScrollBar::handle { background: #333; "
      "border-radius: 2px; }");

  QWidget *optContent = new QWidget();
  optContent->setStyleSheet("background: transparent;");
  QVBoxLayout *optLayout = new QVBoxLayout(optContent);
  optLayout->setContentsMargins(16, 12, 16, 20);
  optLayout->setSpacing(15);

  auto sectionLabel = [&](const QString &text) -> QLabel* {
    QLabel *lbl = new QLabel(text, optContent);
    lbl->setStyleSheet(
        "color: rgba(255,255,255,0.40); font-size: 10px; font-weight: 700;"
        "letter-spacing: 0.5px; background: transparent;");
    return lbl;
  };

  // --- Seiten-Optionen ---
  optLayout->addWidget(sectionLabel("SEITE"));

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

  optLayout->addWidget(new QLabel("Toolbar Style:", optContent));
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

  optLayout->addWidget(new QLabel("Toolbar Size:", optContent));
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
  tabTags->setStyleSheet("background: #0D0B14;");
  QVBoxLayout *tagsLayoutMain = new QVBoxLayout(tabTags);
  tagsLayoutMain->setContentsMargins(16, 16, 16, 20);
  tagsLayoutMain->setSpacing(16);

  // Tags Section
  tagsLayoutMain->addWidget(sectionLabel("QUICK-TAGS"));
  
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
  tagsLayoutMain->addWidget(sectionLabel("NOTIZ INFO"));

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
}

void MainWindow::updateInputMode(bool penOnly) {
  m_penOnlyMode = penOnly;
  QWidget *current = m_editorTabs->currentWidget();
  if (auto *cv = qobject_cast<CanvasView *>(current)) {
    cv->setPenMode(penOnly);
  } else if (auto *editor = qobject_cast<NoteEditor *>(current)) {
    if (editor->view())
      editor->view()->setPenOnlyMode(penOnly);
  }
}

void MainWindow::onPageStyleButtonToggled(QAbstractButton *button,
                                          bool checked) {
  if (!checked)
    return;
  PageStyle style = (PageStyle)m_grpPageStyle->id(button);
  QWidget *current = m_editorTabs->currentWidget();
  if (auto *cv = qobject_cast<CanvasView *>(current)) {
    cv->setPageStyle(style);
    cv->setGridSize(m_sliderGridSpacing->value());
  }
}

void MainWindow::onPageGridSpacingSliderChanged(int value) {
  m_gridSpacingTimer->start();
}
void MainWindow::applyDelayedGridSpacing() {
  QWidget *current = m_editorTabs->currentWidget();
  if (auto *cv = qobject_cast<CanvasView *>(current)) {
    cv->setGridSize(m_sliderGridSpacing->value());
  }
}

void MainWindow::animateSidebar(bool show) {
  QRect r = sidebarPushContentRect();
  int containerHeight = r.height();
  if (containerHeight <= 0)
    containerHeight = qMax(100, height() - r.y());

  if (show) {
    m_isSidebarOpen = true;
    m_sidebarContainer->setGeometry(r.x(), r.y(), 0, containerHeight);
    m_sidebarContainer->raise();
    m_sidebarContainer->show();
  } else {
    m_sidebarContainer->raise();
  }

  QVariantAnimation *anim = new QVariantAnimation(this);
  anim->setDuration(280);
  anim->setEasingCurve(QEasingCurve::OutCubic);
  anim->setStartValue(show ? 0 : SIDEBAR_WIDTH);
  anim->setEndValue(show ? SIDEBAR_WIDTH : 0);
  connect(anim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant &v) {
            const int w = v.toInt();
            QRect r2 = sidebarPushContentRect();
            int h2 = r2.height();
            if (h2 <= 0)
              h2 = qMax(100, height() - r2.y());
#ifdef Q_OS_ANDROID
            if (QVBoxLayout *lay =
                    qobject_cast<QVBoxLayout *>(m_centralContainer->layout()))
              lay->setContentsMargins(w, 0, 0, 0);
#else
            if (m_desktopSidebarPushSpacer)
              m_desktopSidebarPushSpacer->setFixedWidth(w);
#endif
            if (m_sidebarContainer)
              m_sidebarContainer->setGeometry(r2.x(), r2.y(), w, h2);
            // Keep docked toolbar fully visible while sidebar animates.
            if (m_floatingTools) {
              if (ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools)) {
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
    } else {
      m_isSidebarOpen = false;
      if (m_sidebarContainer)
        m_sidebarContainer->hide();
#ifdef Q_OS_ANDROID
      if (QVBoxLayout *lay =
              qobject_cast<QVBoxLayout *>(m_centralContainer->layout()))
        lay->setContentsMargins(0, 0, 0, 0);
#else
      if (m_desktopSidebarPushSpacer)
        m_desktopSidebarPushSpacer->setFixedWidth(0);
#endif
    }
#ifdef Q_OS_ANDROID
    if (m_androidHeader)
      m_androidHeader->raise();
#endif
    updateSidebarState();
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
#ifdef Q_OS_ANDROID
  if (m_androidHeader)
    m_androidHeader->raise();
#endif
}
void MainWindow::onToggleSidebar() {
#ifdef Q_OS_ANDROID
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
  if (m_btnMode)
    m_btnMode->setVisible(!isEditor);
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
  // Overview: floating menu next to welcome; Editor: compact hamburger in top bar
  if (isEditor) {
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->hide();
    if (m_btnAndroidToolbarMenu)
      m_btnAndroidToolbarMenu->setVisible(!m_isSidebarOpen);
    if (m_btnAndroidToolbarSettings)
      m_btnAndroidToolbarSettings->show();
    bool showPageManager = false;
    if (m_editorTabs) {
      if (auto *editor =
              qobject_cast<NoteEditor *>(m_editorTabs->currentWidget())) {
        if (editor->view())
          showPageManager = true;
      }
    }
    if (m_btnAndroidToolbarPageManager) {
      m_btnAndroidToolbarPageManager->setVisible(showPageManager);
    }
    if (m_btnAndroidToolbarExport)
      m_btnAndroidToolbarExport->show();

    if (shouldMorphTopButtons) {
      auto animateScale = [](ModernButton *btn) {
        if (!btn)
          return;
        btn->setButtonScale(0.70);
        btn->update();
        QPropertyAnimation *a =
            new QPropertyAnimation(btn, "buttonScale", btn);
        a->setDuration(220);
        a->setStartValue(0.70);
        a->setEndValue(1.0);
        a->setEasingCurve(QEasingCurve::OutBack);
        a->start(QAbstractAnimation::DeleteWhenStopped);
      };
      animateScale(m_btnAndroidToolbarSettings);
      animateScale(m_btnAndroidToolbarExport);
      if (showPageManager)
        animateScale(m_btnAndroidToolbarPageManager);
    }
  } else {
    m_sidebarStrip->show();
    if (btnEditorMenu)
      btnEditorMenu->show();
    if (m_btnAndroidToolbarMenu)
      m_btnAndroidToolbarMenu->hide();
    if (m_btnAndroidToolbarSettings)
      m_btnAndroidToolbarSettings->hide();
    if (m_btnAndroidToolbarPageManager)
      m_btnAndroidToolbarPageManager->hide();
    if (m_btnAndroidToolbarExport)
      m_btnAndroidToolbarExport->hide();
    if (m_rightSidebar && m_rightSidebar->isVisible())
      animateRightSidebar(false);
  }
#else
  if (isEditor) {
    m_sidebarStrip->hide();
    if (btnEditorMenu)
      btnEditorMenu->show();
  } else {
    m_sidebarStrip->show();
    if (btnEditorMenu)
      btnEditorMenu->hide();
  }
#endif

  m_lastIsEditor = isEditor;
}

void MainWindow::animateRightSidebar(bool show) {
  const int targetWidth = 280;
  int start = show ? 0 : targetWidth;
  int end = show ? targetWidth : 0;
  m_rightSidebar->setMinimumWidth(0);
  m_rightSidebar->setMaximumWidth(show ? 0 : targetWidth);
  m_rightSidebar->setVisible(true);
  QPropertyAnimation *anim =
      new QPropertyAnimation(m_rightSidebar, "maximumWidth");
  anim->setDuration(220);
  anim->setStartValue(start);
  anim->setEndValue(end);
  anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(anim, &QPropertyAnimation::finished, [this, show]() {
    if (!show)
      m_rightSidebar->hide();
    else
      m_rightSidebar->setMaximumWidth(targetWidth);
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::onToggleRightSidebar() {
  bool isVisible = m_rightSidebar->isVisible() && m_rightSidebar->width() > 0;
  animateRightSidebar(!isVisible);
  if (!isVisible) {
    QWidget *current = m_editorTabs->currentWidget();
    CanvasView *cv = qobject_cast<CanvasView *>(current);
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
}

void MainWindow::onTogglePageManager() {
  if (m_pageManager->isVisible()) {
    m_pageManager->hide();
  } else {
    m_pageManager->resize(240, m_editorCenterWidget->height());
    m_pageManager->move(m_editorCenterWidget->width() - 240, 0);
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
      connect(m_pageManager, &PageManager::pageSelected, mpv,
              &MultiPageNoteView::scrollToPage);
      connect(m_pageManager, &PageManager::pageOrderChanged, [mpv, this]() {
        if (mpv->onSaveRequested && mpv->note())
          mpv->onSaveRequested(mpv->note());
      });
      m_pageManager->show();
      m_pageManager->refreshThumbnails();
    } else {
      QMessageBox::information(
          this, "Nicht verfügbar",
          "Der Seitenmanager ist nur für A4-Notizen verfügbar.");
    }
  }
}

void MainWindow::onFileDoubleClicked(const QModelIndex &index) {
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

      // Floating 3-dot note action button (top-right, pinned via Move)
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
          auto *actPdf = menu->addAction("\U00002714  Als PDF exportieren");
          auto *actImg = menu->addAction("\U0001F5BC  Als Bild exportieren");
          auto *chosen = menu->exec(QCursor::pos());
          QFileInfo fi(capPath);
          if (chosen == actPdf) {
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
          }
      });

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
          m_editorTabs->addTab(editor, fileName);
          m_editorTabs->setCurrentWidget(editor);
          addNoteTab(QFileInfo(fileName).baseName());
        }
      }
    }
    m_rightStack->setCurrentWidget(m_editorContainer);
    setActiveTool(m_activeToolType);
    updateSidebarState();
  }
}

void MainWindow::onBackToOverview() {
  m_rightStack->setCurrentWidget(m_overviewContainer);
  updateSidebarState();
}
void MainWindow::showContextMenu(const QPoint &globalPos,
                                 const QModelIndex &index) {
  QMenu menu(this);
  menu.addAction("Open", [this, index]() { onFileDoubleClicked(index); });
  menu.addAction("Rename", [this, index]() { startRename(index); });
  menu.addAction("Delete", [this, index]() { m_fileModel->remove(index); });
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
  if (m_androidHeader)
    m_androidHeader->raise();
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
  fabSize = FAB_SIZE_ANDROID;
  bottomOffset = FAB_DISTANCE_FROM_BOTTOM;
  syncSidebarPushLayout();
  if (m_androidHeader)
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
    ModernToolbar *tb = qobject_cast<ModernToolbar *>(m_floatingTools);
    if (tb) {
      tb->setTopBound(0);
      // Let the toolbar govern its own dragged docking position seamlessly.
    }
  }
  if (m_pageManager && m_pageManager->isVisible()) {
    m_pageManager->resize(240, m_editorCenterWidget->height());
    m_pageManager->move(m_editorCenterWidget->width() - 240, 0);
  }
}

void MainWindow::onOpenSettings() {
  // Close sidebar so modal overlays receive mouse/touch reliably.
  if (m_isSidebarOpen)
    onToggleSidebar();

  QWidget *overlay = new QWidget(this);
  overlay->setGeometry(this->rect());
  overlay->setStyleSheet("background-color: rgba(0, 0, 0, 150);");
  overlay->setAttribute(Qt::WA_DeleteOnClose);
  overlay->show();
  overlay->raise();

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

  int res = dlg.exec();
  if (res == SettingsDialog::EditProfileCode) {
    QString id = dlg.profileIdToEdit();
    UiProfile p = m_profileManager->profileById(id);
    UiProfile original = p;
    ProfileEditorDialog editor(p, this);
    connect(&editor, &ProfileEditorDialog::previewRequested, this,
            &MainWindow::applyProfile);
    if (editor.exec() == QDialog::Accepted) {
      m_profileManager->updateProfile(editor.getProfile(), true);
      applyProfile(editor.getProfile());
    } else {
      m_profileManager->updateProfile(original, true);
      applyProfile(m_profileManager->currentProfile());
    }
  }

  if (overlay) {
    overlay->close();
  }
}

void MainWindow::setPageColor(bool dark) {
  QWidget *current = m_editorTabs->currentWidget();
  if (auto *cv = qobject_cast<CanvasView *>(current)) {
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
  CanvasView *cv = qobject_cast<CanvasView *>(current);
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

  // Sidebar verbergen, wenn keine Notiz geöffnet ist
  if (index < 0 && m_rightSidebar && m_rightSidebar->isVisible()) {
    m_rightSidebar->hide();
    m_rightSidebar->setMinimumWidth(0);
    m_rightSidebar->setMaximumWidth(0);
  }

#ifdef Q_OS_ANDROID
  // Keep Android editor clean by default; user opens settings explicitly.
  if (m_rightSidebar && m_rightSidebar->isVisible()) {
    animateRightSidebar(false);
  }
#endif

  if (m_btnPages) {
    m_btnPages->setVisible(editor != nullptr);
  }
  if (editor == nullptr && m_pageManager && m_pageManager->isVisible()) {
    m_pageManager->hide();
  }
  updateSidebarState();
}


void MainWindow::restoreWindowState() {
  QSettings settings("Blop", "BlopApp");
  
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
  QSettings settings("Blop", "BlopApp");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  QMainWindow::closeEvent(event);
}

void MainWindow::onShareClicked() {
  CanvasView *cv = getCurrentCanvas();
  if (!cv) {
    QMessageBox::information(this, "Export", "Bitte öffne zuerst eine Notiz.");
    return;
  }

  QMenu menu(this);
  menu.setStyleSheet(
      "QMenu { background: #1E1E1E; border: 1px solid #444; border-radius: 6px; padding: 4px; }"
      "QMenu::item { color: #DDD; padding: 8px 16px; border-radius: 4px; font-weight: 500; font-size: 13px; }"
      "QMenu::item:selected { background: #5E5CE6; color: white; }");

  QAction *actPdf = menu.addAction(" Als PDF exportieren");
  QAction *actImg = menu.addAction(" Als Bild exportieren");

  QPoint showPos = QCursor::pos();
  if (m_btnPages) {
    showPos = m_btnPages->mapToGlobal(QPoint(0, m_btnPages->height() + 4));
  }

  QAction *selected = menu.exec(showPos);
  if (!selected) return;

  QString defaultName = "Neue Notiz";
  if (m_lblActiveNote && !m_lblActiveNote->text().isEmpty()) {
    defaultName = m_lblActiveNote->text();
  }

  if (selected == actPdf) {
    QString path = QFileDialog::getSaveFileName(this, "Als PDF exportieren", defaultName + ".pdf", "PDF Dokument (*.pdf)");
    if (!path.isEmpty()) {
      bool ok = cv->exportToPDF(path);
      if (ok) QMessageBox::information(this, "Erfolg", "Erfolgreich als PDF exportiert!");
      else QMessageBox::warning(this, "Fehler", "PDF konnte nicht gespeichert werden.");
    }
  } else if (selected == actImg) {
    QString path = QFileDialog::getSaveFileName(this, "Als Bild exportieren", defaultName + ".png", "Bilder (*.png *.jpg)");
    if (!path.isEmpty()) {
      bool ok = cv->exportToImage(path);
      if (ok) QMessageBox::information(this, "Erfolg", "Erfolgreich als Bild exportiert!");
      else QMessageBox::warning(this, "Fehler", "Bild konnte nicht gespeichert werden.");
    }
  }
}

void MainWindow::showAuthOverlay(const QUrl &url) {
  // Always use the system browser for OAuth. Embedded QWebEngineView in a dialog used the same
  // Chromium stack as Study — on many Windows installs it stays black; the redirect to
  // http://127.0.0.1:8080/ is still handled by QOAuthHttpServerReplyHandler.
  QDesktopServices::openUrl(url);
}
