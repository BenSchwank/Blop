#pragma once

#include <QScrollArea>
#include <QScrollBar>

/// QScrollArea that can freeze vertical scrolling hard (blocks scrollContentsBy).
class DashPageScroll : public QScrollArea {
  Q_OBJECT
public:
  explicit DashPageScroll(QWidget *parent = nullptr) : QScrollArea(parent) {}

  void setScrollFrozen(bool frozen, int y = -1) {
    m_frozen = frozen;
    if (frozen) {
      m_frozenY = (y >= 0) ? y : verticalScrollBar()->value();
      QSignalBlocker block(verticalScrollBar());
      verticalScrollBar()->setValue(m_frozenY);
    }
  }

  bool scrollFrozen() const { return m_frozen; }
  int frozenY() const { return m_frozenY; }

protected:
  void scrollContentsBy(int dx, int dy) override {
    if (m_frozen) {
      QScrollArea::scrollContentsBy(dx, 0);
      QSignalBlocker block(verticalScrollBar());
      verticalScrollBar()->setValue(m_frozenY);
      return;
    }
    QScrollArea::scrollContentsBy(dx, dy);
  }

private:
  bool m_frozen{false};
  int m_frozenY{0};
};
