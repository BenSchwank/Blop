#include "cloudlink.h"

#include "cloudstoragestore.h"
#include "storageprefs.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QXmlStreamReader>

namespace {

constexpr const char *kOrg = "Blop";
constexpr const char *kApp = "BlopApp";
constexpr const char *kNotes = "BlopNotizen";
constexpr const char *kGoogleClientId =
    "571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com";
// Drive only. Sign-in and Calendar stay on 27183 so both loopbacks can listen
// at once. The Desktop OAuth client must also list http://127.0.0.1:27185/
constexpr quint16 kGooglePort = 27185;
constexpr quint16 kMicrosoftPort = 27184;
constexpr quint16 kDropboxPort = 27186;
// Public Dropbox app key (PKCE). Env / QSettings can override for local apps.
constexpr const char *kDropboxClientId = "76gdb16m8axjskl";
constexpr const char *kExchangeUrl =
    "https://www.blop-study.com/api/auth/google/desktop/exchange";

QString notesName() { return QString::fromLatin1(kNotes); }

/// Drive / Graph timestamps often include fractional seconds + Z.
QDateTime parseCloudTime(const QString &raw) {
  if (raw.isEmpty())
    return {};
  QDateTime dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
  if (!dt.isValid())
    dt = QDateTime::fromString(raw, Qt::ISODate);
  if (!dt.isValid()) {
    // Strip fractional seconds: 2024-01-01T12:00:00.123456Z → …00Z
    QString s = raw;
    const int dot = s.indexOf(QLatin1Char('.'));
    if (dot > 0) {
      int end = dot + 1;
      while (end < s.size() && s.at(end).isDigit())
        ++end;
      s.remove(dot, end - dot);
      dt = QDateTime::fromString(s, Qt::ISODate);
    }
  }
  if (dt.isValid() && !dt.timeZone().isValid())
    dt = dt.toUTC();
  return dt.toUTC();
}

QSettings settings() {
  return QSettings(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
}

QString key(const QString &type, const char *field) {
  return QStringLiteral("cloud/%1/%2").arg(type, QString::fromLatin1(field));
}

QString setting(const QString &type, const char *field) {
  return settings().value(key(type, field)).toString();
}

void setSetting(const QString &type, const char *field, const QString &value) {
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  s.setValue(key(type, field), value);
}

qint64 expiry(const QString &type) {
  return settings().value(key(type, "expiresAt"), 0).toLongLong();
}

void setExpiry(const QString &type, int expiresInSec) {
  const qint64 at = QDateTime::currentMSecsSinceEpoch() +
                    qint64(expiresInSec) * 1000;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  s.setValue(key(type, "expiresAt"), at);
}

QString randomToken(int len) {
  const QByteArray raw = QUuid::createUuid().toByteArray() +
                         QByteArray::number(QDateTime::currentMSecsSinceEpoch());
  QByteArray out = QCryptographicHash::hash(raw, QCryptographicHash::Sha256)
                       .toBase64(QByteArray::Base64UrlEncoding |
                                 QByteArray::OmitTrailingEquals);
  if (out.size() < len)
    out += randomToken(len - out.size()).toLatin1();
  return QString::fromLatin1(out.left(len));
}

QByteArray googleSecret() {
  const QByteArray fromEnv = qgetenv("BLOP_GOOGLE_CLIENT_SECRET");
  if (!fromEnv.trimmed().isEmpty())
    return fromEnv.trimmed();
#ifdef Q_OS_WIN
  const QByteArray appdata = qgetenv("APPDATA");
  if (!appdata.isEmpty()) {
    QFile f(QDir(QString::fromLocal8Bit(appdata))
                .filePath(QStringLiteral(
                    "Blop/BlopApp/google_desktop_client_secret.txt")));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
      return f.readAll().trimmed();
  }
#endif
  QFile f(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
              .filePath(QStringLiteral("google_desktop_client_secret.txt")));
  if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    return f.readAll().trimmed();
  return {};
}

QString microsoftClientId() {
  const QByteArray env = qgetenv("BLOP_MICROSOFT_CLIENT_ID");
  if (!env.trimmed().isEmpty())
    return QString::fromUtf8(env.trimmed());
  return settings().value(QStringLiteral("cloud/onedrive/clientId")).toString().trimmed();
}

QString dropboxClientId() {
  const QByteArray env = qgetenv("BLOP_DROPBOX_CLIENT_ID");
  if (!env.trimmed().isEmpty())
    return QString::fromUtf8(env.trimmed());
  const QString fromSettings =
      settings().value(QStringLiteral("cloud/dropbox/clientId")).toString().trimmed();
  if (!fromSettings.isEmpty())
    return fromSettings;
  return QString::fromLatin1(kDropboxClientId);
}

QString mimeFromFileName(const QString &name) {
  const QString ext = QFileInfo(name).suffix().toLower();
  if (ext == QLatin1String("pdf"))
    return QStringLiteral("application/pdf");
  if (ext == QLatin1String("png"))
    return QStringLiteral("image/png");
  if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
    return QStringLiteral("image/jpeg");
  if (ext == QLatin1String("gif") || ext == QLatin1String("webp") ||
      ext == QLatin1String("bmp"))
    return QStringLiteral("image/") + ext;
  if (ext == QLatin1String("mp4") || ext == QLatin1String("mov") ||
      ext == QLatin1String("webm"))
    return QStringLiteral("video/") + ext;
  if (ext == QLatin1String("mp3") || ext == QLatin1String("wav") ||
      ext == QLatin1String("m4a"))
    return QStringLiteral("audio/") + ext;
  if (ext == QLatin1String("txt") || ext == QLatin1String("md"))
    return QStringLiteral("text/plain");
  if (ext == QLatin1String("html") || ext == QLatin1String("htm"))
    return QStringLiteral("text/html");
  if (ext == QLatin1String("json"))
    return QStringLiteral("application/json");
  if (ext == QLatin1String("zip"))
    return QStringLiteral("application/zip");
  if (ext == QLatin1String("docx") || ext == QLatin1String("doc"))
    return QStringLiteral(
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
  if (ext == QLatin1String("xlsx") || ext == QLatin1String("xls"))
    return QStringLiteral(
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
  if (ext == QLatin1String("pptx") || ext == QLatin1String("ppt"))
    return QStringLiteral(
        "application/vnd.openxmlformats-officedocument.presentationml.presentation");
  if (ext == QLatin1String("bnote") || ext == QLatin1String("blop") ||
      ext == QLatin1String("struct"))
    return QStringLiteral("application/x-blop-note");
  return QStringLiteral("application/octet-stream");
}

QString enc(const QString &s) {
  return QString::fromUtf8(QUrl::toPercentEncoding(s));
}

struct HttpResult {
  int status{0};
  QByteArray body;
  QString error;
};

HttpResult http(QNetworkAccessManager *nam, const QNetworkRequest &req,
                const QByteArray &verb, const QByteArray &body) {
  QNetworkReply *reply = nam->sendCustomRequest(req, verb, body);
  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
  timer.start(25000);
  loop.exec();
  HttpResult r;
  if (timer.isActive()) {
    timer.stop();
    r.status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError && r.status == 0)
      r.error = reply->errorString();
  } else {
    reply->abort();
    r.error = QStringLiteral("Zeitüberschreitung");
  }
  reply->deleteLater();
  return r;
}

QNetworkRequest jsonReq(const QUrl &url, const QString &bearer) {
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/json"));
  if (!bearer.isEmpty())
    req.setRawHeader("Authorization",
                     QByteArray("Bearer ") + bearer.toUtf8());
  req.setTransferTimeout(25000);
  return req;
}

QString googleAccess(QNetworkAccessManager *nam, QString *err) {
  const QString type = QStringLiteral("googledrive");
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const QString access = setting(type, "accessToken");
  if (!access.isEmpty() && expiry(type) > now + 60000)
    return access;
  const QString refresh = setting(type, "refreshToken");
  if (refresh.isEmpty()) {
    if (err)
      *err = QStringLiteral("Google Drive ist nicht verbunden.");
    return {};
  }
  QUrlQuery body;
  body.addQueryItem(QStringLiteral("grant_type"),
                    QStringLiteral("refresh_token"));
  body.addQueryItem(QStringLiteral("refresh_token"), refresh);
  body.addQueryItem(QStringLiteral("client_id"),
                    QString::fromLatin1(kGoogleClientId));
  const QByteArray secret = googleSecret();
  if (!secret.isEmpty())
    body.addQueryItem(QStringLiteral("client_secret"),
                      QString::fromUtf8(secret));
  QNetworkRequest req(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded"));
  const HttpResult r =
      http(nam, req, "POST", body.toString(QUrl::FullyEncoded).toUtf8());
  const QJsonObject obj = QJsonDocument::fromJson(r.body).object();
  const QString token = obj.value(QStringLiteral("access_token")).toString();
  if (token.isEmpty()) {
    if (err)
      *err = QStringLiteral("Google-Token konnte nicht erneuert werden.");
    return {};
  }
  setSetting(type, "accessToken", token);
  setExpiry(type, obj.value(QStringLiteral("expires_in")).toInt(3600));
  return token;
}

QString googleFolderId(QNetworkAccessManager *nam, const QString &token,
                       QString *err) {
  const QString cached = setting(QStringLiteral("googledrive"), "folderId");
  auto search = [&](const QString &q) -> QString {
    QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), q);
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name)"));
    query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("drive"));
    url.setQuery(query);
    const HttpResult r = http(nam, jsonReq(url, token), "GET", {});
    const QJsonArray files =
        QJsonDocument::fromJson(r.body).object().value(QStringLiteral("files")).toArray();
    if (!files.isEmpty())
      return files.first().toObject().value(QStringLiteral("id")).toString();
    return {};
  };
  if (!cached.isEmpty()) {
    const QString still = search(
        QStringLiteral("trashed=false and id='%1'").arg(cached));
    if (!still.isEmpty())
      return cached;
  }
  const QString found = search(QStringLiteral(
      "mimeType='application/vnd.google-apps.folder' and "
      "name='BlopNotizen' and trashed=false"));
  if (!found.isEmpty()) {
    setSetting(QStringLiteral("googledrive"), "folderId", found);
    return found;
  }
  QJsonObject meta;
  meta.insert(QStringLiteral("name"), notesName());
  meta.insert(QStringLiteral("mimeType"),
              QStringLiteral("application/vnd.google-apps.folder"));
  const HttpResult created = http(
      nam,
      jsonReq(QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files")),
              token),
      "POST", QJsonDocument(meta).toJson(QJsonDocument::Compact));
  const QString id =
      QJsonDocument::fromJson(created.body).object().value(QStringLiteral("id")).toString();
  if (id.isEmpty()) {
    if (err)
      *err = QStringLiteral(
          "Drive-Ordner BlopNotizen konnte nicht angelegt werden. "
          "In der Google Cloud Console muss die Drive API für diesen "
          "OAuth-Client aktiv sein.");
    return {};
  }
  setSetting(QStringLiteral("googledrive"), "folderId", id);
  return id;
}

