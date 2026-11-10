#include "notetoolbars.h"

#include "ToolSettings.h"
#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "moderntoolbar.h"
#include "notechrome.h"
#include "shapescatalogpopover.h"
#include "tools/ToolManager.h"
#include "tools/ToolUIBridge.h"
#include "uiscale.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QConicalGradient>
#include <cmath>
#include <QCursor>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QRadialGradient>
#include <QSettings>
#include <QSlider>
#include <QTabWidget>
#include <QVariantAnimation>
#include <QVBoxLayout>
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

bool noteMintToolsVisible() {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  return s.value(QStringLiteral("ui/mintToolsVisible"), true).toBool();
}

void setNoteMintToolsVisible(bool on) {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(QStringLiteral("ui/mintToolsVisible"), on);
}

void noteActivateTool(ToolMode mode, int shapeKind) {
  auto &mgr = ToolManager::instance();
  if (shapeKind >= 0) {
    ToolConfig cfg = mgr.configFor(ToolMode::Shape);
    cfg.shapeToolKind =
        static_cast<ShapeToolKind>(qBound(0, shapeKind, 28));
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
QColor NoteToolbarBase::idleGlyph() const { return kGlyph; }
QColor NoteToolbarBase::hoverFill() const { return QColor(255, 255, 255, 16); }

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
    if (!t.hittable || t.action == NoteToolTile::Gap)
      continue;
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
    if (tileActive(t) &&
        (t.mode == ToolMode::Pen || t.mode == ToolMode::Pencil ||
         t.mode == ToolMode::Highlighter)) {
      emit penStudioRequested();
      break;
    }
    // Float dock handles second-click eraser/shapes/measure popovers itself.
    if (inherits("NoteFloatToolbar") && tileActive(t) &&
        (t.mode == ToolMode::Eraser || t.mode == ToolMode::Shape ||
         t.mode == ToolMode::Ruler)) {
      break;
    }
    noteActivateTool(t.mode, t.shapeKind);
    break;
  case NoteToolTile::Width:
    emit penStudioRequested();
    break;
  case NoteToolTile::Swatch:
    noteApplyPenColor(t.tint);
    break;
  case NoteToolTile::Gap:
    break;
  case NoteToolTile::Undo:
    emit undoRequested();
    break;
  case NoteToolTile::Redo:
    emit redoRequested();
    break;
  case NoteToolTile::More: {
    // Radiant ⋯ opens Pen Studio; Bar/Float/Science keep the style picker.
    if (inherits("NoteRadiantToolbar")) {
      emit penStudioRequested();
      break;
    }
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
  if (t.action == NoteToolTile::Swatch) {
    const QColor col = t.tint.isValid() ? t.tint : currentPenColor();
    p.setPen(QPen(QColor(255, 255, 255, 210), 1.5));
    p.setBrush(col);
    const qreal r = t.iconSize * 0.38;
    p.drawEllipse(c, r, r);
    return;
  }
  if (t.action == NoteToolTile::Width) {
    QFont f = p.font();
    f.setPixelSize(qMax(9, int(t.iconSize * 0.42)));
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(glyphColor);
    auto &mgr = ToolManager::instance();
    ToolMode mode = mgr.activeToolMode();
    if (mode != ToolMode::Pen && mode != ToolMode::Pencil &&
        mode != ToolMode::Highlighter)
      mode = ToolMode::Pen;
    const int w = mgr.configFor(mode).penWidth;
    const QString label =
        w > 0 ? QStringLiteral("%1 pt").arg(w) : QStringLiteral("3 pt");
    const QRectF lr(c.x() - t.iconSize, c.y() - t.iconSize * 0.4,
                    t.iconSize * 2, t.iconSize * 0.8);
    p.drawText(lr, Qt::AlignCenter, label);
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
    p.setBrush(active ? activeFill() : hoverFill());
    p.drawRoundedRect(t.rect, t.radius, t.radius);
  }
  paintTileContent(p, t, active ? activeGlyph() : idleGlyph());
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
      {"ruler", "Messen", ToolMode::Ruler},
      {"stickynote", "Notiz", ToolMode::StickyNote},
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
  if (m_tiles.size() > 6) {
    const qreal sx = (m_tiles[5].rect.right() + m_tiles[6].rect.left()) / 2.0;
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
  setAttribute(Qt::WA_TranslucentBackground, true);
  rebuildTiles();
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &) {
            rebuildTiles();
            update();
          });
}

QColor NoteFloatToolbar::activeFill() const { return QColor(91, 157, 255, 52); }
QColor NoteFloatToolbar::activeGlyph() const { return kAccent; }
QColor NoteFloatToolbar::idleGlyph() const {
  return BlopTheme::instance().isDark() ? kGlyph : BlopStyle::paperInk();
}
QColor NoteFloatToolbar::hoverFill() const {
  return BlopTheme::instance().isDark() ? QColor(255, 255, 255, 16)
                                        : BlopStyle::paperHover();
}

QPoint NoteFloatToolbar::studioAnchorGlobal() const {
  auto mapTile = [this](const NoteToolTile &t) {
    return mapToGlobal(t.rect.bottomLeft().toPoint());
  };
  for (const NoteToolTile &t : m_tiles) {
    if (t.action == NoteToolTile::Width)
      return mapTile(t);
  }
  for (const NoteToolTile &t : m_tiles) {
    if (t.action == NoteToolTile::Tool && t.icon == QLatin1String("pen"))
      return mapTile(t);
  }
  return mapToGlobal(QPoint(width() / 2, height()));
}

namespace {
const QColor kProInk[5] = {
    QColor(0x1E, 0x29, 0x3B), QColor(0xF8, 0xFA, 0xFC), QColor(0x25, 0x63, 0xEB),
    QColor(0xE1, 0x1D, 0x48), QColor(0x10, 0xB9, 0x81)};
constexpr qreal kWidthPresets[3] = {1.5, 3.0, 6.0};
} // namespace

void NoteFloatToolbar::rebuildTiles() {
  m_tiles.clear();
  const qreal tool = UiScale::dp(36);
  const qreal swatch = UiScale::dp(22);
  const qreal widthBtn = UiScale::dp(118);
  const qreal shadow = UiScale::dp(6);
  const int surfaceW = parentWidget() ? parentWidget()->width() : 1600;
  const int hudReserve = UiScale::dp(280); // PageHud + margins
  const int maxDockW = qMax(UiScale::dp(320), surfaceW - hudReserve);

  // Shrink horizontal spacing when the dock would clip the surface.
  qreal gap = UiScale::dp(3);
  qreal swatchGap = UiScale::dp(8);
  qreal sep = UiScale::dp(12);
  qreal padX = UiScale::dp(6);
  const qreal padY = UiScale::dp(6);
  const bool mintOn = noteMintToolsVisible();

  auto estimate = [&](bool withSwatches, qreal g, qreal sg, qreal s,
                      qreal px) {
    qreal xx = px;
    auto add = [&](qreal ww) { xx += ww + g; };
    auto addSep = [&] { xx += s - g; };
    add(tool);
    add(tool);
    addSep();
    for (int i = 0; i < (mintOn ? 8 : 7); ++i)
      add(tool);
    addSep();
    if (withSwatches) {
      for (int i = 0; i < 5; ++i)
        add(swatch + (sg - g));
    }
    add(widthBtn);
    addSep();
    add(tool);
    add(tool);
    add(tool);
    return int(std::ceil(xx + px - g + shadow));
  };

  bool showSwatches = true;
  for (int pass = 0; pass < 4; ++pass) {
    const int full = estimate(true, gap, swatchGap, sep, padX);
    if (full <= maxDockW)
      break;
    if (pass == 0) {
      showSwatches = estimate(false, gap, swatchGap, sep, padX) <= maxDockW;
      if (showSwatches)
        break;
      showSwatches = false;
    }
    gap = qMax(1.0, gap - 1.0);
    swatchGap = qMax(2.0, swatchGap - 2.0);
    sep = qMax(4.0, sep - 2.0);
    padX = qMax(4.0, padX - 2.0);
  }
  if (estimate(showSwatches, gap, swatchGap, sep, padX) > maxDockW)
    showSwatches = false;

  qreal x = padX;
  const qreal y = shadow + padY;

  auto push = [&](NoteToolTile t, qreal w, qreal h) {
    t.rect = QRectF(x, y + (tool - h) / 2.0, w, h);
    t.iconSize = qMin(w, h) * 0.55;
    t.radius = UiScale::dp(9);
    m_tiles.append(t);
    x += w + gap;
  };
  auto toolTile = [&](const char *icon, ToolMode mode, int shape = -1,
                      const QColor &tint = QColor()) {
    NoteToolTile t;
    t.icon = QLatin1String(icon);
    t.mode = mode;
    t.shapeKind = shape;
    t.tint = tint;
    push(t, tool, tool);
  };
  auto separator = [&]() {
    x += sep - gap;
    NoteToolTile t;
    t.action = NoteToolTile::Gap;
    t.hittable = false;
    t.rect = QRectF(x - sep / 2.0, y + (tool - UiScale::dp(24)) / 2.0, 1,
                    UiScale::dp(24));
    m_tiles.append(t);
  };

  toolTile("hand", ToolMode::Hand);
  toolTile("lasso", ToolMode::Lasso);
  separator();
  toolTile("pen", ToolMode::Pen);
  toolTile("highlighter", ToolMode::Highlighter, -1, kHighlighter);
  toolTile("eraser", ToolMode::Eraser);
  toolTile("shape", ToolMode::Shape, static_cast<int>(ShapeToolKind::Line));
  if (mintOn)
    toolTile("pi", ToolMode::Formula);
  toolTile("ruler", ToolMode::Ruler);
  toolTile("text", ToolMode::Text);
  toolTile("laser", ToolMode::Laser);
  separator();

  const ToolConfig cfg = ToolManager::instance().config();
  if (showSwatches) {
    for (int i = 0; i < 5; ++i) {
      NoteToolTile t;
      t.action = NoteToolTile::Swatch;
      QColor c = cfg.quickColors[static_cast<size_t>(i)];
      if (!c.isValid() ||
          (c == QColor(Qt::red) || c == QColor(Qt::blue) ||
           c == QColor(0, 200, 0) || c == QColor(Qt::green)))
        c = kProInk[i];
      t.tint = c;
      push(t, swatch, swatch);
      x += (swatchGap - gap);
    }
  }
  {
    NoteToolTile t;
    t.action = NoteToolTile::Width;
    t.icon = QStringLiteral("pen");
    push(t, widthBtn, UiScale::dp(32));
  }
  separator();
  toolTile("undo", ToolMode::Pen);
  m_tiles.last().action = NoteToolTile::Undo;
  toolTile("redo", ToolMode::Pen);
  m_tiles.last().action = NoteToolTile::Redo;
  toolTile("dots_h", ToolMode::Pen);
  m_tiles.last().action = NoteToolTile::More;

  const int w = int(std::ceil(x + padX - gap + shadow));
  const int h = int(shadow + padY + tool + padY + shadow);
  if (width() != w || height() != h)
    setFixedSize(w, h);
  if (QWidget *p = parentWidget()) {
    const int maxX = qMax(0, p->width() - w);
    const int nx = qBound(0, (p->width() - w) / 2, maxX);
    if (QWidget::x() != nx)
      move(nx, QWidget::y());
  }
}

void NoteFloatToolbar::paintSkin(QPainter &p) {
  const QRectF plate = QRectF(rect()).adjusted(1, 1, -7, -7);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 100));
  const qreal rad = UiScale::dp(14);
  p.drawRoundedRect(plate.adjusted(0, 4, 0, 2), rad, rad);
  p.setBrush(QColor(20, 23, 30, 242));
  p.setPen(QPen(QColor(255, 255, 255, 31), 1));
  p.drawRoundedRect(plate, rad, rad);
  p.setPen(QPen(QColor(255, 255, 255, 26), 1));
  for (const NoteToolTile &t : m_tiles) {
    if (t.action == NoteToolTile::Gap)
      p.drawLine(QPointF(t.rect.center().x(), plate.top() + 12),
                 QPointF(t.rect.center().x(), plate.bottom() - 12));
  }
}

