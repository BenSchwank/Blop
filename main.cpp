#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

// --- ANDROID WEBVIEW INCLUDE ---
// Wichtig: Das Include muss bedingt sein, da es auf Desktop oft fehlt
#ifdef Q_OS_ANDROID
#include <QtWebView/QtWebView>
#endif

#include "introscreen.h"
#include "mainwindow.h"
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

  // QApplication ist notwendig, da wir QMainWindow (Widgets) nutzen
  QApplication a(argc, argv);

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

  // --- LADEBILDSCHIRM (Custom Intro Animation) ---
  // Create intro as a child of the MainWindow to overlay it perfectly
  IntroScreen *intro = new IntroScreen(w);

  // Connect Intro Finish to clean it up and enable main window interaction
  QObject::connect(intro, &IntroScreen::introFinished, [intro]() {
    intro->deleteLater();
  });

  // Restore previous window size/position, or default to maximized/fullscreen
  w->restoreWindowState();

  // Now properly size and show the intro over the shown window
  intro->setGeometry(w->rect());
  intro->show();
  intro->raise(); // ensure it's on top
  intro->startAnimation();

  // Starten des normalen Eventloops
  return a.exec();
}
