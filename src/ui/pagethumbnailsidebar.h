#pragma once

// PageThumbnailSidebar - persistent vertical page strip, Drawboard-style.
//
// Shows all pages of the current note as a scrollable list of thumbnails
// on the left edge of the editor. Clicking a thumbnail jumps to that page.

#include <QColor>
#include <QWidget>

class MultiPageNoteView;
class QListWidget;
class QListWidgetItem;
class Note;

class PageThumbnailSidebar : public QWidget {
  Q_OBJECT
public:
  explicit PageThumbnailSidebar(QWidget *parent = nullptr);

  void setNoteView(MultiPageNoteView *view);
  void setAccentColor(const QColor &color);

  void rebuild();

public slots:
  void onCurrentPageChanged(int pageIndex);

signals:
  void pageSelected(int pageIndex);

private:
  void onItemClicked(int row);
  void requestThumbnail(int pageIndex, QListWidgetItem *item, int epoch);

  MultiPageNoteView *m_view{nullptr};
  QListWidget *m_list{nullptr};
  QColor m_accentColor{QColor(QStringLiteral("#7C5CFC"))};
  int m_currentPage{-1};
  int m_rebuildEpoch{0};
};