QString googleFileId(QNetworkAccessManager *nam, const QString &token,
                     const QString &folderId, const QString &name) {
  QString safe = name;
  safe.replace(QLatin1Char('\''), QStringLiteral("\\'"));
  QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
  QUrlQuery query;
  query.addQueryItem(
      QStringLiteral("q"),
      QStringLiteral("'%1' in parents and name='%2' and trashed=false")
          .arg(folderId, safe));
  query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id)"));
  url.setQuery(query);
  const HttpResult r = http(nam, jsonReq(url, token), "GET", {});
  const QJsonArray files =
      QJsonDocument::fromJson(r.body).object().value(QStringLiteral("files")).toArray();
  if (files.isEmpty())
    return {};
  return files.first().toObject().value(QStringLiteral("id")).toString();
}

QString oneDriveAccess(QNetworkAccessManager *nam, QString *err) {
  const QString type = QStringLiteral("onedrive");
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const QString access = setting(type, "accessToken");
  if (!access.isEmpty() && expiry(type) > now + 60000)
    return access;
  const QString refresh = setting(type, "refreshToken");
  const QString client = microsoftClientId();
  if (refresh.isEmpty() || client.isEmpty()) {
    if (err)
      *err = QStringLiteral("OneDrive ist nicht verbunden.");
    return {};
  }
  QUrlQuery body;
  body.addQueryItem(QStringLiteral("client_id"), client);
  body.addQueryItem(QStringLiteral("grant_type"),
                    QStringLiteral("refresh_token"));
  body.addQueryItem(QStringLiteral("refresh_token"), refresh);
  body.addQueryItem(QStringLiteral("redirect_uri"),
                    QStringLiteral("http://127.0.0.1:%1/").arg(kMicrosoftPort));
  body.addQueryItem(QStringLiteral("scope"),
                    QStringLiteral("offline_access Files.ReadWrite"));
  QNetworkRequest req(QUrl(QStringLiteral(
      "https://login.microsoftonline.com/common/oauth2/v2.0/token")));
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded"));
  const HttpResult r =
      http(nam, req, "POST", body.toString(QUrl::FullyEncoded).toUtf8());
  const QJsonObject obj = QJsonDocument::fromJson(r.body).object();
  const QString token = obj.value(QStringLiteral("access_token")).toString();
  if (token.isEmpty()) {
    if (err)
      *err = QStringLiteral("OneDrive-Token konnte nicht erneuert werden.");
    return {};
  }
  setSetting(type, "accessToken", token);
  if (!obj.value(QStringLiteral("refresh_token")).toString().isEmpty())
    setSetting(type, "refreshToken",
               obj.value(QStringLiteral("refresh_token")).toString());
  setExpiry(type, obj.value(QStringLiteral("expires_in")).toInt(3600));
  return token;
}

QString dropboxAccess(QNetworkAccessManager *nam, QString *err) {
  const QString type = QStringLiteral("dropbox");
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const QString access = setting(type, "accessToken");
  if (!access.isEmpty() && expiry(type) > now + 60000)
    return access;
  const QString refresh = setting(type, "refreshToken");
  const QString client = dropboxClientId();
  if (refresh.isEmpty() || client.isEmpty()) {
    if (err)
      *err = QStringLiteral("Dropbox ist nicht verbunden.");
    return {};
  }
  QUrlQuery body;
  body.addQueryItem(QStringLiteral("client_id"), client);
  body.addQueryItem(QStringLiteral("grant_type"),
                    QStringLiteral("refresh_token"));
  body.addQueryItem(QStringLiteral("refresh_token"), refresh);
  QNetworkRequest req(
      QUrl(QStringLiteral("https://api.dropboxapi.com/oauth2/token")));
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded"));
  const HttpResult r =
      http(nam, req, "POST", body.toString(QUrl::FullyEncoded).toUtf8());
  const QJsonObject obj = QJsonDocument::fromJson(r.body).object();
  const QString token = obj.value(QStringLiteral("access_token")).toString();
  if (token.isEmpty()) {
    if (err)
      *err = QStringLiteral("Dropbox-Token konnte nicht erneuert werden.");
    return {};
  }
  setSetting(type, "accessToken", token);
  setExpiry(type, obj.value(QStringLiteral("expires_in")).toInt(14400));
  return token;
}

bool oneDriveEnsureFolder(QNetworkAccessManager *nam, const QString &token,
                          QString *err) {
  const QUrl existing(QStringLiteral(
      "https://graph.microsoft.com/v1.0/me/drive/root:/BlopNotizen"));
  const HttpResult got = http(nam, jsonReq(existing, token), "GET", {});
  if (got.status >= 200 && got.status < 300)
    return true;
  QJsonObject meta;
  meta.insert(QStringLiteral("name"), notesName());
  meta.insert(QStringLiteral("folder"), QJsonObject());
  meta.insert(QStringLiteral("@microsoft.graph.conflictBehavior"),
              QStringLiteral("fail"));
  const HttpResult made = http(
      nam,
      jsonReq(QUrl(QStringLiteral(
                  "https://graph.microsoft.com/v1.0/me/drive/root/children")),
              token),
      "POST", QJsonDocument(meta).toJson(QJsonDocument::Compact));
  if (made.status == 409 || (made.status >= 200 && made.status < 300))
    return true;
  if (err)
    *err = QStringLiteral("OneDrive-Ordner BlopNotizen konnte nicht angelegt werden.");
  return false;
}

QString davUserRoot() {
  const QString server = setting(QStringLiteral("nextcloud"), "server");
  const QString user = setting(QStringLiteral("nextcloud"), "user");
  return server + QStringLiteral("/remote.php/dav/files/") + enc(user);
}

/// BlopNotizen folder — used for note sync uploads, not for browse.
QString davBase() {
  return davUserRoot() + QLatin1Char('/') + enc(notesName());
}

QString davUrlForRelative(const QString &relativePath) {
  QString url = davUserRoot();
  const QStringList parts =
      relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString &p : parts)
    url += QLatin1Char('/') + enc(p);
  return url;
}

QNetworkRequest davReq(const QUrl &url) {
  QNetworkRequest req(url);
  const QByteArray cred =
      setting(QStringLiteral("nextcloud"), "user").toUtf8() + ':' +
      setting(QStringLiteral("nextcloud"), "appPassword").toUtf8();
  req.setRawHeader("Authorization", "Basic " + cred.toBase64());
  req.setTransferTimeout(25000);
  return req;
}

bool davEnsure(QNetworkAccessManager *nam, QString *err) {
  const HttpResult r = http(nam, davReq(QUrl(davBase())), "MKCOL", {});
  if (r.status == 201 || r.status == 405 || r.status == 200 || r.status == 204)
    return true;
  if (err)
    *err = QStringLiteral("Nextcloud-Ordner BlopNotizen konnte nicht angelegt werden.");
  return false;
}

QString icloudFolder() {
  const QString home =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  const QStringList candidates = {
      home + QStringLiteral("/iCloudDrive"),
      home + QStringLiteral("/iCloud Drive"),
      home + QStringLiteral("/Library/Mobile Documents/com~apple~CloudDocs"),
  };
  for (const QString &c : candidates) {
    if (QDir(c).exists())
      return c;
  }
  return {};
}

QString icloudLinkedRoot() {
  for (const CloudStorageEntry &e : CloudStorageStore::load()) {
    if ((e.id == QLatin1String("icloud") || e.type == QLatin1String("icloud")) &&
        StoragePrefs::isUsableFilesystemDir(e.path))
      return e.path;
  }
  return icloudFolder();
}

QString trimServer(QString url) {
  url = url.trimmed();
  while (url.endsWith(QLatin1Char('/')))
    url.chop(1);
  if (url.endsWith(QLatin1String("/index.php")))
    url.chop(10);
  return url;
}

} // namespace

CloudLinkHub &CloudLinkHub::instance() {
  static CloudLinkHub hub;
  return hub;
}

CloudLinkHub::CloudLinkHub(QObject *parent) : QObject(parent) {
  m_nam = new QNetworkAccessManager(this);
  m_ncTimer = new QTimer(this);
  m_ncTimer->setInterval(2000);
  connect(m_ncTimer, &QTimer::timeout, this, &CloudLinkHub::pollNextcloud);
}

void CloudLinkHub::fail(const QString &type, const QString &detail) {
  m_busy = false;
  m_lastError = detail;
  emit connectFinished(type, false, detail);
}

void CloudLinkHub::succeed(const QString &type, const QString &detail) {
  m_busy = false;
  m_lastError.clear();
  emit connectFinished(type, true, detail);
}

void CloudLinkHub::markApi(const QString &type) {
  QVector<CloudStorageEntry> entries = CloudStorageStore::load();
  CloudStorageEntry *cur = CloudStorageStore::findMutable(entries, type);
  if (!cur) {
    CloudStorageEntry e;
    e.id = type;
    e.type = type;
    e.name = CloudStorageStore::displayNameForType(type);
    e.apiConnected = true;
    entries.append(e);
  } else {
    cur->type = type;
    cur->apiConnected = true;
    if (cur->name.isEmpty())
      cur->name = CloudStorageStore::displayNameForType(type);
  }
  CloudStorageStore::save(entries);
  StoragePrefs::setPrimaryCloudId(type);
  StoragePrefs::setMode(StoragePrefs::Mode::LocalAndCloud);
}

bool CloudLinkHub::providerReady(const QString &type) const {
  if (type == QLatin1String("googledrive") ||
      type == QLatin1String("onedrive") || type == QLatin1String("dropbox")) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!setting(type, "refreshToken").isEmpty())
      return true;
    return !setting(type, "accessToken").isEmpty() && expiry(type) > now;
  }
  if (type == QLatin1String("nextcloud")) {
    return !setting(type, "server").isEmpty() &&
           !setting(type, "user").isEmpty() &&
           !setting(type, "appPassword").isEmpty();
  }
  if (type == QLatin1String("icloud")) {
    for (const CloudStorageEntry &e : CloudStorageStore::load()) {
      if ((e.id == type || e.type == type) &&
          StoragePrefs::isUsableFilesystemDir(e.path))
        return true;
    }
    return !icloudFolder().isEmpty();
  }
  return false;
}

