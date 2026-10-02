#pragma once

#include "ToolMode.h"

#include <QWidget>
#include <functional>

class QGraphicsTextItem;
class QHBoxLayout;
class QScrollArea;

/// One row under the window title bar. Contents follow the active tool.
class ToolOptionsStrip : public QWidget {
  Q_OBJECT
public:
  explicit ToolOptionsStrip(QWidget *parent = nullptr);

  void setEditingResolver(std::function<QGraphicsTextItem *()> resolver);
  void refreshTheme();

private:
  void rebuild();
  void clearRow();
  QGraphicsTextItem *editingItem() const;
  void publish(const struct ToolConfig &cfg);

  QScrollArea *m_scroll{nullptr};
  QWidget *m_row{nullptr};
  QHBoxLayout *m_rowLay{nullptr};
  std::function<QGraphicsTextItem *()> m_resolver;
  ToolMode m_mode{ToolMode::Pen};
  bool m_rebuilding{false};
};
