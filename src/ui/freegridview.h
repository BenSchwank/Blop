#ifndef FREEGRIDVIEW_H
#define FREEGRIDVIEW_H

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QListView>
#include <QPainter>

class QTimer;


class FreeGridView : public QListView {
  Q_OBJECT

public:
  explicit FreeGridView(QWidget *parent = nullptr);

  void setAccentColor(const QColor &c) { m_accentColor = c; }

  // Sets the visual size of the item (click area)
  void setItemSize(const QSize &size);
  QSize itemSize() const { return m_itemSize; }

signals:
  void itemDropped(const QModelIndex &sourceIndex,
                   const QModelIndex &targetIndex);
  // Finger hold on a tile or the empty list. Right-click stays a separate path.
  void longPressed(const QPoint &viewportPos);

protected:
  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dragLeaveEvent(QDragLeaveEvent *e) override;
  void dropEvent(QDropEvent *e) override;
  void startDrag(Qt::DropActions supportedActions) override;

private:
  QPoint m_pressPos;
  bool m_pressTracking{false};
  bool m_longPressFired{false};
  QTimer *m_longPressTimer{nullptr};
  QRect m_ghostRect;
  QRect m_lastGhostRect; // v119: union'd with m_ghostRect for partial update
  bool m_showGhost;
  QColor m_accentColor;
  QSize m_itemSize; // The size of the content (folder)

  QPoint calculateSnapPosition(const QPoint &pos);
};

#endif // FREEGRIDVIEW_H
