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

    QQmlApplicationEngine engine;

    // 1. Image Provider registrieren
    engine.addImageProvider("blop", new BlopAsyncImageProvider);

    // 2. Pfad für Notizen bestimmen
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif

    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    // Pfad an QML übergeben
    engine.rootContext()->setContextProperty("notesPath", QUrl::fromLocalFile(path));

    // 3. QML laden
    const QUrl url("qrc:/Blop/Main.qml");

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &a, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);

    return a.exec();
}
