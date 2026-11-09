#include "penstudiopopover.h"

#include "ToolSettings.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "notechrome.h"
#include "tools/ToolManager.h"
#include "tools/ToolUIBridge.h"
#include "uiscale.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSizePolicy>
#include <QSlider>
#include <QVBoxLayout>
#include <QtMath>

namespace {

QColor panelFill() {
  return NoteChrome::isDark() ? QColor(0x18, 0x1B, 0x22) : QColor(0xFF, 0xFF, 0xFF);
}

QColor panelInk() {
  return NoteChrome::isDark() ? QColor(0xF4, 0xF6, 0xFA) : BlopStyle::paperInk();
}

QColor panelMuted() {
  return NoteChrome::isDark() ? QColor(0xA7, 0xAD, 0xB6) : BlopStyle::paperInkMuted();
}

class StrokePreview : public QWidget {
public:
  explicit StrokePreview(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedHeight(36);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    const ToolConfig cfg = ToolManager::instance().config();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    const qreal mid = height() / 2.0;
    const qreal amp = qBound(2.0, cfg.penWidth * 0.55 + 6.0, height() / 2.0 - 2.0);
    path.moveTo(8, mid);
    for (int x = 8; x <= width() - 8; ++x) {
      const qreal t = (x - 8) / qreal(qMax(1, width() - 16));
      qreal y = mid + std::sin(t * 6.2831853) * amp * 0.55;
      if (cfg.pressureSensitivity)
        y += std::sin(t * 12.566) * cfg.penWidth * 0.15;
      path.lineTo(x, y);
    }
    QColor ink = cfg.penColor.isValid() ? cfg.penColor : QColor(Qt::black);
    ink.setAlphaF(qBound(0.1, cfg.opacity, 1.0));
    Qt::PenStyle style = Qt::SolidLine;
    if (cfg.penStrokeStyle == PenStrokeStyle::Dashed)
      style = Qt::DashLine;
    else if (cfg.penStrokeStyle == PenStrokeStyle::Dotted)
      style = Qt::DotLine;
    QPen pen(ink, qBound(0.5, qreal(cfg.penWidth), 24.0), style, Qt::RoundCap,
             Qt::RoundJoin);
    p.setPen(pen);
    p.drawPath(path);
  }
};

class StudioToggle : public QAbstractButton {
public:
  explicit StudioToggle(const QString &text, QWidget *parent = nullptr)
      : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setText(text);
    setFixedHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  QSize sizeHint() const override {
    return QSize(36 + 8 + fontMetrics().horizontalAdvance(text()) + 4, 22);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool on = isChecked();
    const QRectF track(0, 1, 36, 20);
    p.setPen(Qt::NoPen);
    p.setBrush(on ? QColor(0x5B, 0x9D, 0xFF)
                  : (NoteChrome::isDark() ? QColor(255, 255, 255, 40)
                                          : QColor(0xE5, 0xE7, 0xEB)));
    p.drawRoundedRect(track, 10, 10);
    const qreal knob = 14;
    const qreal kx = on ? track.right() - 3 - knob : track.left() + 3;
    p.setBrush(QColor(255, 255, 255));
    p.drawEllipse(QRectF(kx, track.center().y() - knob / 2.0, knob, knob));
    p.setPen(panelInk());
    QFont f = font();
    f.setPixelSize(12);
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.drawText(QRect(42, 0, width() - 42, height()), Qt::AlignVCenter | Qt::AlignLeft,
               text());
  }
};

QPushButton *styleCard(const QString &text, QWidget *parent) {
  auto *b = new QPushButton(text, parent);
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setFocusPolicy(Qt::NoFocus);
  b->setFixedHeight(UiScale::dp(32));
  return b;
}

void styleCards(QAbstractButton *btn, bool on) {
  const bool dark = NoteChrome::isDark();
  const QString bg =
      on ? QStringLiteral("rgba(91,157,255,0.18)")
         : (dark ? QStringLiteral("rgba(255,255,255,0.06)")
                 : NoteChrome::panelBg().name(QColor::HexRgb));
  const QString border =
      on ? NoteChrome::accent().name(QColor::HexRgb)
         : (dark ? QStringLiteral("rgba(255,255,255,0.12)")
                 : NoteChrome::borderSoft().name(QColor::HexRgb));
  btn->setStyleSheet(QStringLiteral(
                         "QPushButton { background: %1; color: %2; border: 1px solid %3;"
                         " border-radius: 8px; font-size: 10px; font-weight: 600;"
                         " padding: 2px 2px; }")
                         .arg(bg, panelInk().name(QColor::HexRgb), border));
}

QString sliderQss() {
  const QString groove =
      NoteChrome::isDark() ? QStringLiteral("rgba(255,255,255,0.12)")
                           : NoteChrome::borderSoft().name(QColor::HexRgb);
  const QString acc = NoteChrome::accent().name(QColor::HexRgb);
  return QStringLiteral(
             "QSlider::groove:horizontal { height: 4px; background: %1;"
             " border: none; border-radius: 2px; }"
             "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }"
             "QSlider::add-page:horizontal { background: %1; border-radius: 2px; }"
             "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0;"
             " background: %2; border: none; border-radius: 7px; }")
      .arg(groove, acc);
}

QLabel *sectionLabel(const QString &text, QWidget *parent) {
  auto *l = new QLabel(text, parent);
  l->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  return l;
}

} // namespace

