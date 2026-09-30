#include "OpenRouter.h"

#include "SettingsSync.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {

QString complete(const QString &system, const QString &user, QString *error) {
    const QString key = SettingsSync::openRouterKey();
    if (key.isEmpty()) {
        if (error)
            *error = QStringLiteral(
                "OpenRouter-Schlüssel fehlt. Trag ihn in Blop unter Tastatur oder hier unter Konto ein.");
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