bool CloudLinkHub::primaryUsesApi() const {
  const QString id = StoragePrefs::primaryCloudId();
  if (!CloudStorageStore::supportsApiBrowse(id))
    return false;
  if (!providerReady(id))
    return false;
  for (const CloudStorageEntry &e : CloudStorageStore::load()) {
    if (e.id == id && e.apiConnected)
      return true;
  }
  return false;
}

void CloudLinkHub::connectProvider(const QString &type, QWidget *parent) {
#ifdef Q_OS_ANDROID
  Q_UNUSED(parent);
  fail(type, QStringLiteral(
                 "Auf dem Handy bleiben Notizen auf dem Gerät. "
                 "Cloud-APIs werden hier nicht hochgeladen."));
  return;
#else
  if (m_busy) {
    emit connectFinished(
        type, false, QStringLiteral("Eine Verbindung läuft schon."));
    return;
  }
  if (type == QLatin1String("googledrive"))
    beginGoogle();
  else if (type == QLatin1String("onedrive"))
    beginOneDrive();
  else if (type == QLatin1String("dropbox"))
    beginDropbox();
  else if (type == QLatin1String("nextcloud"))
    beginNextcloud(parent);
  else if (type == QLatin1String("icloud"))
    beginIcloud();
  else
    fail(type, QStringLiteral(
                   "Für diesen Anbieter gibt es noch keine API. "
                   "Nutze „Ordner“, wenn ein Sync-Client installiert ist."));
#endif
}

void CloudLinkHub::beginIcloud() {
  const QString folder = icloudFolder();
  if (folder.isEmpty()) {
    fail(QStringLiteral("icloud"),
         QStringLiteral(
             "Kein iCloud-Ordner gefunden. Installiere iCloud für Windows, "
             "melde dich an und versuche es erneut. Eine iCloud-Upload-API "
             "gibt es auf Windows nicht."));
    return;
  }
  if (!StoragePrefs::connectProviderForNotes(QStringLiteral("icloud"), folder)) {
    fail(QStringLiteral("icloud"),
         QStringLiteral("Der iCloud-Ordner konnte nicht verknüpft werden."));
    return;
  }
  markApi(QStringLiteral("icloud"));
  succeed(QStringLiteral("icloud"),
          QStringLiteral(
              "iCloud verknüpft. Dateien erscheinen in der Bibliothek "
              "(Sync-Notizen weiter unter BlopNotizen). Ordner: %1")
              .arg(folder));
}

void CloudLinkHub::beginNextcloud(QWidget *parent) {
  bool ok = false;
  const QString typed = QInputDialog::getText(
      parent, QStringLiteral("Nextcloud"),
      QStringLiteral("Adresse deines Nextcloud-Servers"), QLineEdit::Normal,
      setting(QStringLiteral("nextcloud"), "server"), &ok);
  if (!ok || typed.trimmed().isEmpty()) {
    fail(QStringLiteral("nextcloud"), QStringLiteral("Abgebrochen."));
    return;
  }
  const QString server = trimServer(typed);
  const QUrl base(server);
  if (!base.isValid() || base.scheme().isEmpty()) {
    fail(QStringLiteral("nextcloud"),
         QStringLiteral("Die Adresse braucht http:// oder https://."));
    return;
  }
  m_busy = true;
  m_ncServer = server;
  // Login Flow v2. Current Nextcloud servers reject the old /login/flow path.
  QNetworkRequest req(QUrl(server + QStringLiteral("/index.php/login/v2")));
  req.setRawHeader("OCS-APIRequest", "true");
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BlopDesktop"));
  req.setTransferTimeout(20000);
  QNetworkReply *reply = m_nam->post(req, QByteArray());
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    const QJsonObject poll = obj.value(QStringLiteral("poll")).toObject();
    m_ncEndpoint = poll.value(QStringLiteral("endpoint")).toString();
    m_ncToken = poll.value(QStringLiteral("token")).toString();
    const QString login = obj.value(QStringLiteral("login")).toString();
    if (status >= 400 || m_ncEndpoint.isEmpty() || m_ncToken.isEmpty() ||
        login.isEmpty()) {
      fail(QStringLiteral("nextcloud"),
           QStringLiteral(
               "Nextcloud Login Flow wurde von diesem Server nicht angenommen."));
      return;
    }
    QDesktopServices::openUrl(QUrl(login));
    m_ncPolls = 0;
    m_ncTimer->start();
  });
}

void CloudLinkHub::pollNextcloud() {
  if (m_ncEndpoint.isEmpty()) {
    m_ncTimer->stop();
    return;
  }
  if (++m_ncPolls > 150) {
    m_ncTimer->stop();
    fail(QStringLiteral("nextcloud"),
         QStringLiteral("Nextcloud-Anmeldung hat zu lange gedauert."));
    return;
  }
  QNetworkRequest req{QUrl(m_ncEndpoint)};
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/x-www-form-urlencoded"));
  req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("BlopDesktop"));
  QUrlQuery body;
  body.addQueryItem(QStringLiteral("token"), m_ncToken);
  QNetworkReply *reply =
      m_nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    reply->deleteLater();
    if (status == 404)
      return;
    m_ncTimer->stop();
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    const QString server = obj.value(QStringLiteral("server")).toString();
    const QString user = obj.value(QStringLiteral("loginName")).toString();
    const QString pass = obj.value(QStringLiteral("appPassword")).toString();
    if (status >= 400 || user.isEmpty() || pass.isEmpty()) {
      fail(QStringLiteral("nextcloud"),
           QStringLiteral("Nextcloud hat keine Zugangsdaten geliefert."));
      return;
    }
    QTimer::singleShot(0, this, [this, server, user, pass]() {
      finishNextcloud(server.isEmpty() ? m_ncServer : trimServer(server), user,
                      pass);
    });
  });
}

void CloudLinkHub::finishNextcloud(const QString &server, const QString &user,
                                   const QString &appPassword) {
  setSetting(QStringLiteral("nextcloud"), "server", trimServer(server));
  setSetting(QStringLiteral("nextcloud"), "user", user);
  setSetting(QStringLiteral("nextcloud"), "appPassword", appPassword);
  QString err;
  if (!davEnsure(m_nam, &err)) {
    fail(QStringLiteral("nextcloud"), err);
    return;
  }
  markApi(QStringLiteral("nextcloud"));
  succeed(QStringLiteral("nextcloud"),
          QStringLiteral(
              "Nextcloud verbunden. Dateien erscheinen in der Bibliothek "
              "(Sync-Notizen weiter unter BlopNotizen)."));
}

void CloudLinkHub::beginGoogle() {
  if (m_loopback)
    m_loopback->close();
  delete m_loopback;
  m_loopback = new QTcpServer(this);
  if (!m_loopback->listen(QHostAddress(QStringLiteral("127.0.0.1")),
                          kGooglePort)) {
    fail(QStringLiteral("googledrive"),
         QStringLiteral(
             "Port 27185 ist belegt. Die Google-Anmeldung nutzt 27183 und "
             "blockiert Drive nicht mehr. Beende das Programm, das 27185 hält, "
             "und versuche es erneut."));
    return;
  }
  m_busy = true;
  m_verifier = randomToken(64);
  m_state = randomToken(32);
  m_redirect = QStringLiteral("http://127.0.0.1:%1/").arg(kGooglePort);
  const QByteArray challenge = QCryptographicHash::hash(
      m_verifier.toUtf8(), QCryptographicHash::Sha256);
  const QString codeChallenge =
      QString::fromLatin1(challenge.toBase64(QByteArray::Base64UrlEncoding |
                                             QByteArray::OmitTrailingEquals));
  connect(m_loopback, &QTcpServer::newConnection, this, [this]() {
    QTcpSocket *sock = m_loopback ? m_loopback->nextPendingConnection() : nullptr;
    if (!sock)
      return;
    auto handle = [this, sock]() {
      const QByteArray raw = sock->readAll();
      const QString req = QString::fromUtf8(raw);
      const QString first = req.section(QLatin1Char('\n'), 0, 0).trimmed();
      const QString path = first.section(QLatin1Char(' '), 1, 1);
      const QUrlQuery q(QUrl(QStringLiteral("http://127.0.0.1") + path));
      const QByteArray page =
          "<!DOCTYPE html><html><body style='font-family:sans-serif;padding:2rem'>"
          "<h2>Blop</h2><p>Google Drive ist verbunden. Dieses Fenster kannst "
          "du schlie&szlig;en.</p></body></html>";
      sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                  "Connection: close\r\nContent-Length: " +
                  QByteArray::number(page.size()) + "\r\n\r\n" + page);
      sock->waitForBytesWritten(2000);
      sock->disconnectFromHost();
      sock->deleteLater();
      if (m_loopback)
        m_loopback->close();
      const QString err = q.queryItemValue(QStringLiteral("error"));
      const QString state = q.queryItemValue(QStringLiteral("state"));
      const QString code = q.queryItemValue(QStringLiteral("code"));
      if (!err.isEmpty() || state != m_state || code.isEmpty()) {
        fail(QStringLiteral("googledrive"),
             err.isEmpty() ? QStringLiteral("Google-Anmeldung abgebrochen.")
                           : err);
        return;
      }
      QJsonObject payload;
      payload.insert(QStringLiteral("code"), code);
      payload.insert(QStringLiteral("code_verifier"), m_verifier);
      payload.insert(QStringLiteral("redirect_uri"), m_redirect);
      payload.insert(QStringLiteral("client_id"),
                     QString::fromLatin1(kGoogleClientId));
      QNetworkRequest ex(QUrl(QString::fromLatin1(kExchangeUrl)));
      ex.setHeader(QNetworkRequest::ContentTypeHeader,
                   QStringLiteral("application/json"));
      QNetworkReply *reply = m_nam->post(
          ex, QJsonDocument(payload).toJson(QJsonDocument::Compact));
      connect(reply, &QNetworkReply::finished, this, [this, reply, code]() {
        const QByteArray body = reply->readAll();
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        auto store = [this](const QJsonObject &obj) {
          const QString access =
              obj.value(QStringLiteral("access_token")).toString();
          const QString refresh =
              obj.value(QStringLiteral("refresh_token")).toString();
          if (access.isEmpty()) {
            fail(QStringLiteral("googledrive"),
                 QStringLiteral("Google hat kein Zugriffstoken geliefert."));
            return;
          }
          setSetting(QStringLiteral("googledrive"), "accessToken", access);
          if (!refresh.isEmpty())
            setSetting(QStringLiteral("googledrive"), "refreshToken", refresh);
          setExpiry(QStringLiteral("googledrive"),
                    obj.value(QStringLiteral("expires_in")).toInt(3600));
          QTimer::singleShot(0, this, [this]() {
            QString err;
            const QString token = googleAccess(m_nam, &err);
            if (token.isEmpty() || googleFolderId(m_nam, token, &err).isEmpty()) {
              fail(QStringLiteral("googledrive"),
                   err.isEmpty()
                       ? QStringLiteral("Drive-Ordner fehlgeschlagen.")
                       : err);
              return;
            }
            markApi(QStringLiteral("googledrive"));
            succeed(QStringLiteral("googledrive"),
                    QStringLiteral(
                        "Google Drive verbunden. Dateien erscheinen in der "
                        "Bibliothek (Sync-Notizen weiter unter BlopNotizen)."));
          });
        };
        if (status >= 200 && status < 300) {
          store(QJsonDocument::fromJson(body).object());
          return;
        }
        if (QString::fromUtf8(body).contains(
                QLatin1String("redirect_uri_mismatch"))) {
          fail(QStringLiteral("googledrive"),
               QStringLiteral(
                   "Google kennt http://127.0.0.1:27185/ noch nicht. "
                   "Trag die Adresse beim Desktop-OAuth-Client unter "
                   "„Autorisierte Weiterleitungs-URIs“ ein. Die Anmeldung "
                   "bleibt auf http://127.0.0.1:27183/."));
          return;
        }
        const QByteArray secret = googleSecret();
        if (secret.isEmpty()) {
          fail(QStringLiteral("googledrive"),
               QStringLiteral(
                   "Google-Anmeldung braucht das Desktop-Client-Geheimnis "
                   "(BLOP_GOOGLE_CLIENT_SECRET oder google_desktop_client_secret.txt)."));
          return;
        }
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("grant_type"),
                          QStringLiteral("authorization_code"));
        form.addQueryItem(QStringLiteral("code"), code);
        form.addQueryItem(QStringLiteral("redirect_uri"), m_redirect);
        form.addQueryItem(QStringLiteral("client_id"),
                          QString::fromLatin1(kGoogleClientId));
        form.addQueryItem(QStringLiteral("code_verifier"), m_verifier);
        form.addQueryItem(QStringLiteral("client_secret"),
                          QString::fromUtf8(secret));
        QNetworkRequest treq(
            QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
        treq.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/x-www-form-urlencoded"));
        QNetworkReply *tr = m_nam->post(
            treq, form.toString(QUrl::FullyEncoded).toUtf8());
        connect(tr, &QNetworkReply::finished, this, [this, tr, store]() {
          const QByteArray raw = tr->readAll();
          const int st =
              tr->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
          tr->deleteLater();
          if (st >= 400) {
            const QString text = QString::fromUtf8(raw);
            fail(QStringLiteral("googledrive"),
                 text.contains(QLatin1String("redirect_uri_mismatch"))
                     ? QStringLiteral(
                           "Google kennt http://127.0.0.1:27185/ noch nicht. "
                           "Trag die Adresse beim Desktop-OAuth-Client unter "
                           "„Autorisierte Weiterleitungs-URIs“ ein. Die Anmeldung "
                           "bleibt auf http://127.0.0.1:27183/.")
                     : QStringLiteral("Google-Token-Tausch ist fehlgeschlagen."));
            return;
          }
          store(QJsonDocument::fromJson(raw).object());
        });
      });
    };
    if (sock->bytesAvailable() > 0)
      QTimer::singleShot(0, this, handle);
    else
      connect(sock, &QTcpSocket::readyRead, this, handle,
              Qt::SingleShotConnection);
  });

  QUrl auth(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
  q.addQueryItem(QStringLiteral("client_id"),
                 QString::fromLatin1(kGoogleClientId));
  q.addQueryItem(QStringLiteral("redirect_uri"), m_redirect);
  // Full Drive, not drive.file: Blop must see and manage the user's files,
  // not only the ones this app created.
  q.addQueryItem(QStringLiteral("scope"),
                 QStringLiteral("openid email profile "
                                "https://www.googleapis.com/auth/drive"));
  q.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
  q.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
  q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
  q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
  q.addQueryItem(QStringLiteral("state"), m_state);
  auth.setQuery(q);
  QDesktopServices::openUrl(auth);
}

