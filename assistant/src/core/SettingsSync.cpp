#include "SettingsSync.h"

#include "NoteWriter.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

namespace {

constexpr const char *kClientId =
    "571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com";
constexpr const char *kExchangeUrl =
    "https://www.blop-study.com/api/auth/google/desktop/exchange";
constexpr quint16 kPort = 27185;

QSettings blopSettings() {
    return QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
}

QString cloudKey(const char *field) {
    return QStringLiteral("cloud/googledrive/%1").arg(QString::fromLatin1(field));
}

QByteArray googleSecret() {
    const QByteArray fromEnv = qgetenv("BLOP_GOOGLE_CLIENT_SECRET");
    if (!fromEnv.trimmed().isEmpty())
        return fromEnv.trimmed();
    const QByteArray appdata = qgetenv("APPDATA");
    if (!appdata.isEmpty()) {
        QFile file(QDir(QString::fromLocal8Bit(appdata))
                       .filePath(QStringLiteral(
                           "Blop/BlopApp/google_desktop_client_secret.txt")));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            return file.readAll().trimmed();
    }
    return {};
}

QString randomToken(int len) {
    const QByteArray raw = QUuid::createUuid().toByteArray() +
                           QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    QByteArray out = QCryptographicHash::hash(raw, QCryptographicHash::Sha256)
                         .toBase64(QByteArray::Base64UrlEncoding |
                                   QByteArray::OmitTrailingEquals);
    return QString::fromLatin1(out.left(len));
}

void storeTokens(const QJsonObject &obj) {
    QSettings s = blopSettings();
    const QString access = obj.value(QStringLiteral("access_token")).toString();
    const QString refresh = obj.value(QStringLiteral("refresh_token")).toString();
    if (!access.isEmpty())
        s.setValue(cloudKey("accessToken"), access);
    if (!refresh.isEmpty())
        s.setValue(cloudKey("refreshToken"), refresh);
    const qint64 at = QDateTime::currentMSecsSinceEpoch() +
                      qint64(obj.value(QStringLiteral("expires_in")).toInt(3600)) * 1000;
    s.setValue(cloudKey("expiresAt"), at);
}

QByteArray httpBody(QNetworkAccessManager *nam, const QNetworkRequest &req,
                    const QByteArray &verb, const QByteArray &body, int *status,
                    QString *error) {
    QEventLoop loop;
    QNetworkReply *reply = nullptr;
    if (verb == "GET")
        reply = nam->get(req);
    else if (verb == "PATCH")
        reply = nam->sendCustomRequest(req, "PATCH", body);
    else
        reply = nam->post(req, body);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(20000);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
        if (error)
            *error = QStringLiteral("Google hat nicht geantwortet.");
        reply->deleteLater();
        return {};
    }
    if (status)
        *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    if (reply->error() != QNetworkReply::NoError && error && error->isEmpty())
        *error = reply->errorString();
    reply->deleteLater();
    return data;
}

QString accessToken(QNetworkAccessManager *nam, QString *error) {
    QSettings s = blopSettings();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString access = s.value(cloudKey("accessToken")).toString();
    if (!access.isEmpty() && s.value(cloudKey("expiresAt")).toLongLong() > now + 60000)
        return access;
    const QString refresh = s.value(cloudKey("refreshToken")).toString();
    if (refresh.isEmpty()) {
        if (error)
            *error = QStringLiteral("Google ist nicht verbunden.");
        return {};
    }
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    body.addQueryItem(QStringLiteral("refresh_token"), refresh);
    body.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(kClientId));
    const QByteArray secret = googleSecret();
    if (!secret.isEmpty())
        body.addQueryItem(QStringLiteral("client_secret"), QString::fromUtf8(secret));
    QNetworkRequest req(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    int status = 0;
    const QByteArray raw = httpBody(nam, req, "POST",
                                    body.toString(QUrl::FullyEncoded).toUtf8(),
                                    &status, error);
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    const QString token = obj.value(QStringLiteral("access_token")).toString();
    if (token.isEmpty()) {
        if (error && error->isEmpty())
            *error = QStringLiteral("Google-Token konnte nicht erneuert werden.");
        return {};
    }
    storeTokens(obj);
    return token;
}

