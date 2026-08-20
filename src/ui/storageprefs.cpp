#include "storageprefs.h"

#include "cloudstoragestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace StoragePrefs {

namespace {

QString settingsOrg() { return QStringLiteral("Blop"); }
QString settingsApp() { return QStringLiteral("BlopApp"); }

QString modeSettingsKey() { return QStringLiteral("storage/mode"); }
QString primarySettingsKey() {
  return QStringLiteral("storage/primaryCloudId");
}

QString cloudMirrorSubdir() { return QStringLiteral("BlopNotizen"); }

/// Android Storage Access Framework tree/document URIs, http(s) links, and
/// similar schemes are not filesystem folders. Qt still reports
/// `QDir(content://…).exists()` for some Drive tree URIs, then `mkpath` of
/// `…/BlopNotizen` logs "Cannot create file, parent doesn't exist" and the
/// subsequent QFile / QFileSystemModel work can abort the Android EGL surface.
QString localFilesystemPath(const QString &p) {
  const QString s = p.trimmed();
  if (s.isEmpty())
    return {};
  if (s.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
    const QUrl u(s);
    return u.isLocalFile() ? u.toLocalFile() : QString();
  }
  const int schemeEnd = s.indexOf(QLatin1String("://"));
  if (schemeEnd > 0)
    return {};
  return s;
}

} // namespace

bool isUsableFilesystemDir(const QString &path) {
  const QString local = localFilesystemPath(path);
  return !local.isEmpty() && QDir(local).exists();
}

bool isNonFilesystemPath(const QString &path) {
  const QString s = path.trimmed();
  return !s.isEmpty() && localFilesystemPath(s).isEmpty();
}

namespace {

bool pathExistsDir(const QString &p) { return isUsableFilesystemDir(p); }

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
        "Notizen liegen im verknüpften Sync-Ordner (z. B. Google Drive). "
        "Blop Study bleibt auf Supabase (Konto, Teilen, Lernmaterial).");
  case Mode::LocalAndCloud:
    return QStringLiteral(
        "Notizen werden lokal gespeichert und in Google Drive / Cloud "
        "gespiegelt — nutzbar auf Handy, Laptop und Mac. "
        "Blop Study bleibt auf Supabase.");
  case Mode::LocalOnly:
  default:
    return QStringLiteral(
        "Notizen nur auf diesem Gerät. Für Cloud: Google Drive verbinden "
        "(unten). Blop Study nutzt weiter Supabase — nicht den Notiz-Speicher.");
  }
}

QString architectureHint() {
  return QStringLiteral(
      "Notizen → Google Drive / Nextcloud (wachsen stark). "
      "Blop Study → Supabase (Konto, Teile-Links, Lernkarten; "
      "große PDFs/Videos später ggf. auslagern).");
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
  auto usablePath = [](const CloudStorageEntry &e) -> QString {
    const QString local = localFilesystemPath(e.path);
    return pathExistsDir(local) ? local : QString();
  };
  if (!preferred.isEmpty()) {
    for (const CloudStorageEntry &e : entries) {
      if (e.id == preferred) {
        if (const QString p = usablePath(e); !p.isEmpty())
          return p;
      }
    }
  }
  for (const CloudStorageEntry &e : entries) {
    if (const QString p = usablePath(e); !p.isEmpty())
      return p;
  }
  return {};
}

bool isProviderLinked(const QString &providerId) {
  if (providerId.isEmpty())
    return false;
  for (const CloudStorageEntry &e : CloudStorageStore::load()) {
    if (e.id == providerId &&
        (pathExistsDir(e.path) || e.webConnected || !e.webUrl.isEmpty()))
      return true;
  }
  return false;
}

bool isGoogleDriveLinked() {
  return isProviderLinked(QStringLiteral("googledrive"));
}

