#include "core/ActionRunner.h"
#include "core/CommandEngine.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QTextStream>

#ifdef Q_OS_ANDROID
#include "ui/PhoneShell.h"
#else
#include <QMenu>
#include <QSettings>
#include <QSystemTrayIcon>

#include "ui/NotchWindow.h"
#include "ui/SetupWindow.h"

QIcon blopTrayIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#24262B")));
    painter.drawEllipse(1, 1, 30, 30);
    painter.setBrush(QColor(QStringLiteral("#5B9DFF")));
    painter.drawEllipse(9, 9, 14, 14);
    return QIcon(pixmap);
}
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
    QApplication::setQuitOnLastWindowClosed(false);

    NotchWindow notch;
    SetupWindow setup;

    QSystemTrayIcon tray(blopTrayIcon());
    tray.setToolTip(QStringLiteral("Blop Assistent"));
    QMenu menu;
    QAction *settings = menu.addAction(QStringLiteral("Einstellungen"));
    menu.addSeparator();
    QAction *quit = menu.addAction(QStringLiteral("Beenden"));
    tray.setContextMenu(&menu);

    QObject::connect(settings, &QAction::triggered, &setup, [&setup]() {
        setup.refresh();
        setup.present();
    });
    QObject::connect(quit, &QAction::triggered, &app, &QApplication::quit);
    QObject::connect(&tray, &QSystemTrayIcon::activated, &notch,
                     [&notch](QSystemTrayIcon::ActivationReason reason) {
                         if (reason == QSystemTrayIcon::Trigger)
                             notch.reveal();
                     });
    QObject::connect(&notch, &NotchWindow::settingsRequested, &setup, [&setup]() {
        setup.refresh();
        setup.present();
    });
    QObject::connect(&setup, &SetupWindow::voiceHotkeyChanged, &notch,
                     &NotchWindow::reloadVoiceHotkey);
    QObject::connect(&setup, &SetupWindow::finished, &notch, [&notch]() {
        notch.show();
        notch.reloadVoiceHotkey();
    });

    tray.show();
    notch.show();
    const QSettings settingsStore;
    if (!settingsStore.value(QStringLiteral("assistant/setupDone")).toBool())
        setup.present();
#endif
    return app.exec();
}
