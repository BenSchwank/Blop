#include "radialtoolbarfab.h"

#include "moderntoolbar.h"
#include "tools/AbstractTool.h"
#include "tools/ToolManager.h"
#include "ToolSettings.h"
#include "uiscale.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

RadialToolbarFab::RadialToolbarFab(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  setCursor(Qt::PointingHandCursor);
  setFixedSize(collapsedSize(), collapsedSize());

  m_sectors = {
      {ToolMode::Hand, QStringLiteral("hand"), QString()},
      {ToolMode::Lasso, QStringLiteral("lasso"), QString()},
      {ToolMode::Pen, QStringLiteral("pen"), QStringLiteral("2")},
      {ToolMode::Pencil, QStringLiteral("pencil"), QStringLiteral("2")},
      {ToolMode::Highlighter, QStringLiteral("highlighter"), QStringLiteral("20")},
      {ToolMode::Eraser, QStringLiteral("eraser"), QStringLiteral("10")},
      {ToolMode::Text, QStringLiteral("text"), QStringLiteral("10")},
      {ToolMode::Shape, QStringLiteral("shape"), QString()},
  };
  refreshBadges();

  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &) { refreshBadges(); update(); });
  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this](AbstractTool *t) {
            if (t)
              setActiveTool(t->mode());
          });
}

int RadialToolbarFab::collapsedSize() const { return UiScale::dp(52); }
int RadialToolbarFab::expandedSize() const { return UiScale::dp(280); }

void RadialToolbarFab::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  m_accent = color;
  update();
}

void RadialToolbarFab::setActiveTool(ToolMode mode) {
  m_active = mode;
  update();
}

void RadialToolbarFab::refreshBadges() {
  const ToolConfig cfg = ToolManager::instance().config();
  const QString w = QString::number(qMax(1, cfg.penWidth));
  for (Sector &s : m_sectors) {
    switch (s.mode) {
    case ToolMode::Pen:
    case ToolMode::Pencil:
    case ToolMode::Eraser:
      s.badge = w;
      break;
    case ToolMode::Highlighter:
      s.badge = QString::number(qMax(8, cfg.penWidth * 4));
      break;
    case ToolMode::Text:
      s.badge = QStringLiteral("10");
      break;
    default:
      s.badge.clear();
      break;
    }
  }
}

void RadialToolbarFab::setExpanded(bool on) {
  if (m_expanded == on)
    return;
  const QPoint center = geometry().center();
  m_expanded = on;
  const int s = on ? expandedSize() : collapsedSize();
  setFixedSize(s, s);
  if (parentWidget()) {
    QPoint tl = center - QPoint(s / 2, s / 2);
    tl.setX(qBound(0, tl.x(), parentWidget()->width() - s));
    tl.setY(qBound(0, tl.y(), parentWidget()->height() - s));
    move(tl);
  }
  raise();
  update();
  emit expandChanged(m_expanded);
}

int RadialToolbarFab::hitSector(const QPoint &pos) const {
  if (!m_expanded)
    return -1;
  const QPointF c(width() / 2.0, height() / 2.0);
  const QPointF d = QPointF(pos) - c;
  const qreal dist = qSqrt(d.x() * d.x() + d.y() * d.y());
  const qreal inner = UiScale::dp(36);
  const qreal outer = width() / 2.0 - UiScale::dp(18);
  if (dist < inner)
    return -2; // center = undo
  if (dist > outer)
    return -1;
  qreal ang = qAtan2(d.y(), d.x()); // -pi..pi, 0 = east
  ang = ang + M_PI / 2.0;           // 0 = north
  if (ang < 0)
    ang += 2 * M_PI;
  const int n = m_sectors.size();
  const int idx = int(ang / (2 * M_PI / n)) % n;
  return idx;
}

