#include "core/ActionRunner.h"
#include "core/CommandEngine.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTextStream>

#ifdef Q_OS_ANDROID
#include "ui/PhoneShell.h"
#else
#include "ui/NotchWindow.h"
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Blop"));
    QCoreApplication::setApplicationName(QStringLiteral("BlopAssistent"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Blop Assistent"));

    const QStringList args = QCoreApplication::arguments();
    const int runAt = args.indexOf(QStringLiteral("--run"));
    if (runAt >= 0) {
        const QString command = args.mid(runAt + 1).join(QLatin1Char(' '));
        CommandEngine engine;
        ActionRunner runner;
        const ActionResult result = runner.runText(command);
        QTextStream(stdout) << result.message << '\n';
        return result.ok ? 0 : 1;
    }

#ifdef Q_OS_ANDROID
    PhoneShell shell;
    shell.show();
#else
    NotchWindow notch;
    notch.show();
#endif
    return app.exec();
}
