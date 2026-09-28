#include "notetoolbars.h"

#include "ToolSettings.h"
#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "moderntoolbar.h"
#include "tools/ToolManager.h"
#include "uiscale.h"

#include <QConicalGradient>
#include <QFont>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QSettings>
#include <QSlider>
#include <QVariantAnimation>
#include <QtMath>

namespace {

const QColor kBg(31, 34, 41);          // #1F2229
const QColor kBgSoft(43, 47, 56);      // ring tiles / separators
const QColor kActiveGray(58, 63, 75);  // #3A3F4B
const QColor kGlyph(242, 244, 248);    // #F2F4F8
const QColor kLabel(201, 205, 214);    // #C9CDD6
const QColor kAccent(91, 157, 255);    // #5B9DFF
const QColor kHighlighter(245, 197, 24);

constexpr int kPenPresets = 4;
const QColor kPresetColors[kPenPresets] = {
    QColor(20, 22, 28), QColor(37, 99, 235), QColor(220, 38, 38),
    QColor(22, 163, 74)};

QPointF polar(const QPointF &c, qreal r, qreal deg) {
  const qreal rad = qDegreesToRadians(deg);
  return QPointF(c.x() + r * std::cos(rad), c.y() - r * std::sin(rad));
}

QPainterPath ringSegment(const QPointF &c, qreal r0, qreal r1, qreal a0,
                         qreal a1) {
  QPainterPath path;
  const QRectF outer(c.x() - r1, c.y() - r1, 2 * r1, 2 * r1);
  const QRectF inner(c.x() - r0, c.y() - r0, 2 * r0, 2 * r0);
  path.arcMoveTo(outer, a0);
  path.arcTo(outer, a0, a1 - a0);
  path.arcTo(inner, a1, -(a1 - a0));
  path.closeSubpath();
  return path;
}

void drawDotsH(QPainter &p, const QPointF &c, qreal size, const QColor &col) {
  p.setPen(Qt::NoPen);
  p.setBrush(col);
  const qreal r = size * 0.085;
  const qreal gap = size * 0.3;
  p.drawEllipse(QPointF(c.x() - gap, c.y()), r, r);
  p.drawEllipse(c, r, r);
  p.drawEllipse(QPointF(c.x() + gap, c.y()), r, r);
}

void drawWheel(QPainter &p, const QRectF &r) {
  QConicalGradient g(r.center(), 90);
  for (int i = 0; i <= 12; ++i)
    g.setColorAt(i / 12.0, QColor::fromHsvF(std::fmod(i / 12.0, 1.0), 0.95, 0.98));
  p.setPen(Qt::NoPen);
  p.setBrush(g);
  p.drawEllipse(r);
  QRadialGradient white(r.center(), r.width() / 2);
  white.setColorAt(0.0, QColor(255, 255, 255, 230));
  white.setColorAt(0.55, QColor(255, 255, 255, 60));
  white.setColorAt(1.0, QColor(255, 255, 255, 0));
  p.setBrush(white);
  p.drawEllipse(r);
  p.setPen(QPen(QColor(15, 17, 22), 2.0));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(r.adjusted(1, 1, -1, -1));
}

} // namespace

// ─── settings / tool helpers ────────────────────────────────────────────────

NoteToolbarStyle noteToolbarStyle() {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QString v =
      s.value(QStringLiteral("ui/note_toolbar"), QStringLiteral("bar")).toString();
  if (v == QLatin1String("floating"))
    return NoteToolbarStyle::Floating;
  if (v == QLatin1String("radiant"))
    return NoteToolbarStyle::Radiant;
  if (v == QLatin1String("science"))
    return NoteToolbarStyle::Science;
  return NoteToolbarStyle::Bar;
}

void setNoteToolbarStyle(NoteToolbarStyle style) {
  QString v = QStringLiteral("bar");
  if (style == NoteToolbarStyle::Floating)
    v = QStringLiteral("floating");
  else if (style == NoteToolbarStyle::Radiant)
    v = QStringLiteral("radiant");
  else if (style == NoteToolbarStyle::Science)
    v = QStringLiteral("science");
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/note_toolbar"), v);
}

QString noteToolbarStyleLabel(NoteToolbarStyle style) {
  switch (style) {
  case NoteToolbarStyle::Floating:
    return QStringLiteral("Floating");
  case NoteToolbarStyle::Radiant:
    return QStringLiteral("Radiant");
  case NoteToolbarStyle::Science:
    return QStringLiteral("Wissenschaftlich");
  case NoteToolbarStyle::Bar:
    break;
  }
  return QStringLiteral("Balken");
}

void noteActivateTool(ToolMode mode, int shapeKind) {
  auto &mgr = ToolManager::instance();
  if (shapeKind >= 0) {
    ToolConfig cfg = mgr.configFor(ToolMode::Shape);
    cfg.shapeToolKind = static_cast<ShapeToolKind>(qBound(0, shapeKind, 7));
    mgr.configFor(ToolMode::Shape) = cfg;
    mode = ToolMode::Shape;
  }
  // selectTool() is a no-op when the tool instance is unchanged; bounce via
  // Hand so Shape Line -> Shape Circle still re-applies the config.
  if (mgr.activeTool() && mgr.activeToolMode() == mode && mode == ToolMode::Shape)
    mgr.selectTool(ToolMode::Hand);
  mgr.selectTool(mode);
  if (mgr.activeTool())
    mgr.activeTool()->setConfig(mgr.configFor(mode));
}

