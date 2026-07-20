#include "toolpropertiespanel.h"

#include "editoroverlays.h"
#include "notechrome.h"
#include "tools/ToolManager.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
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

  // Mode row for Lasso / Shape / Eraser / Highlighter.
  m_modeLbl = new QLabel(QStringLiteral("Modus"), this);
  m_root->addWidget(m_modeLbl);
  m_modeRow = new QWidget(this);
  auto *modeLay = new QGridLayout(m_modeRow);
  modeLay->setContentsMargins(0, 0, 0, 0);
  modeLay->setSpacing(UiScale::dp(6));
  m_modeA = new QPushButton(m_modeRow);
  m_modeB = new QPushButton(m_modeRow);
  m_modeC = new QPushButton(m_modeRow);
  m_modeD = new QPushButton(m_modeRow);
  m_modeE = new QPushButton(m_modeRow);
  m_modeF = new QPushButton(m_modeRow);
  m_modeG = new QPushButton(m_modeRow);
  QList<QPushButton *> modeBtns = {m_modeA, m_modeB, m_modeC, m_modeD,
                                   m_modeE, m_modeF, m_modeG};
  auto *grp = new QButtonGroup(this);
  grp->setExclusive(true);
  for (int i = 0; i < modeBtns.size(); ++i) {
    QPushButton *b = modeBtns[i];
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(UiScale::dp(30));
    modeLay->addWidget(b, i / 2, i % 2);
    grp->addButton(b);
  }
  connect(m_modeA, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Lasso)
      m_config.lassoMode = LassoMode::Freehand;
    else if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Rectangle;
    else if (m_mode == ToolMode::Eraser)
      m_config.eraserMode = EraserMode::Pixel;
    else if (m_mode == ToolMode::Highlighter)
      m_config.tipType = HighlighterTip::Round;
    applyConfig();
  });
  connect(m_modeB, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Lasso)
      m_config.lassoMode = LassoMode::Rectangle;
    else if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Circle;
    else if (m_mode == ToolMode::Eraser)
      m_config.eraserMode = EraserMode::Object;
    else if (m_mode == ToolMode::Highlighter)
      m_config.tipType = HighlighterTip::Chisel;
    applyConfig();
  });
  connect(m_modeC, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Line;
    applyConfig();
  });
  connect(m_modeD, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Arrow;
    applyConfig();
  });
  connect(m_modeE, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Axes2D;
    applyConfig();
  });
  connect(m_modeF, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::SineGraph;
    applyConfig();
  });
  connect(m_modeG, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::CoordinateGraph;
    applyConfig();
  });
  m_root->addWidget(m_modeRow);

  m_hintLbl = new QLabel(this);
  m_hintLbl->setWordWrap(true);
  m_hintLbl->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: 500;"));
  m_root->addWidget(m_hintLbl);

  m_root->addStretch(1);

  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this](auto *) { syncFromToolManager(); });
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this](const ToolConfig &) {
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
  refreshSwatchSelection();
  update();
}

