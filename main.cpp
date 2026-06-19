#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QGuiApplication>
#include <QStandardPaths>
#ifdef Q_OS_ANDROID
#include <QSurfaceFormat>
#include <QSslSocket>
#include <QNetworkAccessManager>
#endif
#include <QUrl>

// --- ANDROID WEBVIEW INCLUDE ---
// Wichtig: Das Include muss bedingt sein, da es auf Desktop oft fehlt
#ifdef Q_OS_ANDROID
#include <QtWebView/QtWebView>
#endif

#include "blop_crash_backend.h"
#include "blop_diag.h"
#include "blop_observability.h"
#include "blop_theme.h"
#include "mainwindow.h"
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QPixmapCache>
#include <QTimer>

int main(int argc, char *argv[]) {
  // --- ANDROID WEBVIEW INIT ---
  // Muss zwingend VOR der Erstellung der QApplication aufgerufen werden,
  // damit das native Android-Web-Backend geladen wird.
#ifdef Q_OS_ANDROID
  // Belt-and-braces: disable Qt's Android accessibility bridge before
  // QApplication is constructed. Background TalkBack-style services on
  // Android 16 (Nothing Pong et al.) hold the EGL surface lock via
  // QtAndroidAccessibility::runInObjectContext(), and any subsequent
  // top-level QWindow creation (QMenu, QDialog, ...) trips the Qt 6.10
  // deadlock protector -> qFatal -> abort. The v3.16.9 menu refactor
  // already removes the QMenu code paths; this env var prevents the
  // EGL-lock contention from arising in the first place. Cheap; no-op
  // if the env var name changes in a future Qt release.
  qputenv("QT_ANDROID_NO_ACCESSIBILITY", "1");
  qputenv("QT_ACCESSIBILITY", "0");

  QtWebView::initialize();
  
  // v3.18.25: Initialize TLS/SSL backend for Google Login on Android
  qDebug() << "SSL Support:" << QSslSocket::supportsSsl();
  qDebug() << "SSL Library Build Version:" << QSslSocket::sslLibraryBuildVersionString();
  qDebug() << "SSL Library Runtime Version:" << QSslSocket::sslLibraryVersionString();
  
  if (!QSslSocket::supportsSsl()) {
    qDebug() << "SSL not supported, forcing Android SSL backend";
    // Force loading of Android SSL backend
    QNetworkAccessManager nam;
    qDebug() << "Network SSL support:" << nam.supportedSchemes();
  }
  
  if (qgetenv("BLOP_ANDROID_SOFTWARE_GL") == "1") {
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
  } else {
    // Make every QOpenGLWidget instance share the same low-overhead format
    // (no MSAA, vsync on, GLES). The MultiPageNoteView's GL viewport is
    // dramatically faster than the software raster engine for pan/zoom.
    QSurfaceFormat fmt;
    fmt.setSamples(0);
    fmt.setSwapInterval(1);
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    QSurfaceFormat::setDefaultFormat(fmt);
  }
  // Share GL contexts across QOpenGLWidget / QQuickWidget so background
  // texture uploads from QML (Study) don't fight the canvas viewport.
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

  // --- WINDOWS + Qt WebEngine (Study / Login) ---
  // Must be set before QApplication when mixing WebEngine with other GL users.
  // Packaged installs (installer / windeployqt) often hit a black WebView without this.
#ifdef Q_OS_WIN
  // Last resort for stubborn black WebView: set BLOP_FORCE_SOFTWARE_OPENGL=1 before launch.
  if (qgetenv("BLOP_FORCE_SOFTWARE_OPENGL") == "1")
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
#  ifndef QT_DEBUG
  // Release default: prefer software GL on Windows unless explicitly overridden.
  // Symptom fixed by this path: Web content receives mouse/keyboard input but paints as black.
  if (qgetenv("BLOP_ALLOW_HARDWARE_GL") != "1")
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
#  endif
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  // Chromium sandbox can fail under some AV / locked-down installs; WebView stays black.
  if (qgetenv("QTWEBENGINE_DISABLE_SANDBOX").isEmpty())
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", "1");
  // Default Chromium flags (--disable-gpu, …) must NOT run in Debug builds: Qt WebEngine
  // asserts gpu_info.IsInitialized() in content_gpu_client_qt.cpp; disabling the GPU prevents that.
  // Packaged Release: set flags here. Debug: leave unset (or set QTWEBENGINE_CHROMIUM_FLAGS yourself).
#  ifndef QT_DEBUG
  if (qgetenv("QTWEBENGINE_CHROMIUM_FLAGS").isEmpty()) {
    QByteArray flags =
        "--disable-gpu --disable-gpu-compositing "
        "--ignore-gpu-blocklist "
        "--disable-features=Vulkan "
        "--disable-backgrounding-occluded-windows "
        "--disable-renderer-backgrounding";
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
  }
#  endif
#endif

  // QApplication ist notwendig, da wir QMainWindow (Widgets) nutzen
  QApplication a(argc, argv);

  // v3.17.6: bump the global QPixmapCache. The default is 10240 KB on
  // desktop and 1024 KB on mobile -- our note-thumbnail workload needs
  // more headroom (a single A4 page-icon is ~46 KB and we may keep
  // hundreds of them in a long note). 16 MB keeps long-session memory
  // bounded while practically eliminating cache thrashing.
  QPixmapCache::setCacheLimit(16 * 1024);

  // Install in-app crash diagnostics (Qt msg handler + POSIX signal handler).
  // Must run as early as possible after QApplication so we capture early
  // crashes; before blopInitCrashReporting() so our async-signal-safe writer
  // is the first responder for SIGSEGV/etc.
  BlopDiag::install();

  // Read persisted theme (Dark/Light + Accent) before any widget construction
  // so that QSS bricks built during MainWindow setup see the correct tokens.
  BlopTheme::instance().install();

  blopLogObservabilityBootstrap();
  blopInitCrashReporting();
  QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
    blopShutdownCrashReporting();
  });

#ifdef Q_OS_ANDROID
  qInfo() << "[BlopDiag] Native crash capture: adb logcat -d | grep -iE "
             "\"SIGSEGV|DEBUG|AndroidRuntime|blop|Qt\" ; software GL test: "
             "BLOP_ANDROID_SOFTWARE_GL=1 before launch";
#endif

#ifdef Q_OS_WIN
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

  // set global app font (Blop Study Redesign)
  QFont appFont("Segoe UI", 10);
  appFont.setStyleHint(QFont::SansSerif);
  a.setFont(appFont);

  // --- SPEICHERPFADE EINRICHTEN ---
#ifdef Q_OS_ANDROID
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif

  // Sicherstellen, dass das Verzeichnis existiert
  QDir dir(path);
  if (!dir.exists())
    dir.mkpath(".");

  // App Logo als Fenster Icon setzen
  a.setWindowIcon(QIcon(":/assets/logo.jpg"));

  // --- HAUPTFENSTER STARTEN ---
  MainWindow *w = new MainWindow();

  // Restore previous window size/position, or default to maximized/fullscreen
  w->restoreWindowState();
  w->show(); // ensure it's visible

  // Starten des normalen Eventloops
  return a.exec();
}
