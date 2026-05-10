#ifndef ANDROIDICONS_H
#define ANDROIDICONS_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Material-3 inspired icon factory used exclusively on the Android UI path.
//
// Why a dedicated file: createModernIcon() in mainwindow.cpp draws ~30 glyphs
// using fixed 64-coordinate QPainter primitives that originate from the
// Windows UI design. On Android they end up either tiny (when scaled down
// for header buttons) or blocky/grey (when scaled up for welcome tiles via
// the default QIconEngine). This factory renders icons fresh into the
// requested pixel size, then caches them per (name, rgb, pxSize) so the
// same icon is generated only once per app session - no per-frame painter
// allocations and consistent quality at every call site.
namespace AndroidIcons {

// Returns a QIcon containing a single high-resolution pixmap. The pixmap
// is sized at dpSize material design units (default 48 dp, which is the
// standard Material toolbar / navigation icon size). The icon is suitable
// for QToolButton, QPushButton, QListView decorations etc.
QIcon icon(const QString &name, const QColor &color, int dpSize = 48);

// Returns the raw cached pixmap at exactly the requested pixel size.
// Used by the welcome-tile delegate which knows the target rect in pixels
// and wants to drawPixmap() directly without going through QIcon scaling.
QPixmap pixmap(const QString &name, const QColor &color, int pxSize);

} // namespace AndroidIcons

#endif // ANDROIDICONS_H
