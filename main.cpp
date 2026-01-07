#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QQmlEngine>

// Deine Header
#include "mainwindow.h"
#include "tools/ToolUIBridge.h"
#include "tools/ToolFactory.h" // <--- WICHTIG: Include hinzugefügt

int main(int argc, char *argv[])
{
    // QApplication ist zwingend für Widget-Mix
    QApplication a(argc, argv);

    // --- SETUP FÜR ALLE PLATTFORMEN ---
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif

    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    // --- WICHTIG: WERKZEUGE REGISTRIEREN ---
    // Das hat gefehlt! Ohne das ist der ToolManager leer.
    ToolFactory::registerAllTools();

    // --- QML BRIDGE REGISTRIEREN ---
    qmlRegisterSingletonInstance("Blop", 1, 0, "Bridge", &ToolUIBridge::instance());

    // --- HAUPTFENSTER STARTEN ---
    MainWindow w;

#ifdef Q_OS_ANDROID
    w.showFullScreen();
#else
    w.showFullScreen();
#endif

    return a.exec();
}