void CloudLinkHub::beginOneDrive() {
  const QString client = microsoftClientId();
  if (client.isEmpty()) {
    fail(QStringLiteral("onedrive"),
         QStringLiteral(
             "Die Microsoft-App fehlt. Setze BLOP_MICROSOFT_CLIENT_ID "
             "oder den Einstellungswert cloud/onedrive/clientId. "
             "Redirect: http://127.0.0.1:27184/"));
    return;
  }
  if (m_loopback)
    m_loopback->close();
  delete m_loopback;
  m_loopback = new QTcpServer(this);
  if (!m_loopback->listen(QHostAddress(QStringLiteral("127.0.0.1")),
                          kMicrosoftPort)) {
    fail(QStringLiteral("onedrive"),
         QStringLiteral("Port 27184 ist belegt."));
    return;
  }
  m_busy = true;
  m_verifier = randomToken(64);
  m_state = randomToken(32);
  m_redirect = QStringLiteral("http://127.0.0.1:%1/").arg(kMicrosoftPort);
  const QByteArray challenge = QCryptographicHash::hash(
      m_verifier.toUtf8(), QCryptographicHash::Sha256);
  const QString codeChallenge =
      QString::fromLatin1(challenge.toBase64(QByteArray::Base64UrlEncoding |
                                             QByteArray::OmitTrailingEquals));
  connect(m_loopback, &QTcpServer::newConnection, this, [this, client]() {
    QTcpSocket *sock = m_loopback ? m_loopback->nextPendingConnection() : nullptr;
    if (!sock)
      return;
    auto handle = [this, sock, client]() {
      const QString req = QString::fromUtf8(sock->readAll());
      const QString path =
          req.section(QLatin1Char('\n'), 0, 0).section(QLatin1Char(' '), 1, 1);
      const QUrlQuery q(QUrl(QStringLiteral("http://127.0.0.1") + path.trimmed()));
      const QByteArray page =
          "<!DOCTYPE html><html><body style='font-family:sans-serif;padding:2rem'>"
          "<h2>Blop</h2><p>OneDrive ist verbunden. Dieses Fenster kannst du "
          "schlie&szlig;en.</p></body></html>";
      sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                  "Connection: close\r\nContent-Length: " +
                  QByteArray::number(page.size()) + "\r\n\r\n" + page);
      sock->waitForBytesWritten(2000);
      sock->disconnectFromHost();
      sock->deleteLater();
      if (m_loopback)
        m_loopback->close();
      const QString code = q.queryItemValue(QStringLiteral("code"));
      if (q.queryItemValue(QStringLiteral("state")) != m_state || code.isEmpty()) {
        fail(QStringLiteral("onedrive"),
             QStringLiteral("OneDrive-Anmeldung abgebrochen."));
        return;
      }
      QUrlQuery form;
      form.addQueryItem(QStringLiteral("client_id"), client);
      form.addQueryItem(QStringLiteral("grant_type"),
                        QStringLiteral("authorization_code"));
      form.addQueryItem(QStringLiteral("code"), code);
      form.addQueryItem(QStringLiteral("redirect_uri"), m_redirect);
      form.addQueryItem(QStringLiteral("code_verifier"), m_verifier);
      form.addQueryItem(QStringLiteral("scope"),
                        QStringLiteral("offline_access Files.ReadWrite"));
      QNetworkRequest treq(QUrl(QStringLiteral(
          "https://login.microsoftonline.com/common/oauth2/v2.0/token")));
      treq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("application/x-www-form-urlencoded"));
      QNetworkReply *tr =
          m_nam->post(treq, form.toString(QUrl::FullyEncoded).toUtf8());
      connect(tr, &QNetworkReply::finished, this, [this, tr]() {
        const QByteArray raw = tr->readAll();
        const int st =
            tr->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        tr->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        const QString access = obj.value(QStringLiteral("access_token")).toString();
        if (st >= 400 || access.isEmpty()) {
          fail(QStringLiteral("onedrive"),
               QStringLiteral(
                   "OneDrive-Anmeldung fehlgeschlagen. Die Redirect-URI "
                   "http://127.0.0.1:27184/ muss in der Microsoft-App stehen."));
          return;
        }
        setSetting(QStringLiteral("onedrive"), "accessToken", access);
        setSetting(QStringLiteral("onedrive"), "refreshToken",
                   obj.value(QStringLiteral("refresh_token")).toString());
        setExpiry(QStringLiteral("onedrive"),
                  obj.value(QStringLiteral("expires_in")).toInt(3600));
        QTimer::singleShot(0, this, [this]() {
          QString err;
          const QString token = oneDriveAccess(m_nam, &err);
          if (token.isEmpty() || !oneDriveEnsureFolder(m_nam, token, &err)) {
            fail(QStringLiteral("onedrive"),
                 err.isEmpty() ? QStringLiteral("OneDrive-Ordner fehlgeschlagen.")
                               : err);
            return;
          }
          markApi(QStringLiteral("onedrive"));
          succeed(QStringLiteral("onedrive"),
                  QStringLiteral(
                      "OneDrive verbunden. Dateien erscheinen in der "
                      "Bibliothek (Sync-Notizen weiter unter BlopNotizen)."));
        });
      });
    };
    if (sock->bytesAvailable() > 0)
      QTimer::singleShot(0, this, handle);
    else
      connect(sock, &QTcpSocket::readyRead, this, handle,
              Qt::SingleShotConnection);
  });

  QUrl auth(QStringLiteral(
      "https://login.microsoftonline.com/common/oauth2/v2.0/authorize"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("client_id"), client);
  q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
  q.addQueryItem(QStringLiteral("redirect_uri"), m_redirect);
  q.addQueryItem(QStringLiteral("response_mode"), QStringLiteral("query"));
  q.addQueryItem(QStringLiteral("scope"),
                 QStringLiteral("offline_access Files.ReadWrite"));
  q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
  q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
  q.addQueryItem(QStringLiteral("state"), m_state);
  auth.setQuery(q);
  QDesktopServices::openUrl(auth);
}

