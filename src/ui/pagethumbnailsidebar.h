#pragma once

// PageThumbnailSidebar - persistent vertical page strip, Drawboard-style.
//
// Drawboard Pages panel — scrollable page thumbnails on the left of the
// canvas. Clicking a thumbnail jumps to that page; "+ Seite" appends a blank
// page. On desktop this is a solid left column (not a floating pill).

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
  QColor m_accentColor{QColor(QStringLiteral("#5B9DFF"))};
  int m_currentPage{-1};
  int m_rebuildEpoch{0};
  bool m_collapsed{false};
  bool m_floatingMode{false};
  int m_expandedWidth{0};
};
