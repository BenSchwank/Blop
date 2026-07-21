#include "toolpropertiespanel.h"

#include "editoroverlays.h"
#include "notechrome.h"
#include "tools/ToolManager.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QVBoxLayout>

ToolPropertiesPanel::ToolPropertiesPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ToolPropertiesPanel"));
  setAttribute(Qt::WA_StyledBackground, true);
  // Opaque child card — translucent + failed RHI made the panel invisible on
  // software-GL Cloud Desktop. paintEvent still draws the rounded chrome.
  setAttribute(Qt::WA_TranslucentBackground, false);
  setMinimumWidth(preferredWidth());
  setMaximumWidth(preferredWidth());
  // Allow the host to clamp height; scrolling prevents widget overlap.
  setMinimumHeight(UiScale::dp(180));
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

  m_outer = new QVBoxLayout(this);
  m_outer->setContentsMargins(UiScale::dp(14), UiScale::dp(14), UiScale::dp(14),
                              UiScale::dp(14));
  m_outer->setSpacing(UiScale::dp(10));

  auto *header = new QHBoxLayout;
  header->setSpacing(UiScale::dp(8));
  m_title = new QLabel(QStringLiteral("Eigenschaften"), this);
  m_title->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 700;"));
  header->addWidget(m_title, 1);
  auto *closeBtn = new QPushButton(QStringLiteral("×"), this);
  closeBtn->setObjectName(QStringLiteral("ToolPropsClose"));
  closeBtn->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setFlat(true);
  connect(closeBtn, &QPushButton::clicked, this,
          &ToolPropertiesPanel::closeRequested);
  header->addWidget(closeBtn);
  m_outer->addLayout(header);

  m_scroll = new QScrollArea(this);
  m_scroll->setObjectName(QStringLiteral("ToolPropsScroll"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scroll->viewport()->setAutoFillBackground(false);

  m_body = new QWidget(m_scroll);
  m_body->setObjectName(QStringLiteral("ToolPropsBody"));
  m_root = new QVBoxLayout(m_body);
  m_root->setContentsMargins(0, 0, UiScale::dp(6), UiScale::dp(8));
  m_root->setSpacing(UiScale::dp(14));

  // Compact single-line style chips (subtitle as tooltip) — avoids tall
  // stacked tiles that overflow the floating card and overlap other controls.
  m_styleLbl = new QLabel(QStringLiteral("Stil"), m_body);
  m_root->addWidget(m_styleLbl);
  m_styleRow = new QWidget(m_body);
  auto *styleLay = new QHBoxLayout(m_styleRow);
  styleLay->setContentsMargins(0, 0, 0, 0);
  styleLay->setSpacing(UiScale::dp(8));
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

  m_widthLbl = new QLabel(QStringLiteral("Dicke"), m_body);
  m_root->addWidget(m_widthLbl);
  m_widthSlider = new QSlider(Qt::Horizontal, m_body);
  m_widthSlider->setRange(1, 50);
  m_widthSlider->setMinimumHeight(UiScale::dp(24));
  connect(m_widthSlider, &QSlider::valueChanged, this, [this](int v) {
    m_config.penWidth = v;
    if (m_mode == ToolMode::Text || m_mode == ToolMode::StickyNote)
      m_widthLbl->setText(QStringLiteral("Schriftgröße  %1").arg(v));
    else
      m_widthLbl->setText(QStringLiteral("Dicke  %1").arg(v));
    applyConfig();
  });
  m_root->addWidget(m_widthSlider);

  m_opacityLbl = new QLabel(QStringLiteral("Deckkraft"), m_body);
  m_root->addWidget(m_opacityLbl);
  m_opacitySlider = new QSlider(Qt::Horizontal, m_body);
  m_opacitySlider->setRange(10, 100);
  m_opacitySlider->setMinimumHeight(UiScale::dp(24));
  connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
    m_config.opacity = v / 100.0;
    if (m_mode == ToolMode::Image)
      m_config.imageOpacity = m_config.opacity;
    m_opacityLbl->setText(QStringLiteral("Deckkraft  %1%").arg(v));
    applyConfig();
  });
  m_root->addWidget(m_opacitySlider);

  m_colorLbl = new QLabel(QStringLiteral("Farbe"), m_body);
  m_root->addWidget(m_colorLbl);
  m_colorRow = new QWidget(m_body);
  addColorRow(m_root);

  m_fontLbl = new QLabel(QStringLiteral("Schrift"), m_body);
  m_root->addWidget(m_fontLbl);
  m_fontRow = new QWidget(m_body);
  auto *fontLay = new QHBoxLayout(m_fontRow);
  fontLay->setContentsMargins(0, 0, 0, 0);
  fontLay->setSpacing(UiScale::dp(8));
  auto makeFontChip = [this, fontLay](const QString &label,
                                      const QString &familyKey) {
    auto *btn = new QPushButton(label, m_fontRow);
    btn->setObjectName(QStringLiteral("ToolPropsMode"));
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(UiScale::dp(34));
    btn->setProperty("fontFamilyKey", familyKey);
    connect(btn, &QPushButton::clicked, this, [this, familyKey]() {
      m_config.fontFamily = familyKey;
      applyConfig();
      if (m_fontRow) {
        for (auto *ch : m_fontRow->findChildren<QPushButton *>()) {
          if (ch)
            ch->setChecked(ch->property("fontFamilyKey").toString() ==
                           m_config.fontFamily);
        }
      }
    });
    fontLay->addWidget(btn, 1);
    return btn;
  };
  makeFontChip(QStringLiteral("System"), QString());
  makeFontChip(QStringLiteral("Serif"), QStringLiteral("Serif"));
  makeFontChip(QStringLiteral("Mono"), QStringLiteral("Mono"));
  makeFontChip(QStringLiteral("Round"), QStringLiteral("Round"));
  m_root->addWidget(m_fontRow);

  m_fillLbl = new QLabel(QStringLiteral("Füllung"), m_body);
  m_root->addWidget(m_fillLbl);
  m_fillRow = new QWidget(m_body);
  addFillColorRow(m_root);

  // Mode row for Lasso / Shape / Eraser / Highlighter.
  m_modeLbl = new QLabel(QStringLiteral("Modus"), m_body);
  m_root->addWidget(m_modeLbl);
  m_modeRow = new QWidget(m_body);
  auto *modeLay = new QGridLayout(m_modeRow);
  modeLay->setContentsMargins(0, 0, 0, 0);
  modeLay->setHorizontalSpacing(UiScale::dp(8));
  modeLay->setVerticalSpacing(UiScale::dp(8));
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
    b->setObjectName(QStringLiteral("ToolPropsMode"));
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(UiScale::dp(36));
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

  m_smartLbl = new QLabel(QStringLiteral("Smart Ink"), m_body);
  m_root->addWidget(m_smartLbl);
  m_smartRow = new QWidget(m_body);
  auto *smartLay = new QVBoxLayout(m_smartRow);
  smartLay->setContentsMargins(0, 0, 0, 0);
  smartLay->setSpacing(UiScale::dp(12));
  m_chkPressure =
      new QCheckBox(QStringLiteral("Druckempfindlichkeit"), m_smartRow);
  m_chkInkToShape =
      new QCheckBox(QStringLiteral("Tinte → Form (halten)"), m_smartRow);
  m_chkSmartLine = new QCheckBox(QStringLiteral("Smart Line"), m_smartRow);
  for (QCheckBox *c : {m_chkPressure, m_chkInkToShape, m_chkSmartLine}) {
    c->setCursor(Qt::PointingHandCursor);
    c->setMinimumHeight(UiScale::dp(30));
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

  m_hintLbl = new QLabel(m_body);
  m_hintLbl->setWordWrap(true);
  m_hintLbl->setStyleSheet(QStringLiteral("font-size: 11px; font-weight: 500;"));
  m_root->addWidget(m_hintLbl);
  m_root->addStretch(0);

  m_scroll->setWidget(m_body);
  m_outer->addWidget(m_scroll, 1);

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

int ToolPropertiesPanel::preferredWidth() const { return UiScale::dp(340); }

int ToolPropertiesPanel::preferredHeight() const {
  // Prefer natural content height, but keep a sensible floating-card size.
  const int header = UiScale::dp(52);
  const int body = m_body ? m_body->sizeHint().height() : UiScale::dp(360);
  return qBound(UiScale::dp(280), header + body + UiScale::dp(28),
               UiScale::dp(720));
}

void ToolPropertiesPanel::updateScrollMetrics() {
  if (!m_scroll || !m_body)
    return;
  m_body->adjustSize();
  const bool needScroll =
      m_body->sizeHint().height() > m_scroll->viewport()->height() + 2;
  m_scroll->setVerticalScrollBarPolicy(needScroll ? Qt::ScrollBarAsNeeded
                                                 : Qt::ScrollBarAlwaysOff);
}

void ToolPropertiesPanel::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateScrollMetrics();
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
  if (m_colorLbl)
    m_colorLbl->setVisible(colorful);
  if (m_colorRow)
    m_colorRow->setVisible(colorful);
  if (m_colorLbl && colorful) {
    m_colorLbl->setText(mode == ToolMode::StickyNote
                            ? QStringLiteral("Hintergrund")
                            : QStringLiteral("Farbe"));
  }

  const bool showFont = (mode == ToolMode::Text);
  if (m_fontLbl)
    m_fontLbl->setVisible(showFont);
  if (m_fontRow) {
    m_fontRow->setVisible(showFont);
    if (showFont) {
      for (auto *ch : m_fontRow->findChildren<QPushButton *>()) {
        if (ch)
          ch->setChecked(ch->property("fontFamilyKey").toString() ==
                         m_config.fontFamily);
      }
    }
  }

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

  auto hideExtraModes = [this]() {
    for (QPushButton *b : {m_modeC, m_modeD, m_modeE, m_modeF, m_modeG, m_modeH})
      if (b)
        b->setVisible(false);
  };

  if (mode == ToolMode::Lasso) {
    m_modeA->setText(QStringLiteral("Freihand"));
    m_modeB->setText(QStringLiteral("Rechteck"));
    m_modeA->setChecked(m_config.lassoMode == LassoMode::Freehand);
    m_modeB->setChecked(m_config.lassoMode == LassoMode::Rectangle);
    m_modeA->setVisible(true);
    m_modeB->setVisible(true);
    hideExtraModes();
  } else if (mode == ToolMode::Eraser) {
    m_modeA->setText(QStringLiteral("Pixel"));
    m_modeB->setText(QStringLiteral("Objekt"));
    m_modeA->setChecked(m_config.eraserMode == EraserMode::Pixel);
    m_modeB->setChecked(m_config.eraserMode == EraserMode::Object);
    m_modeA->setVisible(true);
    m_modeB->setVisible(true);
    hideExtraModes();
  } else if (mode == ToolMode::Highlighter) {
    m_modeA->setText(QStringLiteral("Rund"));
    m_modeB->setText(QStringLiteral("Meißel"));
    m_modeA->setChecked(m_config.tipType == HighlighterTip::Round);
    m_modeB->setChecked(m_config.tipType == HighlighterTip::Chisel);
    m_modeA->setVisible(true);
    m_modeB->setVisible(true);
    hideExtraModes();
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
          "Tippe auf die Seite für eine Haftnotiz. Hintergrundfarbe und "
          "Schriftgröße steuern das Aussehen.");
      break;
    case ToolMode::Text:
      hint = QStringLiteral(
          "Tippe auf die Seite, um Text zu setzen. Schrift, Größe und Farbe "
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
  updateScrollMetrics();
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
  updateScrollMetrics();
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
  auto styleBtn = [this](QPushButton *btn, bool selected) {
    if (!btn)
      return;
    const QString border =
        selected ? m_accent.name() : NoteChrome::border().name();
    const QString bg =
        selected ? QStringLiteral("rgba(91,157,255,0.18)")
                 : QStringLiteral("rgba(255,255,255,0.04)");
    btn->setStyleSheet(
        QStringLiteral("QPushButton#ToolPropsStyle {"
                       "  background: %1; border: 1px solid %2; border-radius: 10px;"
                       "  text-align: center; padding: 10px 8px;"
                       "  color: %3; font-size: 12px; font-weight: 700;"
                       "}"
                       "QPushButton#ToolPropsStyle:hover { background: rgba(255,255,255,0.08); }")
            .arg(bg, border, NoteChrome::textPrimary().name()));
    btn->setChecked(selected);
  };
  styleBtn(m_styleEinfach, cur == PenInkStyle::Einfach);
  styleBtn(m_stylePro, cur == PenInkStyle::Pro);
  styleBtn(m_styleKalli, cur == PenInkStyle::Kalligrafie);
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
  btn->setObjectName(QStringLiteral("ToolPropsStyle"));
  btn->setCheckable(true);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setMinimumHeight(UiScale::dp(40));
  btn->setText(title);
  btn->setToolTip(subtitle);
  connect(btn, &QPushButton::clicked, this,
          [this, style]() { applyPenInkStyle(style); });
  return btn;
}

void ToolPropertiesPanel::addColorRow(QVBoxLayout *lay) {
  auto *grid = new QGridLayout(m_colorRow);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(UiScale::dp(10));
  grid->setVerticalSpacing(UiScale::dp(10));
  const QList<QColor> colors = {
      Qt::black, Qt::white, QColor(220, 50, 50), QColor(40, 120, 255),
      QColor(40, 180, 80), QColor(250, 180, 30), QColor(160, 60, 200),
      QColor(30, 30, 30)};
  for (int i = 0; i < colors.size(); ++i) {
    auto *btn = makeSwatch(colors[i], false);
    m_swatches.append(btn);
    grid->addWidget(btn, i / 4, i % 4, Qt::AlignLeft);
  }
  m_customColorBtn = new QPushButton(QStringLiteral("…"), m_colorRow);
  m_customColorBtn->setObjectName(QStringLiteral("ToolPropsSwatch"));
  m_customColorBtn->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  m_customColorBtn->setCursor(Qt::PointingHandCursor);
  m_customColorBtn->setToolTip(QStringLiteral("Eigene Farbe…"));
  connect(m_customColorBtn, &QPushButton::clicked, this, [this]() {
    QColor c = (m_mode == ToolMode::StickyNote && m_config.stickyBgColor.isValid())
                   ? m_config.stickyBgColor
                   : m_config.penColor;
    if (!showColorPickerOverlay(window(), &c,
                                m_mode == ToolMode::StickyNote
                                    ? QStringLiteral("Hintergrund")
                                    : QStringLiteral("Stiftfarbe")))
      return;
    if (m_mode == ToolMode::StickyNote) {
      m_config.stickyBgColor = c;
      m_config.penColor = c;
    } else {
      m_config.penColor = c;
    }
    applyConfig();
  });
  grid->addWidget(m_customColorBtn, 2, 0, Qt::AlignLeft);
  lay->addWidget(m_colorRow);
}

void ToolPropertiesPanel::addFillColorRow(QVBoxLayout *lay) {
  auto *grid = new QGridLayout(m_fillRow);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(UiScale::dp(10));
  grid->setVerticalSpacing(UiScale::dp(10));
  const QList<QColor> colors = {
      QColor(0, 0, 0, 0), Qt::white, QColor(220, 50, 50), QColor(40, 120, 255),
      QColor(40, 180, 80), QColor(250, 180, 30), QColor(160, 60, 200),
      QColor(30, 30, 30, 80)};
  for (int i = 0; i < colors.size(); ++i) {
    auto *btn = makeSwatch(colors[i], true);
    if (i == 0)
      btn->setToolTip(QStringLiteral("Keine Füllung"));
    m_fillSwatches.append(btn);
    grid->addWidget(btn, i / 4, i % 4, Qt::AlignLeft);
  }
  m_customFillBtn = new QPushButton(QStringLiteral("…"), m_fillRow);
  m_customFillBtn->setObjectName(QStringLiteral("ToolPropsSwatch"));
  m_customFillBtn->setFixedSize(UiScale::dp(40), UiScale::dp(40));
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
  grid->addWidget(m_customFillBtn, 2, 0, Qt::AlignLeft);
  lay->addWidget(m_fillRow);
}

void ToolPropertiesPanel::refreshSwatchSelection() {
  const QColor activeColor = (m_mode == ToolMode::StickyNote &&
                              m_config.stickyBgColor.isValid())
                                 ? m_config.stickyBgColor
                                 : m_config.penColor;
  for (QPushButton *btn : m_swatches) {
    if (!btn)
      continue;
    const QColor c = btn->property("swatchColor").value<QColor>();
    const bool selected = c.isValid() && c == activeColor;
    const QString border =
        selected ? m_accent.name()
                 : ((c == Qt::white) ? QStringLiteral("#888")
                                     : c.darker(120).name());
    const int bw = selected ? 2 : 1;
    btn->setStyleSheet(QStringLiteral(
        "QPushButton#ToolPropsSwatch { background: %1; border: %2px solid %3;"
        " border-radius: 8px; padding: 0; margin: 0; }"
        "QPushButton#ToolPropsSwatch:hover { border: 2px solid %4; }")
                           .arg(c.name())
                           .arg(bw)
                           .arg(border, m_accent.name()));
  }
  if (m_customColorBtn) {
    m_customColorBtn->setStyleSheet(QStringLiteral(
        "QPushButton#ToolPropsSwatch { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 8px; font-weight: 700; padding: 0; }"
        "QPushButton#ToolPropsSwatch:hover { border-color: %4; }")
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
        isNone ? none
               : (!none && c.rgb() == fill.rgb() && c.alpha() == fill.alpha());
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
        "QPushButton#ToolPropsSwatch { background: %1; border: %2px solid %3;"
        " border-radius: 8px; padding: 0; margin: 0; }"
        "QPushButton#ToolPropsSwatch:hover { border: 2px solid %4; }")
                           .arg(bg)
                           .arg(bw)
                           .arg(border, m_accent.name()));
  }
  if (m_customFillBtn) {
    m_customFillBtn->setStyleSheet(QStringLiteral(
        "QPushButton#ToolPropsSwatch { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 8px; font-weight: 700; padding: 0; }"
        "QPushButton#ToolPropsSwatch:hover { border-color: %4; }")
                                       .arg(NoteChrome::panelElevated().name(),
                                            NoteChrome::textPrimary().name(),
                                            NoteChrome::border().name(),
                                            m_accent.name()));
  }
}