void CloudLinkHub::beginDropbox() {
  const QString client = dropboxClientId();
  if (client.isEmpty()) {
    fail(QStringLiteral("dropbox"),
         QStringLiteral(
             "Die Dropbox-App fehlt. Setze BLOP_DROPBOX_CLIENT_ID "
             "oder den Einstellungswert cloud/dropbox/clientId. "
             "Redirect: http://127.0.0.1:27186/"));
    return;
  }
  if (m_loopback)
    m_loopback->close();
  delete m_loopback;
  m_loopback = new QTcpServer(this);
  if (!m_loopback->listen(QHostAddress(QStringLiteral("127.0.0.1")),
                          kDropboxPort)) {
    fail(QStringLiteral("dropbox"),
         QStringLiteral("Port 27186 ist belegt."));
    return;
  }
  m_busy = true;
  m_verifier = randomToken(64);
  m_state = randomToken(32);
  m_redirect = QStringLiteral("http://127.0.0.1:%1/").arg(kDropboxPort);
  const QByteArray challenge = QCryptographicHash::hash(
      m_verifier.toUtf8(), QCryptographicHash::Sha256);
  const QString codeChallenge =
      QString::fromLatin1(challenge.toBase64(QByteArray::Base64UrlEncoding |
                                             QByteArray::OmitTrailingEquals));
  connect(m_loopback, &QTcpServer::newConnection, this, [this, client]() {
    QTcpSocket *sock = m_loopback ? m_loopback->nextPendingConnection() : nullptr;
    if (!sock)
      return;
    auto handle = [this, sock, client]() {
      const QString req = QString::fromUtf8(sock->readAll());
      const QString path =
          req.section(QLatin1Char('\n'), 0, 0).section(QLatin1Char(' '), 1, 1);
      const QUrlQuery q(QUrl(QStringLiteral("http://127.0.0.1") + path.trimmed()));
      const QByteArray page =
          "<!DOCTYPE html><html><body style='font-family:sans-serif;padding:2rem'>"
          "<h2>Blop</h2><p>Dropbox ist verbunden. Dieses Fenster kannst du "
          "schlie&szlig;en.</p></body></html>";
      sock->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                  "Connection: close\r\nContent-Length: " +
                  QByteArray::number(page.size()) + "\r\n\r\n" + page);
      sock->waitForBytesWritten(2000);
      sock->disconnectFromHost();
      sock->deleteLater();
      if (m_loopback)
        m_loopback->close();
      const QString code = q.queryItemValue(QStringLiteral("code"));
      if (q.queryItemValue(QStringLiteral("state")) != m_state || code.isEmpty()) {
        fail(QStringLiteral("dropbox"),
             QStringLiteral("Dropbox-Anmeldung abgebrochen."));
        return;
      }
      QUrlQuery form;
      form.addQueryItem(QStringLiteral("client_id"), client);
      form.addQueryItem(QStringLiteral("grant_type"),
                        QStringLiteral("authorization_code"));
      form.addQueryItem(QStringLiteral("code"), code);
      form.addQueryItem(QStringLiteral("redirect_uri"), m_redirect);
      form.addQueryItem(QStringLiteral("code_verifier"), m_verifier);
      QNetworkRequest treq(
          QUrl(QStringLiteral("https://api.dropboxapi.com/oauth2/token")));
      treq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("application/x-www-form-urlencoded"));
      QNetworkReply *tr =
          m_nam->post(treq, form.toString(QUrl::FullyEncoded).toUtf8());
      connect(tr, &QNetworkReply::finished, this, [this, tr]() {
        const QByteArray raw = tr->readAll();
        const int st =
            tr->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        tr->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        const QString access = obj.value(QStringLiteral("access_token")).toString();
        if (st >= 400 || access.isEmpty()) {
          fail(QStringLiteral("dropbox"),
               QStringLiteral(
                   "Dropbox-Anmeldung fehlgeschlagen. Die Redirect-URI "
                   "http://127.0.0.1:27186/ muss in der Dropbox-App stehen."));
          return;
        }
        setSetting(QStringLiteral("dropbox"), "accessToken", access);
        const QString refresh =
            obj.value(QStringLiteral("refresh_token")).toString();
        if (!refresh.isEmpty())
          setSetting(QStringLiteral("dropbox"), "refreshToken", refresh);
        setExpiry(QStringLiteral("dropbox"),
                  obj.value(QStringLiteral("expires_in")).toInt(14400));
        markApi(QStringLiteral("dropbox"));
        succeed(QStringLiteral("dropbox"),
                QStringLiteral(
                    "Dropbox verbunden. Dateien erscheinen in der Bibliothek "
                    "(Sync-Notizen weiter unter BlopNotizen)."));
      });
    };
    if (sock->bytesAvailable() > 0)
      QTimer::singleShot(0, this, handle);
    else
      connect(sock, &QTcpSocket::readyRead, this, handle,
              Qt::SingleShotConnection);
  });

  QUrl auth(QStringLiteral("https://www.dropbox.com/oauth2/authorize"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("client_id"), client);
  q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
  q.addQueryItem(QStringLiteral("redirect_uri"), m_redirect);
  q.addQueryItem(QStringLiteral("token_access_type"), QStringLiteral("offline"));
  q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
  q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
  q.addQueryItem(QStringLiteral("state"), m_state);
  auth.setQuery(q);
  QDesktopServices::openUrl(auth);
}

bool CloudLinkHub::googlePut(const QString &localPath) {
  QString err;
  const QString token = googleAccess(m_nam, &err);
  const QString folder = token.isEmpty() ? QString() : googleFolderId(m_nam, token, &err);
  if (folder.isEmpty()) {
    m_lastError = err;
    return false;
  }
  const QString name = QFileInfo(localPath).fileName();
  QFile f(localPath);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  const QByteArray bytes = f.readAll();
  const QString existing = googleFileId(m_nam, token, folder, name);
  if (!existing.isEmpty()) {
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media")
                                 .arg(existing)));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/octet-stream"));
    const HttpResult r = http(m_nam, req, "PATCH", bytes);
    return r.status >= 200 && r.status < 300;
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
  const HttpResult r = http(m_nam, req, "POST", payload);
  return r.status >= 200 && r.status < 300;
}

bool CloudLinkHub::googleRemove(const QString &fileName) {
  QString err;
  const QString token = googleAccess(m_nam, &err);
  const QString folder = token.isEmpty() ? QString() : googleFolderId(m_nam, token, &err);
  if (folder.isEmpty())
    return false;
  const QString id = googleFileId(m_nam, token, folder, fileName);
  if (id.isEmpty())
    return true;
  const HttpResult r = http(
      m_nam,
      jsonReq(QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                       .arg(id)),
              token),
      "DELETE", {});
  return r.status == 204 || (r.status >= 200 && r.status < 300);
}

bool CloudLinkHub::googleRename(const QString &oldName, const QString &newName) {
  QString err;
  const QString token = googleAccess(m_nam, &err);
  const QString folder = token.isEmpty() ? QString() : googleFolderId(m_nam, token, &err);
  if (folder.isEmpty())
    return false;
  const QString id = googleFileId(m_nam, token, folder, oldName);
  if (id.isEmpty())
    return true;
  QJsonObject meta;
  meta.insert(QStringLiteral("name"), newName);
  const HttpResult r = http(
      m_nam,
      jsonReq(QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                       .arg(id)),
              token),
      "PATCH", QJsonDocument(meta).toJson(QJsonDocument::Compact));
  return r.status >= 200 && r.status < 300;
}

QVector<CloudRemoteItem> CloudLinkHub::googleList(const QString &folderId) {
  QVector<CloudRemoteItem> out;
  QString err;
  const QString token = googleAccess(m_nam, &err);
  if (token.isEmpty()) {
    m_lastError = err;
    return out;
  }
  const QString parent =
      folderId.isEmpty() ? QStringLiteral("root") : folderId;
  QString pageToken;
  do {
    QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
    QUrlQuery query;
    query.addQueryItem(
        QStringLiteral("q"),
        QStringLiteral("'%1' in parents and trashed=false").arg(parent));
    query.addQueryItem(
        QStringLiteral("fields"),
        QStringLiteral("nextPageToken,files(id,name,mimeType,modifiedTime,"
                       "createdTime,size)"));
    query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("drive"));
    query.addQueryItem(QStringLiteral("orderBy"),
                       QStringLiteral("folder,name_natural"));
    if (!pageToken.isEmpty())
      query.addQueryItem(QStringLiteral("pageToken"), pageToken);
    url.setQuery(query);
    const HttpResult r = http(m_nam, jsonReq(url, token), "GET", {});
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Drive-Liste fehlgeschlagen.");
      break;
    }
    const QJsonObject root = QJsonDocument::fromJson(r.body).object();
    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    for (const QJsonValue &v : files) {
      const QJsonObject o = v.toObject();
      CloudRemoteItem item;
      item.id = o.value(QStringLiteral("id")).toString();
      item.name = o.value(QStringLiteral("name")).toString();
      item.mimeType = o.value(QStringLiteral("mimeType")).toString();
      item.isFolder =
          item.mimeType ==
          QLatin1String("application/vnd.google-apps.folder");
      item.modified =
          parseCloudTime(o.value(QStringLiteral("modifiedTime")).toString());
      item.created =
          parseCloudTime(o.value(QStringLiteral("createdTime")).toString());
      if (o.contains(QStringLiteral("size")))
        item.sizeBytes = o.value(QStringLiteral("size")).toString().toLongLong();
      if (!item.id.isEmpty() && !item.name.isEmpty())
        out.append(item);
    }
    pageToken = root.value(QStringLiteral("nextPageToken")).toString();
  } while (!pageToken.isEmpty());
  return out;
}

bool CloudLinkHub::googleFetch(const QString &remoteId,
                               const QString &localPath) {
  if (remoteId.isEmpty() || localPath.isEmpty())
    return false;
  QString err;
  const QString token = googleAccess(m_nam, &err);
  if (token.isEmpty()) {
    m_lastError = err;
    return false;
  }
  QNetworkRequest req(QUrl(
      QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media")
          .arg(remoteId)));
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setTransferTimeout(60000);
  const HttpResult r = http(m_nam, req, "GET", {});
  if (r.status < 200 || r.status >= 300 || r.body.isEmpty()) {
    m_lastError = QStringLiteral("Download von Drive fehlgeschlagen.");
    return false;
  }
  QDir().mkpath(QFileInfo(localPath).absolutePath());
  QFile f(localPath);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(r.body) == r.body.size();
}

bool CloudLinkHub::nextcloudPut(const QString &localPath) {
  QString err;
  if (!davEnsure(m_nam, &err)) {
    m_lastError = err;
    return false;
  }
  QFile f(localPath);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  QNetworkRequest req(QUrl(davBase() + QLatin1Char('/') +
                           enc(QFileInfo(localPath).fileName())));
  const QByteArray cred =
      setting(QStringLiteral("nextcloud"), "user").toUtf8() + ':' +
      setting(QStringLiteral("nextcloud"), "appPassword").toUtf8();
  req.setRawHeader("Authorization", "Basic " + cred.toBase64());
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/octet-stream"));
  const HttpResult r = http(m_nam, req, "PUT", f.readAll());
  return r.status == 201 || r.status == 204 || (r.status >= 200 && r.status < 300);
}

