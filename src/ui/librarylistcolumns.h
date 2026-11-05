#pragma once

#include "notepreviewicon.h"

#include <QRect>
#include <QString>
#include <array>

/// Shared 6-column geometry for the desktop library high-density list
/// (header + ModernItemDelegate rows must stay aligned).
namespace LibraryListColumns {

enum Col : int {
  Select = 0,
  Name = 1,
  Tags = 2,
  Place = 3,
  Extent = 4,
  Modified = 5,
  Count = 6
};

struct Meta {
  NotePreviewIcon::Kind kind{NotePreviewIcon::Kind::A4};
  int pageCount{-1}; // -1 unknown / N/A, >=0 for A4 notes
  qint64 fileBytes{-1};
  QString formatBadge; // "DIN A4" | "Unendlich" | "Struktur" | empty
  QString placeLabel;  // "Lokal · Standard" | "Cloud · …"
  QString extentLabel; // "2 S. · 248 B" | "∞ · 1,2 KB" | "—"
  QString modifiedLabel;
};

/// Horizontal padding inside the row / header.
int rowPad();
int selectColWidth();
int placeColWidth();
int extentColWidth();
int modifiedColWidth();
int nameMinWidth();
int nameMaxWidth();
int tagsMinWidth();

/// Six column rects inside `row` (full item/header height).
std::array<QRect, Count> columnRects(const QRect &row);

/// Hit targets inside a data row (relative to same `row` as columnRects).
QRect checkboxRect(const QRect &selectCol);
QRect starRect(const QRect &selectCol);
QRect menuRect(const QRect &modifiedCol);
QRect tagGhostRect(const QRect &tagsCol);

Meta peekMeta(const QString &path, bool isFolder, const QString &localRoot);

QString humanSize(qint64 bytes);
QString formatModified(const QString &path);

} // namespace LibraryListColumns
