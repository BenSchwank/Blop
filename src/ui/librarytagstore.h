#pragma once

#include <QString>
#include <QStringList>

/// Persists the library tag catalog and per-note tag assignments in QSettings.
/// Paths are stored as absolute note file paths; the catalog is global.
class LibraryTagStore {
public:
  static QStringList catalog();
  static void setCatalog(const QStringList &tags);

  static QStringList tagsForPath(const QString &absolutePath);
  static void setTagsForPath(const QString &absolutePath, const QStringList &tags);

  static bool addTagToCatalog(const QString &tag);
  static bool renameTag(const QString &from, const QString &to);
  static bool removeTag(const QString &tag);

  /// Keep tag assignments when notes/folders are moved or renamed.
  static void remapPath(const QString &fromPath, const QString &toPath);

  static QString normalize(const QString &raw);
};