int NoteFloatToolbar::widthPresetAt(const NoteToolTile &t,
                                    const QPointF &local) const {
  if (t.action != NoteToolTile::Width)
    return -1;
  const qreal zone = (t.rect.width() - UiScale::dp(44)) / 3.0;
  if (zone <= 1.0)
    return -1;
  const qreal x = local.x() - t.rect.left();
  if (x < 0 || x > t.rect.width() - UiScale::dp(44))
    return -1;
  return qBound(0, int(x / zone), 2);
}

void NoteFloatToolbar::paintTile(QPainter &p, const NoteToolTile &t, int index) {
  if (t.action == NoteToolTile::Gap)
    return;
  const bool hover = index == m_hover;
  if (t.action == NoteToolTile::Swatch) {
    const QColor cur = currentPenColor();
    const bool on =
        t.tint.isValid() && cur.isValid() && t.tint.rgb() == cur.rgb();
    QRectF r = t.rect;
    if (on)
      r = QRectF(r.center().x() - 12, r.center().y() - 12, 24, 24);
    p.setBrush(t.tint);
    if (on) {
      // Offset ring: dark gap then white outer.
      p.setPen(QPen(QColor(20, 23, 30), 2.0));
      p.drawEllipse(r.adjusted(-2, -2, 2, 2));
      p.setPen(QPen(QColor(255, 255, 255), 2.0));
      p.drawEllipse(r.adjusted(-4, -4, 4, 4));
    } else if (t.tint.lightness() < 40) {
      p.setPen(QPen(QColor(255, 255, 255, 76), 1.0));
    } else {
      p.setPen(Qt::NoPen);
    }
    p.drawEllipse(r);
    if (hover && !on) {
      p.setPen(QPen(kAccent, 1.5));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(r.adjusted(-1, -1, 1, 1));
    }
    return;
  }
  if (t.action == NoteToolTile::Width) {
    const int w = qBound(1, ToolManager::instance().config().penWidth, 24);
    p.setPen(QPen(QColor(255, 255, 255, 26), 1));
    p.setBrush(QColor(255, 255, 255, 15));
    p.drawRoundedRect(t.rect, UiScale::dp(8), UiScale::dp(8));
    const QColor dot(0xE2, 0xE8, 0xF0);
    const qreal zone = (t.rect.width() - UiScale::dp(44)) / 3.0;
    for (int i = 0; i < 3; ++i) {
      const qreal d = 4.0 + i * 3.0;
      const bool on = qAbs(w - kWidthPresets[i]) < 0.6;
      const QPointF c(t.rect.left() + zone * (i + 0.5), t.rect.center().y());
      p.setPen(Qt::NoPen);
      p.setBrush(on ? QColor(255, 255, 255) : dot);
      p.drawEllipse(c, d / 2.0, d / 2.0);
    }
    p.setPen(QColor(0xE2, 0xE8, 0xF0));
    QFont f = p.font();
    f.setPixelSize(11);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.drawText(t.rect.adjusted(t.rect.width() - UiScale::dp(44), 0, -4, 0),
               Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("%1 pt ▾").arg(w));
    return;
  }
  NoteToolbarBase::paintTile(p, t, index);
  if (t.action == NoteToolTile::Tool && t.icon == QLatin1String("pen")) {
    const ToolConfig cfg = ToolManager::instance().configFor(ToolMode::Pen);
    const QColor ink = cfg.penColor.isValid() ? cfg.penColor : QColor(Qt::black);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    const QRectF bar(t.rect.center().x() - 8, t.rect.bottom() - 5, 16, 3);
    p.drawRoundedRect(bar, 1.5, 1.5);
  }
}

