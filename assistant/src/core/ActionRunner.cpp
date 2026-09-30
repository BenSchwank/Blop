#include "ActionRunner.h"

#include "NoteWriter.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {

QString standardDir(QStandardPaths::StandardLocation location) {
    return QStandardPaths::writableLocation(location);
}

} // namespace

ActionResult ActionRunner::run(const Command &command) const {
    switch (command.kind) {
    case CommandKind::Help:
        return {true, CommandEngine::helpText()};
    case CommandKind::Unknown:
        return {false, command.error.isEmpty()
                           ? QStringLiteral("Das kenne ich nicht. Sag „Hilfe“.")
                           : command.error};
    case CommandKind::OpenFolder: {
        QString error;
        const QString path = resolveFolder(command.text, &error);
        if (path.isEmpty())
            return {false, error};
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            return {false, QStringLiteral("Der Ordner ließ sich nicht öffnen.")};
        return {true, QStringLiteral("Ordner geöffnet.")};
    }
    case CommandKind::OpenUrl: {
        QString error;
        const QUrl url(command.text);
        if (!openUrl(url, &error))
            return {false, error};
        return {true, QStringLiteral("Browser geöffnet.")};
    }
    case CommandKind::LaunchApp: {
        QString error;
        if (!launchApp(command.app, &error))
            return {false, error};
        if (command.app == AppKind::Browser)
            return {true, QStringLiteral("Browser geöffnet.")};
        if (command.app == AppKind::Blop)
            return {true, QStringLiteral("Blop gestartet.")};
        return {true, QStringLiteral("Programm gestartet.")};
    }
    case CommandKind::CreateNote: {
        QString error;
        const QString path = NoteWriter::write(command.title, command.text, &error);
        if (path.isEmpty())
            return {false, error};
        return {true, QStringLiteral("Notiz gespeichert: %1")
                          .arg(QFileInfo(path).fileName())};
    }
    }
    return {false, QStringLiteral("Das kenne ich nicht. Sag „Hilfe“.")};
}

QString ActionRunner::resolveFolder(const QString &raw, QString *error) const {
    QString arg = raw.trimmed();
    if (arg.size() >= 2 &&
        ((arg.startsWith(QLatin1Char('"')) && arg.endsWith(QLatin1Char('"'))) ||
         (arg.startsWith(QLatin1Char('\'')) && arg.endsWith(QLatin1Char('\''))))) {
        arg = arg.mid(1, arg.size() - 2).trimmed();
    }
    if (arg.isEmpty()) {
        if (error)
            *error = QStringLiteral("Welchen Ordner soll ich öffnen?");
        return {};
    }

    const QString key = arg.toLower();
    QString path;
    if (key == QLatin1String("downloads") || key == QLatin1String("download"))
        path = standardDir(QStandardPaths::DownloadLocation);
    else if (key == QLatin1String("dokumente") || key == QLatin1String("dokument") ||
             key == QLatin1String("documents"))
        path = standardDir(QStandardPaths::DocumentsLocation);
    else if (key == QLatin1String("desktop") || key == QLatin1String("schreibtisch"))
        path = standardDir(QStandardPaths::DesktopLocation);
    else if (key == QLatin1String("bilder") || key == QLatin1String("bilderordner") ||
             key == QLatin1String("fotos"))
        path = standardDir(QStandardPaths::PicturesLocation);
    else if (key == QLatin1String("musik"))
        path = standardDir(QStandardPaths::MusicLocation);
    else if (key == QLatin1String("videos") || key == QLatin1String("video") ||
             key == QLatin1String("filme"))
        path = standardDir(QStandardPaths::MoviesLocation);
    else if (key == QLatin1String("home") || key == QLatin1String("benutzer") ||
             key == QLatin1String("benutzerordner"))
        path = standardDir(QStandardPaths::HomeLocation);
    else if (key == QLatin1String("blopnotizen") || key == QLatin1String("blop notizen") ||
             key == QLatin1String("notizen") || key == QLatin1String("notizordner"))
        path = NoteWriter::libraryRoot();
    else if (arg.contains(QLatin1Char('\\')) || arg.contains(QLatin1Char('/')) ||
             QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(arg).hasMatch())
        path = QDir::cleanPath(arg);
    else {
        if (error)
            *error = QStringLiteral(
                "Den Ordner kenne ich nicht. Zum Beispiel: öffne ordner downloads");
        return {};
    }

    if (path.isEmpty() || !QFileInfo(path).isDir()) {
        if (error)
            *error = QStringLiteral("Den Ordner gibt es nicht.");
        return {};
    }
    return path;
}