bool CloudLinkHub::nextcloudRemove(const QString &fileName) {
  const HttpResult r =
      http(m_nam, davReq(QUrl(davBase() + QLatin1Char('/') + enc(fileName))),
           "DELETE", {});
  return r.status == 204 || r.status == 404 || (r.status >= 200 && r.status < 300);
}

bool CloudLinkHub::nextcloudRename(const QString &oldName, const QString &newName) {
  QNetworkRequest req(davReq(QUrl(davBase() + QLatin1Char('/') + enc(oldName))));
  req.setRawHeader(
      "Destination",
      (davBase() + QLatin1Char('/') + enc(newName)).toUtf8());
  req.setRawHeader("Overwrite", "T");
  const HttpResult r = http(m_nam, req, "MOVE", {});
  return r.status == 201 || r.status == 204 || r.status == 404;
}

QVector<CloudRemoteItem> CloudLinkHub::nextcloudList(const QString &folderPath) {
  QVector<CloudRemoteItem> out;
  const QString user = setting(QStringLiteral("nextcloud"), "user");
  const QString server = setting(QStringLiteral("nextcloud"), "server");
  if (user.isEmpty() || server.isEmpty()) {
    m_lastError = QStringLiteral("Nextcloud ist nicht verbunden.");
    return out;
  }
  const QString listUrl = davUrlForRelative(folderPath) + QLatin1Char('/');
  QNetworkRequest req = davReq(QUrl(listUrl));
  req.setRawHeader("Depth", "1");
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/xml; charset=utf-8"));
  const QByteArray body =
      "<?xml version=\"1.0\"?>"
      "<d:propfind xmlns:d=\"DAV:\">"
      "<d:prop><d:displayname/><d:getlastmodified/><d:resourcetype/>"
      "<d:getcontenttype/><d:getcontentlength/><d:creationdate/>"
      "</d:prop></d:propfind>";
  const HttpResult r = http(m_nam, req, "PROPFIND", body);
  if (r.status != 207 && (r.status < 200 || r.status >= 300)) {
    m_lastError = QStringLiteral("Nextcloud-Liste fehlgeschlagen.");
    return out;
  }
  QString folderBase = davUrlForRelative(folderPath);
  while (folderBase.endsWith(QLatin1Char('/')))
    folderBase.chop(1);
  const QString davPrefix = davUserRoot();
  QXmlStreamReader xml(r.body);
  CloudRemoteItem cur;
  bool inResponse = false;
  bool isCollection = false;
  QString href;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      const QStringView n = xml.name();
      if (n == QLatin1String("response")) {
        inResponse = true;
        cur = CloudRemoteItem{};
        isCollection = false;
        href.clear();
      } else if (inResponse && n == QLatin1String("href")) {
        href = xml.readElementText();
      } else if (inResponse && n == QLatin1String("displayname")) {
        cur.name = xml.readElementText();
      } else if (inResponse && n == QLatin1String("getlastmodified")) {
        cur.modified =
            QDateTime::fromString(xml.readElementText(), Qt::RFC2822Date);
      } else if (inResponse && n == QLatin1String("creationdate")) {
        cur.created = parseCloudTime(xml.readElementText());
      } else if (inResponse && n == QLatin1String("getcontenttype")) {
        cur.mimeType = xml.readElementText();
      } else if (inResponse && n == QLatin1String("getcontentlength")) {
        bool ok = false;
        const qint64 nBytes = xml.readElementText().toLongLong(&ok);
        if (ok)
          cur.sizeBytes = nBytes;
      } else if (inResponse && n == QLatin1String("collection")) {
        isCollection = true;
      }
    } else if (xml.isEndElement() && xml.name() == QLatin1String("response")) {
      inResponse = false;
      cur.isFolder = isCollection;
      if (cur.isFolder)
        cur.mimeType = QStringLiteral("application/vnd.google-apps.folder");
      else if (cur.mimeType.isEmpty() && !cur.name.isEmpty())
        cur.mimeType = mimeFromFileName(cur.name);
      QString path = QUrl::fromPercentEncoding(href.toUtf8());
      while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
      if (path == folderBase)
        continue;
      if (cur.name.isEmpty())
        cur.name = path.section(QLatin1Char('/'), -1);
      // Relative path from user root for fetch / nested list.
      QString rel = path;
      if (rel.startsWith(davPrefix))
        rel = rel.mid(davPrefix.size());
      while (rel.startsWith(QLatin1Char('/')))
        rel = rel.mid(1);
      cur.id = rel;
      if (!cur.name.isEmpty() && !cur.id.isEmpty())
        out.append(cur);
    }
  }
  return out;
}

bool CloudLinkHub::nextcloudFetch(const QString &remoteId,
                                  const QString &localPath) {
  if (remoteId.isEmpty() || localPath.isEmpty())
    return false;
  const QString user = setting(QStringLiteral("nextcloud"), "user");
  const QString server = setting(QStringLiteral("nextcloud"), "server");
  if (user.isEmpty() || server.isEmpty()) {
    m_lastError = QStringLiteral("Nextcloud ist nicht verbunden.");
    return false;
  }
  QNetworkRequest req = davReq(QUrl(davUrlForRelative(remoteId)));
  req.setTransferTimeout(60000);
  const HttpResult r = http(m_nam, req, "GET", {});
  if (r.status < 200 || r.status >= 300) {
    m_lastError = QStringLiteral("Download von Nextcloud fehlgeschlagen.");
    return false;
  }
  QDir().mkpath(QFileInfo(localPath).absolutePath());
  QFile f(localPath);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(r.body) == r.body.size();
}

bool CloudLinkHub::oneDrivePut(const QString &localPath) {
  QString err;
  const QString token = oneDriveAccess(m_nam, &err);
  if (token.isEmpty() || !oneDriveEnsureFolder(m_nam, token, &err)) {
    m_lastError = err;
    return false;
  }
  QFile f(localPath);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  const QString name = enc(QFileInfo(localPath).fileName());
  QNetworkRequest req(QUrl(QStringLiteral(
      "https://graph.microsoft.com/v1.0/me/drive/root:/BlopNotizen/%1:/content")
                               .arg(name)));
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/octet-stream"));
  const HttpResult r = http(m_nam, req, "PUT", f.readAll());
  return r.status >= 200 && r.status < 300;
}

bool CloudLinkHub::oneDriveRemove(const QString &fileName) {
  QString err;
  const QString token = oneDriveAccess(m_nam, &err);
  if (token.isEmpty())
    return false;
  const HttpResult r = http(
      m_nam,
      jsonReq(QUrl(QStringLiteral(
                       "https://graph.microsoft.com/v1.0/me/drive/root:/"
                       "BlopNotizen/%1")
                       .arg(enc(fileName))),
              token),
      "DELETE", {});
  return r.status == 204 || r.status == 404 || (r.status >= 200 && r.status < 300);
}

bool CloudLinkHub::oneDriveRename(const QString &oldName, const QString &newName) {
  QString err;
  const QString token = oneDriveAccess(m_nam, &err);
  if (token.isEmpty())
    return false;
  QJsonObject meta;
  meta.insert(QStringLiteral("name"), newName);
  const HttpResult r = http(
      m_nam,
      jsonReq(QUrl(QStringLiteral(
                       "https://graph.microsoft.com/v1.0/me/drive/root:/"
                       "BlopNotizen/%1")
                       .arg(enc(oldName))),
              token),
      "PATCH", QJsonDocument(meta).toJson(QJsonDocument::Compact));
  return r.status == 404 || (r.status >= 200 && r.status < 300);
}

QVector<CloudRemoteItem> CloudLinkHub::oneDriveList(const QString &folderId) {
  QVector<CloudRemoteItem> out;
  QString err;
  const QString token = oneDriveAccess(m_nam, &err);
  if (token.isEmpty()) {
    m_lastError = err;
    return out;
  }
  QUrl url(
      folderId.isEmpty()
          ? QUrl(QStringLiteral(
                "https://graph.microsoft.com/v1.0/me/drive/root/children"
                "?$select=id,name,folder,file,size,createdDateTime,"
                "lastModifiedDateTime&$top=200&$orderby=name"))
          : QUrl(QStringLiteral(
                     "https://graph.microsoft.com/v1.0/me/drive/items/"
                     "%1/children?$select=id,name,folder,file,size,"
                     "createdDateTime,lastModifiedDateTime&$top=200"
                     "&$orderby=name")
                     .arg(folderId)));
  for (;;) {
    const HttpResult r = http(m_nam, jsonReq(url, token), "GET", {});
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("OneDrive-Liste fehlgeschlagen.");
      break;
    }
    const QJsonObject root = QJsonDocument::fromJson(r.body).object();
    const QJsonArray values = root.value(QStringLiteral("value")).toArray();
    for (const QJsonValue &v : values) {
      const QJsonObject o = v.toObject();
      CloudRemoteItem item;
      item.id = o.value(QStringLiteral("id")).toString();
      item.name = o.value(QStringLiteral("name")).toString();
      item.isFolder = o.contains(QStringLiteral("folder"));
      item.mimeType = o.value(QStringLiteral("file"))
                          .toObject()
                          .value(QStringLiteral("mimeType"))
                          .toString();
      if (item.isFolder)
        item.mimeType = QStringLiteral("application/vnd.google-apps.folder");
      else if (item.mimeType.isEmpty())
        item.mimeType = mimeFromFileName(item.name);
      item.modified = parseCloudTime(
          o.value(QStringLiteral("lastModifiedDateTime")).toString());
      item.created =
          parseCloudTime(o.value(QStringLiteral("createdDateTime")).toString());
      if (o.contains(QStringLiteral("size")))
        item.sizeBytes =
            static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
      if (!item.id.isEmpty() && !item.name.isEmpty())
        out.append(item);
    }
    const QString next =
        root.value(QStringLiteral("@odata.nextLink")).toString();
    if (next.isEmpty())
      break;
    url = QUrl(next);
  }
  return out;
}

bool CloudLinkHub::oneDriveFetch(const QString &remoteId,
                                 const QString &localPath) {
  if (remoteId.isEmpty() || localPath.isEmpty())
    return false;
  QString err;
  const QString token = oneDriveAccess(m_nam, &err);
  if (token.isEmpty()) {
    m_lastError = err;
    return false;
  }
  QNetworkRequest req(QUrl(
      QStringLiteral(
          "https://graph.microsoft.com/v1.0/me/drive/items/%1/content")
          .arg(remoteId)));
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setTransferTimeout(60000);
  const HttpResult r = http(m_nam, req, "GET", {});
  if (r.status < 200 || r.status >= 300) {
    m_lastError = QStringLiteral("Download von OneDrive fehlgeschlagen.");
    return false;
  }
  QDir().mkpath(QFileInfo(localPath).absolutePath());
  QFile f(localPath);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(r.body) == r.body.size();
}

