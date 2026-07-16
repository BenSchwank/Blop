#include "penpresetbar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QSettings>
#include <QTimer>

#include "blop_theme.h"
#include "tools/ToolManager.h"
#include "uiscale.h"

namespace {

QString modeToKey(ToolMode m) {
  switch (m) {
  case ToolMode::Pen: return QStringLiteral("pen");
  case ToolMode::Pencil: return QStringLiteral("pencil");
  case ToolMode::Highlighter: return QStringLiteral("highlighter");
  case ToolMode::Eraser: return QStringLiteral("eraser");
  case ToolMode::Lasso: return QStringLiteral("lasso");
  case ToolMode::Image: return QStringLiteral("image");
  case ToolMode::Ruler: return QStringLiteral("ruler");
  case ToolMode::Shape: return QStringLiteral("shape");
  case ToolMode::StickyNote: return QStringLiteral("sticky");
  case ToolMode::Text: return QStringLiteral("text");
  case ToolMode::Hand: return QStringLiteral("hand");
  }
  return QStringLiteral("pen");
}

ToolMode keyToMode(const QString &k) {
  if (k == QLatin1String("pencil")) return ToolMode::Pencil;
  if (k == QLatin1String("highlighter")) return ToolMode::Highlighter;
  if (k == QLatin1String("eraser")) return ToolMode::Eraser;
  if (k == QLatin1String("lasso")) return ToolMode::Lasso;
  if (k == QLatin1String("image")) return ToolMode::Image;
  if (k == QLatin1String("ruler")) return ToolMode::Ruler;
  if (k == QLatin1String("shape")) return ToolMode::Shape;
  if (k == QLatin1String("sticky")) return ToolMode::StickyNote;
  if (k == QLatin1String("text")) return ToolMode::Text;
  if (k == QLatin1String("hand")) return ToolMode::Hand;
  return ToolMode::Pen;
}

QString encodePreset(const PenPreset &p) {
  return QStringLiteral("%1|%2|%3|%4")
      .arg(modeToKey(p.mode))
      .arg(QString::number(p.color.rgba(), 16))
      .arg(p.width)
      .arg(p.opacity, 0, 'f', 3);
}

bool decodePreset(const QString &s, PenPreset *out) {
  const QStringList parts = s.split(QLatin1Char('|'));
  if (parts.size() < 4)
    return false;
  bool ok = false;
  const uint argb = parts[1].toUInt(&ok, 16);
  if (!ok)
    return false;
  out->mode = keyToMode(parts[0]);
  out->color = QColor::fromRgba(argb);
  out->width = qBound(1, parts[2].toInt(), 48);
  out->opacity = qBound(0.0, parts[3].toDouble(), 1.0);
  return true;
}

QVector<PenPreset> defaultPresets() {
  return {
      PenPreset{ToolMode::Pen, QColor(20, 20, 24), 3, 1.0},
      PenPreset{ToolMode::Pen, QColor(0x5E, 0x5C, 0xE6), 5, 1.0},
      PenPreset{ToolMode::Pen, QColor(0xE5, 0x3E, 0x3E), 4, 1.0},
      PenPreset{ToolMode::Highlighter, QColor(0xFF, 0xD5, 0x4A), 16, 0.45},
      PenPreset{ToolMode::Pencil, QColor(0x3A, 0x3A, 0x42), 2, 0.9},
  };
}

} // namespace

// ===========================================================================
// Internal chip widget
// ===========================================================================
class PresetChip : public QWidget {
  Q_OBJECT
  Q_PROPERTY(double animScale READ animScale WRITE setAnimScale)
  Q_PROPERTY(double holdProgress READ holdProgress WRITE setHoldProgress)
public:
  explicit PresetChip(QWidget *parent = nullptr) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground, true);
    m_holdTimer.setInterval(16);
    connect(&m_holdTimer, &QTimer::timeout, this, [this]() {
      m_holdProgress = qMin(1.0, m_holdProgress + 16.0 / 480.0);
      update();
      if (m_holdProgress >= 1.0 && !m_longFired) {
        m_longFired = true;
        m_holdTimer.stop();
        emit longPressed();
      }
    });
  }

  void setPreset(const PenPreset &p) {
    m_preset = p;
    update();
  }
  PenPreset preset() const { return m_preset; }

  void setActive(bool a) {
    if (m_active == a)
      return;
    m_active = a;
    update();
  }

  void setAccent(const QColor &c) {
    m_accent = c;
    update();
  }

  double animScale() const { return m_animScale; }
  void setAnimScale(double s) {
    m_animScale = s;
    update();
  }
  double holdProgress() const { return m_holdProgress; }
  void setHoldProgress(double v) {
    m_holdProgress = v;
    update();
  }

