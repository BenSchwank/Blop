#pragma once

#include <QColor>
#include <QWidget>

/// Blop desktop window chrome: a compact segmented capsule (min / max / close)
/// with soft traffic-light hovers and custom vector glyphs — Windows order,
/// iOS softness, Blop blue accent on press.
class BlopWindowControls : public QWidget {
  Q_OBJECT
public:
  explicit BlopWindowControls(QWidget *parent = nullptr);

  void setChrome(const QColor &foreground, bool lightTitleBar);
  void setMaximized(bool max);

signals:
  void minimizeRequested();
  void maximizeRequested();
  void closeRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  QSize sizeHint() const override;

private:
  enum class Hit { None, Min, Max, Close };

  QRect pillRect() const;
  QRect cellRect(int index) const;
  Hit hitTest(const QPoint &pos) const;
  void paintGlyph(QPainter &p, Hit which, const QRect &cell,
                  const QColor &iconColor) const;
  QColor hoverFill(Hit which) const;
  void setHover(Hit h);

  QColor m_fg{0x1C, 0x1E, 0x24};
  bool m_lightBar{true};
  bool m_maximized{false};
  Hit m_hover{Hit::None};
  Hit m_pressed{Hit::None};
};