void noteApplyPenColor(const QColor &color) {
  auto &mgr = ToolManager::instance();
  ToolMode mode = mgr.activeToolMode();
  if (mode != ToolMode::Pen && mode != ToolMode::Pencil &&
      mode != ToolMode::Highlighter)
    mode = ToolMode::Pen;
  ToolConfig cfg = mgr.configFor(mode);
  cfg.penColor = color;
  mgr.configFor(mode) = cfg;
  mgr.selectTool(mode);
  if (mgr.activeTool())
    mgr.activeTool()->setConfig(cfg);
  emit mgr.configChanged(cfg);
}

void noteApplyPenWidth(int width) {
  auto &mgr = ToolManager::instance();
  ToolMode mode = mgr.activeToolMode();
  if (mode != ToolMode::Pen && mode != ToolMode::Pencil &&
      mode != ToolMode::Highlighter)
    mode = ToolMode::Pen;
  ToolConfig cfg = mgr.configFor(mode);
  cfg.penWidth = qBound(1, width, 40);
  mgr.configFor(mode) = cfg;
  if (mgr.activeTool() && mgr.activeToolMode() == mode)
    mgr.activeTool()->setConfig(cfg);
}

// ─── NoteToolbarBase ────────────────────────────────────────────────────────

NoteToolbarBase::NoteToolbarBase(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_Hover, true);
  m_pressAnim = new QVariantAnimation(this);
  m_pressAnim->setDuration(BlopMotion::kFast);
  m_pressAnim->setEasingCurve(QEasingCurve(BlopMotion::kEaseStandard));
  connect(m_pressAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant &v) {
            m_pressScale = v.toReal();
            update();
          });
  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this](AbstractTool *) { update(); });
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &) { update(); });
}

void NoteToolbarBase::setUndoRedoAvailable(bool canUndo, bool canRedo) {
  if (m_canUndo == canUndo && m_canRedo == canRedo)
    return;
  m_canUndo = canUndo;
  m_canRedo = canRedo;
  update();
}

void NoteToolbarBase::refreshActive() { update(); }

QColor NoteToolbarBase::activeFill() const { return kAccent; }
QColor NoteToolbarBase::activeGlyph() const { return QColor(255, 255, 255); }

bool NoteToolbarBase::tileActive(const NoteToolTile &t) const {
  if (t.action != NoteToolTile::Tool)
    return false;
  auto &mgr = ToolManager::instance();
  if (!mgr.activeTool() || mgr.activeToolMode() != t.mode)
    return false;
  if (t.mode == ToolMode::Shape && t.shapeKind >= 0) {
    return static_cast<int>(mgr.configFor(ToolMode::Shape).shapeToolKind) ==
           t.shapeKind;
  }
  return true;
}

int NoteToolbarBase::tileAt(const QPointF &pos) const {
  for (int i = 0; i < m_tiles.size(); ++i) {
    const NoteToolTile &t = m_tiles[i];
    if (!t.path.isEmpty()) {
      if (t.path.contains(pos))
        return i;
    } else if (t.rect.contains(pos)) {
      return i;
    }
  }
  return -1;
}

QColor NoteToolbarBase::currentPenColor() const {
  auto &mgr = ToolManager::instance();
  ToolMode mode = mgr.activeToolMode();
  if (mode != ToolMode::Pen && mode != ToolMode::Pencil &&
      mode != ToolMode::Highlighter)
    mode = ToolMode::Pen;
  return mgr.configFor(mode).penColor;
}

void NoteToolbarBase::activate(int index) {
  if (index < 0 || index >= m_tiles.size())
    return;
  const NoteToolTile &t = m_tiles[index];
  switch (t.action) {
  case NoteToolTile::Tool:
    noteActivateTool(t.mode, t.shapeKind);
    break;
  case NoteToolTile::Undo:
    emit undoRequested();
    break;
  case NoteToolTile::Redo:
    emit redoRequested();
    break;
  case NoteToolTile::More: {
    const QPointF c = t.path.isEmpty() ? t.rect.center() : t.iconCenter;
    emit moreRequested(mapToGlobal(c.toPoint()));
    break;
  }
  case NoteToolTile::Color:
    m_colorStep = (m_colorStep + 1) % kPenPresets;
    noteApplyPenColor(kPresetColors[m_colorStep]);
    break;
  case NoteToolTile::Wheel:
    break; // handled in mousePressEvent (needs the click position)
  case NoteToolTile::Options:
  case NoteToolTile::Center:
    emit toolOptionsRequested();
    break;
  }
  update();
}