signals:
  void clicked();
  void longPressed();

protected:
  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() != Qt::LeftButton)
      return;
    m_pressing = true;
    m_longFired = false;
    m_holdProgress = 0.0;
    m_holdTimer.start();
    auto *a = new QPropertyAnimation(this, "animScale", this);
    a->setDuration(BlopMotion::kFast);
    a->setEndValue(0.92);
    a->setEasingCurve(BlopMotion::kEaseStandard);
    a->start(QAbstractAnimation::DeleteWhenStopped);
  }
  void mouseReleaseEvent(QMouseEvent *e) override {
    if (e->button() != Qt::LeftButton)
      return;
    m_holdTimer.stop();
    const bool wasLong = m_longFired;
    m_pressing = false;
    m_holdProgress = 0.0;
    auto *a = new QPropertyAnimation(this, "animScale", this);
    a->setDuration(BlopMotion::kStandard);
    a->setEndValue(1.0);
    a->setEasingCurve(BlopMotion::kEaseOvershoot);
    a->start(QAbstractAnimation::DeleteWhenStopped);
    if (!wasLong && rect().contains(e->pos()))
      emit clicked();
    update();
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = rect().adjusted(1, 1, -1, -1);
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    if (!qFuzzyCompare(m_animScale, 1.0)) {
      p.translate(cx, cy);
      p.scale(m_animScale, m_animScale);
      p.translate(-cx, -cy);
    }

    const qreal rad = UiScale::dp(12);
    QPainterPath bg;
    bg.addRoundedRect(r, rad, rad);

    // Chip surface.
    QColor surface = BlopTheme::surfaceMuted();
    p.fillPath(bg, surface);

    // Border: accent when active, subtle otherwise.
    QColor border = m_active ? m_accent : BlopTheme::borderSubtle();
    QPen bp(border, m_active ? 2.0 : 1.0);
    p.setPen(bp);
    p.drawPath(bg);

    // Mini stroke preview: a gentle S-curve rendered in the preset's
    // color/width so the chip *looks like* the pen it stores.
    QPainterPath stroke;
    const qreal padX = r.width() * 0.22;
    const qreal x0 = r.left() + padX;
    const qreal x1 = r.right() - padX;
    stroke.moveTo(x0, r.bottom() - r.height() * 0.34);
    stroke.cubicTo(x0 + (x1 - x0) * 0.35, r.top() + r.height() * 0.20,
                   x0 + (x1 - x0) * 0.65, r.bottom() - r.height() * 0.18,
                   x1, r.top() + r.height() * 0.36);

    QColor sc = m_preset.color;
    qreal penW = qBound<qreal>(1.5, m_preset.width * 0.7, r.height() * 0.42);
    Qt::PenCapStyle cap = Qt::RoundCap;
    if (m_preset.mode == ToolMode::Highlighter) {
      sc.setAlphaF(qMax(0.35, m_preset.opacity));
      penW = qMax<qreal>(penW, r.height() * 0.34);
      cap = Qt::FlatCap;
    } else if (m_preset.mode == ToolMode::Eraser) {
      sc = BlopTheme::textSecondary();
    } else {
      sc.setAlphaF(m_preset.opacity <= 0.0 ? 1.0 : m_preset.opacity);
    }
    QPen sp(sc, penW, Qt::SolidLine, cap, Qt::RoundJoin);
    if (m_preset.mode == ToolMode::Eraser)
      sp.setStyle(Qt::DotLine);
    p.setPen(sp);
    p.drawPath(stroke);

    // Long-press progress ring hint at the chip's bottom edge.
    if (m_holdProgress > 0.01) {
      QPen prog(m_accent, UiScale::dp(2), Qt::SolidLine, Qt::RoundCap);
      p.setPen(prog);
      const qreal y = r.bottom() - UiScale::dp(3);
      const qreal w = (r.width() - 2 * rad) * m_holdProgress;
      p.drawLine(QPointF(r.left() + rad, y), QPointF(r.left() + rad + w, y));
    }
  }

