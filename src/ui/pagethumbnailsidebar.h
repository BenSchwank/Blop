#pragma once

// Drawboard Pages panel — scrollable page thumbnails on the left of the
// canvas. Supports 1/2-column layout, context actions, and bookmarks.

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

  /// Floating Drawboard panel (rounded, translucent) vs docked column.
  void setFloatingMode(bool on);
  bool isFloatingMode() const { return m_floatingMode; }

  void setTwoColumnMode(bool on);
  bool twoColumnMode() const { return m_twoColumn; }
  void toggleTwoColumnMode();

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
  void pagesMutated();
  void openAllPagesRequested();

private:
  void onItemClicked(int row);
  void showItemContextMenu(const QPoint &pos);
  void requestThumbnail(int pageIndex, QListWidgetItem *item, int epoch);
  void refreshListStyle();
  void applyCollapsedState();
  void applyViewMode();

  MultiPageNoteView *m_view{nullptr};
  QListWidget *m_list{nullptr};
  QPushButton *m_btnAddPage{nullptr};
  QPushButton *m_btnToggle{nullptr};
  QPushButton *m_btnColumns{nullptr};
  QWidget *m_railBody{nullptr};
  QColor m_accentColor{QColor(QStringLiteral("#5B9DFF"))};
  int m_currentPage{-1};
  int m_rebuildEpoch{0};
  bool m_collapsed{false};
  bool m_floatingMode{false};
  bool m_twoColumn{false};
  int m_expandedWidth{0};
};