void NoteToolbarBase::paintGlyphAt(QPainter &p, const QString &icon,
                                   const QPointF &center, qreal size,
                                   const QColor &color) const {
  if (icon == QLatin1String("dots_h")) {
    drawDotsH(p, center, size, color);
    return;
  }
  // The shared 64px glyphs use ~3px strokes, which shrink to ~1px at 22px and
  // look washed out on the dark skins. Overdraw with sub-pixel offsets so the
  // outline reads as a crisp ~2px stroke.
  const QPointF offsets[] = {QPointF(0, 0), QPointF(0.55, 0), QPointF(-0.55, 0),
                             QPointF(0, 0.55), QPointF(0, -0.55)};
  for (const QPointF &o : offsets) {
    p.save();
    p.translate(center.x() - size / 2.0 + o.x(), center.y() - size / 2.0 + o.y());
    p.scale(size / 64.0, size / 64.0);
    blopDrawToolbarGlyph64(&p, icon, color);
    p.restore();
  }
}

void NoteToolbarBase::paintTileContent(QPainter &p, const NoteToolTile &t,
                                       const QColor &glyphColor) const {
  const QPointF c = t.iconCenter.isNull() ? t.rect.center() : t.iconCenter;
  if (t.action == NoteToolTile::Color) {
    const QColor col = currentPenColor();
    p.setPen(QPen(QColor(255, 255, 255, 200), 1.6));
    p.setBrush(col);
    const qreal r = t.iconSize * 0.42;
    p.drawEllipse(c, r, r);
    return;
  }
  if (t.action == NoteToolTile::Wheel) {
    const qreal r = t.iconSize / 2.0;
    drawWheel(p, QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
    return;
  }
  QColor col = glyphColor;
  if (t.tint.isValid() && !tileActive(t))
    col = t.tint;
  if ((t.action == NoteToolTile::Undo && !m_canUndo) ||
      (t.action == NoteToolTile::Redo && !m_canRedo))
    col.setAlpha(40); // overdrawn 5x in paintGlyphAt -> reads as ~35 %
  paintGlyphAt(p, t.icon, c, t.iconSize, col);
}

void NoteToolbarBase::paintTile(QPainter &p, const NoteToolTile &t, int index) {
  const bool active = tileActive(t);
  const bool hover = index == m_hover;
  const bool pressed = index == m_pressed;
  p.save();
  if (pressed && m_pressScale < 0.999) {
    const QPointF c = t.rect.center();
    p.translate(c);
    p.scale(m_pressScale, m_pressScale);
    p.translate(-c);
  }
  if (active || hover) {
    p.setPen(Qt::NoPen);
    p.setBrush(active ? activeFill() : QColor(255, 255, 255, 16));
    p.drawRoundedRect(t.rect, t.radius, t.radius);
  }
  paintTileContent(p, t, active ? activeGlyph() : kGlyph);
  if (!t.label.isEmpty()) {
    QFont f = font();
    f.setPixelSize(UiScale::dp(10));
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.setPen(active ? QColor(255, 255, 255) : kLabel);
    const QRectF lr(t.rect.left(), t.rect.bottom() - UiScale::dp(17),
                    t.rect.width(), UiScale::dp(14));
    p.drawText(lr, Qt::AlignHCenter | Qt::AlignTop, t.label);
  }
  p.restore();
}

void NoteToolbarBase::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  paintSkin(p);
  for (int i = 0; i < m_tiles.size(); ++i)
    paintTile(p, m_tiles[i], i);
}

void NoteToolbarBase::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return;
  const int idx = tileAt(e->position());
  if (idx < 0)
    return;
  m_pressed = idx;
  m_pressAnim->stop();
  m_pressAnim->setStartValue(m_pressScale);
  m_pressAnim->setEndValue(0.94);
  m_pressAnim->start();
  if (m_tiles[idx].action == NoteToolTile::Wheel) {
    const QPointF c = m_tiles[idx].iconCenter.isNull()
                          ? m_tiles[idx].rect.center()
                          : m_tiles[idx].iconCenter;
    const QPointF d = e->position() - c;
    qreal deg = qRadiansToDegrees(std::atan2(-d.y(), d.x()));
    if (deg < 0)
      deg += 360.0;
    // Gradient starts at 90° (up) and runs clockwise in screen space.
    qreal hue = std::fmod((90.0 - deg) + 360.0, 360.0) / 360.0;
    const qreal dist = std::hypot(d.x(), d.y()) / (m_tiles[idx].iconSize / 2.0);
    const qreal sat = qBound(0.15, dist, 1.0);
    noteApplyPenColor(QColor::fromHsvF(hue, sat, 0.95));
  }
  update();
}

void NoteToolbarBase::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return;
  const int idx = tileAt(e->position());
  const int pressed = m_pressed;
  m_pressed = -1;
  m_pressAnim->stop();
  m_pressAnim->setStartValue(m_pressScale);
  m_pressAnim->setEndValue(1.0);
  m_pressAnim->start();
  if (pressed >= 0 && idx == pressed && m_tiles[idx].action != NoteToolTile::Wheel)
    activate(idx);
  update();
}

void NoteToolbarBase::mouseMoveEvent(QMouseEvent *e) {
  const int idx = tileAt(e->position());
  if (idx != m_hover) {
    m_hover = idx;
    update();
  }
}

void NoteToolbarBase::mouseDoubleClickEvent(QMouseEvent *e) {
  const int idx = tileAt(e->position());
  if (idx >= 0 && m_tiles[idx].action == NoteToolTile::Tool && tileActive(m_tiles[idx]))
    emit toolOptionsRequested();
}

