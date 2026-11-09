#pragma once

#include <QWidget>

class GraphCanvasItem;
class QLabel;
class QTableWidget;

/// Compact x | f(x) value table for the selected graph function.
class GraphValueTableDock : public QWidget {
  Q_OBJECT
public:
  explicit GraphValueTableDock(QWidget *parent = nullptr);

  void bind(GraphCanvasItem *item);
  void refreshChrome();

private:
  void rebuildTable(GraphCanvasItem *item);

  QLabel *m_title{nullptr};
  QTableWidget *m_table{nullptr};
};
