#pragma once

#include "ToolMode.h"

#include <QColor>
#include <QVector>
#include <QWidget>

/// Drawboard-style movable circular FAB that expands into a radial tool wheel.
class RadialToolbarFab : public QWidget {
  Q_OBJECT
public:
  explicit RadialToolbarFab(QWidget *parent = nullptr);

  void setAccentColor(const QColor &color);
  void setExpanded(bool on);
  bool isExpanded() const { return m_expanded; }
  void setActiveTool(ToolMode mode);

  int collapsedSize() const;
  int expandedSize() const;

signals:
  void toolSelected(ToolMode mode);
  void undoRequested();
  void expandChanged(bool expanded);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  struct Sector {
    ToolMode mode;
    QString icon;
    QString badge;
  };

  int hitSector(const QPoint &pos) const;
  void refreshBadges();

  QColor m_accent{QColor(QStringLiteral("#7C5CFC"))};
  bool m_expanded{false};
  bool m_dragging{false};
  bool m_moved{false};
  QPoint m_pressPos;
  QPoint m_dragOffset;
  ToolMode m_active{ToolMode::Pen};
  QVector<Sector> m_sectors;
};
