#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QUrl>

// --- ANDROID WEBVIEW INCLUDE ---
// Wichtig: Das Include muss bedingt sein, da es auf Desktop oft fehlt
#ifdef Q_OS_ANDROID
#include <QtWebView/QtWebView>
#endif

#include "introscreen.h"
#include "mainwindow.h"
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

int main(int argc, char *argv[]) {
  // --- ANDROID WEBVIEW INIT ---
  // Muss zwingend VOR der Erstellung der QApplication aufgerufen werden,
  // damit das native Android-Web-Backend geladen wird.
#ifdef Q_OS_ANDROID
  QtWebView::initialize();
#endif

  // --- WINDOWS + Qt WebEngine (Study / Login) ---
  // Must be set before QApplication when mixing WebEngine with other GL users.
  // Packaged installs (installer / windeployqt) often hit a black WebView without this.
#ifdef Q_OS_WIN
  // Last resort for stubborn black WebView: set BLOP_FORCE_SOFTWARE_OPENGL=1 before launch.
  if (qgetenv("BLOP_FORCE_SOFTWARE_OPENGL") == "1")
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
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

  /* --- LADEBILDSCHIRM (Custom Intro Animation) DEAKTIVIERT ---
  IntroScreen *intro = new IntroScreen(w);
  QObject::connect(intro, &IntroScreen::introFinished, [intro]() {
    intro->deleteLater();
  });
  intro->setGeometry(w->rect());
  intro->show();
  intro->raise(); // ensure it's on top
  intro->startAnimation();
  */

  // Starten des normalen Eventloops
  return a.exec();
}
