#pragma once

#include <QString>
#include <QVector>

/// Local cloud sync folders used as Blop library roots (Google Drive,
/// Nextcloud, OneDrive, Dropbox, or any custom sync path). Paths are
/// persisted in QSettings — no OAuth required; the vendor sync client
/// keeps the folder up to date.
struct CloudStorageEntry {
  QString id;
  QString name;
  QString type; // googledrive | nextcloud | onedrive | dropbox | custom
  QString path;
};

namespace CloudStorageStore {
QVector<CloudStorageEntry> load();
void save(const QVector<CloudStorageEntry> &entries);
CloudStorageEntry *findMutable(QVector<CloudStorageEntry> &entries,
                               const QString &id);
QString displayNameForType(const QString &type);
QString iconForType(const QString &type);
} // namespace CloudStorageStore
