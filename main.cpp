#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>

// --- ANDROID WEBVIEW INCLUDE ---
#ifdef Q_OS_ANDROID
#include <QtWebView/QtWebView>
#endif

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // --- ANDROID WEBVIEW INIT (Muss VOR QApplication sein!) ---
#ifdef Q_OS_ANDROID
    QtWebView::initialize();
#endif

    QApplication a(argc, argv);

    // --- SETUP FÜR ALLE PLATTFORMEN ---
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif

    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    MainWindow w;

#ifdef Q_OS_ANDROID
    w.showFullScreen();
#else
    w.showMaximized(); // Maximized ist auf Desktop besser als einfaches show()
#endif

    return a.exec();
}