void NoteFloatToolbar::mousePressEvent(QMouseEvent *event) {
  const int idx = tileAt(event->position());
  if (idx >= 0 && idx < m_tiles.size()) {
    const NoteToolTile &t = m_tiles[idx];
    if (t.action == NoteToolTile::Width) {
      const int preset = widthPresetAt(t, event->position());
      if (preset >= 0) {
        noteApplyPenWidth(qRound(kWidthPresets[preset]));
        update();
        event->accept();
        return;
      }
      emit penStudioRequested();
      event->accept();
      return;
    }
    if (t.action == NoteToolTile::Tool && tileActive(t)) {
      if (t.mode == ToolMode::Eraser) {
        showEraserPopover(mapToGlobal(t.rect.bottomLeft().toPoint()));
        event->accept();
        return;
      }
      if (t.mode == ToolMode::Shape) {
        showShapesPopover(mapToGlobal(t.rect.bottomLeft().toPoint()));
        event->accept();
        return;
      }
    }
    // Measure: first tap activates + opens popover; second tap reopens it.
    if (t.action == NoteToolTile::Tool && t.mode == ToolMode::Ruler) {
      if (!tileActive(t))
        noteActivateTool(ToolMode::Ruler);
      showMeasurePopover(mapToGlobal(t.rect.bottomLeft().toPoint()));
      event->accept();
      return;
    }
  }
  NoteToolbarBase::mousePressEvent(event);
}

