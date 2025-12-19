#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>

// Deine Header
#include "mainwindow.h"
#include "BlopAsyncImageProvider.h"

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

    // --- UNTERSCHEIDUNG: WINDOWS/DESKTOP vs. ANDROID ---

#ifdef Q_OS_ANDROID
    // === ANDROID: Starte QML ===
    QQmlApplicationEngine engine;
    engine.addImageProvider("blop", new BlopAsyncImageProvider);
    engine.rootContext()->setContextProperty("notesPath", QUrl::fromLocalFile(path));

    const QUrl url("qrc:/Blop/Main.qml");
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &a, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    engine.load(url);

#else
    // === WINDOWS / DESKTOP: Starte C++ MainWindow ===
    // Das hier hat gefehlt!
    MainWindow w;
    w.show();

#endif

    return a.exec();
}
