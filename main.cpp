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

  // --- LADEBILDSCHIRM (Custom Intro Animation) ---
  IntroScreen *intro = new IntroScreen();
  intro->show();
  intro->startAnimation();

  // --- HAUPTFENSTER STARTEN ---
  // Create Main Window initially hidden
  MainWindow *w = new MainWindow();

  // Connect Intro Finish to Main Window show
  QObject::connect(intro, &IntroScreen::introFinished, [w, intro]() {
    w->show();
    intro->deleteLater();
  });

#ifdef Q_OS_ANDROID
  // Auf Android sieht Vollbild besser aus
  w->showFullScreen();
#else
  // Auf Desktop starten wir maximiert
  w->showMaximized();
#endif

  // Starten des normalen Eventloops
  return a.exec();
}
