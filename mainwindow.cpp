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
#include <QButtonGroup>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
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
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>
#include <utility>
#include <QCloseEvent>

#ifdef Q_OS_ANDROID
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QTemporaryFile>
#include <QWidget>
#else
#ifdef BLOP_HAS_WEBENGINE
#include <QtWebEngineWidgets/QWebEngineView>
#endif
#endif

// ============================================================================
// KONFIGURATION FÜR ANDROID SCALING & MARGINS
// ============================================================================
#ifdef Q_OS_ANDROID
static const int SIDEBAR_WIDTH = 280;
static const int ROW_HEIGHT_HEADER = 50;
static const int ROW_HEIGHT_ITEM = 64;
static const int FONT_SIZE_BASE = 16;
static const int FONT_SIZE_HEADER = 22;
static const int MARGIN_ANDROID_TOP = 50;
static const int MARGIN_ANDROID_SIDE = 16;
static const int FAB_SIZE_ANDROID = 56;
static const int FAB_DISTANCE_FROM_BOTTOM = 30;
#else
static const int SIDEBAR_WIDTH = 250;
static const int ROW_HEIGHT_HEADER = 40;
static const int ROW_HEIGHT_ITEM = 44;
static const int FONT_SIZE_BASE = 10;
static const int FONT_SIZE_HEADER = 18;
static const int MARGIN_OVERVIEW = 30;
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

  bool isHeader = index.data(Qt::UserRole + 1).toBool();
  bool isExpandable = index.data(Qt::UserRole + 6).toBool();
  int depth = index.data(Qt::UserRole + 9).toInt();
  int indent = 15 * depth;

  QString text = index.data(Qt::DisplayRole).toString();

  if (isHeader) {
    painter->setPen(QColor("#888888"));
    QFont f = m_window->font();
    f.setBold(true);
    f.setPointSize(FONT_SIZE_BASE - 2);
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
    QRect rect = option.rect.adjusted(8 + indent, 4, -8, -4);
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
    iconSize = 32;
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
    f.setPointSize(FONT_SIZE_BASE);
    if (selected)
      f.setBold(true);
    painter->setFont(f);

    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight,
                                                        textRect.width()));

    QString count = index.data(Qt::UserRole + 2).toString();
    if (!count.isEmpty()) {
      int badgeW = 30;
      int badgeH = 20;
      QRect badgeRect(rect.right() - badgeW - 5, rect.center().y() - badgeH / 2,
                      badgeW, badgeH);
      painter->setPen(selected ? Qt::white : m_window->currentAccentColor());
      QFont bf = f;
      bf.setBold(true);
      bf.setPointSize(FONT_SIZE_BASE - 2);
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
  setIconSize(QSize(24, 24));
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
  m_penOnlyMode = false; // Allow mouse input for tools by default; touch panning is still handled separately
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

  // --- Startup Mode: Always show the Study/Login web view first ---
  // After web login, the SSO bridge (ssoTimer in setupWebBrowser) detects the
  // session and calls updateSidebarUser(), which switches us to Notes mode.
  if (m_modeSelector) {
    m_modeSelector->setCurrentIndex(1); // Always start in Study/Login mode
  }
  // Update sidebar to show saved username if available
  QString savedUser = QSettings("Blop", "BlopApp").value("username").toString();
  if (!savedUser.isEmpty()) {
    updateSidebarUser(savedUser);
  }

  QTimer::singleShot(100, this, &MainWindow::updateGrid);
  // Delay update check by 5s so the web view has time to render before the
  // modal blocks
  QTimer::singleShot(5000, this, &MainWindow::checkForUpdates);
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
  connect(btnMode, &QPushButton::clicked, this,
          [this]() { m_modeSelector->showPopup(); });
  connect(m_modeSelector, &QComboBox::currentIndexChanged,
          [btnMode, this](int idx) {
            btnMode->setText(m_modeSelector->itemText(idx) + "  ▾");
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
        tb->setGeometry((m_editorCenterWidget->width() - idealW) / 2, 0, idealW, 48);
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
  if (m_fileListView)
    m_fileListView->setAccentColor(m_currentAccentColor);

  // Blop Notes Redesign (Etappe 1): #0D0B14 Main, #14121F Sidebar
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
          "QMenu::item:selected { background-color: %2; color: white; }")
          .arg(c, c_light);
  this->setStyleSheet(style);

  if (m_sidebarContainer)
    m_sidebarContainer->setStyleSheet(
        "background-color: #14121F; border-right: "
        "1px solid #201E2E;");
  if (m_sidebarStrip)
    m_sidebarStrip->setStyleSheet(
        "background-color: #14121F; border-right: 1px solid #201E2E;");
  if (m_navSidebar)
    m_navSidebar->setStyleSheet(
        "QListWidget { background-color: transparent; border: none; outline: "
        "0; margin-left: 5px; margin-right: 5px; } QListWidget::item { border: "
        "none; }");

  if (m_rightSidebar)
    m_rightSidebar->setStyleSheet(
        "background-color: #0D0B14; border-left: 1px solid #201E2E;");

  if (m_titleBarWidget)
    m_titleBarWidget->setStyleSheet(
        "background-color: #0D0B14; border-bottom: 1px solid #201E2E;");
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
  int screenWidth = m_fileListView->width();
  if (screenWidth <= 0)
    screenWidth = QApplication::primaryScreen()->availableGeometry().width();

  int columns = 2;
  if (screenWidth > 600)
    columns = 4;
  if (screenWidth > 900)
    columns = 6;

  int spacing = 15;
  m_fileListView->setSpacing(spacing);

  int totalSpacing = (columns + 1) * spacing;
  int itemWidth = (screenWidth - totalSpacing) / columns;
  int itemHeight = itemWidth + 40;

  m_fileListView->setItemSize(QSize(itemWidth, itemHeight));
  m_fileListView->setIconSize(QSize(itemWidth - 20, itemWidth - 20));
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
  int iconSize = btnSize - 16;
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
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(28, 14, 8, 8);
    p.drawEllipse(28, 28, 8, 8);
    p.drawEllipse(28, 42, 8, 8);
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
    p.drawEllipse(20, 20, 24, 24);
    p.drawLine(32, 10, 32, 20);
    p.drawLine(32, 44, 32, 54);
    p.drawLine(10, 32, 20, 32);
    p.drawLine(44, 32, 54, 32);
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
    p.drawRect(14, 14, 28, 36);
    p.drawRect(22, 22, 28, 36);
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
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

#ifdef Q_OS_ANDROID
  QWidget *androidHeader = new QWidget(this);
  androidHeader->setFixedHeight(60);
  androidHeader->setStyleSheet(
      "background-color: #0F111A; border-bottom: 1px solid #1F2335;");
  QHBoxLayout *headerLay = new QHBoxLayout(androidHeader);
  headerLay->setContentsMargins(15, 10, 15, 10);

  QLabel *lblLogo = new QLabel("Blop", androidHeader);
  lblLogo->setStyleSheet("color: white; font-weight: bold; font-size: 18px;");
  headerLay->addWidget(lblLogo);

  m_modeSelector = new QComboBox(androidHeader);
  m_modeSelector->addItem("Notizen");
  m_modeSelector->addItem("Study");
  m_modeSelector->setStyleSheet(
      "QComboBox { background-color: #151525; color: white; border: 1px solid "
      "#2A2A40; border-radius: 4px; padding: 5px 10px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background-color: #151525; color: white; "
      "selection-background-color: #5E5CE6; }");
  connect(m_modeSelector, &QComboBox::currentIndexChanged,
          this, &MainWindow::onModeChanged);
  headerLay->addWidget(m_modeSelector);
  headerLay->addStretch();

  mainLayout->addWidget(androidHeader);
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
  // KEIN installEventFilter - damit Klicks auf Buttons korrekt weitergeleitet werden!
  QVBoxLayout *overviewLayout = new QVBoxLayout(m_overviewContainer);
#ifdef Q_OS_ANDROID
  overviewLayout->setContentsMargins(MARGIN_ANDROID_SIDE, 20,
                                     MARGIN_ANDROID_SIDE, 120);
#else
  overviewLayout->setContentsMargins(MARGIN_OVERVIEW, MARGIN_OVERVIEW,
                                     MARGIN_OVERVIEW, MARGIN_OVERVIEW);
#endif

  QHBoxLayout *topBar = new QHBoxLayout();
  btnOverviewMenu = new ModernButton(this);
  btnOverviewMenu->setIcon(createModernIcon("menu", Qt::white));
  connect(btnOverviewMenu, &QAbstractButton::clicked, this,
          &MainWindow::onToggleSidebar);
  topBar->addWidget(btnOverviewMenu);
  btnBackOverview = new ModernButton(this);
  btnBackOverview->setIcon(createModernIcon("arrow_left", Qt::white));
  btnBackOverview->hide();
  connect(btnBackOverview, &QAbstractButton::clicked, this,
          &MainWindow::onNavigateUp);
  topBar->addWidget(btnBackOverview);
  topBar->addStretch();
  overviewLayout->addLayout(topBar);

  // --- NEW HEADER BEREICH (Blop Notes Redesign Etappe 3) ---
  QVBoxLayout *headerLayout = new QVBoxLayout();
  headerLayout->setContentsMargins(10, 20, 10, 30);
  headerLayout->setSpacing(15);
  
  QLabel *lblWelcome = new QLabel("Willkommen zurück!", m_overviewContainer);
  lblWelcome->setStyleSheet("color: white; font-size: 28px; font-weight: bold; font-family: 'Segoe UI';");
  headerLayout->addWidget(lblWelcome);

  QHBoxLayout *searchActionLayout = new QHBoxLayout();
  searchActionLayout->setSpacing(15);

  QLineEdit *searchBar = new QLineEdit(m_overviewContainer);
  searchBar->setPlaceholderText("Notizen durchsuchen...");
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
  overviewLayout->addLayout(headerLayout);
  // --------------------------------------------------------

  m_fileListView = new FreeGridView(this);
  m_fileListView->setModel(m_fileModel);
  m_fileListView->setRootIndex(m_fileModel->index(m_rootPath));
  m_fileListView->setSpacing(20);
  m_fileListView->setFrameShape(QFrame::NoFrame);
  m_fileListView->setItemDelegate(new ModernItemDelegate(this));
  QScroller::grabGesture(m_fileListView, QScroller::LeftMouseButtonGesture);
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

  mainLayout->addWidget(m_mainContentStack);

  setWindowTitle("Blop");
  
  // Force authentication state check at startup so the user is locked into Anmeldescreen if empty
  QString currentUser = QSettings("Blop", "BlopApp").value("username").toString();
  updateSidebarUser(currentUser);
  
  qDebug() << "setupUi() Ende";
}

void MainWindow::setupWebBrowser() {
  m_studyContainer = new QWidget(this);
  // Dark background so it's not pitch-black while WebEngine loads
  m_studyContainer->setStyleSheet("background-color: #1e1e1e;");
  QVBoxLayout *layout = new QVBoxLayout(m_studyContainer);
  layout->setContentsMargins(0, 0, 0, 0);

#ifdef Q_OS_ANDROID
  QQuickView *view = new QQuickView();
  view->setResizeMode(QQuickView::SizeRootObjectToView);

  QString tempPath =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      "/blop_webview.qml";
  QFile f(tempPath);
  if (f.open(QIODevice::WriteOnly)) {
    f.write(R"(
            import QtQuick 2.0
            import QtWebView 1.1
            Item {
                WebView {
                    anchors.fill: parent
                    url: "https://blop-six.vercel.app"
                }
            }
        )");
    f.close();
    view->setSource(QUrl::fromLocalFile(tempPath));
  } else {
    QLabel *err =
        new QLabel("Fehler: Konnte Web-Modul nicht laden.", m_studyContainer);
    layout->addWidget(err);
    return;
  }

  QWidget *container = QWidget::createWindowContainer(view, m_studyContainer);
  layout->addWidget(container);

#else
#ifdef BLOP_HAS_WEBENGINE
  QWebEngineView *view = new QWebEngineView(m_studyContainer);
  view->setStyleSheet("background: transparent;");
  // FIX: FramelessWindowHint + QWebEngineView auf Windows braucht WA_NativeWindow,
  // damit der interne Chromium-HWND korrekte Mouse-Events bekommt (sonst Glasscheibe).
  view->setAttribute(Qt::WA_NativeWindow);

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

  view->load(QUrl("https://blop-six.vercel.app"));
  layout->addWidget(view);

  // --- SSO Bridge: Poll localStorage to support SPA Routing ---
  // In Next.js, navigating to Dashboard after login doesn't trigger
  // loadFinished, so we must poll localStorage to detect login state changes.
  QTimer *ssoTimer = new QTimer(m_studyContainer);
  connect(ssoTimer, &QTimer::timeout, this, [this, view]() {
    view->page()->runJavaScript(
        R"js(
          (function() {
            var u = localStorage.getItem('username');
            var s = localStorage.getItem('session_id');
            return (u && s) ? u : '';
          })();
        )js",
        [this](const QVariant &result) {
          QString u = result.toString().trimmed();
          QString currentUser =
              QSettings("Blop", "BlopApp").value("username").toString();
          if (u != currentUser) {
            updateSidebarUser(u);
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

  m_btnLogin = new QPushButton(" Mit Google Desktop-Login anmelden", m_studyContainer);
  m_btnLogin->setIcon(QIcon(":/icons/google_logo.svg")); // optional
  m_btnLogin->setCursor(Qt::PointingHandCursor);
  m_btnLogin->setFixedHeight(48);
  m_btnLogin->setStyleSheet(
      "QPushButton {"
      "  background-color: #4285F4;"
      "  color: white;"
      "  border-radius: 0px;"
      "  font-weight: bold;"
      "  font-size: 14px;"
      "  border: none;"
      "}"
      "QPushButton:hover { background-color: #5a9df8; }"
  );
  connect(m_btnLogin, &QPushButton::clicked, this, []() {
      GoogleAuthManager::instance().login();
  });
  
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::userInfoUpdated, this, [this]() {
      if (m_btnLogin) m_btnLogin->setText(" " + GoogleAuthManager::instance().userName());
  });
  layout->addWidget(m_btnLogin);

}

void MainWindow::onModeChanged(int index) {
  if (m_mainContentStack) {
    m_mainContentStack->setCurrentIndex(index);
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
    if (m_btnLogin) m_btnLogin->hide();
    
    if (m_modeSelector) {
      m_modeSelector->setCurrentIndex(0); // Switch to Notes mode
    }
    if (btnStripMenu)
      btnStripMenu->show();

    // Ensure sidebar is closed (not double-visible)
    if (m_isSidebarOpen) {
      onToggleSidebar();
    }
  } else {
    // Logged out: Switch back to Study/Login web view
    if (m_topNavControls) m_topNavControls->hide();
    if (m_btnLogin) m_btnLogin->show();

    if (m_modeSelector) {
      m_modeSelector->setCurrentIndex(1); // Force back to web login
    }
    if (btnStripMenu)
      btnStripMenu->hide(); // Hide the sidebar hamburger when logged out to fully trap user in login

    if (m_isSidebarOpen)
      onToggleSidebar();
  }
  
  // Re-sync sidebar strip/full sidebar visibility after mode switch
  updateSidebarState();
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
  m_sidebarContainer->setFixedWidth(SIDEBAR_WIDTH);

  QVBoxLayout *layout = new QVBoxLayout(m_sidebarContainer);

#ifdef Q_OS_ANDROID
  layout->setContentsMargins(0, MARGIN_ANDROID_TOP, 0, 0);
#else
  layout->setContentsMargins(0, 0, 0, 0);
#endif

  layout->setSpacing(0);

  // --- HEADER: Blop Study style ---
  QWidget *header = new QWidget(m_sidebarContainer);
  header->setFixedHeight(74);
  header->setStyleSheet("border-bottom: 1px solid #333;");
  QHBoxLayout *headerLay = new QHBoxLayout(header);
  headerLay->setContentsMargins(16, 16, 16, 16);
  headerLay->setSpacing(10);

  // Logo box (oval, accent color)
  QLabel *lblLogo = new QLabel(header);
  lblLogo->setFixedSize(38, 38);
  lblLogo->setAlignment(Qt::AlignCenter);
  lblLogo->setStyleSheet(
      "background-color: #5E5CE6; border-radius: 10px; color: white; "
      "font-weight: bold; font-size: 14px;");
  lblLogo->setText("B");
  headerLay->addWidget(lblLogo);

  QVBoxLayout *titleCol = new QVBoxLayout();
  titleCol->setSpacing(2);
  QLabel *lblTitle = new QLabel("Blop", header);
  lblTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: white; "
                          "background: transparent;");
  QLabel *lblSub = new QLabel("Notiz-App", header);
  lblSub->setStyleSheet(
      "font-size: 10px; color: #888; background: transparent;");
  titleCol->addWidget(lblTitle);
  titleCol->addWidget(lblSub);
  headerLay->addLayout(titleCol);
  headerLay->addStretch();

  m_closeSidebarBtn = new QPushButton("«", header);
  m_closeSidebarBtn->setFixedSize(24, 24);
  m_closeSidebarBtn->setCursor(Qt::PointingHandCursor);
  m_closeSidebarBtn->setStyleSheet(
      "QPushButton { background: transparent; color: #888; border: none; "
      "font-size: 16px; } QPushButton:hover { color: white; background: #333; "
      "border-radius: 4px; }");
  connect(m_closeSidebarBtn, &QPushButton::clicked, this,
          &MainWindow::onToggleSidebar);
  headerLay->addWidget(m_closeSidebarBtn);
  layout->addWidget(header);

  m_navSidebar = new QListWidget(m_sidebarContainer);
  m_navSidebar->setItemDelegate(new SidebarNavDelegate(this));
  m_navSidebar->setFrameShape(QFrame::NoFrame);
  m_navSidebar->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  QScroller::grabGesture(m_navSidebar, QScroller::LeftMouseButtonGesture);

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
  bottomBar->setStyleSheet("QWidget#BottomBar { border-top: 1px solid #201E2E; }");

#ifdef Q_OS_ANDROID
  bottomBar->setFixedHeight(80);
#else
  bottomBar->setFixedHeight(72);
#endif

  QHBoxLayout *bottomLay = new QHBoxLayout(bottomBar);
  bottomLay->setContentsMargins(14, 10, 14, 14);
  bottomLay->setSpacing(10);

  // Read username from QSettings (saved by Blop Study web app via localStorage)
  QString username =
      QSettings("Blop", "BlopApp").value("username", "Gast").toString();
  QString initial = username.isEmpty() ? "G" : username.left(1).toUpper();

  // Avatar circle
  m_lblSidebarAvatar = new QLabel(initial, bottomBar);
  m_lblSidebarAvatar->setFixedSize(36, 36);
  m_lblSidebarAvatar->setAlignment(Qt::AlignCenter);
  m_lblSidebarAvatar->setStyleSheet(
      "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #5E5CE6,stop:1 "
      "#7D7AFF); "
      "border-radius: 18px; color: white; font-weight: bold; font-size: 14px;");
  bottomLay->addWidget(m_lblSidebarAvatar);

  // Username + settings link
  QVBoxLayout *userCol = new QVBoxLayout();
  userCol->setSpacing(1);
  m_lblSidebarUser =
      new QLabel(username.isEmpty() ? "Gast" : username, bottomBar);
  m_lblSidebarUser->setStyleSheet(
      "font-size: 12px; font-weight: 600; color: white; "
      "background: transparent;");
  m_lblSidebarUser->setMaximumWidth(130);
  m_lblSidebarUser->setWordWrap(false);
  userCol->addWidget(m_lblSidebarUser);

  m_btnSidebarSettings = new QPushButton("Einstellungen", bottomBar);
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
      "font-size: 10px; color: #555; background: transparent;");
  userCol->addWidget(lblVersion);

  bottomLay->addLayout(userCol);
  bottomLay->addStretch();

  layout->addWidget(bottomBar);

#ifdef Q_OS_ANDROID
  m_sidebarContainer->setParent(this);
  m_sidebarContainer->setGeometry(0, 0, SIDEBAR_WIDTH, this->height());
  m_sidebarContainer->hide();
#else
  m_mainSplitter->addWidget(m_sidebarContainer);
  if (!m_isSidebarOpen) {
      m_sidebarContainer->setFixedWidth(0);
      m_sidebarContainer->hide();
  }
#endif
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
  m_rightSidebar->setFixedWidth(240); // Slightly wider to fit tabs comfortably
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
      "QTabWidget::pane { border: none; border-top: 1px solid rgba(255,255,255,0.05); }"
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
  m_btnColorWhite->setChecked(true);
  m_btnColorWhite->setCursor(Qt::PointingHandCursor);
  m_btnColorWhite->setStyleSheet(
      "QPushButton { background: #333; color: white; border: 1px solid #444; "
      "padding: 10px; border-radius: 5px; } QPushButton:checked { background: "
      "#5E5CE6; border: 1px solid #5E5CE6; }");
  connect(m_btnColorWhite, &QPushButton::clicked,
          [this]() { setPageColor(false); });
  m_btnColorDark = new QPushButton("Dark");
  m_btnColorDark->setCheckable(true);
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
#ifdef Q_OS_ANDROID
  m_isSidebarOpen = show;
  int startX = show ? -SIDEBAR_WIDTH : 0;
  int endX = show ? 0 : -SIDEBAR_WIDTH;
  if (show) {
    m_sidebarContainer->move(-SIDEBAR_WIDTH, 0);
    m_sidebarContainer->raise();
    m_sidebarContainer->show();
  }
  QPropertyAnimation *anim = new QPropertyAnimation(m_sidebarContainer, "pos");
  anim->setDuration(250);
  anim->setStartValue(QPoint(startX, 0));
  anim->setEndValue(QPoint(endX, 0));
  anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(anim, &QPropertyAnimation::finished, [this, show]() {
    if (!show)
      m_sidebarContainer->hide();
    updateSidebarState();
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
#else
  int start = show ? 0 : SIDEBAR_WIDTH;
  int end = show ? SIDEBAR_WIDTH : 0;
  m_isSidebarOpen = show;
  
  if (show) {
    m_sidebarContainer->setMaximumWidth(0);
    m_sidebarContainer->setMinimumWidth(0);
    m_sidebarContainer->setVisible(true);
    updateSidebarState();
  } else {
    m_sidebarContainer->setMinimumWidth(0);
  }
  
  QPropertyAnimation *anim = new QPropertyAnimation(m_sidebarContainer, "maximumWidth");
  anim->setDuration(250);
  anim->setStartValue(start);
  anim->setEndValue(end);
  anim->setEasingCurve(QEasingCurve::OutExpo);
  
  connect(anim, &QPropertyAnimation::finished, this, [this, show]() {
    if (!show) {
      m_sidebarContainer->hide();
      updateSidebarState();
    } else {
      m_sidebarContainer->setFixedWidth(SIDEBAR_WIDTH);
    }
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
#endif
}
void MainWindow::onToggleSidebar() {
  bool isVisible =
      m_sidebarContainer->isVisible() && m_sidebarContainer->width() > 0;
  animateSidebar(!isVisible);
}
void MainWindow::updateSidebarState() {
  bool isEditor = (m_rightStack->currentWidget() == m_editorContainer);
  if (m_floatingTools) {
    m_floatingTools->setVisible(isEditor);
  }
  if (m_editorTitleControls) {
    m_editorTitleControls->setVisible(isEditor);
  }
  if (m_modeSelector) {
    // Only show mode selector on Desktop if we are in the Overview AND not in Android mode (where it has its own header)
#ifndef Q_OS_ANDROID
    m_modeSelector->setVisible(!isEditor);
#endif
  }
  
  if (m_isSidebarOpen) {
    m_sidebarStrip->hide();
    if (btnEditorMenu) btnEditorMenu->hide();
    return;
  }
  if (isEditor) {
    m_sidebarStrip->hide();
    if (btnEditorMenu) btnEditorMenu->show();
  } else {
    m_sidebarStrip->show();
    if (btnEditorMenu) btnEditorMenu->hide();
  }
}

void MainWindow::animateRightSidebar(bool show) {
  int start = show ? 0 : SIDEBAR_WIDTH;
  int end = show ? SIDEBAR_WIDTH : 0;
  m_rightSidebar->setVisible(true);
  QPropertyAnimation *anim =
      new QPropertyAnimation(m_rightSidebar, "maximumWidth");
  anim->setDuration(250);
  anim->setStartValue(start);
  anim->setEndValue(end);
  anim->setEasingCurve(QEasingCurve::OutExpo);
  connect(anim, &QPropertyAnimation::finished, [this, show]() {
    if (!show)
      m_rightSidebar->hide();
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
void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  updateGrid();
  int fabSize = 56;
  int bottomOffset = 80;
#ifdef Q_OS_ANDROID
  fabSize = FAB_SIZE_ANDROID;
  bottomOffset = FAB_DISTANCE_FROM_BOTTOM;
  if (m_isSidebarOpen && m_sidebarContainer) {
    m_sidebarContainer->setGeometry(0, 0, SIDEBAR_WIDTH,
                                    m_centralContainer->height());
  }
#else
  if (isTouchMode())
    bottomOffset = 100;
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
  QWidget *overlay = new QWidget(this);
  overlay->setGeometry(this->rect());
  overlay->setStyleSheet("background-color: rgba(0, 0, 0, 150);");
  overlay->show();
  bool keepOpen = true;
  while (keepOpen) {
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
      overlay->hide();
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
      overlay->show();
    } else {
      keepOpen = false;
    }
  }
  delete overlay;
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
  CanvasView *cv = getCurrentCanvas();
  if (cv)
    cv->undo();
}
void MainWindow::onRedo() {
  CanvasView *cv = getCurrentCanvas();
  if (cv)
    cv->redo();
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
  }

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