void NoteToolbarBase::leaveEvent(QEvent *) {
  m_hover = -1;
  update();
}

void NoteToolbarBase::resizeEvent(QResizeEvent *) { rebuildTiles(); }

// ─── Bar ────────────────────────────────────────────────────────────────────

namespace {
constexpr int kBarH = 64;
constexpr int kBarTileW = 56;
constexpr int kBarTileH = 52;
constexpr int kBarSmallW = 40;
constexpr int kBarPad = 10;
constexpr int kBarSep = 18;
} // namespace

NoteBarToolbar::NoteBarToolbar(QWidget *parent) : NoteToolbarBase(parent) {
  const int w = UiScale::dp(kBarPad) * 2 + UiScale::dp(kBarTileW) * 7 +
                UiScale::dp(2) * 6 + UiScale::dp(kBarSep) +
                UiScale::dp(kBarSmallW) * 3 + UiScale::dp(2) * 2;
  setFixedSize(w, UiScale::dp(kBarH));
  rebuildTiles();
}

QColor NoteBarToolbar::activeFill() const { return kActiveGray; }
QColor NoteBarToolbar::activeGlyph() const { return QColor(255, 255, 255); }

void NoteBarToolbar::rebuildTiles() {
  m_tiles.clear();
  struct Spec {
    const char *icon;
    const char *label;
    ToolMode mode;
  };
  const Spec specs[] = {
      {"hand", "Hand", ToolMode::Hand},
      {"pen", "Stift", ToolMode::Pen},
      {"eraser", "Radierer", ToolMode::Eraser},
      {"lasso", "Auswahl", ToolMode::Lasso},
      {"pi", "Formel", ToolMode::Formula},
      {"ruler", "Messen", ToolMode::Ruler},
      {"molecule", "Molekül", ToolMode::Molecule},
  };
  qreal x = UiScale::dp(kBarPad);
  const qreal y = (height() - UiScale::dp(kBarTileH)) / 2.0;
  for (const Spec &s : specs) {
    NoteToolTile t;
    t.icon = QLatin1String(s.icon);
    t.label = QString::fromUtf8(s.label);
    t.mode = s.mode;
    t.rect = QRectF(x, y, UiScale::dp(kBarTileW), UiScale::dp(kBarTileH));
    t.iconCenter = QPointF(t.rect.center().x(), t.rect.top() + UiScale::dp(17));
    t.iconSize = UiScale::dp(22);
    t.radius = UiScale::dp(9);
    m_tiles.append(t);
    x += UiScale::dp(kBarTileW) + UiScale::dp(2);
  }
  x += UiScale::dp(kBarSep) - UiScale::dp(2);
  auto small = [&](const char *icon, NoteToolTile::Action a) {
    NoteToolTile t;
    t.icon = QLatin1String(icon);
    t.action = a;
    t.rect = QRectF(x, (height() - UiScale::dp(kBarSmallW)) / 2.0,
                    UiScale::dp(kBarSmallW), UiScale::dp(kBarSmallW));
    t.iconSize = UiScale::dp(21);
    t.radius = UiScale::dp(9);
    m_tiles.append(t);
    x += UiScale::dp(kBarSmallW) + UiScale::dp(2);
  };
  small("undo", NoteToolTile::Undo);
  small("redo", NoteToolTile::Redo);
  small("dots_h", NoteToolTile::More);
}

void NoteBarToolbar::paintSkin(QPainter &p) {
  p.setPen(Qt::NoPen);
  p.setBrush(kBg);
  p.drawRoundedRect(rect(), UiScale::dp(12), UiScale::dp(12));
  // Thin separator before undo/redo.
  if (m_tiles.size() > 7) {
    const qreal sx = (m_tiles[6].rect.right() + m_tiles[7].rect.left()) / 2.0;
    p.setPen(QPen(QColor(255, 255, 255, 34), 1));
    p.drawLine(QPointF(sx, UiScale::dp(16)), QPointF(sx, height() - UiScale::dp(16)));
  }
}

// ─── Floating pill ──────────────────────────────────────────────────────────

namespace {
constexpr int kPillH = 46;
constexpr int kPillTile = 36;
constexpr int kPillPad = 6;
constexpr int kPillGap = 2;
constexpr int kPillSep = 12;
} // namespace

NoteFloatToolbar::NoteFloatToolbar(QWidget *parent) : NoteToolbarBase(parent) {
  const int tiles = 12;
  const int w = UiScale::dp(kPillPad) * 2 + UiScale::dp(kPillTile) * tiles +
                UiScale::dp(kPillGap) * (tiles - 1) + UiScale::dp(kPillSep);
  setFixedSize(w, UiScale::dp(kPillH));
  rebuildTiles();
}

QColor NoteFloatToolbar::activeFill() const { return QColor(91, 157, 255, 52); }
QColor NoteFloatToolbar::activeGlyph() const { return kAccent; }