PenStudioPopover::PenStudioPopover(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("PenStudioPopover"));
  setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFixedWidth(300);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(16, 16, 16, 16);
  root->setSpacing(10);

  m_preview = new StrokePreview(this);
  root->addWidget(m_preview);

  m_styles = new QButtonGroup(this);
  m_styles->setExclusive(true);
  const QStringList names = {
      QStringLiteral("Füller"),   QStringLiteral("Fineliner"), QStringLiteral("Bleistift"),
      QStringLiteral("Kalligrafie"), QStringLiteral("Gestrichelt"), QStringLiteral("Gepunktet")};
  auto *styleGrid = new QGridLayout;
  styleGrid->setHorizontalSpacing(6);
  styleGrid->setVerticalSpacing(6);
  for (int i = 0; i < names.size(); ++i) {
    auto *b = styleCard(names[i], this);
    m_styles->addButton(b, i);
    styleGrid->addWidget(b, i / 3, i % 3);
  }
  root->addLayout(styleGrid);
  connect(m_styles, &QButtonGroup::idClicked, this, [this](int id) {
    if (!m_syncing)
      applyStyle(id);
  });

  m_widthLabel = sectionLabel(QStringLiteral("Stärke"), this);
  root->addWidget(m_widthLabel);

  auto *widthRow = new QHBoxLayout;
  m_width = new QSlider(Qt::Horizontal, this);
  m_width->setRange(1, 240); // 0.1 pt steps → display /10
  m_width->setFocusPolicy(Qt::NoFocus);
  m_widthValue = new QLabel(this);
  m_widthValue->setFixedWidth(48);
  m_widthValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  widthRow->addWidget(m_width, 1);
  widthRow->addWidget(m_widthValue);
  root->addLayout(widthRow);
  connect(m_width, &QSlider::valueChanged, this, [this](int v) {
    const qreal pt = v / 10.0;
    m_widthValue->setText(QStringLiteral("%1 pt").arg(pt, 0, 'f', 1));
    if (!m_syncing)
      ToolUIBridge::instance().setPenWidth(qMax(1, qRound(pt)));
    refreshPreview();
    applyChrome();
  });

  auto *presets = new QHBoxLayout;
  presets->setSpacing(8);
  for (int w : {1, 3, 6, 12}) {
    auto *dot = new QPushButton(QString::number(w), this);
    dot->setFixedSize(28, 28);
    dot->setCursor(Qt::PointingHandCursor);
    dot->setFocusPolicy(Qt::NoFocus);
    connect(dot, &QPushButton::clicked, this, [this, w]() { m_width->setValue(w * 10); });
    presets->addWidget(dot);
    m_presets.append(dot);
  }
  presets->addStretch(1);
  root->addLayout(presets);

  m_colorLabel = sectionLabel(QStringLiteral("Farbe"), this);
  root->addWidget(m_colorLabel);

  auto *colors = new QHBoxLayout;
  colors->setSpacing(6);
  m_colors = {QColor(0x11, 0x18, 0x27), QColor(0xF8, 0xFA, 0xFC),
              QColor(0xEF, 0x44, 0x44), QColor(0xF9, 0x73, 0x16),
              QColor(0xEA, 0xB3, 0x08), QColor(0x22, 0xC5, 0x5E),
              QColor(0x3B, 0x82, 0xF6), QColor(0xA7, 0x8B, 0xFA)};
  for (const QColor &c : m_colors) {
    auto *b = new QPushButton(this);
    b->setFixedSize(22, 22);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    connect(b, &QPushButton::clicked, this, [this, c]() {
      ToolUIBridge::instance().setPenColor(c);
      refreshPreview();
      applyChrome();
    });
    colors->addWidget(b);
    m_colorBtns.append(b);
  }
  m_plus = new QPushButton(QStringLiteral("+"), this);
  m_plus->setFixedSize(22, 22);
  m_plus->setCursor(Qt::PointingHandCursor);
  m_plus->setFocusPolicy(Qt::NoFocus);
  connect(m_plus, &QPushButton::clicked, this, [this]() {
    const QColor cur = ToolManager::instance().config().penColor;
    const QColor picked = QColorDialog::getColor(cur, this, QStringLiteral("Stiftfarbe"));
    if (!picked.isValid())
      return;
    ToolUIBridge::instance().setPenColor(picked);
    refreshPreview();
    applyChrome();
  });
  colors->addWidget(m_plus);
  root->addLayout(colors);

  m_opacityLabel = sectionLabel(QStringLiteral("Deckkraft"), this);
  root->addWidget(m_opacityLabel);

  auto *opRow = new QHBoxLayout;
  m_opacity = new QSlider(Qt::Horizontal, this);
  m_opacity->setRange(10, 100);
  m_opacity->setFocusPolicy(Qt::NoFocus);
  m_opacityValue = new QLabel(this);
  m_opacityValue->setFixedWidth(40);
  m_opacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  opRow->addWidget(m_opacity, 1);
  opRow->addWidget(m_opacityValue);
  root->addLayout(opRow);
  connect(m_opacity, &QSlider::valueChanged, this, [this](int v) {
    m_opacityValue->setText(QStringLiteral("%1%").arg(v));
    if (!m_syncing)
      ToolUIBridge::instance().setOpacity(v / 100.0);
    refreshPreview();
  });

  m_smoothLabel = sectionLabel(QStringLiteral("Glättung / Stabilizer"), this);
  root->addWidget(m_smoothLabel);
  auto *smRow = new QHBoxLayout;
  m_smooth = new QSlider(Qt::Horizontal, this);
  m_smooth->setRange(0, 100);
  m_smooth->setFocusPolicy(Qt::NoFocus);
  m_smoothValue = new QLabel(this);
  m_smoothValue->setFixedWidth(40);
  m_smoothValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  smRow->addWidget(m_smooth, 1);
  smRow->addWidget(m_smoothValue);
  root->addLayout(smRow);
  connect(m_smooth, &QSlider::valueChanged, this, [this](int v) {
    m_smoothValue->setText(QString::number(v));
    if (!m_syncing)
      ToolUIBridge::instance().setSmoothing(v);
  });

  m_rule = new QWidget(this);
  m_rule->setFixedHeight(1);
  root->addWidget(m_rule);

  m_holdLine = new StudioToggle(QStringLiteral("Tinte → Linie (350 ms)"), this);
  m_holdShape = new StudioToggle(QStringLiteral("Tinte → Form (350 ms)"), this);
  root->addWidget(m_holdLine);
  root->addWidget(m_holdShape);
  connect(m_holdLine, &QAbstractButton::toggled, this, [this](bool on) {
    if (m_syncing)
      return;
    ToolUIBridge::instance().setHoldEnableLine(on);
    ToolUIBridge::instance().setHoldStillDelayMs(350);
  });
  connect(m_holdShape, &QAbstractButton::toggled, this, [this](bool on) {
    if (m_syncing)
      return;
    ToolUIBridge::instance().setHoldEnableCircle(on);
    ToolUIBridge::instance().setHoldEnableTriangle(on);
    ToolUIBridge::instance().setHoldStillDelayMs(350);
  });

  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &) {
            if (!m_syncing && isVisible())
              syncFromTool();
          });
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this, [this]() {
    applyChrome();
    if (isVisible())
      syncFromTool();
    update();
  });

  applyChrome();
}

