#pragma once

#include "ToolMode.h"

#include <QEvent>
#include <QSet>
#include <QWidget>
#include <functional>

/// Drawboard-style "Neues Tool wählen" overlay: categories + tool grid.
/// Picking a tool adds it to the vertical Favorites/rail toolbar.
class ToolPickerOverlay : public QWidget {
  Q_OBJECT
public:
  using SelectFn = std::function<void(ToolMode)>;

  static void present(QWidget *host, const QColor &accent, SelectFn onSelect);
  static void present(QWidget *host, const QColor &accent,
                      const QSet<ToolMode> &alreadyInToolbar, SelectFn onSelect);

signals:
  void toolPicked(ToolMode mode);

protected:
  bool event(QEvent *e) override;

private:
  explicit ToolPickerOverlay(QWidget *host, const QColor &accent,
                             const QSet<ToolMode> &alreadyInToolbar,
                             SelectFn onSelect);
  void rebuildGrid(int categoryIndex);

  QColor m_accent;
  SelectFn m_onSelect;
  QSet<ToolMode> m_inToolbar;
  class QButtonGroup *m_tabs{nullptr};
  class QWidget *m_gridHost{nullptr};
  class QLineEdit *m_search{nullptr};
};