void NoteFloatToolbar::showEraserPopover(const QPoint &globalAnchor) {
  QWidget *host = parentWidget() ? parentWidget() : window();
  auto *pop = new QFrame(host, Qt::Popup | Qt::FramelessWindowHint);
  pop->setAttribute(Qt::WA_DeleteOnClose);
  pop->setObjectName(QStringLiteral("StudioEraserPopover"));
  pop->setFixedWidth(UiScale::dp(280));
  pop->setStyleSheet(NoteChrome::studioPopoverStyle(
      QStringLiteral("StudioEraserPopover")));
  auto *shadow = new QGraphicsDropShadowEffect(pop);
  shadow->setBlurRadius(32);
  shadow->setOffset(0, 12);
  shadow->setColor(QColor(0, 0, 0, 115));
  pop->setGraphicsEffect(shadow);
  auto *lay = new QVBoxLayout(pop);
  lay->setContentsMargins(14, 14, 14, 14);
  lay->setSpacing(8);
  auto *stroke = new QRadioButton(QStringLiteral("Strich-Radierer (Objekt)"), pop);
  auto *pixel = new QRadioButton(QStringLiteral("Pixel-Radierer"), pop);
  auto *grp = new QButtonGroup(pop);
  grp->setExclusive(true);
  grp->addButton(stroke);
  grp->addButton(pixel);
  const ToolConfig cfg = ToolManager::instance().configFor(ToolMode::Eraser);
  stroke->setChecked(cfg.eraserMode == EraserMode::Object);
  pixel->setChecked(cfg.eraserMode != EraserMode::Object);
  lay->addWidget(stroke);
  lay->addWidget(pixel);
  auto *sizeLbl = new QLabel(QStringLiteral("Größe"), pop);
  lay->addWidget(sizeLbl);
  auto *slider = new QSlider(Qt::Horizontal, pop);
  slider->setRange(1, 40);
  slider->setValue(qBound(1, cfg.penWidth, 40));
  lay->addWidget(slider);
  auto *keepHl = new QCheckBox(QStringLiteral("Nur Textmarker löschen"), pop);
  keepHl->setChecked(cfg.eraserKeepInk);
  lay->addWidget(keepHl);
  connect(keepHl, &QCheckBox::toggled, pop, [](bool on) {
    auto &mgr = ToolManager::instance();
    ToolConfig c = mgr.configFor(ToolMode::Eraser);
    c.eraserKeepInk = on;
    mgr.configFor(ToolMode::Eraser) = c;
    if (mgr.activeTool() && mgr.activeToolMode() == ToolMode::Eraser)
      mgr.activeTool()->setConfig(c);
  });
  auto *clearBtn = new QPushButton(QStringLiteral("Aktuelle Seite leeren"), pop);
  clearBtn->setStyleSheet(
      QStringLiteral("QPushButton { background: rgba(248,113,113,0.16); color: #FCA5A5;"
                     "  border: 1px solid rgba(248,113,113,0.35); }"));
  lay->addWidget(clearBtn);

  auto applyMode = [](EraserMode mode) {
    auto &mgr = ToolManager::instance();
    ToolConfig c = mgr.configFor(ToolMode::Eraser);
    c.eraserMode = mode;
    mgr.configFor(ToolMode::Eraser) = c;
    mgr.selectTool(ToolMode::Eraser);
    if (mgr.activeTool())
      mgr.activeTool()->setConfig(c);
  };
  connect(stroke, &QRadioButton::toggled, pop, [applyMode](bool on) {
    if (on)
      applyMode(EraserMode::Object);
  });
  connect(pixel, &QRadioButton::toggled, pop, [applyMode](bool on) {
    if (on)
      applyMode(EraserMode::Pixel);
  });
  connect(slider, &QSlider::valueChanged, pop, [](int v) {
    noteApplyPenWidth(v);
    auto &mgr = ToolManager::instance();
    ToolConfig c = mgr.configFor(ToolMode::Eraser);
    c.penWidth = v;
    mgr.configFor(ToolMode::Eraser) = c;
    if (mgr.activeTool() && mgr.activeToolMode() == ToolMode::Eraser)
      mgr.activeTool()->setConfig(c);
  });
  connect(clearBtn, &QPushButton::clicked, pop, [pop]() {
    if (!BlopDialogs::confirm(pop, QStringLiteral("Seite leeren"),
                              QStringLiteral("Alles auf dieser Seite löschen?"),
                              QStringLiteral("Löschen"),
                              QStringLiteral("Abbrechen")))
      return;
    QGraphicsView *view = nullptr;
    if (QWidget *fw = QApplication::focusWidget()) {
      view = qobject_cast<QGraphicsView *>(fw);
      if (!view)
        view = fw->findChild<QGraphicsView *>();
      if (!view)
        if (QWidget *win = fw->window())
          view = win->findChild<QGraphicsView *>();
    }
    if (!view) {
      for (QWidget *tl : QApplication::topLevelWidgets()) {
        view = tl->findChild<QGraphicsView *>();
        if (view)
          break;
      }
    }
    if (view && view->scene()) {
      const QList<QGraphicsItem *> items = view->scene()->items();
      for (QGraphicsItem *item : items) {
        if (dynamic_cast<QGraphicsPathItem *>(item)) {
          view->scene()->removeItem(item);
          delete item;
        }
      }
      view->viewport()->update();
    }
    pop->close();
  });

  pop->adjustSize();
  NoteChrome::clampPopupToHost(pop, globalAnchor + QPoint(0, 6), host);
  pop->show();
}

