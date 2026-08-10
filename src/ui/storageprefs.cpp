#include "storageprefs.h"

#include "cloudstoragestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace StoragePrefs {

namespace {

QString settingsOrg() { return QStringLiteral("Blop"); }
QString settingsApp() { return QStringLiteral("BlopApp"); }

QString modeSettingsKey() { return QStringLiteral("storage/mode"); }
QString primarySettingsKey() {
  return QStringLiteral("storage/primaryCloudId");
}

QString cloudMirrorSubdir() { return QStringLiteral("BlopNotizen"); }

} // namespace

QString modeKey() { return modeSettingsKey(); }

Mode mode() {
  QSettings s(settingsOrg(), settingsApp());
  const int v = s.value(modeSettingsKey(), int(Mode::LocalOnly)).toInt();
  if (v == int(Mode::CloudOnly))
    return Mode::CloudOnly;
  if (v == int(Mode::LocalAndCloud))
    return Mode::LocalAndCloud;
  return Mode::LocalOnly;
}

void setMode(Mode m) {
  QSettings s(settingsOrg(), settingsApp());
  s.setValue(modeSettingsKey(), int(m));
}

QString primaryCloudId() {
  QSettings s(settingsOrg(), settingsApp());
  return s.value(primarySettingsKey()).toString();
}

void setPrimaryCloudId(const QString &id) {
  QSettings s(settingsOrg(), settingsApp());
  if (id.isEmpty())
    s.remove(primarySettingsKey());
  else
    s.setValue(primarySettingsKey(), id);
}

QString modeLabel(Mode m) {
  switch (m) {
  case Mode::CloudOnly:
    return QStringLiteral("Nur Cloud");
  case Mode::LocalAndCloud:
    return QStringLiteral("Lokal + Cloud");
  case Mode::LocalOnly:
  default:
    return QStringLiteral("Nur lokal");
  }
}

QString modeHint(Mode m) {
  switch (m) {
  case Mode::CloudOnly:
    return QStringLiteral(
        "Notizen liegen im verknüpften Sync-Ordner (Drive, Nextcloud, …). "
        "Supabase speichert keine Notizen.");
  case Mode::LocalAndCloud:
    return QStringLiteral(
        "Notizen werden lokal gespeichert und zusätzlich in den "
        "verknüpften Cloud-Ordner gespiegelt. Supabase speichert keine Notizen.");
  case Mode::LocalOnly:
  default:
    return QStringLiteral(
        "Notizen bleiben auf diesem Gerät im Ordner „BlopNotizen“. "
        "Supabase speichert keine Notizen.");
  }
}

QString ensureLocalLibraryRoot() {
#ifdef Q_OS_ANDROID
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
  const QString root = base + QStringLiteral("/BlopNotizen");
  QDir dir(root);
  if (!dir.exists())
    dir.mkpath(QStringLiteral("."));
  return root;
}

QString primaryLinkedCloudPath() {
  const QVector<CloudStorageEntry> entries = CloudStorageStore::load();
  const QString preferred = primaryCloudId();
  auto pathOk = [](const QString &p) {
    return !p.isEmpty() && QDir(p).exists();
  };

  if (!preferred.isEmpty()) {
    for (const CloudStorageEntry &e : entries) {
      if (e.id == preferred && pathOk(e.path))
        return e.path;
    }
  }
  for (const CloudStorageEntry &e : entries) {
    if (pathOk(e.path))
      return e.path;
  }
  return {};
}

QString noteWriteRoot(const QString &localRoot) {
  const Mode m = mode();
  const QString local =
      localRoot.isEmpty() ? ensureLocalLibraryRoot() : localRoot;
  if (m == Mode::LocalOnly || m == Mode::LocalAndCloud)
    return local;

  // CloudOnly — prefer a BlopNotizen subfolder inside the linked sync root.
  const QString cloud = primaryLinkedCloudPath();
  if (cloud.isEmpty())
    return {};
  const QString nested = cloud + QLatin1Char('/') + cloudMirrorSubdir();
  QDir().mkpath(nested);
  return nested;
}

bool mirrorNoteToCloudIfNeeded(const QString &localNotePath) {
  if (mode() != Mode::LocalAndCloud)
    return false;
  if (localNotePath.isEmpty() || !QFileInfo::exists(localNotePath))
    return false;

  const QString cloud = primaryLinkedCloudPath();
  if (cloud.isEmpty())
    return false;

  const QString destDir = cloud + QLatin1Char('/') + cloudMirrorSubdir();
  if (!QDir().mkpath(destDir))
    return false;

  const QString dest =
      destDir + QLatin1Char('/') + QFileInfo(localNotePath).fileName();
  if (QFileInfo(dest).canonicalFilePath() ==
      QFileInfo(localNotePath).canonicalFilePath())
    return true;

  if (QFile::exists(dest))
    QFile::remove(dest);
  return QFile::copy(localNotePath, dest);
}

} // namespace StoragePrefs