QVector<CloudRemoteItem> CloudLinkHub::dropboxList(const QString &folderPath) {
  QVector<CloudRemoteItem> out;
  QString err;
  const QString token = dropboxAccess(m_nam, &err);
  if (token.isEmpty()) {
    m_lastError = err;
    return out;
  }
  auto parseEntries = [&](const QJsonArray &entries) {
    for (const QJsonValue &v : entries) {
      const QJsonObject o = v.toObject();
      const QString tag =
          o.value(QStringLiteral(".tag")).toString();
      CloudRemoteItem item;
      item.name = o.value(QStringLiteral("name")).toString();
      item.id = o.value(QStringLiteral("path_display")).toString();
      if (item.id.isEmpty())
        item.id = o.value(QStringLiteral("path_lower")).toString();
      item.isFolder = tag == QLatin1String("folder");
      if (item.isFolder)
        item.mimeType = QStringLiteral("application/vnd.google-apps.folder");
      else
        item.mimeType = mimeFromFileName(item.name);
      item.modified =
          parseCloudTime(o.value(QStringLiteral("client_modified")).toString());
      if (!item.modified.isValid())
        item.modified = parseCloudTime(
            o.value(QStringLiteral("server_modified")).toString());
      if (o.contains(QStringLiteral("size")))
        item.sizeBytes =
            static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
      if (!item.id.isEmpty() && !item.name.isEmpty())
        out.append(item);
    }
  };

  QJsonObject body;
  body.insert(QStringLiteral("path"),
              folderPath.isEmpty() ? QStringLiteral("") : folderPath);
  body.insert(QStringLiteral("recursive"), false);
  body.insert(QStringLiteral("include_media_info"), false);
  body.insert(QStringLiteral("include_deleted"), false);
  HttpResult r = http(
      m_nam,
      jsonReq(QUrl(QStringLiteral(
                  "https://api.dropboxapi.com/2/files/list_folder")),
              token),
      "POST", QJsonDocument(body).toJson(QJsonDocument::Compact));
  if (r.status < 200 || r.status >= 300) {
    m_lastError = QStringLiteral("Dropbox-Liste fehlgeschlagen.");
    return out;
  }
  for (;;) {
    const QJsonObject root = QJsonDocument::fromJson(r.body).object();
    parseEntries(root.value(QStringLiteral("entries")).toArray());
    if (!root.value(QStringLiteral("has_more")).toBool())
      break;
    QJsonObject cont;
    cont.insert(QStringLiteral("cursor"),
                root.value(QStringLiteral("cursor")).toString());
    r = http(m_nam,
             jsonReq(QUrl(QStringLiteral(
                         "https://api.dropboxapi.com/2/files/list_folder/"
                         "continue")),
                     token),
             "POST", QJsonDocument(cont).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Dropbox-Liste fehlgeschlagen.");
      break;
    }
  }
  return out;
}

