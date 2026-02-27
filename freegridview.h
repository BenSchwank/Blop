#ifndef FREEGRIDVIEW_H
#define FREEGRIDVIEW_H

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QListView>
#include <QPainter>


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

protected:
  void paintEvent(QPaintEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dragLeaveEvent(QDragLeaveEvent *e) override;
  void dropEvent(QDropEvent *e) override;
  void startDrag(Qt::DropActions supportedActions) override;

private:
  QRect m_ghostRect;
  bool m_showGhost;
  QColor m_accentColor;
  QSize m_itemSize; // The size of the content (folder)

  QPoint calculateSnapPosition(const QPoint &pos);
};

#endif // FREEGRIDVIEW_H
