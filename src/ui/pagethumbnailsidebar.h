#pragma once

// PageThumbnailSidebar - persistent vertical page strip, Drawboard-style.
//
// Shows all pages of the current note as a scrollable list of thumbnails
// on the left edge of the editor (Drawboard-style). Clicking a thumbnail
// jumps to that page. A quiet "+" control at the bottom appends a blank page.
// Collapsible so the canvas can go full-bleed when the rail is not needed.

#include <QColor>
#include <QWidget>

class MultiPageNoteView;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class Note;

class PageThumbnailSidebar : public QWidget {
  Q_OBJECT
public:
  explicit PageThumbnailSidebar(QWidget *parent = nullptr);

  void setNoteView(MultiPageNoteView *view);
  void setAccentColor(const QColor &color);

  void rebuild();

  bool isCollapsed() const { return m_collapsed; }
  void setCollapsed(bool collapsed);

public slots:
  void onCurrentPageChanged(int pageIndex);
  void toggleCollapsed();

signals:
  void pageSelected(int pageIndex);
  void addPageRequested();
  void collapsedChanged(bool collapsed);

private:
  void onItemClicked(int row);
  void requestThumbnail(int pageIndex, QListWidgetItem *item, int epoch);
  void refreshListStyle();
  void applyCollapsedState();

  MultiPageNoteView *m_view{nullptr};
  QListWidget *m_list{nullptr};
  QPushButton *m_btnAddPage{nullptr};
  QPushButton *m_btnToggle{nullptr};
  QWidget *m_railBody{nullptr};
  QColor m_accentColor{QColor(QStringLiteral("#7C5CFC"))};
  int m_currentPage{-1};
  int m_rebuildEpoch{0};
  bool m_collapsed{false};
  int m_expandedWidth{0};
};
