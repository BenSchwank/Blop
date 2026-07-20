#include "toolpropertiespanel.h"

#include "editoroverlays.h"
#include "notechrome.h"
#include "tools/ToolManager.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

ToolPropertiesPanel::ToolPropertiesPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ToolPropertiesPanel"));
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setMinimumWidth(preferredWidth());
  setMaximumWidth(preferredWidth());

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

  // Pen ink style tiles (Einfach / Pro / Kalligrafie) — Drawboard-like.
  m_styleLbl = new QLabel(QStringLiteral("Stil"), this);
  m_root->addWidget(m_styleLbl);
  m_styleRow = new QWidget(this);
  auto *styleLay = new QHBoxLayout(m_styleRow);
  styleLay->setContentsMargins(0, 0, 0, 0);
  styleLay->setSpacing(UiScale::dp(6));
  m_styleEinfach =
      makeStyleTile(QStringLiteral("Einfach"), QStringLiteral("Gleichmäßig"),
                    PenInkStyle::Einfach);
  m_stylePro = makeStyleTile(QStringLiteral("Pro"),
                             QStringLiteral("Druck + Glättung"), PenInkStyle::Pro);
  m_styleKalli =
      makeStyleTile(QStringLiteral("Kalligrafie"), QStringLiteral("Dynamisch"),
                    PenInkStyle::Kalligrafie);
  styleLay->addWidget(m_styleEinfach, 1);
  styleLay->addWidget(m_stylePro, 1);
  styleLay->addWidget(m_styleKalli, 1);
  m_root->addWidget(m_styleRow);

  m_widthLbl = new QLabel(QStringLiteral("Dicke"), this);
  m_root->addWidget(m_widthLbl);
  m_widthSlider = new QSlider(Qt::Horizontal, this);
  m_widthSlider->setRange(1, 50);
  connect(m_widthSlider, &QSlider::valueChanged, this, [this](int v) {
    m_config.penWidth = v;
    if (m_mode == ToolMode::Text || m_mode == ToolMode::StickyNote)
      m_widthLbl->setText(QStringLiteral("Schriftgröße  %1").arg(v));
    else
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
    if (m_mode == ToolMode::Image)
      m_config.imageOpacity = m_config.opacity;
    m_opacityLbl->setText(QStringLiteral("Deckkraft  %1%").arg(v));
    applyConfig();
  });
  m_root->addWidget(m_opacitySlider);

  auto *colorLbl = new QLabel(QStringLiteral("Farbe"), this);
  m_root->addWidget(colorLbl);
  m_colorRow = new QWidget(this);
  addColorRow(m_root);

  m_fillLbl = new QLabel(QStringLiteral("Füllung"), this);
  m_root->addWidget(m_fillLbl);
  m_fillRow = new QWidget(this);
  addFillColorRow(m_root);

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
  m_modeH = new QPushButton(m_modeRow);
  QList<QPushButton *> modeBtns = {m_modeA, m_modeB, m_modeC, m_modeD,
                                   m_modeE, m_modeF, m_modeG, m_modeH};
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
      m_config.shapeToolKind = ShapeToolKind::Ellipse;
    applyConfig();
  });
  connect(m_modeD, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Line;
    applyConfig();
  });
  connect(m_modeE, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Arrow;
    applyConfig();
  });
  connect(m_modeF, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::Axes2D;
    applyConfig();
  });
  connect(m_modeG, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::SineGraph;
    applyConfig();
  });
  connect(m_modeH, &QPushButton::clicked, this, [this]() {
    if (m_mode == ToolMode::Shape)
      m_config.shapeToolKind = ShapeToolKind::CoordinateGraph;
    applyConfig();
  });
  m_root->addWidget(m_modeRow);

  // Smart Ink — pressure / ink→shape / smart line.
  m_smartLbl = new QLabel(QStringLiteral("Smart Ink"), this);
  m_root->addWidget(m_smartLbl);
  m_smartRow = new QWidget(this);
  auto *smartLay = new QVBoxLayout(m_smartRow);
  smartLay->setContentsMargins(0, 0, 0, 0);
  smartLay->setSpacing(UiScale::dp(4));
  m_chkPressure = new QCheckBox(QStringLiteral("Druckempfindlichkeit"), m_smartRow);
  m_chkInkToShape =
      new QCheckBox(QStringLiteral("Tinte → Form (halten)"), m_smartRow);
  m_chkSmartLine = new QCheckBox(QStringLiteral("Smart Line"), m_smartRow);
  for (QCheckBox *c : {m_chkPressure, m_chkInkToShape, m_chkSmartLine}) {
    c->setCursor(Qt::PointingHandCursor);
    smartLay->addWidget(c);
  }
  connect(m_chkPressure, &QCheckBox::toggled, this, [this](bool on) {
    m_config.pressureSensitivity = on;
    if (!on)
      m_config.smoothing = 0;
    applyConfig();
    refreshStyleTiles();
  });
  connect(m_chkInkToShape, &QCheckBox::toggled, this, [this](bool on) {
    m_config.holdEnableCircle = on;
    m_config.holdEnableTriangle = on;
    applyConfig();
  });
  connect(m_chkSmartLine, &QCheckBox::toggled, this, [this](bool on) {
    m_config.smartLine = on;
    applyConfig();
  });
  m_root->addWidget(m_smartRow);

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

