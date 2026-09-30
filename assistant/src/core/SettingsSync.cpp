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

QString messageFromBody(const QByteArray &raw, const QString &fallback);

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
        *error = messageFromBody(data, reply->errorString());
    reply->deleteLater();
    return data;
}

QString messageFromBody(const QByteArray &raw, const QString &fallback) {
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    QString detail = obj.value(QStringLiteral("error_description")).toString();
    if (detail.isEmpty())
        detail = obj.value(QStringLiteral("error")).toString();
    const QJsonValue bodyDetail = obj.value(QStringLiteral("detail"));
    if (detail.isEmpty() && bodyDetail.isString())
        detail = bodyDetail.toString();
    if (detail.isEmpty() && bodyDetail.isArray() && !bodyDetail.toArray().isEmpty())
        detail = bodyDetail.toArray().at(0).toObject().value(QStringLiteral("msg")).toString();
    if (detail.contains(QStringLiteral("GOOGLE_CLIENT_SECRET")) ||
        detail.contains(QStringLiteral("GOOGLE_DESKTOP_CLIENT_SECRET"))) {
        return QStringLiteral(
            "Der Server hat das Google-Geheimnis nicht. Lege es in "
            "google_desktop_client_secret.txt oder als BLOP_GOOGLE_CLIENT_SECRET ab.");
    }
    if (detail.contains(QStringLiteral("redirect_uri_mismatch"))) {
        return QStringLiteral(
            "Google kennt http://127.0.0.1:27185/ noch nicht. Trag die Adresse "
            "beim Desktop-Client unter „Autorisierte Weiterleitungs-URIs“ ein.");
    }
    if (detail.contains(QStringLiteral("invalid_grant")))
        return QStringLiteral("Der Google-Code ist abgelaufen. Bitte noch einmal anmelden.");
    if (!detail.isEmpty())
        return detail;
    if (fallback.contains(QStringLiteral("server replied:")))
        return QStringLiteral("Der Anmelde-Server hat abgelehnt, ohne eine Erklärung zu schicken.");
    return fallback;
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

qint64 stampOf(const QJsonObject &obj) {
    return obj.value(QStringLiteral("updatedAt")).toVariant().toLongLong();
}

QJsonObject completeBlob(const QJsonObject &src) {
    const struct Row {
        const char *id;
        const char *keys;
        const char *color;
        int width;
    } rows[] = {
        {"pen1", "Ctrl+1", "#000000", 3},
        {"pen2", "Ctrl+2", "#2F6FED", 3},
        {"pen3", "Ctrl+3", "#E23B3B", 3},
        {"marker", "Ctrl+4", nullptr, 0},
        {"pen", "P", nullptr, 0},
        {"eraser", "E", nullptr, 0},
        {"lasso", "V", nullptr, 0},
        {"text", "T", nullptr, 0},
        {"hand", "H", nullptr, 0},
        {"markerKey", "M", nullptr, 0},
    };
    const QJsonObject saved = src.value(QStringLiteral("bindings")).toObject();
    QJsonObject bindings;
    for (const Row &row : rows) {
        const QString id = QString::fromLatin1(row.id);
        const QJsonObject incoming = saved.value(id).toObject();
        QJsonObject out;
        const QString keys = incoming.value(QStringLiteral("keys")).toString().trimmed();
        out.insert(QStringLiteral("keys"),
                   keys.isEmpty() ? QString::fromLatin1(row.keys) : keys);
        if (row.color) {
            const QString color = incoming.value(QStringLiteral("color")).toString().trimmed();
            out.insert(QStringLiteral("color"),
                       color.isEmpty() ? QString::fromLatin1(row.color) : color);
            const int width = incoming.value(QStringLiteral("width")).toInt(row.width);
            out.insert(QStringLiteral("width"), qBound(1, width, 40));
        }
        bindings.insert(id, out);
    }
    QJsonObject out;
    out.insert(QStringLiteral("version"), 1);
    const QString model = src.value(QStringLiteral("openrouterModel")).toString().trimmed();
    out.insert(QStringLiteral("openrouterModel"),
               model.isEmpty() ? QStringLiteral("openai/gpt-4o-mini") : model);
    out.insert(QStringLiteral("openrouterKey"),
               src.value(QStringLiteral("openrouterKey")).toString());
    const QString voice = src.value(QStringLiteral("voiceHotkey")).toString().trimmed();
    out.insert(QStringLiteral("voiceHotkey"),
               voice.isEmpty() ? QStringLiteral("Ctrl+Space") : voice);
    out.insert(QStringLiteral("bindings"), bindings);
    if (stampOf(src) > 0)
        out.insert(QStringLiteral("updatedAt"), stampOf(src));
    return out;
}

QJsonObject readSettingsBlob() {
    return QJsonDocument::fromJson(
               blopSettings().value(QStringLiteral("ui/tool_hotkeys")).toByteArray())
        .object();
}

QJsonObject readFileBlob() {
    QFile file(SettingsSync::settingsPath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject newerRawBlob() {
    const QJsonObject settings = readSettingsBlob();
    const QJsonObject file = readFileBlob();
    if (file.isEmpty())
        return settings;
    if (settings.isEmpty())
        return file;
    return stampOf(file) > stampOf(settings) ? file : settings;
}

void rememberBlob(const QJsonObject &obj, bool stampNow = true) {
    QJsonObject full = completeBlob(obj);
    if (stampNow)
        full.insert(QStringLiteral("updatedAt"), QDateTime::currentMSecsSinceEpoch());
    QSettings s = blopSettings();
    s.setValue(QStringLiteral("ui/tool_hotkeys"),
               QJsonDocument(full).toJson(QJsonDocument::Compact));
    s.sync();
    QSaveFile file(SettingsSync::settingsPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(full).toJson(QJsonDocument::Indented));
    file.commit();
}

} // namespace

QString SettingsSync::settingsPath() {
    return NoteWriter::libraryRoot() + QStringLiteral("/assistent-einstellungen.json");
}

QJsonObject SettingsSync::load() {
    return completeBlob(newerRawBlob());
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

QString SettingsSync::studySessionId() {
    return blopSettings().value(QStringLiteral("session_id")).toString().trimmed();
}

QString SettingsSync::voiceHotkey() {
    const QString keys = load().value(QStringLiteral("voiceHotkey")).toString().trimmed();
    return keys.isEmpty() ? QStringLiteral("Ctrl+Space") : keys;
}

QVector<ToolBinding> SettingsSync::toolBindings() {
    const QJsonObject saved = load().value(QStringLiteral("bindings")).toObject();
    const struct Row {
        const char *id;
        const char *label;
        const char *fallback;
    } rows[] = {
        {"pen1", "Stift 1", "Ctrl+1"},
        {"pen2", "Stift 2", "Ctrl+2"},
        {"pen3", "Stift 3", "Ctrl+3"},
        {"marker", "Textmarker", "Ctrl+4"},
        {"pen", "Stift", "P"},
        {"eraser", "Radierer", "E"},
        {"lasso", "Lasso", "V"},
        {"text", "Text", "T"},
        {"hand", "Hand", "H"},
        {"markerKey", "Marker", "M"},
    };
    QVector<ToolBinding> out;
    for (const Row &row : rows) {
        ToolBinding binding;
        binding.id = QString::fromLatin1(row.id);
        binding.label = QString::fromUtf8(row.label);
        const QString keys = saved.value(binding.id).toObject().value(QStringLiteral("keys")).toString();
        binding.keys = keys.isEmpty() ? QString::fromLatin1(row.fallback) : keys;
        out.append(binding);
    }
    return out;
}

void SettingsSync::setVoiceHotkey(const QString &keys) {
    QJsonObject blob = load();
    if (blob.isEmpty())
        blob.insert(QStringLiteral("version"), 1);
    const QString trimmed = keys.trimmed();
    blob.insert(QStringLiteral("voiceHotkey"),
                trimmed.isEmpty() ? QStringLiteral("Ctrl+Space") : trimmed);
    rememberBlob(blob);
}

void SettingsSync::setToolBinding(const QString &id, const QString &keys) {
    QJsonObject blob = load();
    if (blob.isEmpty())
        blob.insert(QStringLiteral("version"), 1);
    QJsonObject bindings = blob.value(QStringLiteral("bindings")).toObject();
    QJsonObject row = bindings.value(id).toObject();
    row.insert(QStringLiteral("keys"), keys.trimmed());
    bindings.insert(id, row);
    blob.insert(QStringLiteral("bindings"), bindings);
    rememberBlob(blob);
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
    QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj.value(QStringLiteral("access_token")).toString().isEmpty()) {
        const QByteArray secret = googleSecret();
        if (!secret.isEmpty()) {
            QUrlQuery form;
            form.addQueryItem(QStringLiteral("grant_type"),
                              QStringLiteral("authorization_code"));
            form.addQueryItem(QStringLiteral("code"), code);
            form.addQueryItem(QStringLiteral("redirect_uri"), redirect);
            form.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(kClientId));
            form.addQueryItem(QStringLiteral("code_verifier"), verifier);
            form.addQueryItem(QStringLiteral("client_secret"), QString::fromUtf8(secret));
            QNetworkRequest direct(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
            direct.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/x-www-form-urlencoded"));
            int directStatus = 0;
            QString directError;
            const QByteArray directRaw =
                httpBody(&nam, direct, "POST",
                         form.toString(QUrl::FullyEncoded).toUtf8(), &directStatus,
                         &directError);
            obj = QJsonDocument::fromJson(directRaw).object();
            if (obj.value(QStringLiteral("access_token")).toString().isEmpty()) {
                if (error)
                    *error = directError.isEmpty()
                                 ? messageFromBody(raw, httpError)
                                 : messageFromBody(directRaw, directError);
                return {};
            }
        } else {
            if (error)
                *error = httpError.isEmpty()
                             ? QStringLiteral("Google hat kein Zugriffstoken geliefert.")
                             : messageFromBody(raw, httpError);
            return {};
        }
    }
    storeTokens(obj);
    QString pullError;
    if (!pull(&pullError, true) && error)
        *error = pullError;
    return obj.value(QStringLiteral("access_token")).toString();
}

bool SettingsSync::pull(QString *error, bool keepNewerLocal) {
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
    if (keepNewerLocal && stampOf(newerRawBlob()) > stampOf(obj) && stampOf(obj) > 0) {
        if (error)
            *error = QStringLiteral(
                "Die Hotkeys auf diesem Gerät sind neuer und bleiben erhalten.");
        return true;
    }
    rememberBlob(obj, stampOf(obj) <= 0);
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
