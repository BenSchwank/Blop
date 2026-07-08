#pragma once

// BloomMenu - Blop's quick tool switcher ("Blop Bloom").
//
// A long-press on a tool button fans the full tool palette out of the
// press point as a staggered arc of squircle petals. Selecting a petal
// switches the tool; tapping anywhere else closes the bloom.
//
// In-window overlay (backdrop + petals are children of window()) - on
// Android (Qt 6.10 inproc, single window surface) top-level popups crash
// the process, same reasoning as PhoneBrushBackdrop.

#include <QColor>
#include <QPoint>
#include <QVector>
#include <QWidget>

#include "ToolMode.h"

class BloomPetal;

struct BloomItem {
  QString icon;
  ToolMode mode;
};

class BloomMenu : public QWidget {
  Q_OBJECT
public:
  // Opens the bloom anchored at `anchorWin` (window-local coordinates).
  // The returned pointer is owned by `win` and deletes itself on close.
  static BloomMenu *popup(QWidget *win, const QPoint &anchorWin,
                          const QVector<BloomItem> &items, ToolMode current,
                          const QColor &accent);

signals:
  void toolSelected(ToolMode mode);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;

private:
  explicit BloomMenu(QWidget *win);
  void buildPetals(const QPoint &anchorWin, const QVector<BloomItem> &items,
                   ToolMode current, const QColor &accent);
  void dismiss(bool instant = false);

  QVector<BloomPetal *> m_petals;
  qreal m_scrim{0.0};
  bool m_closing{false};
};
