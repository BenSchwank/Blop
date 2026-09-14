#pragma once

#include "Note.h"

#include <QImage>
#include <QRectF>
#include <QSize>

/// Shared A4 page raster (thumbnails, PDF, Struktur embeds).
namespace NotePageRenderer {

/// Canonical A4 pixel size used by MultiPageNoteView.
int a4WidthPx();
int a4HeightPx();

/// Full page including paper, background pattern/image, strokes, stickies, graphs.
QImage renderFullPage(const NotePage &page, int pageW = -1, int pageH = -1);

/// Optional cropRect in normalized page coords [0,1]. Null/invalid = full page.
QImage renderPreview(const NotePage &page, const QSize &fitSize,
                     const QRectF &cropNorm = QRectF());

} // namespace NotePageRenderer