int ToolPropertiesPanel::preferredWidth() const { return UiScale::dp(300); }

int ToolPropertiesPanel::preferredHeight() const {
  // Compact floating card — grows with content via layout, clamp for docking.
  return qMax(UiScale::dp(360), sizeHint().height() + UiScale::dp(8));
}

void ToolPropertiesPanel::setAccentColor(const QColor &c) {
  if (!c.isValid())
    return;
  m_accent = c;
  rebuild();
  refreshSwatchSelection();
  refreshFillSwatchSelection();
  refreshStyleTiles();
  update();
}

void ToolPropertiesPanel::setVisibleForTool(ToolMode mode) {
  m_mode = mode;
  const bool writing = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                        mode == ToolMode::Highlighter ||
                        mode == ToolMode::Eraser || mode == ToolMode::Shape ||
                        mode == ToolMode::Text || mode == ToolMode::Ruler ||
                        mode == ToolMode::StickyNote);
  m_widthSlider->setVisible(writing);
  m_widthLbl->setVisible(writing);
  if (mode == ToolMode::Text || mode == ToolMode::StickyNote) {
    m_widthLbl->setText(
        QStringLiteral("Schriftgröße  %1").arg(m_config.penWidth));
    m_widthSlider->setRange(8, 72);
  } else {
    m_widthSlider->setRange(1, 50);
    m_widthLbl->setText(QStringLiteral("Dicke  %1").arg(m_config.penWidth));
  }
  const bool colorful = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                         mode == ToolMode::Highlighter ||
                         mode == ToolMode::Shape || mode == ToolMode::Text ||
                         mode == ToolMode::StickyNote);
  const bool opacityful = colorful || mode == ToolMode::Image;
  m_opacitySlider->setVisible(opacityful);
  m_opacityLbl->setVisible(opacityful);
  if (m_colorRow)
    m_colorRow->setVisible(colorful);

  const bool showStyle = (mode == ToolMode::Pen);
  if (m_styleLbl)
    m_styleLbl->setVisible(showStyle);
  if (m_styleRow)
    m_styleRow->setVisible(showStyle);

  const bool showFill = (mode == ToolMode::Shape);
  if (m_fillLbl)
    m_fillLbl->setVisible(showFill);
  if (m_fillRow)
    m_fillRow->setVisible(showFill);

  const bool modeTools = (mode == ToolMode::Lasso || mode == ToolMode::Shape ||
                          mode == ToolMode::Eraser ||
                          mode == ToolMode::Highlighter);
  if (m_modeLbl)
    m_modeLbl->setVisible(modeTools);
  if (m_modeRow)
    m_modeRow->setVisible(modeTools);

  const bool showSmart = (mode == ToolMode::Pen || mode == ToolMode::Pencil ||
                          mode == ToolMode::Highlighter ||
                          mode == ToolMode::Eraser);
  if (m_smartLbl)
    m_smartLbl->setVisible(showSmart);
  if (m_smartRow)
    m_smartRow->setVisible(showSmart);
  if (m_chkPressure)
    m_chkPressure->setVisible(mode == ToolMode::Pen || mode == ToolMode::Pencil);
  if (m_chkInkToShape)
    m_chkInkToShape->setVisible(mode == ToolMode::Pen ||
                                mode == ToolMode::Pencil ||
                                mode == ToolMode::Eraser);
  if (m_chkSmartLine)
    m_chkSmartLine->setVisible(mode == ToolMode::Highlighter);

  if (mode == ToolMode::Lasso) {
    m_modeA->setText(QStringLiteral("Freihand"));
    m_modeB->setText(QStringLiteral("Rechteck"));
    m_modeA->setChecked(m_config.lassoMode == LassoMode::Freehand);
    m_modeB->setChecked(m_config.lassoMode == LassoMode::Rectangle);
    m_modeA->setVisible(true);
    m_modeB->setVisible(true);
    for (QPushButton *b : {m_modeC, m_modeD, m_modeE, m_modeF, m_modeG, m_modeH})
      if (b)
        b->setVisible(false);
  } else if (mode == ToolMode::Eraser) {
    m_modeA->setText(QStringLiteral("Pixel"));
    m_modeB->setText(QStringLiteral("Objekt"));
    m_modeA->setChecked(m_config.eraserMode == EraserMode::Pixel);
    m_modeB->setChecked(m_config.eraserMode == EraserMode::Object);
    m_modeA->setVisible(true);
    m_modeB->setVisible(true);
    for (QPushButton *b : {m_modeC, m_modeD, m_modeE, m_modeF, m_modeG, m_modeH})
      if (b)
        b->setVisible(false);
  } else if (mode == ToolMode::Highlighter) {
    m_modeA->setText(QStringLiteral("Rund"));
    m_modeB->setText(QStringLiteral("Meißel"));
    m_modeA->setChecked(m_config.tipType == HighlighterTip::Round);
    m_modeB->setChecked(m_config.tipType == HighlighterTip::Chisel);
    m_modeA->setVisible(true);
    m_modeB->setVisible(true);
    for (QPushButton *b : {m_modeC, m_modeD, m_modeE, m_modeF, m_modeG, m_modeH})
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
        {m_modeC, "Ellipse", ShapeToolKind::Ellipse},
        {m_modeD, "Linie", ShapeToolKind::Line},
        {m_modeE, "Pfeil", ShapeToolKind::Arrow},
        {m_modeF, "Achsen", ShapeToolKind::Axes2D},
        {m_modeG, "Sinus", ShapeToolKind::SineGraph},
        {m_modeH, "Graph", ShapeToolKind::CoordinateGraph},
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
      hint = QStringLiteral(
          "Tippe auf die Seite, um ein Bild einzufügen. Deckkraft gilt für "
          "neue Einfügungen.");
      break;
    case ToolMode::StickyNote:
      hint = QStringLiteral(
          "Tippe auf die Seite für eine Haftnotiz. Farbe und Schriftgröße "
          "steuern das Aussehen.");
      break;
    case ToolMode::Text:
      hint = QStringLiteral(
          "Tippe auf die Seite, um Text zu setzen. Schriftgröße und Farbe "
          "gelten für neue Textfelder.");
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
          "Rechteck/Kreis/Ellipse/Linie/Pfeil aufziehen. Achsen, Sinus und "
          "Graph nutzen die Bounds. Füllfarbe optional.");
      break;
    case ToolMode::Pen:
      hint = QStringLiteral(
          "Einfach = gleichmäßig. Pro = Druck + Glättung. Kalligrafie = "
          "dynamische Strichstärke.");
      break;
    default:
      break;
    }
    m_hintLbl->setText(hint);
    m_hintLbl->setVisible(!hint.isEmpty());
  }
  refreshSwatchSelection();
  refreshFillSwatchSelection();
  refreshStyleTiles();
  refreshSmartInk();
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
  const qreal op = (mode == ToolMode::Image && m_config.imageOpacity > 0.01)
                       ? m_config.imageOpacity
                       : m_config.opacity;
  m_opacitySlider->setValue(qRound(op * 100.0));
  if (mode == ToolMode::Text || mode == ToolMode::StickyNote)
    m_widthLbl->setText(
        QStringLiteral("Schriftgröße  %1").arg(m_config.penWidth));
  else
    m_widthLbl->setText(QStringLiteral("Dicke  %1").arg(m_config.penWidth));
  m_opacityLbl->setText(
      QStringLiteral("Deckkraft  %1%").arg(qRound(op * 100.0)));
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
  refreshFillSwatchSelection();
}