void ToolPropertiesPanel::setVisibleForTool(ToolMode mode) {
  m_mode = mode;
  const bool writing = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                        mode == ToolMode::Highlighter ||
                        mode == ToolMode::Eraser || mode == ToolMode::Shape ||
                        mode == ToolMode::Text || mode == ToolMode::Ruler);
  m_widthSlider->setVisible(writing);
  m_widthLbl->setVisible(writing);
  const bool colorful = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                         mode == ToolMode::Highlighter ||
                         mode == ToolMode::Shape || mode == ToolMode::Text);
  m_opacitySlider->setVisible(colorful);
  m_opacityLbl->setVisible(colorful);
  if (m_colorRow)
    m_colorRow->setVisible(colorful);

  const bool modeTools = (mode == ToolMode::Lasso || mode == ToolMode::Shape ||
                          mode == ToolMode::Eraser ||
                          mode == ToolMode::Highlighter);
  if (m_modeLbl)
    m_modeLbl->setVisible(modeTools);
  if (m_modeRow)
    m_modeRow->setVisible(modeTools);

  if (mode == ToolMode::Lasso) {
    m_modeA->setText(QStringLiteral("Freihand"));
    m_modeB->setText(QStringLiteral("Rechteck"));
    m_modeA->setChecked(m_config.lassoMode == LassoMode::Freehand);
    m_modeB->setChecked(m_config.lassoMode == LassoMode::Rectangle);
    for (QPushButton *b : {m_modeC, m_modeD, m_modeE, m_modeF, m_modeG})
      if (b)
        b->setVisible(false);
  } else if (mode == ToolMode::Eraser) {
    m_modeA->setText(QStringLiteral("Pixel"));
    m_modeB->setText(QStringLiteral("Objekt"));
    m_modeA->setChecked(m_config.eraserMode == EraserMode::Pixel);
    m_modeB->setChecked(m_config.eraserMode == EraserMode::Object);
    for (QPushButton *b : {m_modeC, m_modeD, m_modeE, m_modeF, m_modeG})
      if (b)
        b->setVisible(false);
  } else if (mode == ToolMode::Highlighter) {
    m_modeA->setText(QStringLiteral("Rund"));
    m_modeB->setText(QStringLiteral("Meißel"));
    m_modeA->setChecked(m_config.tipType == HighlighterTip::Round);
    m_modeB->setChecked(m_config.tipType == HighlighterTip::Chisel);
    for (QPushButton *b : {m_modeC, m_modeD, m_modeE, m_modeF, m_modeG})
      if (b)
        b->setVisible(false);
  } else if (mode == ToolMode::Shape) {
    struct KindBtn {
      QPushButton *btn;
      const char *label;
      ShapeToolKind kind;
    };
    const KindBtn kinds[] = {
        {m_modeA, "Rechteck", ShapeToolKind::Rectangle},
        {m_modeB, "Kreis", ShapeToolKind::Circle},
        {m_modeC, "Linie", ShapeToolKind::Line},
        {m_modeD, "Pfeil", ShapeToolKind::Arrow},
        {m_modeE, "Achsen", ShapeToolKind::Axes2D},
        {m_modeF, "Sinus", ShapeToolKind::SineGraph},
        {m_modeG, "Graph", ShapeToolKind::CoordinateGraph},
    };
    for (const KindBtn &k : kinds) {
      if (!k.btn)
        continue;
      k.btn->setVisible(true);
      k.btn->setText(QString::fromUtf8(k.label));
      k.btn->setChecked(m_config.shapeToolKind == k.kind);
    }
  }

  if (m_hintLbl) {
    QString hint;
    switch (mode) {
    case ToolMode::Hand:
      hint = QStringLiteral(
          "Ziehen zum Verschieben. Leertaste gedrückt halten zum "
          "vorübergehenden Schwenken.");
      break;
    case ToolMode::Image:
      hint = QStringLiteral("Tippe auf die Seite, um ein Bild einzufügen.");
      break;
    case ToolMode::StickyNote:
      hint = QStringLiteral("Tippe auf die Seite, um eine Notiz zu setzen.");
      break;
    case ToolMode::Ruler:
      hint = QStringLiteral("Lineal auf der Seite positionieren und zeichnen.");
      break;
    case ToolMode::Eraser:
      hint = QStringLiteral(
          "Pixel radiert Tinte; Objekt entfernt ganze Striche.");
      break;
    case ToolMode::Lasso:
      hint = QStringLiteral(
          "Auswahl markieren, dann verschieben, duplizieren oder "
          "zur Bibliothek hinzufügen.");
      break;
    case ToolMode::Shape:
      hint = QStringLiteral(
          "Rechteck/Kreis/Linie/Pfeil aufziehen. Achsen, Sinus und Graph "
          "nutzen die Bounds als Koordinatenrahmen.");
      break;
    default:
      break;
    }
    m_hintLbl->setText(hint);
    m_hintLbl->setVisible(!hint.isEmpty());
  }
  refreshSwatchSelection();
}

