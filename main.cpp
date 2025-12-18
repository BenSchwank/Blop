#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>

// Deine Header einbinden
#include "mainwindow.h"
#include "BlopAsyncImageProvider.h"

int main(int argc, char *argv[])
{
    // QApplication ist wichtig für C++ Widgets Kompatibilität
    QApplication a(argc, argv);

    QQmlApplicationEngine engine;

    // 1. Image Provider registrieren (für image://blop/...)
    engine.addImageProvider("blop", new BlopAsyncImageProvider);

    // 2. Pfad für Notizen an QML übergeben (Property 'notesPath')
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    // Den Pfad als URL setzen, damit FolderListModel ihn lesen kann
    engine.rootContext()->setContextProperty("notesPath", QUrl::fromLocalFile(path));


    // 3. QML laden
    const QUrl url(u"qrc:/Blop/Main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &a, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    engine.load(url);

    return a.exec();
}