QStringList suggestedGoogleDriveRoots() {
  QStringList out;
  const QString home =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  const QStringList candidates = {
      home + QStringLiteral("/Google Drive"),
      home + QStringLiteral("/GoogleDrive"),
      home + QStringLiteral("/google-drive"),
      home + QStringLiteral("/Meine Ablage"),
      home + QStringLiteral("/Library/CloudStorage"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
          QStringLiteral("/Google Drive"),
#ifdef Q_OS_ANDROID
      QStringLiteral("/storage/emulated/0/GoogleDrive"),
      QStringLiteral("/storage/emulated/0/Download/Google Drive"),
#endif
  };
  for (const QString &c : candidates) {
    if (!pathExistsDir(c))
      continue;
    // On macOS CloudStorage, prefer a GoogleDrive-* child if present.
    if (c.endsWith(QLatin1String("/Library/CloudStorage"))) {
      const QFileInfoList kids =
          QDir(c).entryInfoList(QStringList() << QStringLiteral("GoogleDrive*"),
                                QDir::Dirs | QDir::NoDotAndDotDot);
      for (const QFileInfo &fi : kids)
        out.append(fi.absoluteFilePath());
      continue;
    }
    out.append(c);
  }
  out.removeDuplicates();
  return out;
}

QString bestSuggestedGoogleDriveRoot() {
  const QStringList roots = suggestedGoogleDriveRoots();
  return roots.isEmpty() ? QString() : roots.first();
}

QStringList suggestedRootsForProvider(const QString &providerId) {
  if (providerId == QLatin1String("googledrive"))
    return suggestedGoogleDriveRoots();

  const QString home =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  QStringList candidates;
  if (providerId == QLatin1String("dropbox")) {
    candidates << home + QStringLiteral("/Dropbox")
                 << home + QStringLiteral("/Dropbox (Personal)");
  } else if (providerId == QLatin1String("onedrive")) {
    const QFileInfoList entries =
        QDir(home).entryInfoList(QStringList() << QStringLiteral("OneDrive*"),
                                 QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries)
      candidates << fi.absoluteFilePath();
  } else if (providerId == QLatin1String("nextcloud")) {
    candidates << home + QStringLiteral("/Nextcloud")
                 << home + QStringLiteral("/nextcloud");
  } else if (providerId == QLatin1String("icloud")) {
#ifdef Q_OS_MACOS
    candidates << home +
                        QStringLiteral("/Library/Mobile Documents/com~apple~CloudDocs");
#endif
    candidates << home + QStringLiteral("/iCloud Drive")
                 << home + QStringLiteral("/iCloudDrive");
  }

  QStringList out;
  for (const QString &c : candidates) {
    if (QDir(c).exists())
      out.append(c);
  }
  out.removeDuplicates();
  return out;
}

QString bestSuggestedRootForProvider(const QString &providerId) {
  const QStringList roots = suggestedRootsForProvider(providerId);
  return roots.isEmpty() ? QString() : roots.first();
}

bool connectProviderForNotes(const QString &providerId,
                             const QString &folderPath) {
  const QString folder = localFilesystemPath(folderPath);
  if (providerId.isEmpty() || !pathExistsDir(folder))
    return false;

  QVector<CloudStorageEntry> entries = CloudStorageStore::load();
  CloudStorageEntry *entry = CloudStorageStore::findMutable(entries, providerId);
  if (!entry) {
    CloudStorageEntry e;
    e.id = providerId;
    e.type = providerId;
    e.name = CloudStorageStore::displayNameForType(providerId);
    e.path = folder;
    entries.append(e);
  } else {
    entry->path = folder;
  }
  CloudStorageStore::save(entries);

  const QString nested = folder + QLatin1Char('/') + cloudMirrorSubdir();
  QDir().mkpath(nested);

  setPrimaryCloudId(providerId);
  // Default usable sync: keep a local copy and mirror into Drive.
  setMode(Mode::LocalAndCloud);
  return true;
}

QString noteWriteRoot(const QString &localRoot) {
  const Mode m = mode();
  const QString local =
      localRoot.isEmpty() ? ensureLocalLibraryRoot() : localRoot;
  if (m == Mode::LocalOnly || m == Mode::LocalAndCloud)
    return local;

  const QString cloud = primaryLinkedCloudPath();
  if (cloud.isEmpty() || isNonFilesystemPath(cloud))
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

bool removeCloudMirrorIfNeeded(const QString &localNotePath) {
  if (mode() != Mode::LocalAndCloud)
    return false;
  if (localNotePath.isEmpty() || !QFileInfo::exists(localNotePath))
    return false;

  const QString cloud = primaryLinkedCloudPath();
  if (cloud.isEmpty())
    return false;

  const QString destDir = cloud + QLatin1Char('/') + cloudMirrorSubdir();
  const QString dest = destDir + QLatin1Char('/') +
                       QFileInfo(localNotePath).fileName();
  if (!QFile::exists(dest))
    return true;
  return QFile::remove(dest);
}

bool renameCloudMirrorIfNeeded(const QString &oldLocalNotePath,
                                 const QString &newLocalNotePath) {
  if (mode() != Mode::LocalAndCloud)
    return false;
  if (oldLocalNotePath.isEmpty() || newLocalNotePath.isEmpty())
    return false;

  const QString cloud = primaryLinkedCloudPath();
  if (cloud.isEmpty())
    return false;

  const QString destDir = cloud + QLatin1Char('/') + cloudMirrorSubdir();
  const QString oldDest = destDir + QLatin1Char('/') +
                          QFileInfo(oldLocalNotePath).fileName();
  const QString newDest = destDir + QLatin1Char('/') +
                          QFileInfo(newLocalNotePath).fileName();
  if (!QFile::exists(oldDest))
    return true;
  if (QFile::exists(newDest))
    QFile::remove(newDest);
  return QFile::rename(oldDest, newDest);
}

} // namespace StoragePrefs