void NoteFloatToolbar::showShapesPopover(const QPoint &globalAnchor) {
  QWidget *host = parentWidget() ? parentWidget() : window();
  auto *pop = new ShapesCatalogPopover(host);
  connect(pop, &ShapesCatalogPopover::shapeChosen, pop, [](int kind) {
    noteActivateTool(ToolMode::Shape, kind);
  });
  pop->present(globalAnchor, noteMintToolsVisible());
}

void NoteFloatToolbar::showMeasurePopover(const QPoint &globalAnchor) {
  QWidget *host = parentWidget() ? parentWidget() : window();
  auto *pop = new QFrame(host, Qt::Popup | Qt::FramelessWindowHint);
  pop->setAttribute(Qt::WA_DeleteOnClose);
  pop->setObjectName(QStringLiteral("StudioMeasurePopover"));
  pop->setStyleSheet(NoteChrome::studioPopoverStyle(
      QStringLiteral("StudioMeasurePopover")));
  auto *shadow = new QGraphicsDropShadowEffect(pop);
  shadow->setBlurRadius(32);
  shadow->setOffset(0, 12);
  shadow->setColor(QColor(0, 0, 0, 115));
  pop->setGraphicsEffect(shadow);
  auto *lay = new QVBoxLayout(pop);
  lay->setContentsMargins(14, 14, 14, 14);
  lay->setSpacing(8);

  const bool mintOn = noteMintToolsVisible();
  auto &mgr = ToolManager::instance();
  const ToolMode activeMode = mgr.activeToolMode();
  const ToolConfig rulerCfg = mgr.configFor(ToolMode::Ruler);
  const ToolConfig dimCfg = mgr.configFor(ToolMode::Dimension);

  auto *modeLay = new QVBoxLayout;
  modeLay->setSpacing(6);

  auto makeModeBtn = [&](const QString &label, bool checked) {
    auto *b = new QPushButton(label, pop);
    b->setCheckable(true);
    b->setChecked(checked);
    b->setMinimumHeight(UiScale::dp(34));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    modeLay->addWidget(b);
    return b;
  };

  auto *rulerBtn = makeModeBtn(
      QStringLiteral("📏  Lineal"),
      activeMode == ToolMode::Ruler && !rulerCfg.compassMode);
  connect(rulerBtn, &QPushButton::clicked, pop, [pop]() {
    auto &m = ToolManager::instance();
    ToolConfig c = m.configFor(ToolMode::Ruler);
    c.compassMode = false;
    m.configFor(ToolMode::Ruler) = c;
    noteActivateTool(ToolMode::Ruler);
    if (m.activeTool())
      m.activeTool()->setConfig(c);
    pop->close();
  });

  if (mintOn) {
    auto *geoBtn = makeModeBtn(
        QStringLiteral("📐  Geodreieck"),
        activeMode == ToolMode::Ruler && rulerCfg.compassMode);
    connect(geoBtn, &QPushButton::clicked, pop, [pop]() {
      auto &m = ToolManager::instance();
      ToolConfig c = m.configFor(ToolMode::Ruler);
      c.compassMode = true;
      m.configFor(ToolMode::Ruler) = c;
      noteActivateTool(ToolMode::Ruler);
      if (m.activeTool())
        m.activeTool()->setConfig(c);
      pop->close();
    });

    auto *lenBtn = makeModeBtn(
        QStringLiteral("↔  Streckenbemaßung"),
        activeMode == ToolMode::Dimension &&
            dimCfg.dimensionKind == DimensionKind::Length);
    connect(lenBtn, &QPushButton::clicked, pop, [pop]() {
      auto &m = ToolManager::instance();
      ToolConfig c = m.configFor(ToolMode::Dimension);
      c.dimensionKind = DimensionKind::Length;
      m.configFor(ToolMode::Dimension) = c;
      noteActivateTool(ToolMode::Dimension);
      if (m.activeTool())
        m.activeTool()->setConfig(c);
      pop->close();
    });

    auto *angBtn = makeModeBtn(
        QStringLiteral("∠  Winkelmesser"),
        activeMode == ToolMode::Dimension &&
            dimCfg.dimensionKind == DimensionKind::Angle3Point);
    connect(angBtn, &QPushButton::clicked, pop, [pop]() {
      auto &m = ToolManager::instance();
      ToolConfig c = m.configFor(ToolMode::Dimension);
      c.dimensionKind = DimensionKind::Angle3Point;
      m.configFor(ToolMode::Dimension) = c;
      noteActivateTool(ToolMode::Dimension);
      if (m.activeTool())
        m.activeTool()->setConfig(c);
      pop->close();
    });

    auto *areaBtn = makeModeBtn(
        QStringLiteral("⬡  Flächenmesser"),
        activeMode == ToolMode::Dimension &&
            dimCfg.dimensionKind == DimensionKind::Area);
    connect(areaBtn, &QPushButton::clicked, pop, [pop]() {
      auto &m = ToolManager::instance();
      ToolConfig c = m.configFor(ToolMode::Dimension);
      c.dimensionKind = DimensionKind::Area;
      m.configFor(ToolMode::Dimension) = c;
      noteActivateTool(ToolMode::Dimension);
      if (m.activeTool())
        m.activeTool()->setConfig(c);
      pop->close();
    });
  }

  lay->addLayout(modeLay);

  auto *scaleLbl = new QLabel(QStringLiteral("Maßstab: 1 Kästchen ="), pop);
  lay->addWidget(scaleLbl);
  const ToolConfig cfg = ToolManager::instance().config();
  auto *scaleSlider = new QSlider(Qt::Horizontal, pop);
  scaleSlider->setRange(1, 100); // 0.1 … 10.0 units
  scaleSlider->setValue(qBound(1, int(qRound(cfg.measureUnitsPerCell * 10.0)), 100));
  auto *scaleVal = new QLabel(
      QStringLiteral("%1 Einheit(en)").arg(cfg.measureUnitsPerCell, 0, 'g', 3), pop);
  lay->addWidget(scaleSlider);
  lay->addWidget(scaleVal);
  connect(scaleSlider, &QSlider::valueChanged, pop, [scaleVal](int v) {
    const double units = v / 10.0;
    ToolUIBridge::instance().setMeasureUnitsPerCell(units);
    scaleVal->setText(QStringLiteral("%1 Einheit(en)").arg(units, 0, 'g', 3));
  });
  auto *presets = new QHBoxLayout;
  for (double u : {0.5, 1.0, 2.0, 5.0}) {
    auto *b = new QPushButton(QString::number(u), pop);
    connect(b, &QPushButton::clicked, pop, [scaleSlider, u]() {
      scaleSlider->setValue(qBound(1, int(qRound(u * 10.0)), 100));
    });
    presets->addWidget(b);
  }
  lay->addLayout(presets);
  pop->setFixedWidth(UiScale::dp(280));
  pop->adjustSize();
  NoteChrome::clampPopupToHost(pop, globalAnchor + QPoint(0, 6), host);
  pop->show();
}

