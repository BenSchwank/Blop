#include "tooloptionsstrip.h"

#include "blopstyle.h"
#include "notechrome.h"
#include "uiscale.h"
#include "wordtextframe.h"
#include "tools/ToolManager.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>

namespace {

QPushButton *chip(const QString &text, QWidget *parent) {
  auto *b = new QPushButton(text, parent);
  b->setFocusPolicy(Qt::NoFocus);
  b->setCursor(Qt::PointingHandCursor);
  b->setStyleSheet(BlopStyle::noteSegmentQss());
  return b;
}

QComboBox *combo(QWidget *parent) {
  auto *c = new QComboBox(parent);
  c->setFocusPolicy(Qt::ClickFocus);
  c->setMinimumHeight(UiScale::dp(32));
  const QColor ink = NoteChrome::textPrimary();
  const QColor bg = NoteChrome::panelElevated();
  c->setStyleSheet(QStringLiteral(
                       "QComboBox { color: %1; background: %2; border: 1px solid %3;"
                       " border-radius: 8px; padding: 2px 8px; min-width: 88px; }"
                       "QComboBox QAbstractItemView { color: %1; background: %2; }")
                       .arg(ink.name(QColor::HexRgb), bg.name(QColor::HexRgb),
                            NoteChrome::border().name(QColor::HexRgb)));
  return c;
}

QSlider *widthSlider(int min, int max, int value, QWidget *parent) {
  auto *s = new QSlider(Qt::Horizontal, parent);
  s->setFocusPolicy(Qt::NoFocus);
  s->setRange(min, max);
  s->setValue(value);
  s->setFixedWidth(UiScale::dp(120));
  return s;
}

void addSwatches(QHBoxLayout *lay, QWidget *parent, const QColor &current,
                 const std::function<void(const QColor &)> &pick) {
  const QList<QColor> colors = {
      current.isValid() ? current : QColor(Qt::black),
      QColor(Qt::black),
      QColor(Qt::white),
      QColor(0xE5, 0x3E, 0x3E),
      QColor(0x5B, 0x9D, 0xFF),
      QColor(0x3C, 0xB3, 0x71),
      QColor(0xFF, 0xD5, 0x4A)};
  for (const QColor &c : colors) {
    auto *b = new QPushButton(parent);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedSize(UiScale::dp(22), UiScale::dp(22));
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QStringLiteral(
                         "QPushButton { background: %1; border: 1px solid %2;"
                         " border-radius: 11px; }")
                         .arg(c.name(QColor::HexRgb),
                              NoteChrome::border().name(QColor::HexRgb)));
    QObject::connect(b, &QPushButton::clicked, parent, [pick, c]() { pick(c); });
    lay->addWidget(b);
  }
}

} // namespace

ToolOptionsStrip::ToolOptionsStrip(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ToolOptionsStrip"));
  setFixedHeight(UiScale::dp(48));
  auto *outer = new QHBoxLayout(this);
  outer->setContentsMargins(UiScale::dp(10), UiScale::dp(4), UiScale::dp(10),
                            UiScale::dp(4));
  outer->setSpacing(0);

  m_scroll = new QScrollArea(this);
  m_scroll->setObjectName(QStringLiteral("ToolOptionsScroll"));
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setWidgetResizable(true);
  m_scroll->setFocusPolicy(Qt::NoFocus);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  outer->addWidget(m_scroll);

  m_row = new QWidget(m_scroll);
  m_rowLay = new QHBoxLayout(m_row);
  m_rowLay->setContentsMargins(0, 0, 0, 0);
  m_rowLay->setSpacing(UiScale::dp(6));
  m_scroll->setWidget(m_row);

  connect(&ToolManager::instance(), &ToolManager::toolChanged, this,
          [this](AbstractTool *tool) {
            m_mode = tool ? tool->mode() : ToolMode::Pen;
            rebuild();
          });
  connect(&ToolManager::instance(), &ToolManager::configChanged, this,
          [this]() {
            if (!m_rebuilding)
              rebuild();
          });

  if (AbstractTool *tool = ToolManager::instance().activeTool())
    m_mode = tool->mode();
  refreshTheme();
  rebuild();
}

void ToolOptionsStrip::setEditingResolver(
    std::function<QGraphicsTextItem *()> resolver) {
  m_resolver = std::move(resolver);
}