void PenStudioPopover::applyChrome() {
  const bool dark = NoteChrome::isDark();
  const QString ink = panelInk().name(QColor::HexRgb);
  const QString muted = panelMuted().name(QColor::HexRgb);
  const QString labelQss =
      QStringLiteral("QLabel { color: %1; font-size: 11px; font-weight: 600; "
                     "background: transparent; }")
          .arg(muted);
  const QString valueQss =
      QStringLiteral("QLabel { color: %1; font-size: 12px; font-weight: 600; "
                     "background: transparent; }")
          .arg(ink);
  for (QLabel *l : {m_widthLabel, m_colorLabel, m_opacityLabel, m_smoothLabel}) {
    if (l)
      l->setStyleSheet(labelQss);
  }
  for (QLabel *l : {m_widthValue, m_opacityValue, m_smoothValue}) {
    if (l)
      l->setStyleSheet(valueQss);
  }
  for (QSlider *s : {m_width, m_opacity, m_smooth}) {
    if (s)
      s->setStyleSheet(sliderQss());
  }
  if (m_rule) {
    m_rule->setStyleSheet(
        dark ? QStringLiteral("background: rgba(255,255,255,0.10);")
             : QStringLiteral("background: %1;")
                   .arg(NoteChrome::borderSoft().name(QColor::HexRgb)));
  }

  const int currentW = m_width ? qRound(m_width->value() / 10.0) : 0;
  const QString idleBg =
      dark ? QStringLiteral("rgba(255,255,255,0.08)")
           : NoteChrome::panelBg().name(QColor::HexRgb);
  const QString idleBd =
      dark ? QStringLiteral("rgba(255,255,255,0.12)")
           : NoteChrome::borderSoft().name(QColor::HexRgb);
  const QString accHex = NoteChrome::accent().name(QColor::HexRgb);
  for (QPushButton *dot : m_presets) {
    const bool on = dot->text().toInt() == currentW;
    dot->setStyleSheet(QStringLiteral(
                           "QPushButton { background: %1; color: %2;"
                           " border: 1px solid %3; border-radius: 14px;"
                           " font-size: 11px; font-weight: 600; }")
                           .arg(on ? QStringLiteral("rgba(91,157,255,0.22)") : idleBg,
                                ink, on ? accHex : idleBd));
  }

  const QColor cur = ToolManager::instance().config().penColor;
  const QString idleColorBd =
      dark ? QStringLiteral("rgba(255,255,255,0.20)") : QStringLiteral("rgba(0,0,0,0.16)");
  for (int i = 0; i < m_colorBtns.size() && i < m_colors.size(); ++i) {
    const bool on = cur.isValid() && m_colors[i].rgb() == cur.rgb();
    m_colorBtns[i]->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border-radius: 11px;"
                       " border: %2px solid %3; }"
                       "QPushButton:hover { border: 2px solid #5B9DFF; }")
            .arg(m_colors[i].name(QColor::HexRgb),
                 on ? QStringLiteral("2") : QStringLiteral("1"),
                 on ? QStringLiteral("#5B9DFF") : idleColorBd));
  }
  if (m_plus) {
    m_plus->setStyleSheet(QStringLiteral(
                              "QPushButton { background: %1; color: %2;"
                              " border-radius: 11px; border: 1px solid %3;"
                              " font-weight: 700; }")
                              .arg(idleBg, ink, idleBd));
  }
  if (m_styles) {
    for (auto *b : m_styles->buttons())
      styleCards(b, b->isChecked());
  }
  if (m_holdLine)
    m_holdLine->update();
  if (m_holdShape)
    m_holdShape->update();
}