void NoteFloatToolbar::rebuildTiles() {
  m_tiles.clear();
  qreal x = UiScale::dp(kPillPad);
  const qreal y = (height() - UiScale::dp(kPillTile)) / 2.0;
  auto add = [&](const char *icon, ToolMode mode, int shape = -1,
                 NoteToolTile::Action a = NoteToolTile::Tool,
                 const QColor &tint = QColor()) {
    NoteToolTile t;
    t.icon = QLatin1String(icon);
    t.mode = mode;
    t.shapeKind = shape;
    t.action = a;
    t.tint = tint;
    t.rect = QRectF(x, y, UiScale::dp(kPillTile), UiScale::dp(kPillTile));
    t.iconSize = UiScale::dp(21);
    t.radius = UiScale::dp(9);
    m_tiles.append(t);
    x += UiScale::dp(kPillTile) + UiScale::dp(kPillGap);
  };
  add("pen", ToolMode::Pen);
  add("highlighter", ToolMode::Highlighter, -1, NoteToolTile::Tool, kHighlighter);
  add("eraser", ToolMode::Eraser);
  add("text", ToolMode::Text);
  add("image", ToolMode::Image);
  add("line", ToolMode::Shape, static_cast<int>(ShapeToolKind::Line));
  add("circle", ToolMode::Shape, static_cast<int>(ShapeToolKind::Circle));
  add("pi", ToolMode::Formula);
  add("molecule", ToolMode::Molecule);
  add("ruler", ToolMode::Ruler);
  x += UiScale::dp(kPillSep);
  add("undo", ToolMode::Pen, -1, NoteToolTile::Undo);
  add("dots_h", ToolMode::Pen, -1, NoteToolTile::More);
}

void NoteFloatToolbar::paintSkin(QPainter &p) {
  p.setPen(Qt::NoPen);
  p.setBrush(kBg);
  p.drawRoundedRect(rect(), UiScale::dp(14), UiScale::dp(14));
  if (m_tiles.size() > 10) {
    const qreal sx = (m_tiles[9].rect.right() + m_tiles[10].rect.left()) / 2.0;
    p.setPen(QPen(QColor(255, 255, 255, 34), 1));
    p.drawLine(QPointF(sx, UiScale::dp(12)), QPointF(sx, height() - UiScale::dp(12)));
  }
}

// ─── Radiant bloom ──────────────────────────────────────────────────────────

namespace {
constexpr int kRadiantSize = 262;
constexpr int kRadiantInset = 54; // center offset from the bottom-right corner
constexpr qreal kDiscR = 58;
constexpr qreal kInnerR0 = 76;
constexpr qreal kInnerR1 = 122;
constexpr qreal kOuterR0 = 131;
constexpr qreal kOuterR1 = 186;
constexpr qreal kArcStart = 91.5;
constexpr qreal kArcEnd = 178.5;
} // namespace

NoteRadiantToolbar::NoteRadiantToolbar(QWidget *parent) : NoteToolbarBase(parent) {
  setFixedSize(UiScale::dp(kRadiantSize), UiScale::dp(kRadiantSize));
  rebuildTiles();
}

void NoteRadiantToolbar::rebuildTiles() {
  m_tiles.clear();
  const qreal s = UiScale::dp(1);
  m_center = QPointF(width() - kRadiantInset * s, height() - kRadiantInset * s);

  // Center disc: active tool, opens tool options.
  {
    NoteToolTile t;
    t.action = NoteToolTile::Center;
    t.icon = QStringLiteral("pen");
    t.path.addEllipse(m_center, kDiscR * s, kDiscR * s);
    t.iconCenter = m_center;
    t.iconSize = UiScale::dp(30);
    m_tiles.append(t);
  }

  struct Spec {
    const char *icon;
    ToolMode mode;
    int shape;
    NoteToolTile::Action action;
    QColor tint;
  };
  const Spec inner[] = {
      {"pi", ToolMode::Formula, -1, NoteToolTile::Tool, QColor()},
      {"text", ToolMode::Text, -1, NoteToolTile::Tool, QColor()},
      {"pen", ToolMode::Pen, -1, NoteToolTile::Tool, QColor()},
      {"lasso", ToolMode::Lasso, -1, NoteToolTile::Tool, QColor()},
      {"hand", ToolMode::Hand, -1, NoteToolTile::Tool, QColor()},
  };
  const Spec outer[] = {
      {"lasso_loop", ToolMode::Lasso, -1, NoteToolTile::Tool, QColor()},
      {"highlighter", ToolMode::Highlighter, -1, NoteToolTile::Tool, kHighlighter},
      {"pencil", ToolMode::Pencil, -1, NoteToolTile::Tool, QColor()},
      {"eraser", ToolMode::Eraser, -1, NoteToolTile::Tool, QColor()},
      {"line", ToolMode::Shape, static_cast<int>(ShapeToolKind::Line),
       NoteToolTile::Tool, QColor()},
      {"", ToolMode::Pen, -1, NoteToolTile::Color, QColor()},
      {"dots_h", ToolMode::Pen, -1, NoteToolTile::More, QColor()},
  };

  auto ring = [&](const Spec *specs, int n, qreal r0, qreal r1, qreal gapDeg) {
    const qreal span = (kArcEnd - kArcStart - gapDeg * (n - 1)) / n;
    for (int i = 0; i < n; ++i) {
      const qreal a0 = kArcStart + i * (span + gapDeg);
      const qreal a1 = a0 + span;
      NoteToolTile t;
      t.icon = QLatin1String(specs[i].icon);
      t.mode = specs[i].mode;
      t.shapeKind = specs[i].shape;
      t.action = specs[i].action;
      t.tint = specs[i].tint;
      t.path = ringSegment(m_center, r0 * s, r1 * s, a0, a1);
      t.iconCenter = polar(m_center, (r0 + r1) / 2.0 * s, (a0 + a1) / 2.0);
      t.iconSize = UiScale::dp(r1 - r0 > 50 ? 22 : 20);
      m_tiles.append(t);
    }
  };
  // Inner ring: wider gaps because the radius is smaller.
  ring(inner, 5, kInnerR0, kInnerR1, 2.2);
  ring(outer, 7, kOuterR0, kOuterR1, 1.4);
}

