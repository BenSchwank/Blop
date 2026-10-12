#pragma once

#include <QColor>
#include <QPixmap>
#include <QRect>
#include <QString>

class QPainter;

/// Library-tile miniatures: A4 vs infinite, lined/grid/dots, and folders.
namespace NotePreviewIcon {

enum class Kind { Folder, A4, Infinite, Struktur };

struct Spec {
  Kind kind{Kind::A4};
  /// PageBackgroundType: 0 Blank, 1 Lined, 2 Grid, 3 Dotted, 4 Legal
  int backgroundType{2};
  QColor paper{QColor(252, 250, 245)};
};

Spec specForPath(const QString &path, bool isDirectory);
QPixmap pixmap(const Spec &spec, int px);
QPixmap pixmapForPath(const QString &path, bool isDirectory, int px);
/// Drive-style type icon from MIME / file name (folders, Docs, PDF, …).
QPixmap pixmapForCloud(const QString &name, const QString &mimeType,
                       bool isFolder, int px);
/// Full-bleed paper preview for library hero cards.
void paintHero(QPainter *p, const QRect &r, const Spec &spec);
void paintCloudHero(QPainter *p, const QRect &r, const QString &name,
                    const QString &mimeType, bool isFolder);

} // namespace NotePreviewIcon