ToolPropertiesPanel::PenInkStyle ToolPropertiesPanel::detectPenInkStyle() const {
  if (!m_config.pressureSensitivity)
    return PenInkStyle::Einfach;
  if (m_config.smoothing >= 30)
    return PenInkStyle::Pro;
  return PenInkStyle::Kalligrafie;
}

void ToolPropertiesPanel::applyPenInkStyle(PenInkStyle style) {
  switch (style) {
  case PenInkStyle::Einfach:
    m_config.pressureSensitivity = false;
    m_config.smoothing = 0;
    break;
  case PenInkStyle::Pro:
    m_config.pressureSensitivity = true;
    m_config.smoothing = 45;
    break;
  case PenInkStyle::Kalligrafie:
    m_config.pressureSensitivity = true;
    m_config.smoothing = 12;
    break;
  }
  applyConfig();
  refreshStyleTiles();
  refreshSmartInk();
}

void ToolPropertiesPanel::refreshStyleTiles() {
  const PenInkStyle cur = detectPenInkStyle();
  auto styleBtn = [this](QPushButton *btn, PenInkStyle s, bool selected) {
    if (!btn)
      return;
    Q_UNUSED(s);
    const QString border =
        selected ? m_accent.name() : NoteChrome::border().name();
    const QString bg =
        selected ? QStringLiteral("rgba(91,157,255,0.18)")
                 : QStringLiteral("rgba(255,255,255,0.04)");
    btn->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background: %1; border: 1px solid %2; border-radius: 10px;"
                       "  text-align: left; padding: 8px 10px;"
                       "  color: %3; font-size: 12px; font-weight: 700;"
                       "}"
                       "QPushButton:hover { background: rgba(255,255,255,0.08); }")
            .arg(bg, border, NoteChrome::textPrimary().name()));
    btn->setChecked(selected);
  };
  styleBtn(m_styleEinfach, PenInkStyle::Einfach, cur == PenInkStyle::Einfach);
  styleBtn(m_stylePro, PenInkStyle::Pro, cur == PenInkStyle::Pro);
  styleBtn(m_styleKalli, PenInkStyle::Kalligrafie, cur == PenInkStyle::Kalligrafie);
}