void NoteRadiantToolbar::paintSkin(QPainter &p) {
  const qreal s = UiScale::dp(1);
  // Dark quarter plate behind the rings.
  QPainterPath plate;
  const qreal R = (kOuterR1 + 10) * s;
  plate.moveTo(m_center);
  plate.arcTo(QRectF(m_center.x() - R, m_center.y() - R, 2 * R, 2 * R), 88, 94);
  plate.closeSubpath();
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(24, 26, 32, 235));
  p.drawPath(plate);
}

void NoteRadiantToolbar::paintTile(QPainter &p, const NoteToolTile &t, int index) {
  const bool active = tileActive(t);
  const bool hover = index == m_hover;
  const bool pressed = index == m_pressed;
  p.save();
  if (pressed && m_pressScale < 0.999) {
    p.translate(t.iconCenter);
    p.scale(m_pressScale, m_pressScale);
    p.translate(-t.iconCenter);
  }
  p.setPen(Qt::NoPen);
  if (t.action == NoteToolTile::Center) {
    QRadialGradient g(m_center, kDiscR * UiScale::dp(1));
    g.setColorAt(0.0, QColor(52, 57, 68));
    g.setColorAt(1.0, QColor(34, 37, 45));
    p.setBrush(hover ? QBrush(QColor(58, 63, 75)) : QBrush(g));
    p.drawPath(t.path);
    p.setPen(QPen(QColor(255, 255, 255, 28), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(t.path);
    // Show the active tool in the disc.
    QString icon = QStringLiteral("pen");
    auto &mgr = ToolManager::instance();
    switch (mgr.activeToolMode()) {
    case ToolMode::Hand: icon = QStringLiteral("hand"); break;
    case ToolMode::Pencil: icon = QStringLiteral("pencil"); break;
    case ToolMode::Highlighter: icon = QStringLiteral("highlighter"); break;
    case ToolMode::Eraser: icon = QStringLiteral("eraser"); break;
    case ToolMode::Lasso: icon = QStringLiteral("lasso"); break;
    case ToolMode::Text: icon = QStringLiteral("text"); break;
    case ToolMode::Image: icon = QStringLiteral("image"); break;
    case ToolMode::Formula: icon = QStringLiteral("pi"); break;
    case ToolMode::Ruler: icon = QStringLiteral("ruler"); break;
    case ToolMode::Molecule: icon = QStringLiteral("molecule"); break;
    case ToolMode::Shape: icon = QStringLiteral("line"); break;
    default: break;
    }
    paintGlyphAt(p, icon, m_center, t.iconSize, kGlyph);
    p.restore();
    return;
  }
  QColor fill = kBgSoft;
  if (active)
    fill = kAccent;
  else if (hover)
    fill = QColor(60, 65, 78);
  p.setBrush(fill);
  p.drawPath(t.path);
  paintTileContent(p, t, active ? QColor(255, 255, 255) : kGlyph);
  p.restore();
}

// ─── Science rail ───────────────────────────────────────────────────────────

namespace {
constexpr int kRailW = 100;
constexpr int kRailPad = 10;
constexpr int kRailTile = 40;
constexpr int kRailWheel = 72;
constexpr int kRailRows = 8;
} // namespace

NoteScienceToolbar::NoteScienceToolbar(QWidget *parent) : NoteToolbarBase(parent) {
  const int h = UiScale::dp(kRailPad) * 2 + UiScale::dp(kRailTile) * kRailRows +
                UiScale::dp(10) + UiScale::dp(kRailWheel) + UiScale::dp(4);
  setFixedSize(UiScale::dp(kRailW), h);
  rebuildTiles();
}

QColor NoteScienceToolbar::activeFill() const { return QColor(255, 255, 255); }
QColor NoteScienceToolbar::activeGlyph() const { return kBg; }

void NoteScienceToolbar::rebuildTiles() {
  m_tiles.clear();
  struct Spec {
    const char *icon;
    ToolMode mode;
    int shape;
    NoteToolTile::Action action;
    QColor tint;
  };
  const Spec grid[kRailRows][2] = {
      {{"pen", ToolMode::Pen, -1, NoteToolTile::Tool, QColor()},
       {"", ToolMode::Pen, -1, NoteToolTile::Color, QColor()}},
      {{"highlighter", ToolMode::Highlighter, -1, NoteToolTile::Tool, kHighlighter},
       {"settings", ToolMode::Pen, -1, NoteToolTile::Options, QColor()}},
      {{"pencil", ToolMode::Pencil, -1, NoteToolTile::Tool, QColor()},
       {"eraser", ToolMode::Eraser, -1, NoteToolTile::Tool, QColor()}},
      {{"lasso", ToolMode::Lasso, -1, NoteToolTile::Tool, QColor()},
       {"text", ToolMode::Text, -1, NoteToolTile::Tool, QColor()}},
      {{"pi", ToolMode::Formula, -1, NoteToolTile::Tool, QColor()},
       {"ruler", ToolMode::Ruler, -1, NoteToolTile::Tool, QColor()}},
      {{"molecule", ToolMode::Molecule, -1, NoteToolTile::Tool, QColor()},
       {"image", ToolMode::Image, -1, NoteToolTile::Tool, QColor()}},
      {{"line", ToolMode::Shape, static_cast<int>(ShapeToolKind::Line),
        NoteToolTile::Tool, QColor()},
       {"circle", ToolMode::Shape, static_cast<int>(ShapeToolKind::Circle),
        NoteToolTile::Tool, QColor()}},
      {{"hand", ToolMode::Hand, -1, NoteToolTile::Tool, QColor()},
       {"dots_h", ToolMode::Pen, -1, NoteToolTile::More, QColor()}},
  };
  const qreal tile = UiScale::dp(kRailTile);
  const qreal x0 = UiScale::dp(kRailPad);
  const qreal y0 = UiScale::dp(kRailPad);
  for (int r = 0; r < kRailRows; ++r) {
    for (int c = 0; c < 2; ++c) {
      const Spec &sp = grid[r][c];
      NoteToolTile t;
      t.icon = QLatin1String(sp.icon);
      t.mode = sp.mode;
      t.shapeKind = sp.shape;
      t.action = sp.action;
      t.tint = sp.tint;
      t.rect = QRectF(x0 + c * tile, y0 + r * tile, tile, tile).adjusted(1, 1, -1, -1);
      t.iconSize = UiScale::dp(21);
      t.radius = UiScale::dp(9);
      m_tiles.append(t);
    }
  }
  NoteToolTile wheel;
  wheel.action = NoteToolTile::Wheel;
  const qreal wr = UiScale::dp(kRailWheel);
  wheel.rect = QRectF((width() - wr) / 2.0, y0 + kRailRows * tile + UiScale::dp(10),
                      wr, wr);
  wheel.iconCenter = wheel.rect.center();
  wheel.iconSize = wr;
  m_tiles.append(wheel);
}

void NoteScienceToolbar::paintSkin(QPainter &p) {
  p.setPen(Qt::NoPen);
  p.setBrush(kBg);
  p.drawRoundedRect(rect(), UiScale::dp(16), UiScale::dp(16));
}

void NoteScienceToolbar::paintTile(QPainter &p, const NoteToolTile &t, int index) {
  if (t.action == NoteToolTile::Wheel) {
    p.save();
    if (index == m_pressed && m_pressScale < 0.999) {
      p.translate(t.rect.center());
      p.scale(m_pressScale, m_pressScale);
      p.translate(-t.rect.center());
    }
    drawWheel(p, t.rect);
    p.restore();
    return;
  }
  NoteToolbarBase::paintTile(p, t, index);
}

// ─── Pen width chip ─────────────────────────────────────────────────────────

NotePenWidthChip::NotePenWidthChip(QWidget *parent) : QWidget(parent) {
  setFixedSize(UiScale::dp(150), UiScale::dp(36));
  auto *lay = new QHBoxLayout(this);
  lay->setContentsMargins(UiScale::dp(34), 0, UiScale::dp(12), 0);
  lay->setSpacing(0);
  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setRange(1, 24);
  m_slider->setCursor(Qt::PointingHandCursor);
  m_slider->setStyleSheet(QStringLiteral(
      "QSlider::groove:horizontal { height: 3px; background: #D5D9E2; border-radius: 1px; }"
      "QSlider::sub-page:horizontal { background: %1; border-radius: 1px; }"
      "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -5px 0;"
      "  background: %1; border-radius: 6px; }")
                              .arg(kAccent.name(QColor::HexRgb)));
  lay->addWidget(m_slider, 1);
  connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
    if (!m_syncing)
      noteApplyPenWidth(v);
    update();
  });
  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this](AbstractTool *) { syncFromToolManager(); });
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &) { syncFromToolManager(); });
  syncFromToolManager();
}