bool ActionRunner::openUrl(const QUrl &url, QString *error) const {
    if (!url.isValid() || url.scheme().isEmpty()) {
        if (error)
            *error = QStringLiteral("Das ist keine Adresse.");
        return false;
    }
    if (!QDesktopServices::openUrl(url)) {
        if (error)
            *error = QStringLiteral("Der Browser ließ sich nicht öffnen.");
        return false;
    }
    return true;
}

bool ActionRunner::launchApp(AppKind app, QString *error) const {
    switch (app) {
    case AppKind::None:
        if (error)
            *error = QStringLiteral("Das Programm kenne ich nicht.");
        return false;
    case AppKind::Explorer:
#ifdef Q_OS_WIN
        if (!QProcess::startDetached(QStringLiteral("explorer.exe"))) {
            if (error)
                *error = QStringLiteral("Explorer ließ sich nicht öffnen.");
            return false;
        }
        return true;
#else
        if (error)
            *error = QStringLiteral("Den Explorer gibt es hier nicht.");
        return false;
#endif
    case AppKind::Calculator:
#ifdef Q_OS_WIN
        if (!QProcess::startDetached(QStringLiteral("calc.exe"))) {
            if (error)
                *error = QStringLiteral("Der Rechner ließ sich nicht öffnen.");
            return false;
        }
        return true;
#else
        if (error)
            *error = QStringLiteral("Den Rechner gibt es hier nicht.");
        return false;
#endif
    case AppKind::Notepad:
#ifdef Q_OS_WIN
        if (!QProcess::startDetached(QStringLiteral("notepad.exe"))) {
            if (error)
                *error = QStringLiteral("Der Editor ließ sich nicht öffnen.");
            return false;
        }
        return true;
#else
        if (error)
            *error = QStringLiteral("Den Editor gibt es hier nicht.");
        return false;
#endif
    case AppKind::Browser:
        return openUrl(QUrl(QStringLiteral("https://www.google.com")), error);
    case AppKind::Blop:
        return launchBlop(error);
    }
    if (error)
        *error = QStringLiteral("Das Programm kenne ich nicht.");
    return false;
}

bool ActionRunner::launchBlop(QString *error) const {
#ifdef Q_OS_ANDROID
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        if (error)
            *error = QStringLiteral("Android-Kontext fehlt.");
        return false;
    }
    const QJniObject packageManager = context.callObjectMethod(
        "getPackageManager", "()Landroid/content/pm/PackageManager;");
    const QJniObject intent = packageManager.callObjectMethod(
        "getLaunchIntentForPackage", "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(QStringLiteral("com.benschwank.blop")).object<jstring>());
    if (!intent.isValid()) {
        if (error)
            *error = QStringLiteral("Blop ist auf diesem Handy nicht installiert.");
        return false;
    }
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V",
                             intent.object<jobject>());
    return true;
#else
    const QString dir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        dir + QStringLiteral("/Blop.exe"),
        dir + QStringLiteral("/Blop"),
        QDir(dir).filePath(QStringLiteral("../Blop.exe")),
        QStandardPaths::findExecutable(QStringLiteral("Blop")),
    };
    for (const QString &candidate : candidates) {
        if (candidate.isEmpty() || !QFileInfo::exists(candidate))
            continue;
        if (QProcess::startDetached(QFileInfo(candidate).absoluteFilePath(), {}))
            return true;
    }
    if (error)
        *error = QStringLiteral("Blop ist hier nicht installiert.");
    return false;
#endif
}
