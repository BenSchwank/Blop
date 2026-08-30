#include "blopwindowcontrols.h"

#include "uiscale.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {
constexpr int kCells = 3;

QColor mix(const QColor &a, const QColor &b, qreal t) {
  return QColor(qRound(a.red() + (b.red() - a.red()) * t),
                qRound(a.green() + (b.green() - a.green()) * t),
                qRound(a.blue() + (b.blue() - a.blue()) * t),
                qRound(a.alpha() + (b.alpha() - a.alpha()) * t));
}

QPainterPath segmentHoverPath(const QRectF &cell, int index, qreal radius) {
  const qreal inset = UiScale::dp(2);
  QPainterPath path;
  if (index == 0) {
    path.moveTo(cell.right() - inset, cell.top());
    path.lineTo(cell.left() + radius, cell.top());
    path.arcTo(cell.left(), cell.top(), radius * 2, radius * 2, 90, 90);
    path.lineTo(cell.left(), cell.bottom() - radius);
    path.arcTo(cell.left(), cell.bottom() - radius * 2, radius * 2, radius * 2,
               180, 90);
    path.lineTo(cell.right() - inset, cell.bottom());
    path.closeSubpath();
  } else if (index == kCells - 1) {
    path.moveTo(cell.left() + inset, cell.top());
    path.lineTo(cell.right() - radius, cell.top());
    path.arcTo(cell.right() - radius * 2, cell.top(), radius * 2, radius * 2,
               90, -90);
    path.lineTo(cell.right(), cell.bottom() - radius);
    path.arcTo(cell.right() - radius * 2, cell.bottom() - radius * 2,
               radius * 2, radius * 2, 0, -90);
    path.lineTo(cell.left() + inset, cell.bottom());
    path.closeSubpath();
  } else {
    QRectF mid = cell.adjusted(inset + 1, 0, -(inset + 1), 0);
    path.addRoundedRect(mid, UiScale::dp(4), UiScale::dp(4));
  }
  return path;
}
} // namespace

BlopWindowControls::BlopWindowControls(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("BlopWindowControls"));
  setAttribute(Qt::WA_Hover, true);
  setMouseTracking(true);
  setCursor(Qt::ArrowCursor);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setFixedSize(UiScale::dp(124), UiScale::dp(38));
}

void BlopWindowControls::setChrome(const QColor &foreground, bool lightTitleBar) {
  m_fg = foreground;
  m_lightBar = lightTitleBar;
  update();
}

void BlopWindowControls::setMaximized(bool max) {
  if (m_maximized == max)
    return;
  m_maximized = max;
  update();
}

QSize BlopWindowControls::sizeHint() const {
  return QSize(UiScale::dp(124), UiScale::dp(38));
}

QRect BlopWindowControls::pillRect() const {
  return rect().adjusted(UiScale::dp(4), UiScale::dp(5), -UiScale::dp(4),
                         -UiScale::dp(5));
}

QRect BlopWindowControls::cellRect(int index) const {
  const QRect pill = pillRect();
  const int cellW = pill.width() / kCells;
  return QRect(pill.left() + index * cellW, pill.top(), cellW, pill.height());
}

BlopWindowControls::Hit BlopWindowControls::hitTest(const QPoint &pos) const {
  if (!pillRect().contains(pos))
    return Hit::None;
  const QRect pill = pillRect();
  const int cellW = qMax(1, pill.width() / kCells);
  const int idx = qBound(0, (pos.x() - pill.left()) / cellW, kCells - 1);
  if (idx == 0)
    return Hit::Min;
  if (idx == 1)
    return Hit::Max;
  return Hit::Close;
}

void BlopWindowControls::setHover(Hit h) {
  if (m_hover == h)
    return;
  m_hover = h;
  update();
}

QColor BlopWindowControls::hoverFill(Hit which) const {
  if (which == Hit::None)
    return Qt::transparent;
  switch (which) {
  case Hit::Min:
    return QColor(0xF5, 0xC4, 0x51, m_lightBar ? 88 : 110);
  case Hit::Max:
    return QColor(0x34, 0xC7, 0x59, m_lightBar ? 88 : 110);
  case Hit::Close:
    return QColor(0xFF, 0x5F, 0x57, m_lightBar ? 100 : 128);
  default:
    return Qt::transparent;
  }
}