bool CloudLinkHub::dropboxFetch(const QString &remoteId,
                                const QString &localPath) {
  if (remoteId.isEmpty() || localPath.isEmpty())
    return false;
  QString err;
  const QString token = dropboxAccess(m_nam, &err);
  if (token.isEmpty()) {
    m_lastError = err;
    return false;
  }
  QJsonObject arg;
  arg.insert(QStringLiteral("path"), remoteId);
  QNetworkRequest req(
      QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/download")));
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setRawHeader("Dropbox-API-Arg",
                   QJsonDocument(arg).toJson(QJsonDocument::Compact));
  req.setTransferTimeout(60000);
  const HttpResult r = http(m_nam, req, "POST", {});
  if (r.status < 200 || r.status >= 300) {
    m_lastError = QStringLiteral("Download von Dropbox fehlgeschlagen.");
    return false;
  }
  QDir().mkpath(QFileInfo(localPath).absolutePath());
  QFile f(localPath);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(r.body) == r.body.size();
}

bool CloudLinkHub::dropboxPut(const QString &localPath) {
  QString err;
  const QString token = dropboxAccess(m_nam, &err);
  if (token.isEmpty()) {
    m_lastError = err;
    return false;
  }
  QFile f(localPath);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  const QString name = QFileInfo(localPath).fileName();
  QJsonObject arg;
  arg.insert(QStringLiteral("path"),
             QStringLiteral("/BlopNotizen/") + name);
  arg.insert(QStringLiteral("mode"), QStringLiteral("overwrite"));
  arg.insert(QStringLiteral("autorename"), false);
  QNetworkRequest req(
      QUrl(QStringLiteral("https://content.dropboxapi.com/2/files/upload")));
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setRawHeader("Dropbox-API-Arg",
                   QJsonDocument(arg).toJson(QJsonDocument::Compact));
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/octet-stream"));
  const HttpResult r = http(m_nam, req, "POST", f.readAll());
  return r.status >= 200 && r.status < 300;
}

bool CloudLinkHub::dropboxRemove(const QString &fileName) {
  QString err;
  const QString token = dropboxAccess(m_nam, &err);
  if (token.isEmpty())
    return false;
  QJsonObject body;
  body.insert(QStringLiteral("path"),
              QStringLiteral("/BlopNotizen/") + fileName);
  const HttpResult r = http(
      m_nam,
      jsonReq(QUrl(QStringLiteral("https://api.dropboxapi.com/2/files/delete_v2")),
              token),
      "POST", QJsonDocument(body).toJson(QJsonDocument::Compact));
  return r.status >= 200 && r.status < 300;
}

bool CloudLinkHub::dropboxRename(const QString &oldName, const QString &newName) {
  QString err;
  const QString token = dropboxAccess(m_nam, &err);
  if (token.isEmpty())
    return false;
  QJsonObject body;
  body.insert(QStringLiteral("from_path"),
              QStringLiteral("/BlopNotizen/") + oldName);
  body.insert(QStringLiteral("to_path"),
              QStringLiteral("/BlopNotizen/") + newName);
  const HttpResult r = http(
      m_nam,
      jsonReq(QUrl(QStringLiteral("https://api.dropboxapi.com/2/files/move_v2")),
              token),
      "POST", QJsonDocument(body).toJson(QJsonDocument::Compact));
  return r.status >= 200 && r.status < 300;
}

QVector<CloudRemoteItem> CloudLinkHub::icloudList(const QString &folderPath) {
  QVector<CloudRemoteItem> out;
  const QString root = icloudLinkedRoot();
  if (root.isEmpty() || !QDir(root).exists()) {
    m_lastError = QStringLiteral("iCloud-Ordner ist nicht verknüpft.");
    return out;
  }
  QString abs = root;
  if (!folderPath.isEmpty())
    abs = QDir(root).filePath(folderPath);
  QDir dir(abs);
  if (!dir.exists()) {
    m_lastError = QStringLiteral("Ordner nicht gefunden.");
    return out;
  }
  const QFileInfoList entries = dir.entryInfoList(
      QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
      QDir::DirsFirst | QDir::Name);
  for (const QFileInfo &fi : entries) {
    CloudRemoteItem item;
    item.name = fi.fileName();
    QString rel = fi.absoluteFilePath();
    if (rel.startsWith(root, Qt::CaseInsensitive))
      rel = rel.mid(root.size());
    while (rel.startsWith(QLatin1Char('/')) || rel.startsWith(QLatin1Char('\\')))
      rel = rel.mid(1);
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    item.id = rel;
    item.isFolder = fi.isDir();
    item.mimeType = item.isFolder
                        ? QStringLiteral("application/vnd.google-apps.folder")
                        : mimeFromFileName(item.name);
    item.modified = fi.lastModified().toUTC();
    item.created = fi.birthTime().isValid() ? fi.birthTime().toUTC()
                                            : item.modified;
    if (!item.isFolder)
      item.sizeBytes = fi.size();
    out.append(item);
  }
  return out;
}

bool CloudLinkHub::icloudFetch(const QString &remoteId,
                               const QString &localPath) {
  if (remoteId.isEmpty() || localPath.isEmpty())
    return false;
  const QString root = icloudLinkedRoot();
  if (root.isEmpty()) {
    m_lastError = QStringLiteral("iCloud-Ordner ist nicht verknüpft.");
    return false;
  }
  const QString src = QDir(root).filePath(remoteId);
  if (!QFileInfo::exists(src) || QFileInfo(src).isDir()) {
    m_lastError = QStringLiteral("Datei nicht gefunden.");
    return false;
  }
  if (QFileInfo(src).canonicalFilePath() ==
      QFileInfo(localPath).canonicalFilePath())
    return true;
  QDir().mkpath(QFileInfo(localPath).absolutePath());
  QFile::remove(localPath);
  if (!QFile::copy(src, localPath)) {
    m_lastError = QStringLiteral("Kopieren aus iCloud fehlgeschlagen.");
    return false;
  }
  return true;
}

bool CloudLinkHub::icloudCreateFolder(const QString &parentId,
                                      const QString &name) {
  const QString root = icloudLinkedRoot();
  if (root.isEmpty()) {
    m_lastError = QStringLiteral("iCloud-Ordner ist nicht verknüpft.");
    return false;
  }
  QString parent = root;
  if (!parentId.isEmpty())
    parent = QDir(root).filePath(parentId);
  if (!QDir(parent).mkpath(name)) {
    m_lastError = QStringLiteral("Ordner konnte nicht angelegt werden.");
    return false;
  }
  return true;
}

bool CloudLinkHub::icloudDelete(const QString &remoteId) {
  const QString root = icloudLinkedRoot();
  if (root.isEmpty() || remoteId.isEmpty())
    return false;
  const QString path = QDir(root).filePath(remoteId);
  QFileInfo fi(path);
  if (!fi.exists())
    return true;
  if (fi.isDir()) {
    QDir dir(path);
    if (!dir.removeRecursively()) {
      m_lastError = QStringLiteral("Löschen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (!QFile::remove(path)) {
    m_lastError = QStringLiteral("Löschen fehlgeschlagen.");
    return false;
  }
  return true;
}

bool CloudLinkHub::icloudRename(const QString &remoteId, const QString &newName) {
  const QString root = icloudLinkedRoot();
  if (root.isEmpty() || remoteId.isEmpty() || newName.trimmed().isEmpty())
    return false;
  const QString src = QDir(root).filePath(remoteId);
  QFileInfo fi(src);
  if (!fi.exists()) {
    m_lastError = QStringLiteral("Eintrag nicht gefunden.");
    return false;
  }
  const QString dest = fi.dir().filePath(newName.trimmed());
  if (!QFile::rename(src, dest)) {
    m_lastError = QStringLiteral("Umbenennen fehlgeschlagen.");
    return false;
  }
  return true;
}

QVector<CloudRemoteItem> CloudLinkHub::listNotes(const QString &type,
                                                 const QString &folderId) {
  m_lastError.clear();
  if (type == QLatin1String("googledrive"))
    return googleList(folderId);
  if (type == QLatin1String("nextcloud"))
    return nextcloudList(folderId);
  if (type == QLatin1String("onedrive"))
    return oneDriveList(folderId);
  if (type == QLatin1String("dropbox"))
    return dropboxList(folderId);
  if (type == QLatin1String("icloud"))
    return icloudList(folderId);
  m_lastError = QStringLiteral("Dieser Anbieter hat keine API-Liste.");
  return {};
}

bool CloudLinkHub::fetchNote(const QString &type, const QString &remoteId,
                             const QString &localPath) {
  m_lastError.clear();
  if (type == QLatin1String("googledrive"))
    return googleFetch(remoteId, localPath);
  if (type == QLatin1String("nextcloud"))
    return nextcloudFetch(remoteId, localPath);
  if (type == QLatin1String("onedrive"))
    return oneDriveFetch(remoteId, localPath);
  if (type == QLatin1String("dropbox"))
    return dropboxFetch(remoteId, localPath);
  if (type == QLatin1String("icloud"))
    return icloudFetch(remoteId, localPath);
  m_lastError = QStringLiteral("Dieser Anbieter hat keinen API-Download.");
  return false;
}

bool CloudLinkHub::createFolder(const QString &type, const QString &parentId,
                                const QString &name) {
  m_lastError.clear();
  const QString clean = name.trimmed();
  if (clean.isEmpty()) {
    m_lastError = QStringLiteral("Ordnername fehlt.");
    return false;
  }
  if (type == QLatin1String("googledrive")) {
    QString err;
    const QString token = googleAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    QJsonObject meta;
    meta.insert(QStringLiteral("name"), clean);
    meta.insert(QStringLiteral("mimeType"),
                QStringLiteral("application/vnd.google-apps.folder"));
    const QString parent = parentId.isEmpty() ? QStringLiteral("root") : parentId;
    meta.insert(QStringLiteral("parents"), QJsonArray{parent});
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files")),
                token),
        "POST", QJsonDocument(meta).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Ordner konnte nicht angelegt werden.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("onedrive")) {
    QString err;
    const QString token = oneDriveAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    QJsonObject meta;
    meta.insert(QStringLiteral("name"), clean);
    meta.insert(QStringLiteral("folder"), QJsonObject());
    meta.insert(QStringLiteral("@microsoft.graph.conflictBehavior"),
                QStringLiteral("rename"));
    const QUrl url(
        parentId.isEmpty()
            ? QUrl(QStringLiteral(
                  "https://graph.microsoft.com/v1.0/me/drive/root/children"))
            : QUrl(QStringLiteral(
                       "https://graph.microsoft.com/v1.0/me/drive/items/%1/"
                       "children")
                       .arg(parentId)));
    const HttpResult r =
        http(m_nam, jsonReq(url, token), "POST",
             QJsonDocument(meta).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Ordner konnte nicht angelegt werden.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("nextcloud")) {
    const QString user = setting(QStringLiteral("nextcloud"), "user");
    const QString server = setting(QStringLiteral("nextcloud"), "server");
    if (user.isEmpty() || server.isEmpty()) {
      m_lastError = QStringLiteral("Nextcloud ist nicht verbunden.");
      return false;
    }
    QString rel = parentId;
    if (!rel.isEmpty() && !rel.endsWith(QLatin1Char('/')))
      rel += QLatin1Char('/');
    rel += clean;
    const HttpResult r =
        http(m_nam, davReq(QUrl(davUrlForRelative(rel))), "MKCOL", {});
    if (r.status != 201 && r.status != 405 &&
        (r.status < 200 || r.status >= 300)) {
      m_lastError = QStringLiteral("Ordner konnte nicht angelegt werden.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("dropbox")) {
    QString err;
    const QString token = dropboxAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    QString path = parentId;
    if (!path.isEmpty() && !path.endsWith(QLatin1Char('/')))
      path += QLatin1Char('/');
    if (path.isEmpty())
      path = QStringLiteral("/");
    path += clean;
    if (!path.startsWith(QLatin1Char('/')))
      path.prepend(QLatin1Char('/'));
    QJsonObject body;
    body.insert(QStringLiteral("path"), path);
    body.insert(QStringLiteral("autorename"), false);
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral(
                    "https://api.dropboxapi.com/2/files/create_folder_v2")),
                token),
        "POST", QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Ordner konnte nicht angelegt werden.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("icloud"))
    return icloudCreateFolder(parentId, clean);
  m_lastError = QStringLiteral("Ordner anlegen nicht unterstützt.");
  return false;
}

bool CloudLinkHub::deleteRemote(const QString &type, const QString &remoteId) {
  m_lastError.clear();
  if (remoteId.isEmpty())
    return false;
  if (type == QLatin1String("googledrive")) {
    QString err;
    const QString token = googleAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                         .arg(remoteId)),
                token),
        "DELETE", {});
    if (!(r.status == 204 || (r.status >= 200 && r.status < 300))) {
      m_lastError = QStringLiteral("Löschen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("onedrive")) {
    QString err;
    const QString token = oneDriveAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral(
                         "https://graph.microsoft.com/v1.0/me/drive/items/%1")
                         .arg(remoteId)),
                token),
        "DELETE", {});
    if (!(r.status == 204 || r.status == 404 ||
          (r.status >= 200 && r.status < 300))) {
      m_lastError = QStringLiteral("Löschen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("nextcloud")) {
    const HttpResult r =
        http(m_nam, davReq(QUrl(davUrlForRelative(remoteId))), "DELETE", {});
    if (!(r.status == 204 || r.status == 404 ||
          (r.status >= 200 && r.status < 300))) {
      m_lastError = QStringLiteral("Löschen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("dropbox")) {
    QString err;
    const QString token = dropboxAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    QJsonObject body;
    body.insert(QStringLiteral("path"), remoteId);
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral(
                    "https://api.dropboxapi.com/2/files/delete_v2")),
                token),
        "POST", QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Löschen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("icloud"))
    return icloudDelete(remoteId);
  m_lastError = QStringLiteral("Löschen nicht unterstützt.");
  return false;
}

bool CloudLinkHub::renameRemote(const QString &type, const QString &remoteId,
                                const QString &newName) {
  m_lastError.clear();
  const QString clean = newName.trimmed();
  if (remoteId.isEmpty() || clean.isEmpty())
    return false;
  if (type == QLatin1String("googledrive")) {
    QString err;
    const QString token = googleAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    QJsonObject meta;
    meta.insert(QStringLiteral("name"), clean);
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                         .arg(remoteId)),
                token),
        "PATCH", QJsonDocument(meta).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Umbenennen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("onedrive")) {
    QString err;
    const QString token = oneDriveAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    QJsonObject meta;
    meta.insert(QStringLiteral("name"), clean);
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral(
                         "https://graph.microsoft.com/v1.0/me/drive/items/%1")
                         .arg(remoteId)),
                token),
        "PATCH", QJsonDocument(meta).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Umbenennen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("nextcloud")) {
    QString destRel = remoteId;
    const int slash = destRel.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0)
      destRel = destRel.left(slash + 1) + clean;
    else
      destRel = clean;
    QNetworkRequest req = davReq(QUrl(davUrlForRelative(remoteId)));
    req.setRawHeader("Destination", davUrlForRelative(destRel).toUtf8());
    req.setRawHeader("Overwrite", "F");
    const HttpResult r = http(m_nam, req, "MOVE", {});
    if (!(r.status == 201 || r.status == 204 ||
          (r.status >= 200 && r.status < 300))) {
      m_lastError = QStringLiteral("Umbenennen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("dropbox")) {
    QString err;
    const QString token = dropboxAccess(m_nam, &err);
    if (token.isEmpty()) {
      m_lastError = err;
      return false;
    }
    QString dest = remoteId;
    const int slash = dest.lastIndexOf(QLatin1Char('/'));
    if (slash >= 0)
      dest = dest.left(slash + 1) + clean;
    else
      dest = QStringLiteral("/") + clean;
    QJsonObject body;
    body.insert(QStringLiteral("from_path"), remoteId);
    body.insert(QStringLiteral("to_path"), dest);
    const HttpResult r = http(
        m_nam,
        jsonReq(QUrl(QStringLiteral(
                    "https://api.dropboxapi.com/2/files/move_v2")),
                token),
        "POST", QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (r.status < 200 || r.status >= 300) {
      m_lastError = QStringLiteral("Umbenennen fehlgeschlagen.");
      return false;
    }
    return true;
  }
  if (type == QLatin1String("icloud"))
    return icloudRename(remoteId, clean);
  m_lastError = QStringLiteral("Umbenennen nicht unterstützt.");
  return false;
}

bool CloudLinkHub::putNote(const QString &localPath) {
  const QString id = StoragePrefs::primaryCloudId();
  if (id == QLatin1String("googledrive"))
    return googlePut(localPath);
  if (id == QLatin1String("nextcloud"))
    return nextcloudPut(localPath);
  if (id == QLatin1String("onedrive"))
    return oneDrivePut(localPath);
  if (id == QLatin1String("dropbox"))
    return dropboxPut(localPath);
  return false;
}

bool CloudLinkHub::removeNote(const QString &fileName) {
  const QString id = StoragePrefs::primaryCloudId();
  if (id == QLatin1String("googledrive"))
    return googleRemove(fileName);
  if (id == QLatin1String("nextcloud"))
    return nextcloudRemove(fileName);
  if (id == QLatin1String("onedrive"))
    return oneDriveRemove(fileName);
  if (id == QLatin1String("dropbox"))
    return dropboxRemove(fileName);
  return false;
}

bool CloudLinkHub::renameNote(const QString &oldName, const QString &newName) {
  const QString id = StoragePrefs::primaryCloudId();
  if (id == QLatin1String("googledrive"))
    return googleRename(oldName, newName);
  if (id == QLatin1String("nextcloud"))
    return nextcloudRename(oldName, newName);
  if (id == QLatin1String("onedrive"))
    return oneDriveRename(oldName, newName);
  if (id == QLatin1String("dropbox"))
    return dropboxRename(oldName, newName);
  return false;
}
