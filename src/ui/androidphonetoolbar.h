#pragma once

// AndroidPhoneToolbar - schlanke Bottom-Pille fuer Android Phones.
//
// Wird im Editor instanziiert wenn UiScale::isAndroidTablet() == false.
// Auf Desktop und Android-Tablets bleibt ModernToolbar.
//
// Sichtbare Werkzeuge: Pen, Eraser, Lasso, |, Undo, Redo, |, Color, Brush, |, Overflow
// Overflow-QMenu: Pencil, Highlighter, Ruler, Shape, Sticky, Text, Image, Hand, Save, Back

#include <QColor>
#include <QPointer>
#include <QVector>
#include <QWidget>

#include "ToolMode.h"
#include "ToolSettings.h"

class ToolbarBtn;
class QPaintEvent;
class QResizeEvent;

class AndroidPhoneToolbar : public QWidget {
  Q_OBJECT
public:
  explicit AndroidPhoneToolbar(QWidget *parent = nullptr);

  void setToolMode(ToolMode mode);
  ToolMode toolMode() const { return m_mode; }

  void setAccentColor(const QColor &color);

  // Blop Rail: collapse the pill into a small teardrop grip at the bottom
  // right edge; tapping the grip expands it again.
  bool isCollapsed() const { return m_collapsed; }
  void setCollapsed(bool collapsed);

  // Same hint that ModernToolbar exposes - callers don't care which
  // toolbar variant they have, they can just call this.
  int preferredHeightPx() const;

signals:
  void toolChanged(ToolMode mode);
  void undoRequested();
  void redoRequested();
  void penConfigChanged(QColor c, int w);
  void backToOverviewRequested();

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

private:
  ToolbarBtn *btnPen{nullptr};
  ToolbarBtn *btnEraser{nullptr};
  ToolbarBtn *btnLasso{nullptr};
  ToolbarBtn *btnUndo{nullptr};
  ToolbarBtn *btnRedo{nullptr};
  ToolbarBtn *btnColor{nullptr};
  ToolbarBtn *btnBrush{nullptr};
  ToolbarBtn *btnOverflow{nullptr};

  QVector<ToolbarBtn *> m_toolButtons;

  ToolMode m_mode{ToolMode::Pen};
  ToolConfig m_config;
  QColor m_accentColor{QColor("#7C5CFC")};

  void setupButtons();
  void selectTool(ToolMode mode);
  void emitPenConfig();
  void showOverflowMenu();
  void showToolBloom(ToolbarBtn *anchorBtn);
  void showColorPicker();
  void showBrushSizeSheet();
  QString activeToolIconName() const;

  bool m_collapsed{false};
  bool m_swipeTracking{false};
  QPoint m_swipeStart;
  QPointer<QWidget> m_grip;

  // X-positions of vertical separators after layout (drawn in paintEvent).
  QVector<int> m_separatorX;
};
