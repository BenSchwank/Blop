#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

/// Android SAF document picker (ACTION_OPEN_DOCUMENT / CREATE_DOCUMENT).
/// Desktop builds expose no-op stubs that immediately cancel.
class AndroidContentPicker : public QObject {
  Q_OBJECT
public:
  using Callback = std::function<void(const QString &localPath)>;

  static AndroidContentPicker &instance();

  /// Open a document. mimeFilters e.g. {"image/*"} or {"application/pdf"}.
  /// On success, localPath is a copied file under CacheLocation.
  void pickOpen(const QStringList &mimeFilters, Callback cb);

  /// Create/save a document. On success, localPath is a writable cache copy
  /// that the caller fills; after write, call publishSavedFile() is not
  /// needed — for CREATE_DOCUMENT the Java side copies after the callback
  /// returns if using the two-phase API. Simpler: pickSave returns a cache
  /// path; caller writes; then finishSave(cachePath) copies into the SAF URI.
  void pickSave(const QString &mimeType, const QString &suggestedName,
                Callback cb);

  /// After writing to the path from pickSave, push bytes into the SAF URI.
  void finishSave(const QString &cachePath);

  /// Invoked from JNI when a pick finishes (empty path = cancel/error).
  void handlePickedPath(const QString &path);

private:
  explicit AndroidContentPicker(QObject *parent = nullptr);
  Callback m_pending;
  QString m_pendingSaveCache;
  bool m_saveMode{false};
};