// ─── Radiant bloom ──────────────────────────────────────────────────────────

namespace {
constexpr int kRadiantSize = 240;
constexpr int kRadiantInset = 50; // center offset from the bottom-right corner
constexpr qreal kDiscR = 53;
constexpr qreal kInnerR0 = 70;
constexpr qreal kInnerR1 = 112;
constexpr qreal kOuterR0 = 120;
constexpr qreal kOuterR1 = 170;
constexpr qreal kArcStart = 91.5;
constexpr qreal kArcEnd = 178.5;
} // namespace

NoteRadiantToolbar::NoteRadiantToolbar(QWidget *parent) : NoteToolbarBase(parent) {
  setAttribute(Qt::WA_TranslucentBackground, true);
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
    t.iconSize = UiScale::dp(28);
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
      {"stickynote", ToolMode::StickyNote, -1, NoteToolTile::Tool, QColor()},
      {"text", ToolMode::Text, -1, NoteToolTile::Tool, QColor()},
      {"pen", ToolMode::Pen, -1, NoteToolTile::Tool, QColor()},
      {"highlighter", ToolMode::Highlighter, -1, NoteToolTile::Tool, kHighlighter},
      {"lasso", ToolMode::Lasso, -1, NoteToolTile::Tool, QColor()},
      {"hand", ToolMode::Hand, -1, NoteToolTile::Tool, QColor()},
  };
  // Outer: quick colors + stroke width + Pen Studio.
  const Spec outer[] = {
      {"", ToolMode::Pen, -1, NoteToolTile::Swatch, QColor(0x00, 0x00, 0x00)},
      {"", ToolMode::Pen, -1, NoteToolTile::Swatch, QColor(0x25, 0x63, 0xEB)},
      {"", ToolMode::Pen, -1, NoteToolTile::Swatch, QColor(0xEF, 0x44, 0x44)},
      {"", ToolMode::Pen, -1, NoteToolTile::Swatch, QColor(0x10, 0xB9, 0x81)},
      {"", ToolMode::Pen, -1, NoteToolTile::Width, QColor()},
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
      t.iconSize = UiScale::dp(r1 - r0 > 45 ? 20 : 18);
      m_tiles.append(t);
    }
  };
  ring(inner, 6, kInnerR0, kInnerR1, 1.8);
  ring(outer, 6, kOuterR0, kOuterR1, 1.4);

  // Mask to the painted quarter-plate so empty square pixels don't eat clicks.
  QPainterPath plate;
  const qreal R = (kOuterR1 + 10) * s;
  plate.moveTo(m_center);
  plate.arcTo(QRectF(m_center.x() - R, m_center.y() - R, 2 * R, 2 * R), 88, 94);
  plate.closeSubpath();
  setMask(QRegion(plate.toFillPolygon().toPolygon()));
}

