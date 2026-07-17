#include "cloudstoragestore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

namespace CloudStorageStore {

static QString settingsKey() {
  return QStringLiteral("CloudStorage/entries");
}

QVector<CloudStorageEntry> load() {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QByteArray raw = s.value(settingsKey()).toByteArray();
  QVector<CloudStorageEntry> out;
  if (raw.isEmpty()) {
    // Seed known providers without a path — user links a local sync folder.
    const struct {
      const char *type;
      const char *name;
    } presets[] = {
        {"googledrive", "Google Drive"},
        {"nextcloud", "Nextcloud"},
        {"onedrive", "OneDrive"},
        {"dropbox", "Dropbox"},
    };
    for (const auto &p : presets) {
      CloudStorageEntry e;
      e.id = QString::fromLatin1(p.type);
      e.type = e.id;
      e.name = QString::fromUtf8(p.name);
      out.append(e);
    }
    save(out);
    return out;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isArray())
    return out;
  for (const QJsonValue &v : doc.array()) {
    const QJsonObject o = v.toObject();
    CloudStorageEntry e;
    e.id = o.value(QStringLiteral("id")).toString();
    e.name = o.value(QStringLiteral("name")).toString();
    e.type = o.value(QStringLiteral("type")).toString();
    e.path = o.value(QStringLiteral("path")).toString();
    if (e.id.isEmpty())
      e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (e.name.isEmpty())
      e.name = displayNameForType(e.type);
    out.append(e);
  }
  return out;
}

void save(const QVector<CloudStorageEntry> &entries) {
  QJsonArray arr;
  for (const CloudStorageEntry &e : entries) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.id);
    o.insert(QStringLiteral("name"), e.name);
    o.insert(QStringLiteral("type"), e.type);
    o.insert(QStringLiteral("path"), e.path);
    arr.append(o);
  }
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(settingsKey(),
             QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

CloudStorageEntry *findMutable(QVector<CloudStorageEntry> &entries,
                               const QString &id) {
  for (CloudStorageEntry &e : entries) {
    if (e.id == id)
      return &e;
  }
  return nullptr;
}

QString displayNameForType(const QString &type) {
  if (type == QLatin1String("googledrive"))
    return QStringLiteral("Google Drive");
  if (type == QLatin1String("nextcloud"))
    return QStringLiteral("Nextcloud");
  if (type == QLatin1String("onedrive"))
    return QStringLiteral("OneDrive");
  if (type == QLatin1String("dropbox"))
    return QStringLiteral("Dropbox");
  return QStringLiteral("Eigene Cloud");
}

QString iconForType(const QString &type) {
  if (type == QLatin1String("googledrive"))
    return QStringLiteral("drive");
  if (type == QLatin1String("nextcloud"))
    return QStringLiteral("cloud");
  if (type == QLatin1String("onedrive"))
    return QStringLiteral("onedrive");
  if (type == QLatin1String("dropbox"))
    return QStringLiteral("dropbox");
  return QStringLiteral("cloud");
}

} // namespace CloudStorageStore