QString folderId(QNetworkAccessManager *nam, const QString &token, QString *error) {
    QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"),
                   QStringLiteral("name='BlopNotizen' and mimeType='application/vnd.google-apps.folder' and trashed=false"));
    q.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name)"));
    q.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("1"));
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    int status = 0;
    const QByteArray raw = httpBody(nam, req, "GET", {}, &status, error);
    const QJsonArray files =
        QJsonDocument::fromJson(raw).object().value(QStringLiteral("files")).toArray();
    if (files.isEmpty()) {
        if (error)
            *error = QStringLiteral("Drive-Ordner BlopNotizen fehlt. Einmal in Blop verbinden.");
        return {};
    }
    return files.at(0).toObject().value(QStringLiteral("id")).toString();
}

QString fileId(QNetworkAccessManager *nam, const QString &token, const QString &folder,
               const QString &name, QString *error) {
    QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"),
                   QStringLiteral("name='%1' and '%2' in parents and trashed=false")
                       .arg(name, folder));
    q.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id)"));
    q.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("1"));
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    int status = 0;
    const QByteArray raw = httpBody(nam, req, "GET", {}, &status, error);
    const QJsonArray files =
        QJsonDocument::fromJson(raw).object().value(QStringLiteral("files")).toArray();
    if (files.isEmpty())
        return {};
    return files.at(0).toObject().value(QStringLiteral("id")).toString();
}

void rememberBlob(const QJsonObject &obj) {
    if (obj.isEmpty())
        return;
    QSettings s = blopSettings();
    s.setValue(QStringLiteral("ui/tool_hotkeys"),
               QJsonDocument(obj).toJson(QJsonDocument::Compact));
    QSaveFile file(SettingsSync::settingsPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.commit();
}

} // namespace

QString SettingsSync::settingsPath() {
    return NoteWriter::libraryRoot() + QStringLiteral("/assistent-einstellungen.json");
}

QJsonObject SettingsSync::load() {
    QFile file(settingsPath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
        if (!obj.isEmpty())
            return obj;
    }
    const QByteArray raw =
        blopSettings().value(QStringLiteral("ui/tool_hotkeys")).toByteArray();
    return QJsonDocument::fromJson(raw).object();
}

QString SettingsSync::hotkeyFor(const QString &id) {
    const QJsonObject row =
        load().value(QStringLiteral("bindings")).toObject().value(id).toObject();
    return row.value(QStringLiteral("keys")).toString();
}

QString SettingsSync::openRouterModel() {
    const QString model = load().value(QStringLiteral("openrouterModel")).toString().trimmed();
    return model.isEmpty() ? QStringLiteral("openai/gpt-4o-mini") : model;
}

QString SettingsSync::openRouterKey() {
    return load().value(QStringLiteral("openrouterKey")).toString().trimmed();
}

void SettingsSync::setOpenRouter(const QString &model, const QString &key) {
    QJsonObject blob = load();
    if (blob.isEmpty())
        blob.insert(QStringLiteral("version"), 1);
    blob.insert(QStringLiteral("openrouterModel"), model.trimmed());
    blob.insert(QStringLiteral("openrouterKey"), key.trimmed());
    rememberBlob(blob);
}

bool SettingsSync::signedIn() {
    return !blopSettings().value(cloudKey("refreshToken")).toString().isEmpty() ||
           !blopSettings().value(cloudKey("accessToken")).toString().isEmpty();
}

