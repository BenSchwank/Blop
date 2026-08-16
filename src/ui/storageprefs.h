#pragma once

#include <QString>
#include <QStringList>

/// Where Blop notes live. Notes are always filesystem files (.bnote / .blop).
/// Study/Supabase is never used as the note store — only auth / Study materials.
namespace StoragePrefs {

enum class Mode {
  LocalOnly = 0,     ///< App-local BlopNotizen only
  CloudOnly = 1,     ///< Linked vendor sync folder (Drive / Nextcloud / …)
  LocalAndCloud = 2, ///< Local primary + mirror into linked cloud
};

Mode mode();
void setMode(Mode mode);

/// Preferred linked CloudStorage entry id (googledrive, nextcloud, …).
QString primaryCloudId();
void setPrimaryCloudId(const QString &id);

QString modeKey();
QString modeLabel(Mode mode);
QString modeHint(Mode mode);

/// Short product copy: notes → Drive/Nextcloud; Study → Supabase.
QString architectureHint();

/// Absolute path of the always-created local library (Documents/AppData).
QString ensureLocalLibraryRoot();

/// First linked cloud path that exists on disk (prefers primaryCloudId).
QString primaryLinkedCloudPath();

/// True when Google Drive (or primary cloud) has a live linked folder.
bool isProviderLinked(const QString &providerId);
bool isGoogleDriveLinked();

/// Common Google Drive desktop sync roots to pre-select in the picker.
QStringList suggestedGoogleDriveRoots();
QString bestSuggestedGoogleDriveRoot();

/// Link a provider folder and enable Lokal+Cloud note sync immediately.
/// Creates `<path>/BlopNotizen` and sets the provider as primary.
bool connectProviderForNotes(const QString &providerId, const QString &folderPath);

/// Directory used for new notes under the current mode.
/// CloudOnly without a linked folder returns empty.
QString noteWriteRoot(const QString &localRoot);

/// Mirror a note file into the linked cloud BlopNotizen folder (LocalAndCloud).
/// No-op for other modes or when no cloud is linked.
bool mirrorNoteToCloudIfNeeded(const QString &localNotePath);

/// Remove the cloud mirror of a note file in LocalAndCloud mode.
/// No-op for other modes or when no cloud is linked.
bool removeCloudMirrorIfNeeded(const QString &localNotePath);

} // namespace StoragePrefs