void ToolOptionsStrip::refreshTheme() {
  setStyleSheet(QStringLiteral(
                    "QWidget#ToolOptionsStrip { background: %1;"
                    " border-bottom: 1px solid %2; }"
                    "QScrollArea#ToolOptionsScroll { background: transparent; border: none; }"
                    "QLabel { color: %3; }")
                    .arg(NoteChrome::toolbarFill().name(QColor::HexRgb),
                         NoteChrome::borderSoft().name(QColor::HexRgb),
                         NoteChrome::textSecondary().name(QColor::HexRgb)));
  if (m_scroll && m_scroll->viewport())
    m_scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
  rebuild();
}

QGraphicsTextItem *ToolOptionsStrip::editingItem() const {
  if (!m_resolver)
    return nullptr;
  QGraphicsTextItem *item = m_resolver();
  if (!item)
    return nullptr;
  if (item->data(0).toString() != QLatin1String("text_item"))
    return nullptr;
  if (!(item->textInteractionFlags() & Qt::TextEditorInteraction))
    return nullptr;
  return item;
}

void ToolOptionsStrip::publish(const ToolConfig &cfg) {
  const bool nested = m_rebuilding;
  m_rebuilding = true;
  ToolManager::instance().setConfig(cfg);
  if (!nested)
    m_rebuilding = false;
}

void ToolOptionsStrip::clearRow() {
  while (QLayoutItem *it = m_rowLay->takeAt(0)) {
    if (QWidget *w = it->widget())
      delete w;
    delete it;
  }
}

