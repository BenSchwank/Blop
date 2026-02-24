#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>


// --- ANDROID WEBVIEW INCLUDE ---
// Wichtig: Das Include muss bedingt sein, da es auf Desktop oft fehlt
#ifdef Q_OS_ANDROID
#include <QtWebView/QtWebView>
#endif

#include "mainwindow.h"
#include <QIcon>
#include <QPixmap>
#include <QSplashScreen>
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

  // --- LADEBILDSCHIRM (Splash Screen) ---
  QPixmap splashPix(":/assets/logo.jpg");
  // Skaliere das Logo für den Splash-Screen auf eine angenehme Größe
  splashPix =
      splashPix.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QSplashScreen splash(splashPix, Qt::WindowStaysOnTopHint);
  splash.show();
  // Um sicherzugehen, dass das Bild auch beim Start ordentlich gerendert wird
  a.processEvents();

  // --- HAUPTFENSTER STARTEN ---
  MainWindow w;

#ifdef Q_OS_ANDROID
  // Auf Android sieht Vollbild besser aus
  w.showFullScreen();
#else
  // Auf Desktop starten wir maximiert
  w.showMaximized();
#endif

  // Simulierter zusätzlicher Lade-Delay (für den sanften Effekt), entferne
  // falls nicht gewünscht
  QTimer::singleShot(1000, &splash, SLOT(close()));

  return a.exec();
}