void NotePenWidthChip::syncFromToolManager() {
  auto &mgr = ToolManager::instance();
  ToolMode mode = mgr.activeToolMode();
  if (mode != ToolMode::Pen && mode != ToolMode::Pencil &&
      mode != ToolMode::Highlighter)
    mode = ToolMode::Pen;
  m_syncing = true;
  m_slider->setValue(qBound(1, mgr.configFor(mode).penWidth, 24));
  m_syncing = false;
  update();
}

void NotePenWidthChip::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(QColor(0, 0, 0, 26), 1));
  p.setBrush(QColor(255, 255, 255));
  p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), UiScale::dp(10),
                    UiScale::dp(10));
  p.save();
  const qreal size = UiScale::dp(18);
  p.translate(UiScale::dp(10), (height() - size) / 2.0);
  p.scale(size / 64.0, size / 64.0);
  blopDrawToolbarGlyph64(&p, QStringLiteral("pen"), QColor(31, 34, 41));
  p.restore();
}

// ─── Host ───────────────────────────────────────────────────────────────────

NoteToolbarHost::NoteToolbarHost(QWidget *surface)
    : QObject(surface), m_surface(surface) {
  m_bar = new NoteBarToolbar(surface);
  m_float = new NoteFloatToolbar(surface);
  m_radiant = new NoteRadiantToolbar(surface);
  m_science = new NoteScienceToolbar(surface);
  m_chip = new NotePenWidthChip(surface);
  for (NoteToolbarBase *tb :
       {static_cast<NoteToolbarBase *>(m_bar), static_cast<NoteToolbarBase *>(m_float),
        static_cast<NoteToolbarBase *>(m_radiant),
        static_cast<NoteToolbarBase *>(m_science)}) {
    tb->hide();
    connect(tb, &NoteToolbarBase::undoRequested, this, &NoteToolbarHost::undoRequested);
    connect(tb, &NoteToolbarBase::redoRequested, this, &NoteToolbarHost::redoRequested);
    connect(tb, &NoteToolbarBase::toolOptionsRequested, this,
            &NoteToolbarHost::toolOptionsRequested);
    connect(tb, &NoteToolbarBase::moreRequested, this,
            [this](const QPoint &g) { showStyleMenu(g); });
  }
  m_chip->hide();
}