void NoteRadiantToolbar::paintSkin(QPainter &p) {
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
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
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
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
      {{"stickynote", ToolMode::StickyNote, -1, NoteToolTile::Tool, QColor()},
       {"ruler", ToolMode::Ruler, -1, NoteToolTile::Tool, QColor()}},
      {{"hand", ToolMode::Hand, -1, NoteToolTile::Tool, QColor()},
       {"image", ToolMode::Image, -1, NoteToolTile::Tool, QColor()}},
      {{"line", ToolMode::Shape, static_cast<int>(ShapeToolKind::Line),
        NoteToolTile::Tool, QColor()},
       {"circle", ToolMode::Shape, static_cast<int>(ShapeToolKind::Circle),
        NoteToolTile::Tool, QColor()}},
      {{"undo", ToolMode::Pen, -1, NoteToolTile::Undo, QColor()},
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
    connect(tb, &NoteToolbarBase::penStudioRequested, this,
            &NoteToolbarHost::penStudioRequested);
    connect(tb, &NoteToolbarBase::moreRequested, this,
            [this](const QPoint &g) { showStyleMenu(g); });
  }
  m_chip->hide();
}

NoteToolbarStyle NoteToolbarHost::style() const { return noteToolbarStyle(); }

QWidget *NoteToolbarHost::activeWidget() const {
  switch (noteToolbarStyle()) {
  case NoteToolbarStyle::Radiant:
    return m_radiant;
  case NoteToolbarStyle::Science:
    return m_science;
  case NoteToolbarStyle::Floating:
  case NoteToolbarStyle::Bar:
    break;
  }
  return m_float;
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
  m_chip->setVisible(false);
  relayout(m_topInset);
}

