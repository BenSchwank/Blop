#include "freegridview.h"
#include <QScrollBar>
#include <cmath>

FreeGridView::FreeGridView(QWidget *parent)
    : QListView(parent)
    , m_showGhost(false)
    , m_accentColor(QColor(0x5E5CE6))
    , m_itemSize(100, 80) // Default Fallback
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
}

void FreeGridView::setItemSize(const QSize &size) {
    m_itemSize = size;
    updateGeometries();
    viewport()->update();
}

QPoint FreeGridView::calculateSnapPosition(const QPoint &pos)
{
    // Wir snappen am Layout-Grid (gridSize), nicht am feinen visuellen Raster
    QSize grid = gridSize();
    if (grid.isEmpty()) return pos;

    int w = grid.width();
    int h = grid.height();

    // Auf nächste Zelle runden
    int x = qRound((double)pos.x() / w) * w;
    int y = qRound((double)pos.y() / h) * h;

    // Zentrieren im Grid? Optional. Hier: Oben-Links Snap
    return QPoint(x, y);
}

void FreeGridView::paintEvent(QPaintEvent *e)
{
    QPainter p(viewport());

    // 1. Visuelles Raster (FEIN, fix 30px)
    // Unabhängig von gridSize(), damit es immer gut aussieht
    int visualStep = 30;

    p.setPen(QColor(255, 255, 255, 10));

    int startX = horizontalScrollBar()->value();
    int startY = verticalScrollBar()->value();
    int w = viewport()->width();
    int h = viewport()->height();

    int offX = startX % visualStep;
    int offY = startY % visualStep;

    for (int x = -offX; x < w; x += visualStep) {
        for (int y = -offY; y < h; y += visualStep) {
            p.drawPoint(x + 10, y + 10);
        }
    }

    // 2. Items zeichnen
    QListView::paintEvent(e);

    // 3. Ghost (Größe entspricht ItemSize)
    if (m_showGhost) {
        p.save();
        QColor ghostColor = m_accentColor;
        ghostColor.setAlpha(40);
        p.setBrush(ghostColor);
        p.setPen(QPen(m_accentColor, 2, Qt::DashLine));

        QRect ghostVisual = m_ghostRect;
        // Wir nutzen die Item-Größe für den Ghost, nicht das ganze Grid-Feld
        ghostVisual.setSize(m_itemSize);

        // Optional: Zentrieren im Grid-Feld
        // Da m_ghostRect oben links am Grid snappt, schieben wir es etwas ein
        // wenn ItemSize kleiner als GridSize ist.
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

void FreeGridView::dragEnterEvent(QDragEnterEvent *e)
{
    m_showGhost = true;
    QListView::dragEnterEvent(e);
}

void FreeGridView::dragLeaveEvent(QDragLeaveEvent *e)
{
    m_showGhost = false;
    viewport()->update();
    QListView::dragLeaveEvent(e);
}

void FreeGridView::dragMoveEvent(QDragMoveEvent *e)
{
    QPoint pos = e->position().toPoint();
    QPoint snapPos = calculateSnapPosition(pos);
    m_ghostRect = QRect(snapPos, gridSize()); // Basis ist Grid
    viewport()->update();
    QListView::dragMoveEvent(e);
}

void FreeGridView::dropEvent(QDropEvent *e)
{
    m_showGhost = false;
    QListView::dropEvent(e);
    viewport()->update();
}