private:
  PenPreset m_preset;
  bool m_active{false};
  bool m_pressing{false};
  bool m_longFired{false};
  double m_animScale{1.0};
  double m_holdProgress{0.0};
  QColor m_accent{QColor("#7C5CFC")};
  QTimer m_holdTimer;
};

// ===========================================================================
// PenPresetBar
// ===========================================================================
PenPresetBar::PenPresetBar(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFixedHeight(preferredHeightPx());
  loadPresets();
  rebuildChips();
}

int PenPresetBar::preferredHeightPx() const { return UiScale::dp(34); }

void PenPresetBar::setAccentColor(const QColor &c) {
  m_accent = c;
  for (PresetChip *chip : m_chips)
    chip->setAccent(c);
  update();
}

void PenPresetBar::loadPresets() {
  QSettings s("Blop", "BlopApp");
  const QStringList raw = s.value("tools/pen_presets").toStringList();
  m_presets.clear();
  for (const QString &entry : raw) {
    PenPreset p;
    if (decodePreset(entry, &p))
      m_presets.push_back(p);
  }
  if (m_presets.isEmpty())
    m_presets = defaultPresets();
}

void PenPresetBar::savePresets() const {
  QStringList raw;
  raw.reserve(m_presets.size());
  for (const PenPreset &p : m_presets)
    raw << encodePreset(p);
  QSettings s("Blop", "BlopApp");
  s.setValue("tools/pen_presets", raw);
}

void PenPresetBar::rebuildChips() {
  qDeleteAll(m_chips);
  m_chips.clear();
  for (int i = 0; i < m_presets.size(); ++i) {
    auto *chip = new PresetChip(this);
    chip->setPreset(m_presets[i]);
    chip->setAccent(m_accent);
    connect(chip, &PresetChip::clicked, this, [this, i]() {
      if (i < 0 || i >= m_presets.size())
        return;
      m_activeIndex = i;
      for (int j = 0; j < m_chips.size(); ++j)
        m_chips[j]->setActive(j == i);
      emit presetSelected(m_presets[i]);
    });
    connect(chip, &PresetChip::longPressed, this,
            [this, i]() { captureCurrentInto(i); });
    chip->show();
    m_chips.push_back(chip);
  }
  relayoutChips();
}

void PenPresetBar::captureCurrentInto(int index) {
  if (index < 0 || index >= m_presets.size())
    return;
  const ToolConfig cfg = ToolManager::instance().config();
  PenPreset p;
  p.mode = ToolManager::instance().activeToolMode();
  p.color = cfg.penColor;
  p.width = cfg.penWidth;
  p.opacity = cfg.opacity;
  m_presets[index] = p;
  m_chips[index]->setPreset(p);
  savePresets();

  // Playful "saved" confirmation: a quick overshoot pop on the chip.
  auto *a = new QPropertyAnimation(m_chips[index], "animScale", this);
  a->setDuration(BlopMotion::kEmphasis);
  a->setKeyValueAt(0.0, 1.0);
  a->setKeyValueAt(0.4, 1.18);
  a->setKeyValueAt(1.0, 1.0);
  a->setEasingCurve(BlopMotion::kEaseOvershoot);
  a->start(QAbstractAnimation::DeleteWhenStopped);
}

void PenPresetBar::syncActive(ToolMode mode, const QColor &color, int width) {
  int match = -1;
  for (int i = 0; i < m_presets.size(); ++i) {
    const PenPreset &p = m_presets[i];
    if (p.mode == mode && p.color == color && p.width == width) {
      match = i;
      break;
    }
  }
  m_activeIndex = match;
  for (int j = 0; j < m_chips.size(); ++j)
    m_chips[j]->setActive(j == match);
}

void PenPresetBar::relayoutChips() {
  const int h = height();
  const int chipW = qRound(h * 1.35);
  const int gap = UiScale::dp(6);
  int x = 0;
  for (PresetChip *chip : m_chips) {
    chip->setGeometry(x, 0, chipW, h);
    x += chipW + gap;
  }
  setMinimumWidth(qMax(0, x - gap));
}

void PenPresetBar::resizeEvent(QResizeEvent *) { relayoutChips(); }

void PenPresetBar::paintEvent(QPaintEvent *) {}

#include "penpresetbar.moc"
