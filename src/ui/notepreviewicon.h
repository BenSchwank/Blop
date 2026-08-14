#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

/// Library-tile miniatures: A4 vs infinite, lined/grid/dots, and folders.
namespace NotePreviewIcon {

enum class Kind { Folder, A4, Infinite };

struct Spec {
  Kind kind{Kind::A4};
  /// PageBackgroundType: 0 Blank, 1 Lined, 2 Grid, 3 Dotted, 4 Legal
  int backgroundType{2};
  QColor paper{QColor(252, 250, 245)};
};

Spec specForPath(const QString &path, bool isDirectory);
QPixmap pixmap(const Spec &spec, int px);
QPixmap pixmapForPath(const QString &path, bool isDirectory, int px);

} // namespace NotePreviewIcon