NoteToolbarStyle NoteToolbarHost::style() const { return noteToolbarStyle(); }

QWidget *NoteToolbarHost::activeWidget() const {
  switch (noteToolbarStyle()) {
  case NoteToolbarStyle::Floating:
    return m_float;
  case NoteToolbarStyle::Radiant:
    return m_radiant;
  case NoteToolbarStyle::Science:
    return m_science;
  case NoteToolbarStyle::Bar:
    break;
  }
  return m_bar;
}

void NoteToolbarHost::setVisible(bool on) {
  m_visible = on;
  applyStyle();
}

void NoteToolbarHost::applyStyle() {
  QWidget *active = activeWidget();
  for (QWidget *w : {static_cast<QWidget *>(m_bar), static_cast<QWidget *>(m_float),
                     static_cast<QWidget *>(m_radiant),
                     static_cast<QWidget *>(m_science)})
    w->setVisible(m_visible && w == active);
  m_chip->setVisible(m_visible && active == m_float);
  relayout(m_topInset);
}

void NoteToolbarHost::relayout(int topInset) {
  m_topInset = topInset;
  if (!m_surface)
    return;
  const int W = m_surface->width();
  const int H = m_surface->height();
  const int m = UiScale::dp(12);
  m_bar->move((W - m_bar->width()) / 2, topInset + m);
  m_float->move((W - m_float->width()) / 2, topInset + m);
  m_radiant->move(W - m_radiant->width(), H - m_radiant->height());
  m_science->move(W - m_science->width() - m, topInset + m);
  m_chip->move(W - m_chip->width() - m, H - m_chip->height() - m);
  raiseAll();
}

void NoteToolbarHost::raiseAll() {
  if (QWidget *w = activeWidget())
    w->raise();
  if (m_chip->isVisible())
    m_chip->raise();
}

void NoteToolbarHost::setUndoRedoAvailable(bool canUndo, bool canRedo) {
  m_bar->setUndoRedoAvailable(canUndo, canRedo);
  m_float->setUndoRedoAvailable(canUndo, canRedo);
  m_radiant->setUndoRedoAvailable(canUndo, canRedo);
  m_science->setUndoRedoAvailable(canUndo, canRedo);
}

QRect NoteToolbarHost::activeRect() const {
  QWidget *w = activeWidget();
  return w && w->isVisible() ? w->geometry() : QRect();
}

void NoteToolbarHost::showStyleMenu(const QPoint &globalPos) {
  QWidget *anchor = activeWidget();
  if (!anchor)
    return;
  const NoteToolbarStyle cur = noteToolbarStyle();
  QList<BlopInWindowMenu::Item> items;
  auto add = [&](NoteToolbarStyle st) {
    BlopInWindowMenu::Item it;
    it.label = (st == cur ? QStringLiteral("●  ") : QStringLiteral("○  ")) +
               noteToolbarStyleLabel(st);
    it.handler = [this, st]() {
      setNoteToolbarStyle(st);
      applyStyle();
      emit styleChanged();
    };
    items.append(it);
  };
  add(NoteToolbarStyle::Bar);
  add(NoteToolbarStyle::Floating);
  add(NoteToolbarStyle::Radiant);
  add(NoteToolbarStyle::Science);
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.append(sep);
  BlopInWindowMenu::Item opts;
  opts.label = QStringLiteral("Werkzeugoptionen");
  opts.handler = [this]() { emit toolOptionsRequested(); };
  items.append(opts);
  BlopInWindowMenu::show(anchor, globalPos, items);
}
