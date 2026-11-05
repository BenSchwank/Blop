#pragma once

#include <QWidget>

class QAbstractButton;

/// Fixed 32dp column header above the desktop library list (high-density table).
class LibraryListHeader : public QWidget {
  Q_OBJECT
public:
  enum class SortColumn { Name, Place, Extent, Modified };

  explicit LibraryListHeader(QWidget *parent = nullptr);

  void setSort(SortColumn col, Qt::SortOrder order);
  SortColumn sortColumn() const { return m_col; }
  Qt::SortOrder sortOrder() const { return m_order; }

  void setMasterChecked(bool on);
  void setMasterTristate(Qt::CheckState state);

  void syncTheme();

signals:
  void sortRequested(LibraryListHeader::SortColumn col, Qt::SortOrder order);
  void selectAllRequested(bool selectAll);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void layoutCheck();
  QRect labelRect(int col) const;
  SortColumn columnAt(const QPoint &pos) const;

  QAbstractButton *m_master{nullptr};
  SortColumn m_col{SortColumn::Modified};
  Qt::SortOrder m_order{Qt::DescendingOrder};
  int m_hoverCol{-1};
};