void NoteToolbarHost::relayout(int topInset) {
  m_topInset = topInset;
  if (!m_surface)
    return;
  const int W = m_surface->width();
  const int H = m_surface->height();
  const int m = UiScale::dp(12);
  auto clampCenteredX = [W](int dockW) {
    const int maxX = qMax(0, W - dockW);
    return qBound(0, (W - dockW) / 2, maxX);
  };
  m_bar->move(clampCenteredX(m_bar->width()), topInset + m);
  m_float->move(clampCenteredX(m_float->width()), topInset + m);
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

QPoint NoteToolbarHost::studioAnchorGlobal() const {
  if (m_float && m_float->isVisible())
    return m_float->studioAnchorGlobal();
  if (m_radiant && m_radiant->isVisible())
    // Top-left of the Radiant square → PenStudio sits left/above the fan.
    return m_radiant->mapToGlobal(QPoint(0, UiScale::dp(24)));
  if (QWidget *w = activeWidget())
    return w->mapToGlobal(QPoint(w->width() / 2, w->height()));
  return QCursor::pos();
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
  const bool mintOn = noteMintToolsVisible();
  BlopInWindowMenu::Item mint;
  mint.label = (mintOn ? QStringLiteral("●  ") : QStringLiteral("○  ")) +
               QStringLiteral("MINT-Werkzeuge anzeigen");
  mint.handler = [this]() {
    setNoteMintToolsVisible(!noteMintToolsVisible());
    if (m_float)
      m_float->refreshTiles();
    applyStyle();
    emit styleChanged();
  };
  items.append(mint);
  BlopInWindowMenu::Item opts;
  opts.label = QStringLiteral("Werkzeugoptionen");
  opts.handler = [this]() { emit toolOptionsRequested(); };
  items.append(opts);
  BlopInWindowMenu::show(anchor, globalPos, items);
}
