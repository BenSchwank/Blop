#ifndef ANDROIDTILEDELEGATE_H
#define ANDROIDTILEDELEGATE_H

#include <QStyledItemDelegate>

class MainWindow;

// Replaces ModernItemDelegate on Android for the Welcome / file overview
// list. The Windows delegate carries a lot of historic baggage (wide-list
// branch, iconShrink factor, 64-coordinate Windows-flavoured glyphs) which
// produced the "tiny / grey" tile icons reported on phones. This delegate
// is intentionally minimal:
//
// - One layout: square Material card, big centered icon, two-line caption.
// - Icons come from AndroidIcons (cached, path-based, scale to any size).
// - No per-frame QPixmap allocations: AndroidIcons hashes by pixel size.
//
// Click handling matches the Windows delegate: tapping the More-pill in
// the top-right corner opens the context menu. Anything else falls
// through to QListView's default selection / activation handling.
class AndroidTileDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit AndroidTileDelegate(MainWindow *win, QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

private:
  MainWindow *m_window;
};

#endif // ANDROIDTILEDELEGATE_H
