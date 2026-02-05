#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QQmlEngine>
#include <QtQml> // Wichtig für qmlRegisterSingletonInstance

// --- ANDROID WEBVIEW INCLUDE ---
#ifdef Q_OS_ANDROID
#include <QtWebView/QtWebView>
#endif

// Deine Header
#include "mainwindow.h"
#include "tools/ToolUIBridge.h"
#include "tools/ToolFactory.h"

int main(int argc, char *argv[])
{
    // QApplication ist zwingend für Widget-Mix (da wir QMainWindow nutzen)
    QApplication a(argc, argv);

    // --- ANDROID WEBVIEW INIT ---
    // Muss direkt nach der QApplication Instanzierung aufgerufen werden,
    // damit das native Web-Backend geladen wird.
#ifdef Q_OS_ANDROID
    QtWebView::initialize();
#endif

    // --- SETUP FÜR SPEICHERPFADE ---
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif

    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    // --- WICHTIG: WERKZEUGE REGISTRIEREN ---
    // Registriert alle Tools im ToolManager, damit sie verfügbar sind
    ToolFactory::registerAllTools();

    // --- QML BRIDGE REGISTRIEREN ---
    // Das macht die C++ Klasse "ToolUIBridge" in QML unter dem Namen "Bridge" verfügbar.
    qmlRegisterSingletonInstance("Blop", 1, 0, "Bridge", &ToolUIBridge::instance());

    // --- HAUPTFENSTER STARTEN ---
    MainWindow w;

#ifdef Q_OS_ANDROID
    w.showFullScreen();
#else
    w.showMaximized();
#endif

    return a.exec();
}