void BlopWindowControls::paintGlyph(QPainter &p, Hit which, const QRect &cell,
                                    const QColor &iconColor) const {
  const QPointF c = cell.center();
  p.setPen(Qt::NoPen);
  p.setBrush(iconColor);

  if (which == Hit::Min) {
    const qreal w = UiScale::dp(10);
    const qreal h = UiScale::dp(2);
    p.drawRoundedRect(QRectF(c.x() - w / 2, c.y() - h / 2, w, h), h / 2, h / 2);
    return;
  }

  if (which == Hit::Max) {
    p.setBrush(Qt::NoBrush);
    const qreal pen = qMax<qreal>(1.5, UiScale::dp(2));
    p.setPen(QPen(iconColor, pen, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (!m_maximized) {
      // Clear single square (Windows maximize).
      const qreal s = UiScale::dp(7);
      p.drawRoundedRect(QRectF(c.x() - s, c.y() - s, s * 2, s * 2), 1.5, 1.5);
    } else {
      // Restore: back square peek + front square (readable, not chain-like).
      const qreal s = UiScale::dp(6);
      const qreal off = UiScale::dp(3);
      p.drawRoundedRect(QRectF(c.x() - s + off, c.y() - s - off / 2, s * 1.6,
                               s * 1.6),
                        1.2, 1.2);
      p.setBrush(m_lightBar ? QColor(0xFF, 0xFF, 0xFF, 235)
                            : QColor(0x28, 0x2A, 0x30, 240));
      p.drawRoundedRect(QRectF(c.x() - s - off / 2, c.y() - s + off, s * 1.6,
                               s * 1.6),
                        1.2, 1.2);
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(QRectF(c.x() - s - off / 2, c.y() - s + off, s * 1.6,
                               s * 1.6),
                        1.2, 1.2);
    }
    return;
  }

  if (which == Hit::Close) {
    const qreal arm = UiScale::dp(6);
    const qreal pen = qMax<qreal>(1.5, UiScale::dp(2));
    const QColor xCol =
        (m_hover == Hit::Close || m_pressed == Hit::Close)
            ? QColor(0xFF, 0xFF, 0xFF)
            : iconColor;
    p.setPen(QPen(xCol, pen, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(c + QPointF(-arm, -arm), c + QPointF(arm, arm));
    p.drawLine(c + QPointF(arm, -arm), c + QPointF(-arm, arm));
  }
}

void BlopWindowControls::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QRect pill = pillRect();
  const qreal radius = UiScale::dp(10);

  // Soft drop shadow — floats above the title bar.
  p.setPen(Qt::NoPen);
  for (int layer = 0; layer < 3; ++layer) {
    QPainterPath shadow;
    const int spread = layer + 1;
    shadow.addRoundedRect(
        QRectF(pill.adjusted(-spread, spread, spread, spread + 2)),
        radius + spread, radius + spread);
    p.fillPath(shadow, QColor(0, 0, 0, 6 + layer * 7));
  }

  const QColor shellBg =
      m_lightBar ? QColor(0xFF, 0xFF, 0xFF, 235) : QColor(0x28, 0x2A, 0x30, 220);
  const QColor shellBorder =
      m_lightBar ? QColor(0x37, 0x35, 0x2F, 40) : QColor(0xFF, 0xFF, 0xFF, 42);

  QPainterPath shell;
  shell.addRoundedRect(QRectF(pill), radius, radius);
  p.fillPath(shell, shellBg);
  p.setPen(QPen(shellBorder, 1));
  p.drawPath(shell);

  const Hit active = m_pressed != Hit::None ? m_pressed : m_hover;
  const QColor divider =
      m_lightBar ? QColor(0x37, 0x35, 0x2F, 30) : QColor(0xFF, 0xFF, 0xFF, 34);

  for (int i = 0; i < kCells; ++i) {
    const Hit cellHit =
        (i == 0) ? Hit::Min : (i == 1) ? Hit::Max : Hit::Close;
    const QRect cell = cellRect(i);

    if (active == cellHit) {
      QColor fillCol = hoverFill(cellHit);
      if (m_pressed == cellHit)
        fillCol = mix(fillCol, QColor(0, 0, 0, m_lightBar ? 40 : 60), 0.22);
      QPainterPath hover = segmentHoverPath(QRectF(cell), i, radius - 1);
      p.fillPath(shell.intersected(hover), fillCol);
    }

    if (i > 0) {
      p.setPen(QPen(divider, 1));
      p.drawLine(cell.left(), cell.top() + UiScale::dp(7), cell.left(),
                 cell.bottom() - UiScale::dp(7));
    }

    QColor icon = m_fg;
    if (active == cellHit && cellHit == Hit::Close)
      icon = QColor(0xFF, 0xFF, 0xFF);
    else if (active == cellHit)
      icon = mix(m_fg, QColor(0x1C, 0x1E, 0x24), m_lightBar ? 0.12 : -0.08);

    paintGlyph(p, cellHit, cell, icon);
  }

  if (active != Hit::None && active != Hit::Close) {
    const int idx = (active == Hit::Min) ? 0 : 1;
    const QRect cell = cellRect(idx);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x5B, 0x9D, 0xFF, 120));
    const qreal uw = UiScale::dp(12);
    p.drawRoundedRect(
        QRectF(cell.center().x() - uw / 2, cell.bottom() - UiScale::dp(4), uw,
               UiScale::dp(2)),
        1, 1);
  }
}

void BlopWindowControls::mouseMoveEvent(QMouseEvent *event) {
  setHover(hitTest(event->pos()));
  QWidget::mouseMoveEvent(event);
}

void BlopWindowControls::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_pressed = hitTest(event->pos());
    update();
  }
  QWidget::mousePressEvent(event);
}

void BlopWindowControls::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    const Hit released = hitTest(event->pos());
    const Hit was = m_pressed;
    m_pressed = Hit::None;
    update();
    if (was != Hit::None && was == released) {
      switch (was) {
      case Hit::Min:
        emit minimizeRequested();
        break;
      case Hit::Max:
        emit maximizeRequested();
        break;
      case Hit::Close:
        emit closeRequested();
        break;
      default:
        break;
      }
    }
  }
  QWidget::mouseReleaseEvent(event);
}

void BlopWindowControls::leaveEvent(QEvent *event) {
  setHover(Hit::None);
  m_pressed = Hit::None;
  QWidget::leaveEvent(event);
}