void PenStudioPopover::applyStyle(int style) {
  auto &bridge = ToolUIBridge::instance();
  bridge.setPenStrokeStyle(style);
  // Pencil feel: route to pencil tool when Bleistift selected.
  if (style == int(PenStrokeStyle::Pencil))
    bridge.setSmoothing(35);
  else if (style == int(PenStrokeStyle::Fountain) || style == int(PenStrokeStyle::Calligraphy))
    bridge.setSmoothing(qMax(12, bridge.smoothing()));
  else
    bridge.setSmoothing(qMin(bridge.smoothing(), 20));
  refreshPreview();
  syncFromTool();
}

void PenStudioPopover::refreshPreview() {
  if (m_preview)
    m_preview->update();
}

void PenStudioPopover::syncFromTool() {
  m_syncing = true;
  const ToolConfig cfg = ToolManager::instance().config();
  if (m_width)
    m_width->setValue(qBound(5, int(qRound(cfg.penWidth * 10.0)), 240));
  if (m_opacity)
    m_opacity->setValue(qBound(10, int(qRound(cfg.opacity * 100.0)), 100));
  if (m_smooth)
    m_smooth->setValue(qBound(0, cfg.smoothing, 100));
  if (m_holdLine)
    m_holdLine->setChecked(cfg.holdEnableLine);
  if (m_holdShape)
    m_holdShape->setChecked(cfg.holdEnableCircle || cfg.holdEnableTriangle);
  const int style = qBound(0, int(cfg.penStrokeStyle), 5);
  if (m_styles) {
    if (auto *b = m_styles->button(style))
      b->setChecked(true);
  }
  if (m_widthValue && m_width)
    m_widthValue->setText(QStringLiteral("%1 pt").arg(m_width->value() / 10.0, 0, 'f', 1));
  if (m_opacityValue && m_opacity)
    m_opacityValue->setText(QStringLiteral("%1%").arg(m_opacity->value()));
  if (m_smoothValue && m_smooth)
    m_smoothValue->setText(QString::number(m_smooth->value()));
  m_syncing = false;
  applyChrome();
  refreshPreview();
  update();
}

void PenStudioPopover::present(const QPoint &globalAnchor) {
  syncFromTool();
  adjustSize();
  QPoint pos = globalAnchor - QPoint(width() + 12, height() / 3);
  if (QWidget *host = parentWidget()) {
    const QRect bounds = host->rect();
    QPoint local = host->mapFromGlobal(pos);
    if (local.x() < 8)
      local.setX(8);
    if (local.y() < 8)
      local.setY(8);
    local.setX(qBound(8, local.x(), qMax(8, bounds.width() - width() - 8)));
    local.setY(qBound(8, local.y(), qMax(8, bounds.height() - height() - 8)));
    pos = host->mapToGlobal(local);
  }
  move(pos);
  show();
  raise();
}

void PenStudioPopover::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 40));
  p.drawRoundedRect(r.adjusted(0, 2, 0, 0), 12, 12);
  p.setBrush(panelFill());
  p.setPen(QPen(NoteChrome::isDark() ? QColor(255, 255, 255, 31)
                                     : QColor(0xE5, 0xE7, 0xEB),
                1));
  p.drawRoundedRect(r, 12, 12);
}
