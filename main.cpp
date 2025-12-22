#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>

// Deine Header
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // QApplication ist zwingend für Widget-Mix
    QApplication a(argc, argv);

    // --- SETUP FÜR ALLE PLATTFORMEN (Pfade etc.) ---
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif

    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    // --- HAUPTFENSTER STARTEN (JETZT AUCH AUF ANDROID) ---
    MainWindow w;

#ifdef Q_OS_ANDROID
    // Auf Android im Vollbild starten
    w.showFullScreen();
#else
    // Auf Desktop normal anzeigen
    w.show();
#endif

    return a.exec();
}
