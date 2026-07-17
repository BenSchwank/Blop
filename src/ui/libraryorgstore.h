#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

/// Library organization metadata that lives next to notes without requiring
/// every `.bnote` to be opened: favorites, color labels, and recent opens.
class LibraryOrgStore {
public:
  enum class ColorLabel {
    None = 0,
    Rose,
    Amber,
    Lime,
    Sky,
    Violet,
    Slate
  };

  static bool isFavorite(const QString &absolutePath);
  static void setFavorite(const QString &absolutePath, bool favorite);
  static void toggleFavorite(const QString &absolutePath);
  static QStringList favoritePaths();

  static ColorLabel colorLabel(const QString &absolutePath);
  static void setColorLabel(const QString &absolutePath, ColorLabel label);
  static QColor colorForLabel(ColorLabel label);
  static QString labelDisplayName(ColorLabel label);

  /// Record that the user opened this note (drives "Zuletzt").
  static void touchRecent(const QString &absolutePath);
  static QStringList recentPaths(int limit = 24);

  /// Keep metadata attached when a note/folder is moved or renamed.
  static void remapPath(const QString &fromPath, const QString &toPath);
};
