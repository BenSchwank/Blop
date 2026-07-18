#pragma once

// Drawboard-style "All Pages" grid overlay — browse, jump, multi-select
// duplicate/delete across the whole note.

#include <QWidget>
#include <QSet>

class MultiPageNoteView;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;

class AllPagesOverlay : public QWidget {
  Q_OBJECT
public:
  explicit AllPagesOverlay(QWidget *parent = nullptr);

  void setNoteView(MultiPageNoteView *view);
  void setAccentColor(const QColor &c);
  void rebuild();
  void present();
  void dismiss();

signals:
  void pageActivated(int pageIndex);
  void pagesChanged();

protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void onItemClicked(QListWidgetItem *item);
  void applyBatchDuplicate();
  void applyBatchDelete();
  void refreshChrome();

  MultiPageNoteView *m_view{nullptr};
  QWidget *m_card{nullptr};
  QListWidget *m_grid{nullptr};
  QLabel *m_title{nullptr};
  QPushButton *m_btnDup{nullptr};
  QPushButton *m_btnDel{nullptr};
  QPushButton *m_btnClose{nullptr};
  QColor m_accent{QColor(91, 157, 255)};
  int m_epoch{0};
};