void ToolOptionsStrip::rebuild() {
  if (!m_rowLay)
    return;
  const bool nested = m_rebuilding;
  m_rebuilding = true;
  clearRow();
  const ToolConfig cfg = ToolManager::instance().configFor(m_mode);

  auto addHint = [this](const QString &text) {
    auto *lbl = new QLabel(text, m_row);
    m_rowLay->addWidget(lbl);
  };
  auto addWidth = [this, &cfg](const QString &label) {
    auto *lbl = new QLabel(label, m_row);
    m_rowLay->addWidget(lbl);
    auto *s = widthSlider(1, 48, qBound(1, cfg.penWidth, 48), m_row);
    auto *val = new QLabel(QString::number(s->value()), m_row);
    connect(s, &QSlider::valueChanged, this, [this, val](int v) {
      val->setText(QString::number(v));
      ToolConfig next = ToolManager::instance().config();
      next.penWidth = v;
      publish(next);
    });
    m_rowLay->addWidget(s);
    m_rowLay->addWidget(val);
  };

  switch (m_mode) {
  case ToolMode::Text: {
    auto *family = combo(m_row);
    const QStringList families = {QStringLiteral("Standard"),
                                  QStringLiteral("Serif"),
                                  QStringLiteral("Mono"),
                                  QStringLiteral("Segoe UI"),
                                  QStringLiteral("Georgia"),
                                  QStringLiteral("Consolas")};
    family->addItems(families);
    int famIdx = families.indexOf(cfg.fontFamily);
    if (famIdx < 0)
      famIdx = 0;
    family->setCurrentIndex(famIdx);
    connect(family, &QComboBox::currentTextChanged, this, [this](const QString &t) {
      const QString stored = (t == QLatin1String("Standard")) ? QString() : t;
      if (QGraphicsTextItem *item = editingItem())
        WordText::setFamily(item, stored.isEmpty() ? QStringLiteral("Segoe UI") : stored);
      ToolConfig next = ToolManager::instance().config();
      next.fontFamily = stored;
      publish(next);
    });
    m_rowLay->addWidget(family);

    auto *size = combo(m_row);
    const QList<int> sizes = {10, 12, 14, 16, 18, 20, 24, 28, 36, 48};
    for (int pt : sizes)
      size->addItem(QString::number(pt), pt);
    int sizeIdx = sizes.indexOf(qBound(10, cfg.penWidth, 48));
    if (sizeIdx < 0) {
      size->addItem(QString::number(cfg.penWidth), cfg.penWidth);
      sizeIdx = size->count() - 1;
    }
    size->setCurrentIndex(sizeIdx);
    connect(size, &QComboBox::currentIndexChanged, this, [this, size](int idx) {
      const int pt = size->itemData(idx).toInt();
      if (QGraphicsTextItem *item = editingItem())
        WordText::setPointSize(item, pt);
      ToolConfig next = ToolManager::instance().config();
      next.penWidth = pt;
      publish(next);
    });
    m_rowLay->addWidget(size);

    auto *bold = chip(QStringLiteral("F"), m_row);
    auto *italic = chip(QStringLiteral("K"), m_row);
    auto *under = chip(QStringLiteral("U"), m_row);
    bold->setCheckable(true);
    italic->setCheckable(true);
    under->setCheckable(true);
    bold->setChecked(cfg.textBold);
    italic->setChecked(cfg.textItalic);
    under->setChecked(cfg.textUnderline);
    bold->setToolTip(QStringLiteral("Fett (Strg+B)"));
    italic->setToolTip(QStringLiteral("Kursiv (Strg+I)"));
    under->setToolTip(QStringLiteral("Unterstrichen (Strg+U)"));
    connect(bold, &QPushButton::toggled, this, [this](bool on) {
      if (QGraphicsTextItem *item = editingItem())
        WordText::setBold(item, on);
      ToolConfig next = ToolManager::instance().config();
      next.textBold = on;
      publish(next);
    });
    connect(italic, &QPushButton::toggled, this, [this](bool on) {
      if (QGraphicsTextItem *item = editingItem())
        WordText::setItalic(item, on);
      ToolConfig next = ToolManager::instance().config();
      next.textItalic = on;
      publish(next);
    });
    connect(under, &QPushButton::toggled, this, [this](bool on) {
      if (QGraphicsTextItem *item = editingItem())
        WordText::setUnderline(item, on);
      ToolConfig next = ToolManager::instance().config();
      next.textUnderline = on;
      publish(next);
    });
    m_rowLay->addWidget(bold);
    m_rowLay->addWidget(italic);
    m_rowLay->addWidget(under);

    auto *bullets = chip(QStringLiteral("Liste"), m_row);
    auto *numbers = chip(QStringLiteral("1."), m_row);
    bullets->setCheckable(true);
    numbers->setCheckable(true);
    bullets->setChecked(cfg.textListKind == 1);
    numbers->setChecked(cfg.textListKind == 2);
    connect(bullets, &QPushButton::clicked, this, [this](bool on) {
      if (QGraphicsTextItem *item = editingItem())
        WordText::setListKind(item, on ? 1 : 0);
      ToolConfig next = ToolManager::instance().config();
      next.textListKind = on ? 1 : 0;
      publish(next);
    });
    connect(numbers, &QPushButton::clicked, this, [this](bool on) {
      if (QGraphicsTextItem *item = editingItem())
        WordText::setListKind(item, on ? 2 : 0);
      ToolConfig next = ToolManager::instance().config();
      next.textListKind = on ? 2 : 0;
      publish(next);
    });
    m_rowLay->addWidget(bullets);
    m_rowLay->addWidget(numbers);

    auto *left = chip(QStringLiteral("Links"), m_row);
    auto *center = chip(QStringLiteral("Mitte"), m_row);
    auto *right = chip(QStringLiteral("Rechts"), m_row);
    auto *alignGroup = new QButtonGroup(m_row);
    alignGroup->setExclusive(true);
    for (QPushButton *b : {left, center, right}) {
      b->setCheckable(true);
      alignGroup->addButton(b);
    }
    if (cfg.textAlign == 1)
      center->setChecked(true);
    else if (cfg.textAlign == 2)
      right->setChecked(true);
    else
      left->setChecked(true);
    auto applyAlign = [this](int which) {
      const Qt::Alignment al = which == 1   ? Qt::AlignHCenter
                               : which == 2 ? Qt::AlignRight
                                            : Qt::AlignLeft;
      if (QGraphicsTextItem *item = editingItem())
        WordText::setAlign(item, al);
      ToolConfig next = ToolManager::instance().config();
      next.textAlign = which;
      publish(next);
    };
    connect(left, &QPushButton::clicked, this, [applyAlign]() { applyAlign(0); });
    connect(center, &QPushButton::clicked, this, [applyAlign]() { applyAlign(1); });
    connect(right, &QPushButton::clicked, this, [applyAlign]() { applyAlign(2); });
    m_rowLay->addWidget(left);
    m_rowLay->addWidget(center);
    m_rowLay->addWidget(right);

    addSwatches(m_rowLay, m_row, cfg.penColor, [this](const QColor &c) {
      if (QGraphicsTextItem *item = editingItem())
        WordText::setTextColor(item, c);
      ToolConfig next = ToolManager::instance().config();
      next.penColor = c;
      publish(next);
    });
    break;
  }
  case ToolMode::Pen: {
    addSwatches(m_rowLay, m_row, cfg.penColor, [this](const QColor &c) {
      ToolConfig next = ToolManager::instance().config();
      next.penColor = c;
      publish(next);
    });
    addWidth(QStringLiteral("Dicke"));
    auto *pressure = new QCheckBox(QStringLiteral("Druck"), m_row);
    pressure->setFocusPolicy(Qt::NoFocus);
    pressure->setChecked(cfg.pressureSensitivity);
    connect(pressure, &QCheckBox::toggled, this, [this](bool on) {
      ToolConfig next = ToolManager::instance().config();
      next.pressureSensitivity = on;
      publish(next);
    });
    m_rowLay->addWidget(pressure);
    break;
  }
  case ToolMode::Pencil: {
    addSwatches(m_rowLay, m_row, cfg.penColor, [this](const QColor &c) {
      ToolConfig next = ToolManager::instance().config();
      next.penColor = c;
      publish(next);
    });
    addWidth(QStringLiteral("Dicke"));
    auto *group = new QButtonGroup(m_row);
    group->setExclusive(true);
    for (const char *name : {"Fein", "Mittel", "Grob"}) {
      auto *b = chip(QString::fromUtf8(name), m_row);
      b->setCheckable(true);
      b->setChecked(cfg.texture == QLatin1String(name));
      group->addButton(b);
      connect(b, &QPushButton::clicked, this, [this, name]() {
        ToolConfig next = ToolManager::instance().config();
        next.texture = QString::fromUtf8(name);
        publish(next);
      });
      m_rowLay->addWidget(b);
    }
    auto *hardLbl = new QLabel(QStringLiteral("Härte"), m_row);
    auto *hard = widthSlider(0, 100, qBound(0, cfg.hardness, 100), m_row);
    connect(hard, &QSlider::valueChanged, this, [this](int v) {
      ToolConfig next = ToolManager::instance().config();
      next.hardness = v;
      publish(next);
    });
    m_rowLay->addWidget(hardLbl);
    m_rowLay->addWidget(hard);
    break;
  }
  case ToolMode::Highlighter: {
    addSwatches(m_rowLay, m_row, cfg.penColor, [this](const QColor &c) {
      ToolConfig next = ToolManager::instance().config();
      next.penColor = c;
      publish(next);
    });
    addWidth(QStringLiteral("Dicke"));
    auto *straight = new QCheckBox(QStringLiteral("Gerade"), m_row);
    straight->setFocusPolicy(Qt::NoFocus);
    straight->setChecked(cfg.smartLine);
    connect(straight, &QCheckBox::toggled, this, [this](bool on) {
      ToolConfig next = ToolManager::instance().config();
      next.smartLine = on;
      publish(next);
    });
    m_rowLay->addWidget(straight);
    break;
  }
  case ToolMode::Eraser: {
    auto *pixel = chip(QStringLiteral("Pixel"), m_row);
    auto *object = chip(QStringLiteral("Objekt"), m_row);
    auto *group = new QButtonGroup(m_row);
    group->setExclusive(true);
    pixel->setCheckable(true);
    object->setCheckable(true);
    group->addButton(pixel);
    group->addButton(object);
    pixel->setChecked(cfg.eraserMode == EraserMode::Pixel);
    object->setChecked(cfg.eraserMode == EraserMode::Object);
    connect(pixel, &QPushButton::clicked, this, [this]() {
      ToolConfig next = ToolManager::instance().config();
      next.eraserMode = EraserMode::Pixel;
      publish(next);
    });
    connect(object, &QPushButton::clicked, this, [this]() {
      ToolConfig next = ToolManager::instance().config();
      next.eraserMode = EraserMode::Object;
      publish(next);
    });
    m_rowLay->addWidget(pixel);
    m_rowLay->addWidget(object);
    addWidth(QStringLiteral("Breite"));
    auto *keep = new QCheckBox(QStringLiteral("Tinte behalten"), m_row);
    keep->setFocusPolicy(Qt::NoFocus);
    keep->setChecked(cfg.eraserKeepInk);
    connect(keep, &QCheckBox::toggled, this, [this](bool on) {
      ToolConfig next = ToolManager::instance().config();
      next.eraserKeepInk = on;
      publish(next);
    });
    m_rowLay->addWidget(keep);
    break;
  }
  case ToolMode::Lasso: {
    auto *free = chip(QStringLiteral("Freihand"), m_row);
    auto *rect = chip(QStringLiteral("Rechteck"), m_row);
    auto *group = new QButtonGroup(m_row);
    group->setExclusive(true);
    free->setCheckable(true);
    rect->setCheckable(true);
    group->addButton(free);
    group->addButton(rect);
    free->setChecked(cfg.lassoMode == LassoMode::Freehand);
    rect->setChecked(cfg.lassoMode == LassoMode::Rectangle);
    connect(free, &QPushButton::clicked, this, [this]() {
      ToolConfig next = ToolManager::instance().config();
      next.lassoMode = LassoMode::Freehand;
      publish(next);
    });
    connect(rect, &QPushButton::clicked, this, [this]() {
      ToolConfig next = ToolManager::instance().config();
      next.lassoMode = LassoMode::Rectangle;
      publish(next);
    });
    m_rowLay->addWidget(free);
    m_rowLay->addWidget(rect);
    break;
  }
  case ToolMode::Shape: {
    struct Kind {
      const char *label;
      ShapeToolKind kind;
    };
    const Kind kinds[] = {
        {"Rechteck", ShapeToolKind::Rectangle},
        {"Kreis", ShapeToolKind::Circle},
        {"Ellipse", ShapeToolKind::Ellipse},
        {"Linie", ShapeToolKind::Line},
        {"Pfeil", ShapeToolKind::Arrow},
    };
    auto *shapeGroup = new QButtonGroup(m_row);
    shapeGroup->setExclusive(true);
    for (const Kind &k : kinds) {
      auto *b = chip(QString::fromUtf8(k.label), m_row);
      b->setCheckable(true);
      b->setChecked(cfg.shapeToolKind == k.kind);
      shapeGroup->addButton(b);
      connect(b, &QPushButton::clicked, this, [this, kind = k.kind]() {
        ToolConfig next = ToolManager::instance().config();
        next.shapeToolKind = kind;
        publish(next);
      });
      m_rowLay->addWidget(b);
    }
    auto *ink = new QLabel(QStringLiteral("Strich"), m_row);
    m_rowLay->addWidget(ink);
    addSwatches(m_rowLay, m_row, cfg.penColor, [this](const QColor &c) {
      ToolConfig next = ToolManager::instance().config();
      next.penColor = c;
      publish(next);
    });
    addWidth(QStringLiteral("Dicke"));
    auto *fill = new QLabel(QStringLiteral("Füllung"), m_row);
    m_rowLay->addWidget(fill);
    addSwatches(m_rowLay, m_row,
                cfg.fillColor.isValid() ? cfg.fillColor : QColor(Qt::transparent),
                [this](const QColor &c) {
                  ToolConfig next = ToolManager::instance().config();
                  next.fillColor = c;
                  publish(next);
                });
    auto *none = chip(QStringLiteral("Keine"), m_row);
    connect(none, &QPushButton::clicked, this, [this]() {
      ToolConfig next = ToolManager::instance().config();
      next.fillColor = Qt::transparent;
      publish(next);
    });
    m_rowLay->addWidget(none);
    break;
  }
  case ToolMode::Ruler: {
    auto *snap = new QCheckBox(QStringLiteral("Einrasten"), m_row);
    snap->setFocusPolicy(Qt::NoFocus);
    snap->setChecked(cfg.rulerSnap);
    connect(snap, &QCheckBox::toggled, this, [this](bool on) {
      ToolConfig next = ToolManager::instance().config();
      next.rulerSnap = on;
      publish(next);
    });
    m_rowLay->addWidget(snap);
    break;
  }
  case ToolMode::Image: {
    auto *lbl = new QLabel(QStringLiteral("Deckkraft"), m_row);
    auto *s = widthSlider(10, 100, qBound(10, int(cfg.imageOpacity * 100), 100), m_row);
    connect(s, &QSlider::valueChanged, this, [this](int v) {
      ToolConfig next = ToolManager::instance().config();
      next.imageOpacity = v / 100.0;
      next.opacity = next.imageOpacity;
      publish(next);
    });
    m_rowLay->addWidget(lbl);
    m_rowLay->addWidget(s);
    break;
  }
  case ToolMode::StickyNote: {
    addSwatches(m_rowLay, m_row, cfg.stickyBgColor, [this](const QColor &c) {
      ToolConfig next = ToolManager::instance().config();
      next.stickyBgColor = c;
      next.penColor = c;
      publish(next);
    });
    addWidth(QStringLiteral("Schrift"));
    break;
  }
  case ToolMode::Hand:
    addHint(QStringLiteral("Ziehen verschiebt die Seite. Leertaste hält das Schwenken."));
    break;
  case ToolMode::Formula:
    addHint(QStringLiteral("Formel: die Zeichenfläche folgt später."));
    break;
  case ToolMode::Molecule:
    addHint(QStringLiteral("Molekül: die Zeichenfläche folgt später."));
    break;
  default:
    addHint(QStringLiteral("Keine weiteren Einstellungen."));
    break;
  }

  m_rowLay->addStretch(1);
  if (!nested)
    m_rebuilding = false;
}
