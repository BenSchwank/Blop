#include "ActionRunner.h"

#include "BlopBridge.h"
#include "NoteWriter.h"
#include "OpenRouter.h"
#include "SettingsSync.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {

QString standardDir(QStandardPaths::StandardLocation location) {
    return QStandardPaths::writableLocation(location);
}

bool asksForThought(const QString &text) {
    const QString lower = text.trimmed().toLower();
    if (lower.contains(QLatin1Char('?')))
        return true;
    const QStringList leads = {
        QStringLiteral("wie "),      QStringLiteral("was "),     QStringLiteral("wer "),
        QStringLiteral("wann "),     QStringLiteral("wo "),      QStringLiteral("wohin "),
        QStringLiteral("woher "),    QStringLiteral("warum "),   QStringLiteral("weshalb "),
        QStringLiteral("wieso "),    QStringLiteral("welche "),  QStringLiteral("welcher "),
        QStringLiteral("welches "),  QStringLiteral("wieviel "), QStringLiteral("wie viel "),
        QStringLiteral("hallo"),     QStringLiteral("hi "),      QStringLiteral("hey"),
    };
    for (const QString &lead : leads) {
        if (lower.startsWith(lead))
            return true;
    }
    return false;
}

} // namespace

ActionResult ActionRunner::runAll(const QList<Command> &commands) const {
    if (commands.isEmpty())
        return {false, QStringLiteral("Sag einen Befehl, oder „Hilfe“.")};
    QStringList messages;
    bool ok = true;
    for (const Command &command : commands) {
        const ActionResult result = run(command);
        if (!result.message.isEmpty())
            messages.append(result.message);
        if (!result.ok)
            ok = false;
    }
    return {ok, messages.join(QStringLiteral(" "))};
}