void ToolPropertiesPanel::refreshSmartInk() {
  auto blockSet = [](QCheckBox *box, bool on) {
    if (!box)
      return;
    box->blockSignals(true);
    box->setChecked(on);
    box->blockSignals(false);
  };
  blockSet(m_chkPressure, m_config.pressureSensitivity);
  blockSet(m_chkInkToShape,
           m_config.holdEnableCircle || m_config.holdEnableTriangle);
  blockSet(m_chkSmartLine, m_config.smartLine);
}

QPushButton *ToolPropertiesPanel::makeStyleTile(const QString &title,
                                                const QString &subtitle,
                                                PenInkStyle style) {
  auto *btn = new QPushButton(m_styleRow);
  btn->setCheckable(true);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setMinimumHeight(UiScale::dp(52));
  btn->setText(QStringLiteral("%1\n%2").arg(title, subtitle));
  connect(btn, &QPushButton::clicked, this,
          [this, style]() { applyPenInkStyle(style); });
  return btn;
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
    auto *btn = makeSwatch(colors[i], false);
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

void ToolPropertiesPanel::addFillColorRow(QVBoxLayout *lay) {
  auto *grid = new QGridLayout(m_fillRow);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setSpacing(UiScale::dp(6));
  // First swatch = none (transparent).
  const QList<QColor> colors = {
      QColor(0, 0, 0, 0), Qt::white, QColor(220, 50, 50), QColor(40, 120, 255),
      QColor(40, 180, 80), QColor(250, 180, 30), QColor(160, 60, 200),
      QColor(30, 30, 30, 80)};
  for (int i = 0; i < colors.size(); ++i) {
    auto *btn = makeSwatch(colors[i], true);
    if (i == 0)
      btn->setToolTip(QStringLiteral("Keine Füllung"));
    m_fillSwatches.append(btn);
    grid->addWidget(btn, i / 4, i % 4);
  }
  m_customFillBtn = new QPushButton(QStringLiteral("…"), m_fillRow);
  m_customFillBtn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  m_customFillBtn->setCursor(Qt::PointingHandCursor);
  m_customFillBtn->setToolTip(QStringLiteral("Eigene Füllfarbe…"));
  connect(m_customFillBtn, &QPushButton::clicked, this, [this]() {
    QColor c = m_config.fillColor.isValid() ? m_config.fillColor
                                            : QColor(200, 200, 200);
    if (!showColorPickerOverlay(window(), &c, QStringLiteral("Füllfarbe")))
      return;
    m_config.fillColor = c;
    applyConfig();
  });
  grid->addWidget(m_customFillBtn, 2, 0);
  lay->addWidget(m_fillRow);
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

void ToolPropertiesPanel::refreshFillSwatchSelection() {
  const QColor fill = m_config.fillColor;
  const bool none = !fill.isValid() || fill.alpha() == 0;
  for (int i = 0; i < m_fillSwatches.size(); ++i) {
    QPushButton *btn = m_fillSwatches[i];
    if (!btn)
      continue;
    const QColor c = btn->property("swatchColor").value<QColor>();
    const bool isNone = (c.alpha() == 0);
    const bool selected =
        isNone ? none : (!none && c.rgb() == fill.rgb() && c.alpha() == fill.alpha());
    QString bg;
    if (isNone) {
      bg = QStringLiteral(
          "qlineargradient(x1:0,y1:0,x2:1,y2:1,"
          "stop:0 #444, stop:0.45 #444, stop:0.5 transparent, stop:1 transparent)");
    } else if (c.alpha() < 255) {
      bg = QStringLiteral("rgba(%1,%2,%3,%4)")
               .arg(c.red())
               .arg(c.green())
               .arg(c.blue())
               .arg(c.alpha() / 255.0);
    } else {
      bg = c.name();
    }
    const QString border =
        selected ? m_accent.name()
                 : ((c == Qt::white) ? QStringLiteral("#888")
                                     : NoteChrome::border().name());
    const int bw = selected ? 2 : 1;
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: %2px solid %3; border-radius: 6px; }"
        "QPushButton:hover { border: 2px solid %4; }")
                           .arg(bg)
                           .arg(bw)
                           .arg(border, m_accent.name()));
  }
  if (m_customFillBtn) {
    m_customFillBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; font-weight: 700; }"
        "QPushButton:hover { border-color: %4; }")
                                       .arg(NoteChrome::panelElevated().name(),
                                            NoteChrome::textPrimary().name(),
                                            NoteChrome::border().name(),
                                            m_accent.name()));
  }
}

