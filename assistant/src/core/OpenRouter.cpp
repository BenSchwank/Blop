#include "OpenRouter.h"

#include "SettingsSync.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace {

QString messageFromReply(const QByteArray &raw, int status) {
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    const QString detail = obj.value(QStringLiteral("detail")).toString();
    if (status == 401)
        return QStringLiteral("Melde dich in Blop an. Die KI nutzt dein Study-Guthaben.");
    if (status == 402)
        return detail.isEmpty() ? QStringLiteral("Nicht genügend Tokens.") : detail;
    if (status == 404)
        return QStringLiteral("Der Assistent-Dienst ist auf dem Server noch nicht aktiv.");
    if (!detail.isEmpty())
        return detail;
    return QStringLiteral("Die KI ist gerade nicht verfügbar.");
}

QString completeViaStudy(const QString &system, const QString &user, QString *error,
                         double temperature) {
    QJsonObject body;
    body.insert(QStringLiteral("system"), system);
    body.insert(QStringLiteral("user"), user);
    body.insert(QStringLiteral("model"), SettingsSync::openRouterModel());
    if (temperature >= 0)
        body.insert(QStringLiteral("temperature"), temperature);
    QNetworkRequest req(QUrl(QStringLiteral("https://www.blop-study.com/api/assistant/complete")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("X-Session-Id", SettingsSync::studySessionId().toUtf8());
    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply *reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(50000);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
        if (error)
            *error = QStringLiteral("Study hat nicht geantwortet.");
        reply->deleteLater();
        return {};
    }
    const QByteArray raw = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    const QString text = obj.value(QStringLiteral("content")).toString().trimmed();
    if (text.isEmpty() || status >= 400) {
        if (error)
            *error = messageFromReply(raw, status);
        return {};
    }
    return text;
}

QString complete(const QString &system, const QString &user, QString *error,
                 double temperature = -1) {
    if (!SettingsSync::studySessionId().isEmpty())
        return completeViaStudy(system, user, error, temperature);
    const QString key = SettingsSync::openRouterKey();
    if (key.isEmpty()) {
        if (error)
            *error = QStringLiteral(
                "Melde dich in Blop an. Die KI nutzt dein Study-Guthaben.");
        return {};
    }
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                {QStringLiteral("content"), system}});
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), user}});
    QJsonObject body;
    body.insert(QStringLiteral("model"), SettingsSync::openRouterModel());
    body.insert(QStringLiteral("messages"), messages);
    if (temperature >= 0)
        body.insert(QStringLiteral("temperature"), temperature);
    QNetworkRequest req(QUrl(QStringLiteral("https://openrouter.ai/api/v1/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + key.toUtf8());
    QNetworkAccessManager nam;
    QEventLoop loop;
    QNetworkReply *reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(45000);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
        if (error)
            *error = QStringLiteral("OpenRouter hat nicht geantwortet.");
        reply->deleteLater();
        return {};
    }
    const QByteArray raw = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    const QString text = choices.isEmpty()
                             ? QString()
                             : choices.at(0)
                                   .toObject()
                                   .value(QStringLiteral("message"))
                                   .toObject()
                                   .value(QStringLiteral("content"))
                                   .toString()
                                   .trimmed();
    if (text.isEmpty()) {
        if (error)
            *error = status >= 400
                         ? QStringLiteral("OpenRouter hat den Text abgelehnt.")
                         : QStringLiteral("OpenRouter hat keinen Text geliefert.");
        return {};
    }
    return text;
}

QString unwrapJson(QString text) {
    text.remove(QStringLiteral("```json"));
    text.remove(QStringLiteral("```"));
    const int start = text.indexOf(QLatin1Char('{'));
    const int end = text.lastIndexOf(QLatin1Char('}'));
    if (start >= 0 && end > start)
        return text.mid(start, end - start + 1);
    return text;
}

AppKind appKindFrom(const QString &raw) {
    const QString name = raw.toLower();
    if (name.contains(QLatin1String("explor")) || name == QLatin1String("dateien"))
        return AppKind::Explorer;
    if (name.contains(QLatin1String("rechner")) || name.contains(QLatin1String("calc")) ||
        name.contains(QLatin1String("taschen")))
        return AppKind::Calculator;
    if (name.contains(QLatin1String("editor")) || name.contains(QLatin1String("notepad")) ||
        name.contains(QLatin1String("notizblock")))
        return AppKind::Notepad;
    if (name.contains(QLatin1String("browser")) || name == QLatin1String("chrome") ||
        name == QLatin1String("edge") || name == QLatin1String("firefox"))
        return AppKind::Browser;
    if (name == QLatin1String("blop"))
        return AppKind::Blop;
    return AppKind::None;
}

bool allowedFolder(const QString &name) {
    const QString key = name.trimmed().toLower();
    static const QStringList keys = {
        QStringLiteral("downloads"),    QStringLiteral("download"),
        QStringLiteral("dokumente"),    QStringLiteral("dokument"),
        QStringLiteral("documents"),    QStringLiteral("desktop"),
        QStringLiteral("schreibtisch"), QStringLiteral("bilder"),
        QStringLiteral("fotos"),        QStringLiteral("musik"),
        QStringLiteral("videos"),       QStringLiteral("video"),
        QStringLiteral("notizen"),      QStringLiteral("blopnotizen"),
    };
    return keys.contains(key);
}

QString plainName(QString name) {
    name = name.trimmed();
    name.remove(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")));
    if (name == QLatin1String(".") || name == QLatin1String("..") ||
        name.contains(QLatin1String("..")))
        return {};
    return name.trimmed();
}

Command commandFromAction(const QJsonObject &action) {
    const QString kind = action.value(QStringLiteral("kind")).toString().trimmed().toLower();
    Command command;
    if (kind == QLatin1String("launch")) {
        const AppKind app = appKindFrom(action.value(QStringLiteral("app")).toString());
        if (app == AppKind::None)
            return {};
        command.kind = CommandKind::LaunchApp;
        command.app = app;
        return command;
    }
    if (kind == QLatin1String("folder")) {
        const QString name = action.value(QStringLiteral("name")).toString().trimmed();
        if (!allowedFolder(name))
            return {};
        command.kind = CommandKind::OpenFolder;
        command.text = name;
        return command;
    }
    if (kind == QLatin1String("url")) {
        const QUrl url(action.value(QStringLiteral("url")).toString().trimmed());
        if (!url.isValid() ||
            (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")))
            return {};
        command.kind = CommandKind::OpenUrl;
        command.text = url.toString();
        return command;
    }
    if (kind == QLatin1String("mkdir")) {
        const QString name = plainName(action.value(QStringLiteral("name")).toString());
        if (name.isEmpty())
            return {};
        QString place = action.value(QStringLiteral("place")).toString().trimmed().toLower();
        if (place != QLatin1String("dokumente") && place != QLatin1String("documents") &&
            place != QLatin1String("downloads") && place != QLatin1String("download"))
            place = QStringLiteral("desktop");
        command.kind = CommandKind::CreateFolder;
        command.text = name;
        command.title = place;
        return command;
    }
    if (kind == QLatin1String("txt")) {
        const QString content = action.value(QStringLiteral("content")).toString();
        if (content.trimmed().isEmpty())
            return {};
        command.kind = CommandKind::CreateTextFile;
        command.title = plainName(action.value(QStringLiteral("name")).toString());
        command.text = content;
        return command;
    }
    if (kind == QLatin1String("note")) {
        const QString title = plainName(action.value(QStringLiteral("title")).toString());
        if (title.isEmpty())
            return {};
        command.kind = CommandKind::CreateNote;
        command.title = title;
        command.text = action.value(QStringLiteral("text")).toString();
        return command;
    }
    if (kind == QLatin1String("tool")) {
        const QString id = action.value(QStringLiteral("id")).toString().trimmed().toLower();
        if (id != QLatin1String("pen1") && id != QLatin1String("pen2") &&
            id != QLatin1String("pen3") && id != QLatin1String("marker"))
            return {};
        command.kind = CommandKind::SelectTool;
        command.toolId = id;
        return command;
    }
    if (kind == QLatin1String("explain")) {
        const QString text = action.value(QStringLiteral("text")).toString().trimmed();
        if (text.isEmpty())
            return {};
        command.kind = CommandKind::Explain;
        command.text = text;
        command.generate = true;
        return command;
    }
    return {};
}

} // namespace

bool OpenRouter::note(const QString &prompt, QString *heading, QStringList *points,
                      QString *error) {
    const QString text = complete(
        QStringLiteral(
            "Du schreibst eine kurze deutsche Notiz. Antworte nur mit JSON "
            "{\"heading\":\"\",\"points\":[\"\"]}. Keine Markdown-Fences. "
            "Höchstens sechs Punkte, jeder ein kurzer Satz."),
        prompt, error);
    if (text.isEmpty())
        return false;
    const QJsonObject obj = QJsonDocument::fromJson(unwrapJson(text).toUtf8()).object();
    if (heading)
        *heading = obj.value(QStringLiteral("heading")).toString().trimmed();
    if (points) {
        points->clear();
        for (const QJsonValue &value : obj.value(QStringLiteral("points")).toArray()) {
            const QString point = value.toString().trimmed();
            if (!point.isEmpty())
                points->append(point);
        }
        if (points->isEmpty() && heading && heading->isEmpty())
            points->append(text.trimmed());
    }
    return true;
}

bool OpenRouter::explain(const QString &prompt, QString *answer, QString *error) {
    const QString text = complete(
        QStringLiteral("Antworte auf Deutsch in höchstens acht Sätzen. Kein JSON."),
        prompt, error);
    if (text.isEmpty())
        return false;
    if (answer)
        *answer = text;
    return true;
}

bool OpenRouter::plan(const QString &utterance, QList<Command> *commands, QString *error) {
    if (commands)
        commands->clear();
    const QString text = complete(
        QStringLiteral(
            "Du planst Aktionen für den Blop Assistenten. Der Nutzer spricht Deutsch, oft umständlich. "
            "Antworte nur mit JSON {\"actions\":[...]}. Keine Markdown-Fences. Höchstens sechs Schritte, "
            "in der Reihenfolge der Anfrage. Erlaubt sind nur diese kinds: "
            "launch mit app explorer, rechner, editor, browser oder blop; "
            "folder mit name downloads, dokumente, desktop, bilder, musik, videos oder notizen; "
            "url mit https-Adresse; "
            "mkdir mit name und place desktop, dokumente oder downloads; "
            "txt mit content und optionalem name, die Datei liegt auf dem Desktop; "
            "note mit title und text; "
            "tool mit id pen1, pen2, pen3 oder marker; "
            "explain mit text, wenn nur eine Erklärung gewünscht ist. "
            "Kein Löschen, keine Shell, keine anderen Programme, keine beliebigen Pfade. "
            "Was nicht erlaubt ist, lässt du weg."),
        utterance, error, 0);
    if (text.isEmpty())
        return false;
    const QJsonObject obj = QJsonDocument::fromJson(unwrapJson(text).toUtf8()).object();
    const QJsonArray actions = obj.value(QStringLiteral("actions")).toArray();
    QList<Command> planned;
    for (const QJsonValue &value : actions) {
        const Command command = commandFromAction(value.toObject());
        if (command.kind == CommandKind::Unknown)
            continue;
        planned.append(command);
        if (planned.size() >= 6)
            break;
    }
    if (planned.isEmpty()) {
        if (error)
            *error = QStringLiteral("Das kann ich so nicht ausführen.");
        return false;
    }
    if (commands)
        *commands = planned;
    return true;
}