void ToolPropertiesPanel::syncForMode(ToolMode mode) {
  m_mode = mode;
  if (mode != ToolMode::Hand)
    m_config = ToolManager::instance().configFor(mode);
  else
    m_config = ToolManager::instance().config();

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
  switch (mode) {
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
  case ToolMode::Lasso:
    title = QStringLiteral("Auswahl");
    break;
  case ToolMode::Shape:
    title = QStringLiteral("Formen");
    break;
  case ToolMode::Text:
    title = QStringLiteral("Text");
    break;
  case ToolMode::Ruler:
    title = QStringLiteral("Messen");
    break;
  case ToolMode::Hand:
    title = QStringLiteral("Hand");
    break;
  case ToolMode::Image:
    title = QStringLiteral("Bild");
    break;
  case ToolMode::StickyNote:
    title = QStringLiteral("Notiz");
    break;
  default:
    break;
  }
  m_title->setText(title);
  setVisibleForTool(mode);
  rebuild();
}

void ToolPropertiesPanel::syncFromToolManager() {
  syncForMode(ToolManager::instance().activeToolMode());
}

void ToolPropertiesPanel::applyConfig() {
  ToolManager::instance().updateConfig(m_config);
  refreshSwatchSelection();
}

void ToolPropertiesPanel::addColorRow(QVBoxLayout *lay) {
  auto *grid = new QGridLayout(m_colorRow);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(UiScale::dp(6));
  const QList<QColor> colors = {
      Qt::black, Qt::white, QColor(220, 50, 50), QColor(40, 120, 255),
      QColor(40, 180, 80), QColor(250, 180, 30), QColor(160, 60, 200),
      QColor(30, 30, 30)};
  for (int i = 0; i < colors.size(); ++i) {
    auto *btn = makeSwatch(colors[i]);
    m_swatches.append(btn);
    grid->addWidget(btn, i / 4, i % 4);
  }
  m_customColorBtn = new QPushButton(QStringLiteral("…"), m_colorRow);
  m_customColorBtn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  m_customColorBtn->setCursor(Qt::PointingHandCursor);
  m_customColorBtn->setToolTip(QStringLiteral("Eigene Farbe…"));
  connect(m_customColorBtn, &QPushButton::clicked, this, [this]() {
    QColor c = m_config.penColor;
    if (!showColorPickerOverlay(window(), &c, QStringLiteral("Stiftfarbe")))
      return;
    m_config.penColor = c;
    applyConfig();
  });
  grid->addWidget(m_customColorBtn, 2, 0);
  lay->addWidget(m_colorRow);
}

void ToolPropertiesPanel::refreshSwatchSelection() {
  for (QPushButton *btn : m_swatches) {
    if (!btn)
      continue;
    const QColor c = btn->property("swatchColor").value<QColor>();
    const bool selected = c.isValid() && c == m_config.penColor;
    const QString border =
        selected ? m_accent.name()
                 : ((c == Qt::white) ? QStringLiteral("#888")
                                     : c.darker(120).name());
    const int bw = selected ? 2 : 1;
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: %2px solid %3; border-radius: 6px; }"
        "QPushButton:hover { border: 2px solid %4; }")
                           .arg(c.name())
                           .arg(bw)
                           .arg(border, m_accent.name()));
  }
  if (m_customColorBtn) {
    m_customColorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; font-weight: 700; }"
        "QPushButton:hover { border-color: %4; }")
                                        .arg(NoteChrome::panelElevated().name(),
                                             NoteChrome::textPrimary().name(),
                                             NoteChrome::border().name(),
                                             m_accent.name()));
  }
}

QPushButton *ToolPropertiesPanel::makeSwatch(const QColor &c) {
  auto *btn = new QPushButton(m_colorRow);
  btn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setProperty("swatchColor", c);
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
      "QPushButton:checked { background: rgba(91,157,255,0.22); border-radius: 6px; color: %4; }"
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
  refreshSwatchSelection();
}

void ToolPropertiesPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), NoteChrome::panelBg());
  p.setPen(QPen(NoteChrome::borderSoft(), 1));
  p.drawLine(0, 0, 0, height());
}
