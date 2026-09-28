#pragma once

// Digital register tabs for A4 notes: one slim tab per page along the left
// canvas edge (page number + optional bookmark name). Click jumps, hover shows
// a mini preview card, wheel / arrow keys step pages, right-click manages.

#include <QPixmap>
#include <QPointer>
#include <QWidget>

class MultiPageNoteView;
class QTimer;

class PageBookmarkRail : public QWidget {
  Q_OBJECT
public:
  explicit PageBookmarkRail(QWidget *parent = nullptr);

  void setNoteView(MultiPageNoteView *view);
  MultiPageNoteView *noteView() const { return m_view; }

  /// Places the rail in its parent: left edge `x`, available band [top, bottom).
  void placeIn(int x, int top, int bottom);
  void refreshTheme();

public slots:
  void rebuild();
  void syncCurrentPage();

signals:
  void pageActivated(int pageIndex);
  void pagesMutated();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  struct Tab {
    int page{-1}; // -1 = "+" tab
    QString label;
    bool bookmarked{false};
    QRect rect; // content coordinates (before scroll offset)
  };

  void layoutTabs();
  int tabAt(const QPoint &pos) const;
  QRect visualRect(const Tab &t) const;
  void ensureVisible(int page);
  void setScroll(int y);
  void stepPage(int delta);
  void activate(int page);
  void showPreview(int tabIndex);
  void hidePreview();
  void showTabMenu(int page, const QPoint &globalPos);

  QPointer<MultiPageNoteView> m_view;
  QVector<Tab> m_tabs;
  int m_current{0};
  int m_hover{-1};
  int m_scroll{0};
  int m_contentH{0};
  int m_bandTop{0};
  int m_bandBottom{0};
  int m_x{0};
  QWidget *m_preview{nullptr};
  QTimer *m_previewTimer{nullptr};
  QTimer *m_syncTimer{nullptr};
  int m_previewEpoch{0};
};
