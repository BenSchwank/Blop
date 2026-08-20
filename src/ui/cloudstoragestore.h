#pragma once

#include <QString>
#include <QVector>

/// Cloud providers used from the library sidebar. Primary access is the
/// in-app web explorer (sign in at Drive / Nextcloud / …). A local sync
/// folder path is optional.
///
/// Note files (.bnote / .blop) live on the filesystem (local and/or these
/// linked folders). They are never uploaded to Supabase / Study as the
/// primary note store — see StoragePrefs.
struct CloudStorageEntry {
  QString id;
  QString name;
  QString type; // googledrive | nextcloud | onedrive | dropbox | custom
  QString path;    ///< optional local sync folder
  QString webUrl;  ///< provider web app (Drive / Nextcloud / …)
  bool webConnected{false};
};

namespace CloudStorageStore {
QVector<CloudStorageEntry> load();
void save(const QVector<CloudStorageEntry> &entries);
void upsert(const CloudStorageEntry &entry);
CloudStorageEntry *findMutable(QVector<CloudStorageEntry> &entries,
                               const QString &id);
QString displayNameForType(const QString &type);
QString iconForType(const QString &type);
QString defaultWebUrl(const QString &type);
} // namespace CloudStorageStore