void RadialToolbarFab::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);

  if (!m_expanded) {
    // Collapsed FAB — Drawboard-like circular trigger with Blop accent.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 80));
    p.drawEllipse(r.translated(0, 2));
    p.setBrush(QColor(18, 16, 28, 250));
    QColor border = m_accent;
    border.setAlpha(140);
    p.setPen(QPen(border, 1.5));
    p.drawEllipse(r);

    // Lime/white tip mark (Drawboard cue) tinted with accent + white.
    const QPointF c = r.center();
    QPainterPath tip;
    tip.moveTo(c.x() - 6, c.y() + 4);
    tip.lineTo(c.x() - 2, c.y() - 8);
    tip.lineTo(c.x() + 8, c.y() + 2);
    tip.closeSubpath();
    p.setPen(Qt::NoPen);
    QColor tipCol = m_accent.lighter(130);
    tipCol.setGreen(qMin(255, tipCol.green() + 40));
    p.setBrush(tipCol);
    p.drawPath(tip);
    p.setBrush(Qt::white);
    QPainterPath nib;
    nib.moveTo(c.x() + 1, c.y() - 1);
    nib.lineTo(c.x() + 9, c.y() + 3);
    nib.lineTo(c.x() + 3, c.y() + 5);
    nib.closeSubpath();
    p.drawPath(nib);
    return;
  }

  // Expanded radial wheel.
  const QPointF c(width() / 2.0, height() / 2.0);
  const qreal outerR = width() / 2.0 - 4;
  const qreal toolR = outerR * 0.62;
  const qreal hubR = UiScale::dp(28);

  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 90));
  p.drawEllipse(c + QPointF(0, 3), outerR, outerR);

  p.setBrush(QColor(16, 14, 26, 245));
  QColor ring = m_accent;
  ring.setAlpha(90);
  p.setPen(QPen(ring, 1.5));
  p.drawEllipse(c, outerR, outerR);

  // Outer navigation ring (annulus)
  {
    QPainterPath ringPath;
    ringPath.addEllipse(c, outerR - 2, outerR - 2);
    QPainterPath hole;
    hole.addEllipse(c, toolR + UiScale::dp(10), toolR + UiScale::dp(10));
    p.setBrush(QColor(28, 26, 42, 220));
    p.setPen(Qt::NoPen);
    p.drawPath(ringPath.subtracted(hole));
  }

  // Inner disc
  p.setBrush(QColor(20, 18, 34, 250));
  p.setPen(QPen(QColor(255, 255, 255, 18), 1));
  p.drawEllipse(c, toolR + UiScale::dp(8), toolR + UiScale::dp(8));

  const int n = m_sectors.size();
  for (int i = 0; i < n; ++i) {
    const qreal ang = -M_PI / 2.0 + (2.0 * M_PI * i) / n;
    const QPointF pos(c.x() + qCos(ang) * toolR, c.y() + qSin(ang) * toolR);
    const bool active = m_sectors[i].mode == m_active;
    if (active) {
      p.setBrush(m_accent);
      p.setPen(Qt::NoPen);
      p.drawEllipse(pos, UiScale::dp(22), UiScale::dp(22));
    }
    p.save();
    p.translate(pos);
    const qreal g = UiScale::dp(34) / 64.0;
    p.scale(g, g);
    p.translate(-32, -32);
    const QColor fg = active ? Qt::white : QColor(236, 240, 252);
    blopDrawToolbarGlyph64(&p, m_sectors[i].icon, fg);
    p.restore();

    if (!m_sectors[i].badge.isEmpty()) {
      const QRectF badge(pos.x() + UiScale::dp(8), pos.y() - UiScale::dp(18),
                         UiScale::dp(18), UiScale::dp(18));
      p.setBrush(QColor(10, 10, 14, 230));
      p.setPen(QPen(QColor(255, 255, 255, 40), 1));
      p.drawEllipse(badge);
      p.setPen(Qt::white);
      QFont f = p.font();
      f.setPixelSize(UiScale::font(9));
      f.setBold(true);
      p.setFont(f);
      p.drawText(badge, Qt::AlignCenter, m_sectors[i].badge);
    }
  }

  // Center hub — undo
  p.setBrush(m_accent);
  p.setPen(Qt::NoPen);
  p.drawEllipse(c, hubR, hubR);
  p.save();
  p.translate(c);
  const qreal g = UiScale::dp(28) / 64.0;
  p.scale(g, g);
  p.translate(-32, -32);
  blopDrawToolbarGlyph64(&p, QStringLiteral("undo"), Qt::white);
  p.restore();
}

void RadialToolbarFab::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  m_pressPos = event->globalPosition().toPoint();
  m_dragOffset = event->pos();
  m_dragging = true;
  m_moved = false;
  event->accept();
}

void RadialToolbarFab::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging || !parentWidget())
    return;
  const QPoint g = event->globalPosition().toPoint();
  if ((g - m_pressPos).manhattanLength() > 6)
    m_moved = true;
  if (!m_expanded && m_moved) {
    QPoint tl = parentWidget()->mapFromGlobal(g - m_dragOffset);
    tl.setX(qBound(0, tl.x(), parentWidget()->width() - width()));
    tl.setY(qBound(0, tl.y(), parentWidget()->height() - height()));
    move(tl);
  }
  event->accept();
}

void RadialToolbarFab::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  m_dragging = false;
  if (m_moved) {
    event->accept();
    return;
  }
  if (!m_expanded) {
    setExpanded(true);
    event->accept();
    return;
  }
  const int hit = hitSector(event->pos());
  if (hit == -2) {
    emit undoRequested();
    setExpanded(false);
  } else if (hit >= 0 && hit < m_sectors.size()) {
    const ToolMode mode = m_sectors[hit].mode;
    setActiveTool(mode);
    ToolManager::instance().selectTool(mode);
    emit toolSelected(mode);
    setExpanded(false);
  } else {
    setExpanded(false);
  }
  event->accept();
}