QPushButton *ToolPropertiesPanel::makeSwatch(const QColor &c, bool fill) {
  QWidget *parent = fill ? m_fillRow : m_colorRow;
  auto *btn = new QPushButton(parent);
  btn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setProperty("swatchColor", c);
  connect(btn, &QPushButton::clicked, this, [this, c, fill]() {
    if (fill) {
      m_config.fillColor = c;
    } else {
      m_config.penColor = c;
    }
    applyConfig();
    update();
  });
  return btn;
}

void ToolPropertiesPanel::rebuild() {
  const int radius = UiScale::dp(14);
  const QString qss = QStringLiteral(
      "QWidget#ToolPropertiesPanel {"
      "  background: %1;"
      "  border: 1px solid %2;"
      "  border-radius: %6px;"
      "}"
      "QLabel { color: %3; background: transparent; font-size: 12px; font-weight: 600; }"
      "QCheckBox { color: %3; background: transparent; font-size: 12px; font-weight: 600; spacing: 8px; }"
      "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px;"
      "  border: 1px solid %2; background: rgba(255,255,255,0.04); }"
      "QCheckBox::indicator:checked { background: %4; border-color: %4; }"
      "QPushButton { color: %3; background: rgba(255,255,255,0.04);"
      "  border: 1px solid %2; border-radius: 8px; font-size: 12px; font-weight: 650;"
      "  padding: 6px 8px; }"
      "QPushButton:checked { background: rgba(91,157,255,0.20); border-color: %4; color: %5; }"
      "QPushButton:hover { background: rgba(255,255,255,0.08); }"
      "QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }"
      "QSlider::sub-page:horizontal { background: %4; border-radius: 2px; }"
      "QSlider::handle:horizontal { background: %5; width: 14px; height: 14px; "
      "margin: -5px 0; border-radius: 7px; }")
                          .arg(NoteChrome::panelElevated().name(),
                               NoteChrome::border().name(),
                               NoteChrome::textSecondary().name(),
                               NoteChrome::accent().name(),
                               NoteChrome::textPrimary().name())
                          .arg(radius);
  setStyleSheet(qss);
  refreshSwatchSelection();
  refreshFillSwatchSelection();
  refreshStyleTiles();
}

void ToolPropertiesPanel::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  const qreal radius = UiScale::dp(14);
  // Soft drop shadow under the floating options card.
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 55));
  p.drawRoundedRect(r.translated(0, 2), radius, radius);
  QLinearGradient grad(r.topLeft(), r.bottomLeft());
  grad.setColorAt(0, NoteChrome::panelElevated());
  grad.setColorAt(1, NoteChrome::panelBg());
  p.setBrush(grad);
  p.setPen(QPen(NoteChrome::borderSoft(), 1));
  p.drawRoundedRect(r, radius, radius);
}
