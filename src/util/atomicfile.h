#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/// Atomic file writes (QSaveFile: temp file + rename) for small app stores.
/// A crash or full disk mid-write keeps the previous version instead of
/// leaving a truncated file.
namespace AtomicFile {

/// Writes `data` to `path` atomically (creates parent dirs). On failure logs a
/// warning, emits SaveStatus::failed and returns false.
bool write(const QString &path, const QByteArray &data,
           const QString &what = QString());

/// Rolling backup: copies the current file at `path` to `path + ".bak"` (if it
/// exists and is non-empty) before it is overwritten.
void rotateBackup(const QString &path);

} // namespace AtomicFile

/// Global notification hub: stores/saves report failures here so the UI can
/// show one non-blocking message instead of silently losing data.
class SaveStatus : public QObject {
  Q_OBJECT
public:
  static SaveStatus &instance();
  /// Thread-safe: may be called from any thread.
  void reportFailure(const QString &what, const QString &path);
  /// A corrupt file was restored from its .bak copy.
  void reportRecovered(const QString &path);
signals:
  void failed(const QString &what, const QString &path);
  void recovered(const QString &path);

private:
  SaveStatus() = default;
};