QString SettingsSync::signIn(QString *error) {
    QTcpServer server;
    if (!server.listen(QHostAddress(QStringLiteral("127.0.0.1")), kPort)) {
        if (error)
            *error = QStringLiteral(
                "Port 27185 ist belegt. Schließ die Google-Anmeldung in Blop und versuch es noch einmal.");
        return {};
    }
    const QString verifier = randomToken(64);
    const QString state = randomToken(32);
    const QString redirect = QStringLiteral("http://127.0.0.1:%1/").arg(kPort);
    const QByteArray challenge = QCryptographicHash::hash(
        verifier.toUtf8(), QCryptographicHash::Sha256);
    const QString codeChallenge = QString::fromLatin1(
        challenge.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    QUrl auth(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(kClientId));
    q.addQueryItem(QStringLiteral("redirect_uri"), redirect);
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("scope"),
                   QStringLiteral("https://www.googleapis.com/auth/drive"));
    q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    q.addQueryItem(QStringLiteral("state"), state);
    q.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    q.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    auth.setQuery(q);
    QDesktopServices::openUrl(auth);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(120000);
    QString code;
    QString fail;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket *sock = server.nextPendingConnection();
        if (!sock)
            return;
        QObject::connect(sock, &QTcpSocket::readyRead, sock, [&, sock]() {
            const QString req = QString::fromUtf8(sock->readAll());
            const QString path = req.section(QLatin1Char('\n'), 0, 0)
                                     .section(QLatin1Char(' '), 1, 1);
            const QUrlQuery query(QUrl(QStringLiteral("http://127.0.0.1") + path));
            const QByteArray page =
                "<!DOCTYPE html><html><body style='font-family:sans-serif;padding:2rem'>"
                "<h2>Blop Assistent</h2><p>Google ist verbunden. Dieses Fenster kannst "
                "du schlie&szlig;en.</p></body></html>";
            sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                        "Connection: close\r\nContent-Length: " +
                        QByteArray::number(page.size()) + "\r\n\r\n" + page);
            sock->disconnectFromHost();
            if (query.queryItemValue(QStringLiteral("state")) != state ||
                query.queryItemValue(QStringLiteral("code")).isEmpty())
                fail = QStringLiteral("Google-Anmeldung abgebrochen.");
            else
                code = query.queryItemValue(QStringLiteral("code"));
            loop.quit();
        });
    });
    loop.exec();
    server.close();
    if (code.isEmpty()) {
        if (error)
            *error = fail.isEmpty() ? QStringLiteral("Google-Anmeldung abgebrochen.") : fail;
        return {};
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("code"), code);
    payload.insert(QStringLiteral("code_verifier"), verifier);
    payload.insert(QStringLiteral("redirect_uri"), redirect);
    payload.insert(QStringLiteral("client_id"), QString::fromLatin1(kClientId));
    QNetworkAccessManager nam;
    QNetworkRequest ex(QUrl(QString::fromLatin1(kExchangeUrl)));
    ex.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    int status = 0;
    QString httpError;
    const QByteArray raw = httpBody(&nam, ex, "POST",
                                    QJsonDocument(payload).toJson(QJsonDocument::Compact),
                                    &status, &httpError);
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj.value(QStringLiteral("access_token")).toString().isEmpty()) {
        if (error)
            *error = httpError.isEmpty()
                         ? QStringLiteral("Google hat kein Zugriffstoken geliefert.")
                         : httpError;
        return {};
    }
    storeTokens(obj);
    pull(error);
    return obj.value(QStringLiteral("access_token")).toString();
}

bool SettingsSync::pull(QString *error) {
    QNetworkAccessManager nam;
    const QString token = accessToken(&nam, error);
    if (token.isEmpty())
        return false;
    const QString folder = folderId(&nam, token, error);
    if (folder.isEmpty())
        return false;
    const QString id = fileId(&nam, token, folder,
                              QStringLiteral("assistent-einstellungen.json"), error);
    if (id.isEmpty())
        return true;
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(id)));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    int status = 0;
    const QByteArray raw = httpBody(&nam, req, "GET", {}, &status, error);
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj.isEmpty()) {
        if (error && error->isEmpty())
            *error = QStringLiteral("Die Einstellungen von Drive waren leer.");
        return false;
    }
    rememberBlob(obj);
    return true;
}

bool SettingsSync::upload(QString *error) {
    const QString path = settingsPath();
    if (!QFileInfo::exists(path)) {
        QJsonObject blob = load();
        if (blob.isEmpty())
            return false;
        rememberBlob(blob);
    }
    QNetworkAccessManager nam;
    const QString token = accessToken(&nam, error);
    if (token.isEmpty())
        return false;
    const QString folder = folderId(&nam, token, error);
    if (folder.isEmpty())
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = file.readAll();
    const QString name = QStringLiteral("assistent-einstellungen.json");
    const QString existing = fileId(&nam, token, folder, name, error);
    int status = 0;
    if (!existing.isEmpty()) {
        QNetworkRequest req(QUrl(QStringLiteral(
            "https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media")
                                     .arg(existing)));
        req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/octet-stream"));
        httpBody(&nam, req, "PATCH", bytes, &status, error);
        return status >= 200 && status < 300;
    }
    const QByteArray boundary = "blopbound";
    QJsonObject meta;
    meta.insert(QStringLiteral("name"), name);
    meta.insert(QStringLiteral("parents"), QJsonArray{folder});
    QByteArray payload;
    payload += "--" + boundary + "\r\n";
    payload += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
    payload += QJsonDocument(meta).toJson(QJsonDocument::Compact) + "\r\n";
    payload += "--" + boundary + "\r\n";
    payload += "Content-Type: application/octet-stream\r\n\r\n";
    payload += bytes + "\r\n";
    payload += "--" + boundary + "--";
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart")));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("multipart/related; boundary=") +
                      QString::fromLatin1(boundary));
    httpBody(&nam, req, "POST", payload, &status, error);
    return status >= 200 && status < 300;
}