ActionResult ActionRunner::runText(const QString &text) const {
    const QList<Command> local = CommandEngine().parseAll(text);
    bool understood = !local.isEmpty();
    for (const Command &command : local) {
        if (command.kind == CommandKind::Unknown)
            understood = false;
    }
    if (understood)
        return runAll(local);
    if (asksForThought(text)) {
        QString answer;
        QString explainError;
        if (OpenRouter::explain(text, &answer, &explainError) && !answer.trimmed().isEmpty())
            return {true, answer};
        return {false, explainError.isEmpty()
                           ? QStringLiteral("Die KI ist gerade nicht verfügbar.")
                           : explainError};
    }
    QList<Command> planned;
    QString error;
    if (!OpenRouter::plan(text, &planned, &error)) {
        if (error == QStringLiteral("Das kann ich so nicht ausführen.")) {
            QString answer;
            QString explainError;
            if (OpenRouter::explain(text, &answer, &explainError) && !answer.trimmed().isEmpty())
                return {true, answer};
            if (!explainError.isEmpty())
                return {false, explainError};
        }
        return {false, error};
    }
    return runAll(planned);
}

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
    case CommandKind::CreateFolder: {
        QString error;
        const QString path = createFolder(command.text, command.title, &error);
        if (path.isEmpty())
            return {false, error};
        return {true, QStringLiteral("Ordner erstellt: %1").arg(command.text)};
    }
    case CommandKind::CreateTextFile: {
        QString error;
        const QString path = createTextFile(command.title, command.text, &error);
        if (path.isEmpty())
            return {false, error};
        return {true, QStringLiteral("Datei erstellt: %1").arg(QFileInfo(path).fileName())};
    }
    case CommandKind::CreateNote: {
        QString error;
        const QString path = NoteWriter::write(command.title, command.text, &error);
        if (path.isEmpty())
            return {false, error};
        return {true, QStringLiteral("Notiz gespeichert: %1")
                          .arg(QFileInfo(path).fileName())};
    }
    case CommandKind::Explain: {
        QString answer;
        QString error;
        if (!OpenRouter::explain(command.text, &answer, &error))
            return {false, error};
        if (command.alsoWrite && !command.title.trimmed().isEmpty()) {
            QString writeError;
            const QString path =
                NoteWriter::compose(command.title, QString(), {answer}, &writeError);
            if (path.isEmpty())
                return {true, answer + QStringLiteral("\n") + writeError};
            QString ipcError;
            BlopBridge::ask(QStringLiteral("OPEN ") + path, &ipcError);
        }
        return {true, answer};
    }
    case CommandKind::SelectTool: {
        const QString keys = SettingsSync::hotkeyFor(command.toolId);
        QString error;
        if (!BlopBridge::sendHotkey(keys, &error))
            return {false, error};
        return {true, QStringLiteral("Werkzeug gewählt.")};
    }
    case CommandKind::ComposeNote: {
        const QString stem = command.title.trimmed();
        const QString path = NoteWriter::pathFor(stem);
        QString ipcError;
        const QString state = BlopBridge::ask(QStringLiteral("STATUS ") + path, &ipcError);
        if (state == QLatin1String("DIRTY")) {
            return {false, QStringLiteral(
                               "Die Notiz ist in Blop noch nicht gespeichert. "
                               "Warte kurz und sag den Befehl noch einmal.")};
        }
        if (state == QLatin1String("CLEAN"))
            BlopBridge::ask(QStringLiteral("CLOSE ") + path, &ipcError);

        QString heading = command.heading;
        QStringList points = command.points;
        if (command.generate) {
            QString prompt = command.text;
            if (!heading.isEmpty())
                prompt.prepend(QStringLiteral("Überschrift: ") + heading + QStringLiteral(". "));
            QString generatedHeading;
            QString error;
            if (!OpenRouter::note(prompt, &generatedHeading, &points, &error))
                return {false, error};
            if (heading.isEmpty())
                heading = generatedHeading;
        }
        QString error;
        const QString saved = NoteWriter::compose(stem, heading, points, &error);
        if (saved.isEmpty())
            return {false, error};
        const QString opened = BlopBridge::ask(QStringLiteral("OPEN ") + saved, &ipcError);
        if (!command.toolId.isEmpty()) {
            QString keyError;
            BlopBridge::sendHotkey(SettingsSync::hotkeyFor(command.toolId), &keyError);
        }
        if (opened == QLatin1String("DIRTY")) {
            return {true, QStringLiteral("Notiz gespeichert, Blop hatte noch ungespeicherte Änderungen.")};
        }
        if (opened.isEmpty())
            return {true, QStringLiteral("Notiz gespeichert: %1").arg(QFileInfo(saved).fileName())};
        return {true, QStringLiteral("Notiz geöffnet: %1").arg(QFileInfo(saved).fileName())};
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

QString ActionRunner::createFolder(const QString &name, const QString &place,
                                   QString *error) const {
    const QString clean = name.trimmed();
    if (clean.isEmpty() || clean == QLatin1String(".") || clean == QLatin1String("..")) {
        if (error)
            *error = QStringLiteral("Der Ordnername ist ungültig.");
        return {};
    }
    QString base = standardDir(QStandardPaths::DesktopLocation);
    const QString where = place.toLower();
    if (where == QLatin1String("dokumente") || where == QLatin1String("documents"))
        base = standardDir(QStandardPaths::DocumentsLocation);
    else if (where == QLatin1String("downloads") || where == QLatin1String("download"))
        base = standardDir(QStandardPaths::DownloadLocation);
    if (base.isEmpty()) {
        if (error)
            *error = QStringLiteral("Den Desktop habe ich nicht gefunden.");
        return {};
    }
    const QString path = QDir(base).filePath(clean);
    if (!QDir().mkpath(path)) {
        if (error)
            *error = QStringLiteral("Der Ordner konnte nicht erstellt werden.");
        return {};
    }
    return path;
}

QString ActionRunner::createTextFile(const QString &name, const QString &content,
                                     QString *error) const {
    const QString base = standardDir(QStandardPaths::DesktopLocation);
    if (base.isEmpty()) {
        if (error)
            *error = QStringLiteral("Den Desktop habe ich nicht gefunden.");
        return {};
    }
    QString fileName = name.trimmed();
    fileName.remove(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")));
    fileName = fileName.trimmed();
    const bool named = !fileName.isEmpty();
    if (!named)
        fileName = QStringLiteral("Neu.txt");
    if (fileName == QLatin1String(".") || fileName == QLatin1String("..")) {
        if (error)
            *error = QStringLiteral("Der Dateiname ist ungültig.");
        return {};
    }
    if (!fileName.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive))
        fileName += QStringLiteral(".txt");

    QString path = QDir(base).filePath(fileName);
    if (!named) {
        int n = 2;
        while (QFileInfo::exists(path)) {
            path = QDir(base).filePath(QStringLiteral("Neu (%1).txt").arg(n));
            ++n;
        }
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Die Datei konnte nicht erstellt werden.");
        return {};
    }
    file.write(content.toUtf8());
    file.close();
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
#endif
    return path;
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
    QStringList candidates = {
        dir + QStringLiteral("/Blop.exe"),
        dir + QStringLiteral("/Blop"),
        QDir(dir).filePath(QStringLiteral("../Blop.exe")),
        QStandardPaths::findExecutable(QStringLiteral("Blop")),
        QStringLiteral("C:/Program Files/Blop/Blop.exe"),
        QStringLiteral("C:/Program Files (x86)/Blop/Blop.exe"),
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
