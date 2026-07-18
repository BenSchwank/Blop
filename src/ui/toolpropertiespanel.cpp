#include "toolpropertiespanel.h"

#include "notechrome.h"
#include "tools/ToolManager.h"
#include "uiscale.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

ToolPropertiesPanel::ToolPropertiesPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ToolPropertiesPanel"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(preferredWidth());

  m_root = new QVBoxLayout(this);
  m_root->setContentsMargins(UiScale::dp(12), UiScale::dp(14), UiScale::dp(12),
                             UiScale::dp(14));
  m_root->setSpacing(UiScale::dp(10));

  auto *header = new QHBoxLayout;
  m_title = new QLabel(QStringLiteral("Eigenschaften"), this);
  m_title->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700;"));
  header->addWidget(m_title, 1);
  auto *closeBtn = new QPushButton(QStringLiteral("×"), this);
  closeBtn->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setFlat(true);
  connect(closeBtn, &QPushButton::clicked, this,
          &ToolPropertiesPanel::closeRequested);
  header->addWidget(closeBtn);
  m_root->addLayout(header);

  m_widthLbl = new QLabel(QStringLiteral("Dicke"), this);
  m_root->addWidget(m_widthLbl);
  m_widthSlider = new QSlider(Qt::Horizontal, this);
  m_widthSlider->setRange(1, 50);
  connect(m_widthSlider, &QSlider::valueChanged, this, [this](int v) {
    m_config.penWidth = v;
    m_widthLbl->setText(QStringLiteral("Dicke  %1").arg(v));
    applyConfig();
  });
  m_root->addWidget(m_widthSlider);

  m_opacityLbl = new QLabel(QStringLiteral("Deckkraft"), this);
  m_root->addWidget(m_opacityLbl);
  m_opacitySlider = new QSlider(Qt::Horizontal, this);
  m_opacitySlider->setRange(10, 100);
  connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
    m_config.opacity = v / 100.0;
    m_opacityLbl->setText(QStringLiteral("Deckkraft  %1%").arg(v));
    applyConfig();
  });
  m_root->addWidget(m_opacitySlider);

  auto *colorLbl = new QLabel(QStringLiteral("Farbe"), this);
  m_root->addWidget(colorLbl);
  m_colorRow = new QWidget(this);
  addColorRow(m_root);

  m_root->addStretch(1);

  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this](auto *) { syncFromToolManager(); });
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &) {
            // Avoid feedback loops while dragging sliders.
            if (!m_widthSlider->isSliderDown() &&
                !m_opacitySlider->isSliderDown())
              syncFromToolManager();
          });

  syncFromToolManager();
  rebuild();
}

int ToolPropertiesPanel::preferredWidth() const { return UiScale::dp(200); }

void ToolPropertiesPanel::setAccentColor(const QColor &c) {
  if (!c.isValid())
    return;
  m_accent = c;
  rebuild();
  update();
}

void ToolPropertiesPanel::setVisibleForTool(ToolMode mode) {
  m_mode = mode;
  const bool writing = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                        mode == ToolMode::Highlighter ||
                        mode == ToolMode::Eraser);
  m_widthSlider->setVisible(writing);
  m_widthLbl->setVisible(writing);
  const bool colorful = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                         mode == ToolMode::Highlighter);
  m_opacitySlider->setVisible(colorful);
  m_opacityLbl->setVisible(colorful);
  if (m_colorRow)
    m_colorRow->setVisible(colorful);
}

void ToolPropertiesPanel::syncFromToolManager() {
  auto &tm = ToolManager::instance();
  m_mode = tm.activeToolMode();
  m_config = tm.config();
  m_widthSlider->blockSignals(true);
  m_opacitySlider->blockSignals(true);
  m_widthSlider->setValue(m_config.penWidth);
  m_opacitySlider->setValue(qRound(m_config.opacity * 100.0));
  m_widthLbl->setText(QStringLiteral("Dicke  %1").arg(m_config.penWidth));
  m_opacityLbl->setText(
      QStringLiteral("Deckkraft  %1%").arg(qRound(m_config.opacity * 100.0)));
  m_widthSlider->blockSignals(false);
  m_opacitySlider->blockSignals(false);

  QString title = QStringLiteral("Eigenschaften");
  switch (m_mode) {
  case ToolMode::Pen:
    title = QStringLiteral("Stift");
    break;
  case ToolMode::Pencil:
    title = QStringLiteral("Bleistift");
    break;
  case ToolMode::Highlighter:
    title = QStringLiteral("Textmarker");
    break;
  case ToolMode::Eraser:
    title = QStringLiteral("Radierer");
    break;
  default:
    break;
  }
  m_title->setText(title);
  setVisibleForTool(m_mode);
  rebuild();
}

void ToolPropertiesPanel::applyConfig() {
  ToolManager::instance().updateConfig(m_config);
}

void ToolPropertiesPanel::addColorRow(QVBoxLayout *lay) {
  auto *grid = new QGridLayout(m_colorRow);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(UiScale::dp(6));
  QList<QColor> colors = {
      Qt::black, Qt::white, QColor(220, 50, 50), QColor(40, 120, 255),
      QColor(40, 180, 80), QColor(250, 180, 30), QColor(160, 60, 200),
      QColor(30, 30, 30)};
  for (int i = 0; i < colors.size(); ++i) {
    auto *btn = makeSwatch(colors[i]);
    grid->addWidget(btn, i / 4, i % 4);
  }
  lay->addWidget(m_colorRow);
}

QPushButton *ToolPropertiesPanel::makeSwatch(const QColor &c) {
  auto *btn = new QPushButton(m_colorRow);
  btn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  btn->setCursor(Qt::PointingHandCursor);
  const QString border =
      (c == Qt::white) ? QStringLiteral("#888") : c.darker(120).name();
  btn->setStyleSheet(QStringLiteral(
      "QPushButton { background: %1; border: 1px solid %2; border-radius: 6px; }"
      "QPushButton:hover { border: 2px solid %3; }")
                         .arg(c.name(), border, m_accent.name()));
  connect(btn, &QPushButton::clicked, this, [this, c]() {
    m_config.penColor = c;
    applyConfig();
    update();
  });
  return btn;
}

void ToolPropertiesPanel::rebuild() {
  const QString qss = QStringLiteral(
      "QWidget#ToolPropertiesPanel { background: %1; border-left: 1px solid %2; }"
      "QLabel { color: %3; background: transparent; font-size: 12px; font-weight: 600; }"
      "QPushButton { color: %3; background: transparent; border: none; font-size: 18px; }"
      "QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }"
      "QSlider::sub-page:horizontal { background: %4; border-radius: 2px; }"
      "QSlider::handle:horizontal { background: %5; width: 14px; height: 14px; "
      "margin: -5px 0; border-radius: 7px; }")
                          .arg(NoteChrome::panelBg().name(),
                               NoteChrome::border().name(),
                               NoteChrome::textPrimary().name(),
                               NoteChrome::accent().name(),
                               NoteChrome::textPrimary().name());
  setStyleSheet(qss);
}

void ToolPropertiesPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), NoteChrome::panelBg());
  p.setPen(QPen(NoteChrome::borderSoft(), 1));
  p.drawLine(0, 0, 0, height());
}
