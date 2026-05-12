#include "freegridview.h"
#include "overlayscrollindicator.h"
#include <QDrag>
#include <QMimeData>
#include <QScrollBar>
#include <cmath>


FreeGridView::FreeGridView(QWidget *parent)
    : QListView(parent), m_showGhost(false), m_accentColor(QColor(0x6c5ce7)),
      m_itemSize(100, 80) // Default Fallback
{
  setViewMode(QListView::IconMode);
  setMovement(QListView::Snap);
  setResizeMode(QListView::Adjust);
  setSelectionMode(QAbstractItemView::ExtendedSelection);

  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::MoveAction);

  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  horizontalScrollBar()->setSingleStep(10);
  verticalScrollBar()->setSingleStep(10);

  OverlayScrollIndicator::install(this);
}

void FreeGridView::setItemSize(const QSize &size) {
  m_itemSize = size;
  updateGeometries();
  viewport()->update();
}

QPoint FreeGridView::calculateSnapPosition(const QPoint &pos) {
  QSize grid = gridSize();
  if (grid.isEmpty())
    return pos;

  int w = grid.width();
  int h = grid.height();

  int x = qRound((double)pos.x() / w) * w;
  int y = qRound((double)pos.y() / h) * h;

  return QPoint(x, y);
}

void FreeGridView::paintEvent(QPaintEvent *e) {
  QPainter p(viewport());

  // OPTIMIERUNG: Statt jeden Punkt einzeln in einer Schleife zu zeichnen,
  // nutzen wir ein gekacheltes Pixmap (Cached Bitmap). Das ist deutlich
  // schneller.

  static QPixmap tilePix;
  if (tilePix.isNull() || tilePix.width() != 30) {
    tilePix = QPixmap(30, 30);
    tilePix.fill(Qt::transparent);
    QPainter pt(&tilePix);
    pt.setPen(QColor(255, 255, 255, 10));
    pt.drawPoint(10, 10);
  }

  int startX = horizontalScrollBar()->value();
  int startY = verticalScrollBar()->value();

  // drawTiledPixmap füllt den Bereich effizient
  p.drawTiledPixmap(viewport()->rect(), tilePix,
                    QPoint(-startX % 30, -startY % 30));

  // 2. Items zeichnen
  QListView::paintEvent(e);

  // 3. Ghost
  if (m_showGhost) {
    p.save();
    QColor ghostColor = m_accentColor;
    ghostColor.setAlpha(40);
    p.setBrush(ghostColor);
    p.setPen(QPen(m_accentColor, 2, Qt::DashLine));

    QRect ghostVisual = m_ghostRect;
    ghostVisual.setSize(m_itemSize);

    QSize grid = gridSize();
    if (!grid.isEmpty()) {
      int offsetX = (grid.width() - m_itemSize.width()) / 2;
      int offsetY = (grid.height() - m_itemSize.height()) / 2;
      ghostVisual.translate(offsetX, offsetY);
    }

    p.drawRoundedRect(ghostVisual, 8, 8);
    p.restore();
  }
}

void FreeGridView::dragEnterEvent(QDragEnterEvent *e) {
  m_showGhost = true;
  QListView::dragEnterEvent(e);
}

void FreeGridView::dragLeaveEvent(QDragLeaveEvent *e) {
  m_showGhost = false;
  viewport()->update();
  QListView::dragLeaveEvent(e);
}

void FreeGridView::dragMoveEvent(QDragMoveEvent *e) {
  QPoint pos = e->position().toPoint();
  QPoint snapPos = calculateSnapPosition(pos);
  const QRect newGhost(snapPos, gridSize());
  if (newGhost != m_ghostRect) {
    // v119 perf: only repaint the union of the old + new ghost
    // rectangles (with a small inflate for the dashed border) instead
    // of the entire viewport. Cuts dragMoveEvent cost from O(viewport
    // pixels) to O(ghost cell pixels) and is what makes the ghost
    // glide smoothly even on slower Android devices.
    QRect dirty = newGhost.united(m_ghostRect.isNull() ? newGhost : m_ghostRect);
    dirty.adjust(-2, -2, 2, 2);
    m_lastGhostRect = m_ghostRect;
    m_ghostRect = newGhost;
    viewport()->update(dirty);
  }
  QListView::dragMoveEvent(e);
}

void FreeGridView::dropEvent(QDropEvent *e) {
  m_showGhost = false;

  if (e->source() == this && e->mimeData()->hasUrls()) {
    QModelIndex targetIndex = indexAt(e->position().toPoint());

    // We emit the signal so MainWindow can handle the actual move logic
    // if we drop onto a valid target folder.
    QModelIndexList selected = selectionModel()->selectedIndexes();
    if (!selected.isEmpty() && targetIndex.isValid() &&
        targetIndex != selected.first()) {
      emit itemDropped(selected.first(), targetIndex);
      e->acceptProposedAction();
      viewport()->update();
      return;
    }
  }

  QListView::dropEvent(e);
  viewport()->update();
}

void FreeGridView::startDrag(Qt::DropActions supportedActions) {
  QModelIndexList indexes = selectionModel()->selectedIndexes();
  if (indexes.count() > 0) {
    QMimeData *data = model()->mimeData(indexes);
    if (!data)
      return;
    QDrag *drag = new QDrag(this);
    drag->setMimeData(data);

    // Optional: set a pixmap for the drag
    // drag->setPixmap(pixmap);

    drag->exec(supportedActions, Qt::MoveAction);
  }
}