QPushButton *ToolPropertiesPanel::makeSwatch(const QColor &c, bool fill) {
  QWidget *parent = fill ? m_fillRow : m_colorRow;
  auto *btn = new QPushButton(parent);
  btn->setObjectName(QStringLiteral("ToolPropsSwatch"));
  btn->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btn->setCursor(Qt::PointingHandCursor);
  btn->setProperty("swatchColor", c);
  connect(btn, &QPushButton::clicked, this, [this, c, fill]() {
    if (fill) {
      m_config.fillColor = c;
    } else if (m_mode == ToolMode::StickyNote) {
      m_config.stickyBgColor = c;
      m_config.penColor = c;
    } else {
      m_config.penColor = c;
    }
    applyConfig();
    update();
  });
  return btn;
}

void ToolPropertiesPanel::rebuild() {
  const int radius = UiScale::dp(16);
  const QString qss = QStringLiteral(
      "QWidget#ToolPropertiesPanel {"
      "  background: %1;"
      "  border: 1px solid %2;"
      "  border-radius: %6px;"
      "}"
      "QWidget#ToolPropsBody, QScrollArea#ToolPropsScroll,"
      " QScrollArea#ToolPropsScroll > QWidget > QWidget {"
      "  background: transparent;"
      "}"
      "QLabel { color: %3; background: transparent; font-size: 13px; font-weight: 600; }"
      "QCheckBox { color: %3; background: transparent; font-size: 13px; font-weight: 600; spacing: 10px; }"
      "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px;"
      "  border: 1px solid %2; background: rgba(255,255,255,0.04); }"
      "QCheckBox::indicator:checked { background: %4; border-color: %4; }"
      "QPushButton#ToolPropsMode {"
      "  color: %3; background: rgba(255,255,255,0.04);"
      "  border: 1px solid %2; border-radius: 10px; font-size: 12px; font-weight: 650;"
      "  padding: 8px 10px; }"
      "QPushButton#ToolPropsMode:checked {"
      "  background: rgba(91,157,255,0.20); border-color: %4; color: %5; }"
      "QPushButton#ToolPropsMode:hover { background: rgba(255,255,255,0.08); }"
      "QPushButton#ToolPropsClose {"
      "  color: %5; background: transparent; border: none; font-size: 18px; }"
      "QSlider::groove:horizontal { height: 6px; background: %2; border-radius: 3px; }"
      "QSlider::sub-page:horizontal { background: %4; border-radius: 3px; }"
      "QSlider::handle:horizontal { background: %5; width: 16px; height: 16px; "
      "margin: -5px 0; border-radius: 8px; }"
      "QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }"
      "QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 24px; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
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
  const qreal radius = UiScale::dp(16);
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
