#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QQmlEngine>

// Deine Header
#include "mainwindow.h"
#include "tools/ToolUIBridge.h"
#include "tools/ToolFactory.h"  // <--- WICHTIG: Hast du diesen Include?

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // --- SETUP (Pfade) ---
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    // --- WICHTIG: HIER WERDEN DIE TOOLS GELADEN ---
    // Wenn diese Zeile fehlt, kennt die App keine Werkzeuge!
    ToolFactory::registerAllTools();

    // --- QML BRIDGE ---
    qmlRegisterSingletonInstance("Blop", 1, 0, "Bridge", &ToolUIBridge::instance());

    // --- WINDOW STARTEN ---
    MainWindow w;
#ifdef Q_OS_ANDROID
    w.showFullScreen();
#else
    w.show();
#endif

    return a.exec();
}
