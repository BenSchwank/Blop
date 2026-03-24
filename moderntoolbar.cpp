#include "moderntoolbar.h"
#include "UIStyles.h"
#include "tools/ToolManager.h"
#include "tools/ToolUIBridge.h"
#include "tools/WritingTools.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegion>
#include <QSlider>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

// =============================================================================
// 1. ToolbarBtn Implementation
// =============================================================================
ToolbarBtn::ToolbarBtn(const QString &iconName, QWidget *parent)
    : QWidget(parent), m_iconName(iconName) {
  setBtnSize(40);
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_AcceptTouchEvents, true);
}
void ToolbarBtn::setBtnSize(int s) {
  if (m_size != s) {
    m_size = s;
    setFixedSize(s, s);
    update();
  }
}
void ToolbarBtn::setIcon(const QString &name) {
  m_iconName = name;
  update();
}
void ToolbarBtn::setDrawFloatingBg(bool draw) {
  m_drawFloatingBg = draw;
  update();
}
void ToolbarBtn::setActive(bool active) {
  m_active = active;
  update();
}
void ToolbarBtn::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton || e->button() == Qt::NoButton || e->source() != Qt::MouseEventNotSynthesized)
    emit clicked();
}
void ToolbarBtn::enterEvent(QEnterEvent *) {
  m_hover = true;
  update();
}
void ToolbarBtn::leaveEvent(QEvent *) {
  m_hover = false;
  update();
}
void ToolbarBtn::animateSelect() {
  QPropertyAnimation *anim = new QPropertyAnimation(this, "animScale");
  anim->setDuration(300);
  anim->setKeyValueAt(0, 1.0);
  anim->setKeyValueAt(0.5, 1.3);
  anim->setKeyValueAt(1, 1.0);
  anim->setEasingCurve(QEasingCurve::OutBack);
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}
void ToolbarBtn::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  
  if (m_drawFloatingBg) {
    p.setBrush(QColor(30, 28, 52, 245));
    p.setPen(QPen(QColor(124, 92, 252, 60), 1));
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));
  }

  int r = m_size / 2 - 4;
  if (m_active) {
    p.setBrush(UIStyles::Accent);
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect().center(), r, r);
  } else if (m_hover) {
    p.setBrush(QColor(255, 255, 255, 30));
    p.setPen(Qt::NoPen);
    p.drawEllipse(rect().center(), r, r);
  }
  p.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  int w = width();
  int h = height();
  p.translate(w / 2, h / 2);
  double scale = (double)w / 110.0;
  scale *= m_animScale;
  p.scale(scale, scale);
  p.translate(-32, -32);

  QString emoji = m_iconName;
  if (m_iconName == "pen") emoji = "✏️";
  else if (m_iconName == "pencil") emoji = "✒️";
  else if (m_iconName == "highlighter") emoji = "🖊";
  else if (m_iconName == "eraser") emoji = "⬜";
  else if (m_iconName == "lasso") emoji = "⬭";
  else if (m_iconName == "ruler") emoji = "📏";
  else if (m_iconName == "shape") emoji = "◻";
  else if (m_iconName == "stickynote") emoji = "🗒";
  else if (m_iconName == "text") emoji = "T";
  else if (m_iconName == "image") emoji = "🖼";
  else if (m_iconName == "hand") emoji = "✋";
  else if (m_iconName == "undo") emoji = "↩";
  else if (m_iconName == "redo") emoji = "↪";
  // Fallback for dock_toggle or other dynamically set emojis
  
  // Custom font size for the emoji
  p.restore(); // Undo the 110.0 scaling and translations done previously in the method because text draws differently
  
  // We need to redo the scale because we restored
  p.save();
  p.translate(w / 2, h / 2);
  p.scale(m_animScale, m_animScale);
  
  QFont f = p.font();
  f.setPixelSize(static_cast<int>(34 * (w / 40.0))); // Much bigger base size
  p.setFont(f);
  p.setPen(QPen(QColor(255, 255, 255, 220)));
  
  // Da Text aus der Baseline gerendert wird, nutzen wir Text flags zum genauen zentrieren
  p.drawText(QRectF(-w/2.0, -h/2.0, w, h), Qt::AlignCenter, emoji);
  p.restore();
}

// =============================================================================
// 2. Tool Preview Widget
// =============================================================================
class ToolPreviewWidget : public QWidget {
public:
  ToolPreviewWidget(QWidget *parent) : QWidget(parent) {
    setFixedHeight(50);
    setAttribute(Qt::WA_TranslucentBackground);
  }
  void updateConfig(ToolMode m, ToolConfig c) {
    m_mode = m;
    m_config = c;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect paperRect = rect().adjusted(5, 5, -5, -5);
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(paperRect, 6, 6);

    if (m_mode == ToolMode::Highlighter) {
      p.setPen(QPen(QColor(60, 60, 60)));
      p.setFont(QFont("Arial", 12, QFont::Bold));
      p.drawText(paperRect, Qt::AlignCenter, "Text Text");
      p.setCompositionMode(QPainter::CompositionMode_Multiply);
    }

    QColor c = m_config.penColor;
    double alpha = m_config.opacity;
    int penW = m_config.penWidth;
    QPen pen;
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    if (m_mode == ToolMode::Pencil) {
      double hardnessFactor = (m_config.hardness / 100.0) * 0.5;
      alpha = alpha * (1.0 - hardnessFactor);
      c.setAlphaF(std::max(0.05, alpha));
      int noiseDensity = 50;
      if (m_config.texture == "Fein")
        noiseDensity = 25;
      else if (m_config.texture == "Grob")
        noiseDensity = 65;
      else if (m_config.texture == "Kohle") {
        noiseDensity = 80;
        penW = penW * 1.2;
      }
      QPixmap noisePx = getNoiseTexture(c, noiseDensity);
      pen.setBrush(QBrush(noisePx));
      pen.setStyle(Qt::SolidLine);
      pen.setWidth(penW);
    } else if (m_mode == ToolMode::Highlighter) {
      alpha = 0.4 * alpha;
      c.setAlphaF(std::max(0.1, alpha));
      penW = std::max(10, penW * 3);
      if (m_config.tipType == HighlighterTip::Chisel) {
        pen.setCapStyle(Qt::FlatCap);
        pen.setJoinStyle(Qt::MiterJoin);
      }
      pen.setColor(c);
      pen.setStyle(Qt::SolidLine);
      pen.setWidth(penW);
    } else {
      c.setAlphaF(alpha);
      pen.setColor(c);
      pen.setStyle(Qt::SolidLine);
      pen.setWidth(penW);
    }
    p.setPen(pen);
    int w = width();
    int h = height();
    QPainterPath path;
    if (m_mode == ToolMode::Highlighter) {
      path.moveTo(20, h / 2);
      path.lineTo(w - 20, h / 2);
    } else {
      path.moveTo(15, h / 2);
      path.cubicTo(w / 3, 10, 2 * w / 3, h - 10, w - 15, h / 2);
    }
    p.drawPath(path);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  }

private:
  ToolMode m_mode{ToolMode::Pen};
  ToolConfig m_config;
};

// =============================================================================
// 3. Settings Popup
// =============================================================================
class ToolSettingsPopup : public QDialog {
public:
  ToolSettingsPopup(ToolMode mode, ToolConfig config, QWidget *parent)
      : QDialog(parent), m_mode(mode), m_config(config) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                   Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(
        "QLabel { color: #DDD; font-weight: bold; font-size: 11px; background: "
        "transparent; }"
        "QSlider::groove:horizontal { height: 4px; background: #444; "
        "border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #6c5ce7; border-radius: "
        "2px; }"
        "QSlider::handle:horizontal { background: #FFFFFF; width: 14px; "
        "height: 14px; margin: -5px 0; border-radius: 7px; }"
        "QCheckBox { color: #DDD; background: transparent; }"
        "QPushButton { background-color: #444; color: #BBB; border: none; "
        "border-radius: 4px; padding: 5px; }"
        "QPushButton:checked { background-color: #6c5ce7; color: white; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton#DangerBtn { background-color: #A00; color: white; }"
        "QPushButton#DangerBtn:hover { background-color: #C00; }");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(12);

    if (m_mode != ToolMode::Eraser && m_mode != ToolMode::Lasso) {
      m_previewWidget = new ToolPreviewWidget(this);
      m_previewWidget->updateConfig(m_mode, m_config);
      layout->addWidget(m_previewWidget);
    } else {
      m_previewWidget = nullptr;
    }

    if (m_mode == ToolMode::Pen || m_mode == ToolMode::Pencil ||
        m_mode == ToolMode::Highlighter) {
      layout->addWidget(new QLabel("Dicke:"));
      QSlider *slSize = new QSlider(Qt::Horizontal);
      slSize->setRange(1, 50);
      slSize->setValue(m_config.penWidth);
      connect(slSize, &QSlider::valueChanged, [this](int v) {
        m_config.penWidth = v;
        updatePreview();
        apply();
      });
      layout->addWidget(slSize);

      layout->addWidget(new QLabel("Deckkraft:"));
      QSlider *slOpacity = new QSlider(Qt::Horizontal);
      slOpacity->setRange(10, 100);
      slOpacity->setValue(m_config.opacity * 100);
      connect(slOpacity, &QSlider::valueChanged, [this](int v) {
        m_config.opacity = v / 100.0;
        updatePreview();
        apply();
      });
      layout->addWidget(slOpacity);

      if (m_mode == ToolMode::Pen) {
        layout->addWidget(new QLabel("Glättung:"));
        QSlider *slSmooth = new QSlider(Qt::Horizontal);
        slSmooth->setRange(0, 100);
        slSmooth->setValue(m_config.smoothing);
        connect(slSmooth, &QSlider::valueChanged, [this](int v) {
          m_config.smoothing = v;
          apply();
        });
        layout->addWidget(slSmooth);
        QCheckBox *chkPressure = new QCheckBox("Drucksensitivität");
        chkPressure->setChecked(m_config.pressureSensitivity);
        connect(chkPressure, &QCheckBox::toggled, [this](bool c) {
          m_config.pressureSensitivity = c;
          apply();
        });
        layout->addWidget(chkPressure);
      } else if (m_mode == ToolMode::Pencil) {
        layout->addWidget(new QLabel("Härte:"));
        QSlider *slHardness = new QSlider(Qt::Horizontal);
        slHardness->setRange(0, 100);
        slHardness->setValue(m_config.hardness);
        connect(slHardness, &QSlider::valueChanged, [this](int v) {
          m_config.hardness = v;
          updatePreview();
          apply();
        });
        layout->addWidget(slHardness);
        QCheckBox *chkTilt = new QCheckBox("Neigung");
        chkTilt->setChecked(m_config.tiltShading);
        connect(chkTilt, &QCheckBox::toggled, [this](bool c) {
          m_config.tiltShading = c;
          apply();
        });
        layout->addWidget(chkTilt);
        layout->addWidget(new QLabel("Textur:"));
        QHBoxLayout *texLayout = new QHBoxLayout;
        texLayout->setSpacing(5);
        QButtonGroup *texGroup = new QButtonGroup(this);
        QStringList textures = {"Fein", "Grob", "Kohle"};
        for (const QString &tex : textures) {
          QPushButton *btn = new QPushButton(tex);
          btn->setCheckable(true);
          btn->setFixedHeight(25);
          if (m_config.texture == tex)
            btn->setChecked(true);
          texGroup->addButton(btn);
          texLayout->addWidget(btn);
          connect(btn, &QPushButton::clicked, [this, tex]() {
            m_config.texture = tex;
            updatePreview();
            apply();
          });
        }
        layout->addLayout(texLayout);
      } else if (m_mode == ToolMode::Highlighter) {
        QCheckBox *chkSmart = new QCheckBox("Smart Line");
        chkSmart->setChecked(m_config.smartLine);
        connect(chkSmart, &QCheckBox::toggled, [this](bool c) {
          m_config.smartLine = c;
          apply();
        });
        layout->addWidget(chkSmart);
        QCheckBox *chkBehind = new QCheckBox("Hinter Text");
        chkBehind->setChecked(m_config.drawBehind);
        connect(chkBehind, &QCheckBox::toggled, [this](bool c) {
          m_config.drawBehind = c;
          apply();
        });
        layout->addWidget(chkBehind);
        layout->addWidget(new QLabel("Spitze:"));
        QHBoxLayout *tipLayout = new QHBoxLayout;
        tipLayout->setSpacing(5);
        QButtonGroup *tipGroup = new QButtonGroup(this);
        auto addTipBtn = [&](QString text, HighlighterTip type) {
          QPushButton *btn = new QPushButton(text);
          btn->setCheckable(true);
          btn->setFixedHeight(25);
          if (m_config.tipType == type)
            btn->setChecked(true);
          tipGroup->addButton(btn);
          tipLayout->addWidget(btn);
          connect(btn, &QPushButton::clicked, [this, type]() {
            m_config.tipType = type;
            updatePreview();
            apply();
          });
        };
        addTipBtn("Rund", HighlighterTip::Round);
        addTipBtn("Abgeschrägt", HighlighterTip::Chisel);
        layout->addLayout(tipLayout);
      }
      layout->addWidget(new QLabel("Farbe:"));
      QHBoxLayout *colors = new QHBoxLayout;
      colors->setSpacing(8);
      QList<QColor> presets = {Qt::black, Qt::white, Qt::red, Qt::blue,
                               QColor(0, 200, 0)};
      for (auto c : presets) {
        QPushButton *b = new QPushButton;
        b->setFixedSize(24, 24);
        b->setStyleSheet(QString("background-color: %1; border-radius: 12px; "
                                 "border: 1px solid #666;")
                             .arg(c.name()));
        connect(b, &QPushButton::clicked, [this, c]() {
          m_config.penColor = c;
          updatePreview();
          apply();
        });
        colors->addWidget(b);
      }
      QPushButton *btnWheel = new QPushButton("...");
      btnWheel->setFixedSize(24, 24);
      btnWheel->setStyleSheet("background-color: #333; color: white; "
                              "border-radius: 12px; border: 1px solid #666;");
      connect(btnWheel, &QPushButton::clicked, [this]() {
        QColor c = QColorDialog::getColor(m_config.penColor, this);
        if (c.isValid()) {
          m_config.penColor = c;
          updatePreview();
          apply();
        }
      });
      colors->addWidget(btnWheel);
      colors->addStretch();
      layout->addLayout(colors);
    } else if (m_mode == ToolMode::Eraser) {
      layout->addWidget(new QLabel("Modus:"));
      QHBoxLayout *modeLayout = new QHBoxLayout;
      modeLayout->setSpacing(5);
      QButtonGroup *modeGroup = new QButtonGroup(this);
      QPushButton *btnPixel = new QPushButton("Pixel");
      btnPixel->setCheckable(true);
      btnPixel->setFixedHeight(30);
      QPushButton *btnObject = new QPushButton("Objekt");
      btnObject->setCheckable(true);
      btnObject->setFixedHeight(30);
      if (m_config.eraserMode == EraserMode::Pixel)
        btnPixel->setChecked(true);
      else
        btnObject->setChecked(true);
      modeGroup->addButton(btnPixel);
      modeGroup->addButton(btnObject);
      modeLayout->addWidget(btnPixel);
      modeLayout->addWidget(btnObject);
      layout->addLayout(modeLayout);

      QWidget *sizeContainer = new QWidget;
      QVBoxLayout *sizeLayout = new QVBoxLayout(sizeContainer);
      sizeLayout->setContentsMargins(0, 0, 0, 0);
      sizeLayout->addWidget(new QLabel("Größe:"));
      QSlider *slEraserSize = new QSlider(Qt::Horizontal);
      slEraserSize->setRange(5, 100);
      slEraserSize->setValue(m_config.penWidth);
      sizeLayout->addWidget(slEraserSize);
      layout->addWidget(sizeContainer);
      auto updateVisibility = [=]() {
        sizeContainer->setVisible(m_config.eraserMode == EraserMode::Pixel);
      };
      updateVisibility();
      connect(btnPixel, &QPushButton::clicked, [=]() {
        m_config.eraserMode = EraserMode::Pixel;
        updateVisibility();
        apply();
      });
      connect(btnObject, &QPushButton::clicked, [=]() {
        m_config.eraserMode = EraserMode::Object;
        updateVisibility();
        apply();
      });
      connect(slEraserSize, &QSlider::valueChanged, [this](int v) {
        m_config.penWidth = v;
        apply();
      });

      QCheckBox *chkKeepInk = new QCheckBox("Nur Marker löschen");
      chkKeepInk->setChecked(m_config.eraserKeepInk);
      connect(chkKeepInk, &QCheckBox::toggled, [this](bool c) {
        m_config.eraserKeepInk = c;
        apply();
      });
      layout->addWidget(chkKeepInk);

      layout->addSpacing(10);
      QPushButton *btnClear = new QPushButton("Seite leeren");
      btnClear->setObjectName("DangerBtn");
      btnClear->setFixedHeight(35);
      btnClear->setCursor(Qt::PointingHandCursor);
      connect(btnClear, &QPushButton::clicked, [this]() {
        if (QMessageBox::question(this, "Seite leeren", "Alles löschen?",
                                  QMessageBox::Yes | QMessageBox::No) ==
            QMessageBox::Yes) {
          QGraphicsView *view = nullptr;
          for (QWidget *w : QApplication::topLevelWidgets()) {
            view = w->findChild<QGraphicsView *>();
            if (view)
              break;
          }
          if (view && view->scene()) {
            QList<QGraphicsItem *> items = view->scene()->items();
            for (auto *item : items) {
              if (dynamic_cast<QGraphicsPathItem *>(item)) {
                view->scene()->removeItem(item);
                delete item;
              }
            }
            view->viewport()->update();
          }
        }
      });
      layout->addWidget(btnClear);
    } else if (m_mode == ToolMode::Lasso) {
      layout->addWidget(new QLabel("Auswahl-Modus:"));
      QHBoxLayout *modeLayout = new QHBoxLayout;
      modeLayout->setSpacing(5);
      QButtonGroup *modeGroup = new QButtonGroup(this);
      QPushButton *btnFree = new QPushButton("Freihand");
      btnFree->setCheckable(true);
      btnFree->setFixedHeight(30);
      QPushButton *btnRect = new QPushButton("Rechteck");
      btnRect->setCheckable(true);
      btnRect->setFixedHeight(30);
      if (m_config.lassoMode == LassoMode::Freehand)
        btnFree->setChecked(true);
      else
        btnRect->setChecked(true);
      modeGroup->addButton(btnFree);
      modeGroup->addButton(btnRect);
      modeLayout->addWidget(btnFree);
      modeLayout->addWidget(btnRect);
      layout->addLayout(modeLayout);
      connect(btnFree, &QPushButton::clicked, [this]() {
        m_config.lassoMode = LassoMode::Freehand;
        apply();
      });
      connect(btnRect, &QPushButton::clicked, [this]() {
        m_config.lassoMode = LassoMode::Rectangle;
        apply();
      });

      QCheckBox *chkAspect = new QCheckBox("Seitenverhältnis sperren");
      chkAspect->setChecked(m_config.aspectLock);
      connect(chkAspect, &QCheckBox::toggled, [this](bool c) {
        m_config.aspectLock = c;
        apply();
      });
      layout->addWidget(chkAspect);
    } else if (m_mode == ToolMode::Ruler) {
      // --- RULER SETTINGS ---
      layout->addWidget(new QLabel("Einheit:"));
      QHBoxLayout *unitLayout = new QHBoxLayout;
      unitLayout->setSpacing(5);
      QButtonGroup *unitGroup = new QButtonGroup(this);
      struct UnitEntry { QString label; RulerUnit unit; };
      QList<UnitEntry> units = {{"px", RulerUnit::Pixel}, {"cm", RulerUnit::Centimeter}, {"Zoll", RulerUnit::Inch}};
      for (const UnitEntry &u : units) {
        QPushButton *btn = new QPushButton(u.label);
        btn->setCheckable(true);
        btn->setFixedHeight(30);
        if (m_config.rulerUnit == u.unit) btn->setChecked(true);
        unitGroup->addButton(btn);
        unitLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, [this, u]() {
          m_config.rulerUnit = u.unit;
          apply();
        });
      }
      layout->addLayout(unitLayout);

      QCheckBox *chkSnap = new QCheckBox("Einrasten");
      chkSnap->setChecked(m_config.rulerSnap);
      connect(chkSnap, &QCheckBox::toggled, [this](bool v) {
        m_config.rulerSnap = v;
        apply();
      });
      layout->addWidget(chkSnap);

      QCheckBox *chkCompass = new QCheckBox("Kompassmodus (Bogen)");
      chkCompass->setChecked(m_config.compassMode);
      connect(chkCompass, &QCheckBox::toggled, [this](bool v) {
        m_config.compassMode = v;
        apply();
      });
      layout->addWidget(chkCompass);
    }
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(37, 37, 38));
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);
  }

private:
  void updatePreview() {
    if (m_previewWidget)
      m_previewWidget->updateConfig(m_mode, m_config);
  }
  void apply() { ToolManager::instance().updateConfig(m_config); }
  ToolConfig m_config;
  ToolMode m_mode;
  ToolPreviewWidget *m_previewWidget;
};

// =============================================================================
// 4. ModernToolbar Implementation
// =============================================================================

ModernToolbar::ModernToolbar(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_AcceptTouchEvents, true);
  setWindowFlags(Qt::FramelessWindowHint);
  setMouseTracking(true);
  if (parent)
    parent->installEventFilter(this);

  btnPen = new ToolbarBtn("pen", this);
  btnPencil = new ToolbarBtn("pencil", this);
  btnHighlighter = new ToolbarBtn("highlighter", this);
  btnEraser = new ToolbarBtn("eraser", this);
  btnLasso = new ToolbarBtn("lasso", this);
  btnRuler = new ToolbarBtn("ruler", this);
  btnShape = new ToolbarBtn("shape", this);
  btnStickyNote = new ToolbarBtn("stickynote", this);
  btnText = new ToolbarBtn("text", this);
  btnImage = new ToolbarBtn("image", this);
  btnHand = new ToolbarBtn("hand", this);
  btnUndo = new ToolbarBtn("undo", this);
  btnRedo = new ToolbarBtn("redo", this);
  btnDockToggle = new ToolbarBtn("↥", this); // Default detach/dock icon

  btnSettings = new ToolbarBtn("⚙️", this);
  btnSave = new ToolbarBtn("💾", this);
  btnPalette = new ToolbarBtn("🎨", this);
  btnBrushSize = new ToolbarBtn("🖌️", this);
  
  m_dockedOnlyButtons = {btnSettings, btnSave, btnPalette, btnBrushSize};

  m_buttons = {btnSettings, btnSave, btnPen,      btnPencil, btnHighlighter, btnEraser,
               btnLasso,    btnRuler,  btnShape,       btnStickyNote,
               btnText,     btnImage,  btnHand,        btnUndo,
               btnRedo,     btnPalette, btnBrushSize,  btnDockToggle};

  m_customColors = {Qt::black, Qt::white,         Qt::red,
                    Qt::blue,  QColor(0, 150, 0), QColor(255, 140, 0)};

  auto handleToolClick = [this](ToolMode m) {
    if (mode_ == m) {
      if (m_style == Radial) {
          toggleRadialSettings();
      } else {
          showSettingsPopup();
      }
      return;
    }
    if (m_showRadialSettings) {
        toggleRadialSettings(); // Hide settings when switching tools
    }
    ToolbarBtn *btn = getButtonForMode(m);
    if (btn) {
      QPoint pos = btn->mapToGlobal(QPoint(btn->width() + 15, 0));
      ToolUIBridge::instance().setOverlayPosition(pos);
    }
    ToolManager::instance().selectTool(m);
    setToolMode(m);
  };
  
  // Initialize Secondary Settings Ring (Colors & Sizes)
  for (QColor c : m_customColors) {
    QPushButton *btn = new QPushButton(this);
    btn->setFixedSize(28, 28);
    btn->setStyleSheet(QString("background-color: %1; border-radius: 14px; border: 2px solid rgba(255,255,255,100);").arg(c.name()));
    btn->hide();
    m_radialSettingsBtns.append(btn);
    connect(btn, &QPushButton::clicked, this, [this, c]() {
       ToolUIBridge::instance().setPenColor(c);
       toggleRadialSettings();
    });
  }
  
  QList<int> sizes = {2, 6, 12, 24};
  for (int s : sizes) {
    QPushButton *btn = new QPushButton(this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(30, 30);
    btn->setStyleSheet("background-color: rgba(30, 28, 52, 200); border-radius: 15px; border: 1px solid rgba(255,255,255,50); color: white; font-weight: bold;");
    btn->setText(QString::number(s));
    btn->hide();
    m_radialSettingsBtns.append(btn);
    connect(btn, &QPushButton::clicked, this, [this, s]() {
       ToolUIBridge::instance().setPenWidth(s);
       toggleRadialSettings();
    });
  }

  connect(btnPen, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Pen); });
  connect(btnPencil, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Pencil); });
  connect(btnHighlighter, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Highlighter); });
  connect(btnEraser, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Eraser); });

  connect(btnLasso, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Lasso); });
  connect(btnRuler, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Ruler); });
  connect(btnShape, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Shape); });
  connect(btnStickyNote, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::StickyNote); });
  connect(btnText, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Text); });
  connect(btnHand, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Hand); });
  connect(btnImage, &ToolbarBtn::clicked,
          [=]() { handleToolClick(ToolMode::Image); });
  connect(btnUndo, &ToolbarBtn::clicked,
          [this]() { emit undoRequested(); });
  connect(btnRedo, &ToolbarBtn::clicked,
          [this]() { emit redoRequested(); });
  connect(btnDockToggle, &ToolbarBtn::clicked,
          [this]() { setDockMode(!m_isDockedMode); });

  setStyle(Normal);
  setToolMode(ToolMode::Pen);
  ToolManager::instance().selectTool(ToolMode::Pen);
}

// --- WICHTIG: Event-Handler sind jetzt HIER OBEN, damit sie gefunden werden
// ---

void ModernToolbar::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  int cx = width() / 2;
  int cy = height() / 2;
  if (m_style == Normal) {
    int w = width();
    int h = height();

    if (m_orientation == Vertical || m_orientation == Horizontal) {
      if (m_isDockedMode) {
        // Notch appearance: Flush on top, rounded on the bottom corners.
        QLinearGradient grad(0, 0, w, h);
        grad.setColorAt(0, QColor(30, 28, 52, 255));
        grad.setColorAt(1, QColor(20, 18, 40, 255));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        
        int r = 16; // Notch radius
        // Drawing slightly higher so the top corners are un-rounded (cut off by widget bounds)
        p.drawRoundedRect(0, -r, w, h + r, r, r);

        // Subtler bottom border for "leichte Hervorhebung"
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(124, 92, 252, 100), 2));
        p.drawRoundedRect(1, -r, w - 2, h + r - 1, r, r);
        
        // Separators for docked mode
        p.setPen(QPen(QColor(255, 255, 255, 30), 1));
        for (int sx : m_separatorXPositions) {
          p.drawLine(sx, 12, sx, h - 12);
        }
      } else {
        // Floating pill container
        int radius = std::min(w, h) / 2;
        // Outer glow / shadow
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 60));
        p.drawRoundedRect(rect().adjusted(2, 4, -2, -2), radius, radius);
        
        // Container body
        QLinearGradient grad(0, 0, w, h);
        grad.setColorAt(0, QColor(30, 28, 52, 248));
        grad.setColorAt(1, QColor(20, 18, 40, 248));
        p.setBrush(grad);
        p.setPen(QPen(QColor(124, 92, 252, 60), 1));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius, radius);

        // Drag grip
        if (m_draggable) {
          if (m_orientation == Vertical) {
            p.setBrush(QColor(255, 255, 255, 55));
            p.setPen(Qt::NoPen);
            for (int i = -1; i <= 1; ++i)
              p.drawEllipse(w / 2 - 2, h / 2 + i * 7 - 2, 4, 4);
          } else {
            p.setPen(QPen(QColor(255, 255, 255, 50), 2));
            p.drawLine(10, h / 2 - 8, 10, h / 2 + 8);
            p.drawLine(14, h / 2 - 8, 14, h / 2 + 8);
          }
        }
      }
    }
  } else {
    int rMain = 135 * m_scale;
    if (m_radialType == HalfEdge) {
      int paintCx = m_isDockedLeft ? 0 : width();
      QRect box(paintCx - rMain, cy - rMain, rMain * 2, rMain * 2);
      
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, 60));
      p.drawPie(QRect(box.x(), box.y() + 4, box.width(), box.height()), (m_isDockedLeft ? -90 : 90) * 16, 180 * 16);
      
      QRadialGradient bgGrad(paintCx, cy, rMain * 0.8);
      bgGrad.setColorAt(0.0, QColor(35, 33, 58, 245));
      bgGrad.setColorAt(1.0, QColor(20, 18, 40, 245));
      p.setBrush(bgGrad);
      p.setPen(QPen(QColor(124, 92, 252, 90), 1));
      p.drawPie(box, (m_isDockedLeft ? -90 : 90) * 16, 180 * 16);
    } else {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, 60));
      p.drawEllipse(QPoint(cx, cy + 4), rMain, rMain);
      
      QRadialGradient bgGrad(cx, cy, rMain * 0.8);
      bgGrad.setColorAt(0.0, QColor(35, 33, 58, 245));
      bgGrad.setColorAt(1.0, QColor(20, 18, 40, 245));
      p.setBrush(bgGrad);
      p.setPen(QPen(QColor(124, 92, 252, 90), 1));
      p.drawEllipse(QPoint(cx, cy), rMain, rMain);

      int hole = static_cast<int>(55 * m_scale);
      p.setBrush(QColor(15, 14, 25, 255));
      p.setPen(QPen(QColor(0, 0, 0, 150), 2));
      p.drawEllipse(QPoint(cx, cy), hole, hole);
    }
  }
}

void ModernToolbar::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    m_pressedButton = nullptr;
    m_isScrolling = false;
    m_hasScrolled = false;
    m_isDragging = false;
    if (m_style == Radial)
      m_pressedButton = getRadialButtonAt(e->pos());
    int dragR = 45 * m_scale;
    int cx = width() / 2;
    int cy = height() / 2;
    dragStartPos_ = e->pos();
    
    if (m_style == Radial && m_radialType == HalfEdge)
      cx = m_isDockedLeft ? 0 : width();
    int dx = e->pos().x() - cx;
    int dy = e->pos().y() - cy;
    double dist = std::sqrt(dx * dx + dy * dy);
    if (m_style == Radial && m_radialType == HalfEdge) {
      if (dist > dragR) {
        m_dragStartAngle = std::atan2(-dy, dx) * 180.0 / 3.14159;
        m_scrollStartAngleVal = m_scrollAngle;
        m_isScrolling = true;
      }
    }
    if (m_isResizing)
      return;
    if (m_style == Normal && !m_isPreview) {
      int handleSize = 30;
      if (e->pos().x() > width() - handleSize &&
          e->pos().y() > height() - handleSize) {
        m_isResizing = true;
        dragStartPos_ = e->pos();
        resizeStartPos_ = e->pos();
        startSize_ = size();
        return;
      }
    }
    if (!m_isPreview && !m_pressedButton) {
      bool canDrag = false;
      if (m_style != Radial) {
        if (m_orientation == Vertical && e->pos().y() < 50)
          canDrag = true;
        if (m_orientation == Horizontal && e->pos().x() < 50)
          canDrag = true;
      }
      if (canDrag) {
        m_isDragging = true;
        dragStartPos_ = e->pos();
        m_isScrolling = false;
        m_dragOffset =
            e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
        clearMask();
        return;
      }
    }
  }
}

void ModernToolbar::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() == Qt::NoButton && !m_isDragging && !m_isScrolling && e->source() == Qt::MouseEventNotSynthesized) return;

  if (m_style == Radial && !m_isDragging) {
     QPoint delta = e->pos() - dragStartPos_;
     
     if (m_pressedButton) {
         if (delta.manhattanLength() > 5) {
             m_isDragging = true;
             m_pressedButton = nullptr; 
             m_isScrolling = false;
             m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
             clearMask();
             return;
         }
     } else {
         if (m_radialType == HalfEdge) {
             if ((m_isDockedLeft && delta.x() > 10) || (!m_isDockedLeft && delta.x() < -10)) {
                 m_isDragging = true;
                 m_isScrolling = false;
                 m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
                 clearMask();
                 return;
             }
         } else if (m_radialType == FullCircle) {
             if (delta.manhattanLength() > 5) {
                 m_isDragging = true;
                 m_isScrolling = false;
                 m_dragOffset = e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
                 clearMask();
                 return;
             }
         }
     }
  }

  if (m_style == Radial && m_isScrolling && m_radialType == HalfEdge &&
      !m_pressedButton && !m_isDragging) {
    int cx = m_isDockedLeft ? 0 : width();
    int cy = height() / 2;
    int dx = e->pos().x() - cx;
    int dy = e->pos().y() - cy;
    double currentAngle = std::atan2(-dy, dx) * 180.0 / 3.14159;
    double delta = currentAngle - m_dragStartAngle;
    if (delta > 180)
      delta -= 360;
    if (delta < -180)
      delta += 360;
    if (std::abs(delta) > 1.0)
      m_hasScrolled = true;
    m_scrollAngle = m_scrollStartAngleVal - delta;
    double maxScroll = (m_buttons.size() * 30.0);
    if (m_scrollAngle > maxScroll)
      m_scrollAngle = maxScroll;
    if (m_scrollAngle < -maxScroll)
      m_scrollAngle = -maxScroll;
    updateLayout();
    return;
  }
  if (m_isDragging) {
    if (!parentWidget())
      return;
    QPoint newPosGlobal = e->globalPosition().toPoint() - m_dragOffset;
    QPoint newTopLeft = parentWidget()->mapFromGlobal(newPosGlobal);
    QPoint globalMousePosInParent =
        parentWidget()->mapFromGlobal(e->globalPosition().toPoint());
    int parentW = parentWidget()->width();
    int parentH = parentWidget()->height();
    int effectivePadding = 0;
    if (m_style == Radial) {
      int rVisual = 175 * m_scale;
      int rWidget = width() / 2;
      effectivePadding = rWidget - rVisual;

      int snapDist = 60 * m_scale;
      int tearDist = 120 * m_scale;
      if (m_radialType == FullCircle) {
        if (globalMousePosInParent.x() < snapDist) {
          m_isDockedLeft = true;
          setRadialType(HalfEdge);
          move(0, y());
          m_dragOffset =
              e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
          return;
        } else if (globalMousePosInParent.x() > (parentW - snapDist)) {
          m_isDockedLeft = false;
          setRadialType(HalfEdge);
          move(parentW - width(), y());
          m_dragOffset =
              e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
          return;
        }
      } else if (m_radialType == HalfEdge) {
        bool pullAway = false;
        if (m_isDockedLeft && globalMousePosInParent.x() > tearDist)
          pullAway = true;
        else if (!m_isDockedLeft &&
                 globalMousePosInParent.x() < (parentW - tearDist))
          pullAway = true;
        if (pullAway) {
          setRadialType(FullCircle);
          clearMask();
          int newX = globalMousePosInParent.x() - (width() / 2);
          move(newX, y());
          m_dragOffset =
              e->globalPosition().toPoint() - mapToGlobal(QPoint(0, 0));
          return;
        } else {
          int fixedX = m_isDockedLeft ? 0 : (parentW - width());
          int minSlidingY = -effectivePadding;
          int maxSlidingY = parentH - height() + effectivePadding;
          move(fixedX, qBound(minSlidingY, newTopLeft.y(), maxSlidingY));
          return;
        }
      }
    }
    int minX = -effectivePadding;
    int maxX = parentW - width() + effectivePadding;
    int maxY = parentH - height() + effectivePadding;
    
    // Allow dragging all the way to y=0 to merge with header
    int effectiveTop = (m_style == Radial) ? -effectivePadding : 0; 
    
    int newY = qBound(effectiveTop, newTopLeft.y(), maxY);
    int newX = qBound(minX, newTopLeft.x(), maxX);
    
    // Docking Previews
    if (m_style == Normal) {
      if (!m_snapPreview) {
        m_snapPreview = new QWidget(parentWidget());
        m_snapPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_snapPreview->setStyleSheet("background-color: transparent; border: 2px dashed rgba(255, 255, 255, 120); border-radius: 20px;");
      }
      
      int idealL = calculateMinLength() + 20;

      if (newY <= 50) {
        // Top Snap: Centered width 
        m_snapPreview->setGeometry((parentW - idealL) / 2, 10, idealL, 52);
        m_snapPreview->setStyleSheet(
            "background-color: transparent;"
            "border: 2px dashed #5E5CE6;"
            "border-radius: 20px;");
        m_snapPreview->show();
        m_snapPreview->raise();
      } else if (newX <= 50) {
        // Left Snap (Vertical Pill)
        m_snapPreview->setGeometry(15, (parentH - idealL) / 2, 65, idealL);
        m_snapPreview->setStyleSheet("background-color: transparent; border: 2px dashed #5E5CE6; border-radius: 20px;");
        m_snapPreview->show();
        m_snapPreview->raise();
      } else if (newX >= parentW - 100) {
        // Right Snap (Vertical Pill)
        m_snapPreview->setGeometry(parentW - 80, (parentH - idealL) / 2, 65, idealL);
        m_snapPreview->setStyleSheet("background-color: transparent; border: 2px dashed #5E5CE6; border-radius: 20px;");
        m_snapPreview->show();
        m_snapPreview->raise();
      } else {
        m_snapPreview->hide();
      }
    }

    move(newX, newY);
    return;
  }
  if (m_isResizing && m_style == Normal) {
    QPoint delta = e->pos() - resizeStartPos_;
    int newW = startSize_.width() + delta.x();
    int newH = startSize_.height() + delta.y();
    int minL = calculateMinLength();
    if (m_orientation == Vertical) {
      if (newH < minL)
        newH = minL;
      if (newW < 50)
        newW = 50;
      if (newW > 100)
        newW = 100;
    } else {
      if (newW < minL)
        newW = minL;
      if (newH < 50)
        newH = 50;
      if (newH > 100)
        newH = 100;
    }
    resize(newW, newH);
    constrainToParent();
    return;
  }
  if (m_style == Normal && !m_isPreview) {
    if (e->pos().x() > width() - 30 && e->pos().y() > height() - 30)
      setCursor(Qt::SizeFDiagCursor);
    else
      setCursor(Qt::ArrowCursor);
  }
}

void ModernToolbar::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressedButton) {
    if (!(m_style == Radial && m_radialType == HalfEdge && m_hasScrolled))
      m_pressedButton->triggerClick();
    m_pressedButton = nullptr;
  }
  if (m_isDragging) {
    m_cachedMask = QRegion();
    updateHitbox();
    
    if (m_style == Normal && m_snapPreview && m_snapPreview->isVisible()) {
        QRect snapGeom = m_snapPreview->geometry();
        m_snapPreview->hide();
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        
        bool isTopSnap = (snapGeom.y() == 10 && snapGeom.width() > 100);
        bool isVertical = !isTopSnap;
        
        if (isTopSnap) {
            setMaximumSize(snapGeom.size());
            setFixedSize(snapGeom.size());
            setOrientation(Horizontal, false); // no internal animation, we'll do geom animation
        } else {
            setMaximumSize(snapGeom.size());
            setFixedSize(snapGeom.size());
            setOrientation(Vertical, false);
        }
        
        // Smooth snap animation
        QPropertyAnimation *snapAnim = new QPropertyAnimation(this, "geometry");
        snapAnim->setDuration(220);
        snapAnim->setStartValue(geometry());
        snapAnim->setEndValue(snapGeom);
        snapAnim->setEasingCurve(QEasingCurve::OutCubic);
        snapAnim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        if (m_snapPreview) m_snapPreview->hide();
        if (m_style == Normal) checkOrientation(e->globalPosition().toPoint());
    }
  }
  m_isDragging = false;
  m_isResizing = false;
  m_isScrolling = false;
  m_hasScrolled = false;
  if (m_style == Radial && m_radialType == HalfEdge)
    snapToEdge();
  else
    constrainToParent();
}

void ModernToolbar::resizeEvent(QResizeEvent *) {
  updateLayout();
  updateHitbox();
}
void ModernToolbar::leaveEvent(QEvent *) { setCursor(Qt::ArrowCursor); }

// --- RESTLICHE FUNKTIONEN ---

void ModernToolbar::showSettingsPopup() {
  if (mode_ != ToolMode::Pen && mode_ != ToolMode::Pencil &&
      mode_ != ToolMode::Highlighter && mode_ != ToolMode::Eraser &&
      mode_ != ToolMode::Lasso && mode_ != ToolMode::Ruler)
    return;
  ToolConfig currentConfig = ToolManager::instance().config();
  ToolSettingsPopup *popup =
      new ToolSettingsPopup(mode_, currentConfig, this->window());
  ToolbarBtn *btn = getButtonForMode(mode_);
  popup->adjustSize();
  QPoint targetPos;
  if (btn) {
    if (m_orientation == Horizontal) {
      targetPos = btn->mapToGlobal(QPoint((btn->width() - popup->width()) / 2, btn->height() + 10));
    } else {
      targetPos = btn->mapToGlobal(QPoint(btn->width() + 10, 0));
    }
  } else {
    targetPos = mapToGlobal(rect().bottomLeft()) + QPoint(0, 10);
  }

  if (this->window()) {
      QRect windowRect = this->window()->geometry();
      if (targetPos.x() + popup->width() > windowRect.right()) {
          targetPos.setX(windowRect.right() - popup->width() - 10);
      }
      if (targetPos.y() + popup->height() > windowRect.bottom()) {
          targetPos.setY(windowRect.bottom() - popup->height() - 10);
      }
      if (targetPos.x() < windowRect.left()) targetPos.setX(windowRect.left() + 10);
      if (targetPos.y() < windowRect.top()) targetPos.setY(windowRect.top() + 10);
  }

  popup->move(targetPos);
  popup->show();
}

ToolbarBtn *ModernToolbar::getButtonForMode(ToolMode m) {
  switch (m) {
  case ToolMode::Pen:
    return btnPen;
  case ToolMode::Pencil:
    return btnPencil;
  case ToolMode::Highlighter:
    return btnHighlighter;
  case ToolMode::Eraser:
    return btnEraser;
  case ToolMode::Lasso:
    return btnLasso;
  case ToolMode::Ruler:
    return btnRuler;
  case ToolMode::Shape:
    return btnShape;
  case ToolMode::StickyNote:
    return btnStickyNote;
  case ToolMode::Text:
    return btnText;
  case ToolMode::Image:
    return btnImage;
  case ToolMode::Hand:
    return btnHand;
  default:
    return btnPen;
  }
}

ToolbarBtn *ModernToolbar::getRadialButtonAt(const QPoint &pos) {
  for (auto *b : m_buttons) {
    if (b->isVisible() && b->geometry().contains(pos))
      return b;
  }
  return nullptr;
}

void ModernToolbar::showEvent(QShowEvent *) {
  updateLayout();
  if (m_style == Radial && !m_isPreview)
    constrainToParent();
  updateHitbox();
}
bool ModernToolbar::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event->type() == QEvent::Resize &&
      !m_isPreview)
    constrainToParent();
  return QWidget::eventFilter(watched, event);
}
void ModernToolbar::setTopBound(int top) {
  m_topBound = top;
  constrainToParent();
}
void ModernToolbar::setScale(double s) {
  if (s < 0.5)
    s = 0.5;
  if (s > 2.0)
    s = 2.0;
  m_scale = s;
  if (m_style == Radial) {
    int btnSize = 40 * m_scale;
    for (auto *b : m_buttons)
      b->setBtnSize(btnSize);
    int size = 460 * m_scale;
    setFixedSize(size, size);
    updateLayout();
    updateHitbox();
    update();
    if (!m_isPreview)
      constrainToParent();
  }
}
void ModernToolbar::constrainToParent() {
  if (!parentWidget() || m_isPreview)
    return;
  QRect pRect = parentWidget()->rect();
  int padding = 0;
  if (m_style == Radial) {
    int rVisual = 175 * m_scale;
    int rWidget = width() / 2;
    padding = rWidget - rVisual;
  }
  int maxX = pRect.width() - width() + padding;
  int maxY = pRect.height() - height() + padding;
  int minX = -padding;
  int newX = x();
  int newY = y();
  int effectiveTop = (m_style == Radial) ? -padding : m_topBound;
  if (newY < effectiveTop)
    newY = effectiveTop;
  if (newY > maxY)
    newY = maxY;
  if (m_style == Radial && m_radialType == HalfEdge) {
    snapToEdge();
    move(x(), newY);
  } else {
    if (newX < minX)
      newX = minX;
    if (newX > maxX)
      newX = maxX;
    move(newX, newY);
  }
}
void ModernToolbar::toggleRadialSettings() {
  m_showRadialSettings = !m_showRadialSettings;
  updateLayout(true);
  updateHitbox();
  update();
}
void ModernToolbar::reorderButtons() {
  // Deprecated: We no longer mutate the m_buttons array to prevent destroying structural layouts across Docked/Linear modes.
}
void ModernToolbar::setToolMode(ToolMode mode) {
  bool changed = (mode_ != mode);
  mode_ = mode;
  for (auto *b : m_buttons)
    b->setActive(false);
  ToolbarBtn *activeBtn = getButtonForMode(mode);
  if (activeBtn) {
    activeBtn->setActive(true);
    activeBtn->animateSelect();
  }
  if (changed) {
    emit rulerToggled(mode == ToolMode::Ruler);
  }
  if (m_style == Radial && m_radialType == FullCircle && changed) {
    updateLayout(true);
  }
  update();
  updateHitbox();
}
void ModernToolbar::setStyle(Style style) {
  m_style = style;
  m_cachedMask = QRegion();
  m_showRadialSettings = false; // Hide settings rings when changing main styles
  bool buttonsIgnoreMouse = (style == Radial);
  for (auto *b : m_buttons) {
    b->setAttribute(Qt::WA_TransparentForMouseEvents, buttonsIgnoreMouse);
  }
  if (m_style == Normal) {
    setMinimumSize(50, 65);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (m_orientation == Vertical) {
      setFixedWidth(65);
      setMinimumHeight(calculateMinLength());
      setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
      setFixedHeight(65);
      setMinimumWidth(calculateMinLength());
      setMaximumWidth(QWIDGETSIZE_MAX);
    }
  } else {
    int size = 380 * m_scale;
    setFixedSize(size, size);
    reorderButtons();
  }
  for (auto b : m_buttons)
    b->show();
  if (style == Radial && m_radialType == HalfEdge)
    snapToEdge();
  else
    constrainToParent();
  updateLayout();
  updateHitbox();
  update();
}
void ModernToolbar::setRadialType(RadialType type) {
  if (m_radialType == type)
    return;
  m_radialType = type;
  m_scrollAngle = 0.0;
  if (m_style == Radial) {
    if (m_radialType == HalfEdge && !m_isDragging)
      snapToEdge();
    else {
      if (!m_isPreview && parentWidget() && !m_isDragging) {
        constrainToParent();
      }
    }
    updateLayout(true);
    updateHitbox();
    update();
  }
}
void ModernToolbar::updateHitbox() {
  if (m_isDragging)
    return;
  if (m_style != Radial) {
    clearMask();
    m_cachedMask = QRegion();
    return;
  }
  int r = m_showRadialSettings ? 200 * m_scale : 145 * m_scale;
  int maskR = r + 4;
  QRegion newMask;
  if (m_radialType == FullCircle) {
    int cx = width() / 2;
    int cy = height() / 2;
    newMask =
        QRegion(cx - maskR, cy - maskR, 2 * maskR, 2 * maskR, QRegion::Ellipse);
  } else {
    int cy = height() / 2;
    int cx = m_isDockedLeft ? 0 : width();
    QRegion circleReg(cx - maskR, cy - maskR, 2 * maskR, 2 * maskR,
                      QRegion::Ellipse);
    QRegion widgetReg(rect());
    newMask = circleReg.intersected(widgetReg);
  }
  if (newMask == m_cachedMask)
    return;
  setMask(newMask);
  m_cachedMask = newMask;
}
void ModernToolbar::snapToEdge() {
  if (!parentWidget() || m_isPreview)
    return;
  int parentW = parentWidget()->width();
  int mid = parentW / 2;
  int targetX = 0;
  if (x() + width() / 2 < mid) {
    targetX = 0;
    m_isDockedLeft = true;
  } else {
    targetX = parentW - width();
    m_isDockedLeft = false;
  }
  QPropertyAnimation *anim = new QPropertyAnimation(this, "pos");
  anim->setDuration(300);
  anim->setStartValue(pos());
  anim->setEndValue(QPoint(targetX, y()));
  anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(anim, &QPropertyAnimation::finished, this, [this]() {
    updateLayout();
    updateHitbox();
    update();
  });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}
void ModernToolbar::wheelEvent(QWheelEvent *e) {
  if (m_style == Radial && m_radialType == HalfEdge) {
    double delta = e->angleDelta().y() / 8.0;
    m_scrollAngle += delta;
    double maxScroll = (m_buttons.size() * 30.0);
    if (m_scrollAngle > maxScroll)
      m_scrollAngle = maxScroll;
    if (m_scrollAngle < -maxScroll)
      m_scrollAngle = -maxScroll;
    updateLayout();
    e->accept();
  } else {
    QWidget::wheelEvent(e);
  }
}
void ModernToolbar::checkOrientation(const QPoint &globalPos) {
  Q_UNUSED(globalPos);
  if (!parentWidget() || m_style != Normal)
    return;
  QRect myRect = geometry();
  int parentW = parentWidget()->width();
  int parentH = parentWidget()->height();
  int distLeft = myRect.left();
  int distRight = parentW - myRect.right();
  int distTop = myRect.top();
  int distBottom = parentH - myRect.bottom();
  int threshold = 80;
  bool horizontal = false;
  if (distTop < threshold || distBottom < threshold)
    horizontal = true;
  else if (distLeft < threshold || distRight < threshold)
    horizontal = false;
  else
    return;
  setOrientation(horizontal ? Horizontal : Vertical, true);
}
void ModernToolbar::setOrientation(Orientation o, bool animate) {
  if (m_orientation == o)
    return;
  m_orientation = o;
  m_cachedMask = QRegion();
  QSize currentS = size();
  QSize targetS(currentS.height(), currentS.width());
  if (animate) {
    QPropertyAnimation *anim = new QPropertyAnimation(this, "size");
    anim->setDuration(250);
    anim->setStartValue(currentS);
    anim->setEndValue(targetS);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::valueChanged, this,
            [this](const QVariant &) { updateLayout(); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    resize(targetS);
    updateLayout();
  }
}
void ModernToolbar::setDockMode(bool docked) {
  if (m_isDockedMode == docked) return;
  m_isDockedMode = docked;
  if (btnDockToggle) btnDockToggle->setIcon(docked ? "⊞" : "↥");
  m_draggable = !docked;

  // Clear any size constraints set by edge snapping
  setMinimumSize(0, 0);
  setMaximumSize(16777215, 16777215);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  if (QWidget *pw = parentWidget()) {
    QRect targetGeom;
    int idealW = calculateMinLength();
    if (docked) {
      m_orientation = Horizontal;
      targetGeom = QRect((pw->width() - idealW) / 2, 0, idealW, 48);
    } else {
      m_orientation = Horizontal;
      int idealW = calculateMinLength();
      targetGeom = QRect((pw->width() - idealW) / 2, 60, idealW, 52);
    }

    QPropertyAnimation *anim = new QPropertyAnimation(this, "geometry");
    anim->setDuration(300);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setEndValue(targetGeom);
    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
      updateLayout(false);
      update();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    updateLayout(false);
    update();
  }
}

int ModernToolbar::calculateMinLength() {
  int btnS = 40;
  int minGap = 6;
  
  if (m_isDockedMode) {
    QList<ToolbarBtn*> leftGroup = {btnUndo, btnRedo};
    QList<ToolbarBtn*> rightGroup = {btnPalette, btnBrushSize, btnSave, btnSettings, btnDockToggle};
    int centerGroupSize = m_buttons.size() - leftGroup.size() - rightGroup.size();
    
    int leftW = leftGroup.size() * btnS + (leftGroup.size() - 1) * minGap;
    int rightW = rightGroup.size() * btnS + (rightGroup.size() - 1) * minGap;
    int centerW = centerGroupSize * btnS + (centerGroupSize - 1) * minGap + 24; // 24 = 3 separators * 8px
    
    return 20 + leftW + 30 + centerW + 30 + rightW + 20; // 20px margins, 30px group gaps
  } else {
    int dragH = 30;
    int numButtons = 0;
    for (auto *b : m_buttons) {
      if (m_dockedOnlyButtons.contains(b)) continue;
      numButtons++;
    }
    return dragH + (numButtons * btnS) + ((numButtons - 1) * minGap) + 16;
  }
}
void ModernToolbar::updateLayout(bool animate) {
  if (m_isDragging)
    animate = false;
  if (m_style == Normal) {
    int w = width();
    int h = height();
    int btnS = 40;
    if (m_orientation == Vertical) {
      btnS = std::max(30, (w < h ? w - 14 : h - 14));
    } else {
      btnS = std::max(30, (h < w ? h - 12 : w - 12)); 
    }
    
    for (auto *b : m_buttons) {
      b->setBtnSize(btnS);
    }
    
    int numVisible = 0;
    for (auto *b : m_buttons) {
      if (m_dockedOnlyButtons.contains(b)) {
        b->setVisible(m_isDockedMode);
      } else {
        b->setVisible(true);
      }
      if (b->isVisible()) numVisible++;
    }
    
    int dragSize = (m_draggable && !m_isDockedMode) ? 20 : 10;
    int gap = 6;
    m_separatorXPositions.clear();
    
    if (m_isDockedMode) {
      if (btnUndo->parentWidget() != this) {
          QPoint gU = btnUndo->parentWidget()->mapToGlobal(btnUndo->pos());
          QPoint lU = mapFromGlobal(gU);
          btnUndo->setParent(this);
          btnUndo->move(lU);
          btnUndo->setDrawFloatingBg(false);
          btnUndo->show();

          QPoint gR = btnRedo->parentWidget()->mapToGlobal(btnRedo->pos());
          QPoint lR = mapFromGlobal(gR);
          btnRedo->setParent(this);
          btnRedo->move(lR);
          btnRedo->setDrawFloatingBg(false);
          btnRedo->show();
      }

      QList<ToolbarBtn*> leftGroup = {btnUndo, btnRedo}; 
      QList<ToolbarBtn*> rightGroup = {btnPalette, btnBrushSize, btnSave, btnSettings, btnDockToggle};
      QList<ToolbarBtn*> centerGroup;
      for (auto *b : m_buttons) {
          if (!leftGroup.contains(b) && !rightGroup.contains(b)) {
              centerGroup.append(b);
          }
      }
      
      // Calculate widths and starting X positions
      int totalCenterW = centerGroup.size() * btnS + (centerGroup.size() - 1) * gap + (3 * 20);
      int centerStartX = (w - totalCenterW) / 2;
      int leftStartX = 20;
      int rightStartX = w - 20 - (rightGroup.size() * btnS + (rightGroup.size() - 1) * gap);
      
      for (int i = 0; i < m_buttons.size(); ++i) {
        auto *b = m_buttons[i];
        if (!b->isVisible()) continue;
        
        int bx = 0;
        int by = (h - btnS) / 2;

        if (leftGroup.contains(b)) {
            bx = leftStartX;
            leftStartX += btnS + gap;
        } else if (rightGroup.contains(b)) {
            bx = rightStartX;
            rightStartX += btnS + gap;
        } else {
            bx = centerStartX;
            centerStartX += btnS + gap;
            
            if (b == btnRuler || b == btnImage || b == btnHand) {
              centerStartX += 4;
              m_separatorXPositions.append(centerStartX);
              centerStartX += gap + 4;
            }
        }
        
        if (animate) {
          QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
          anim->setDuration(200);
          anim->setEndValue(QPoint(bx, by));
          anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
          b->move(bx, by);
        }
      }
    } else {
      int currentPos = dragSize;
      
      // Undock floating buttons
      for (auto *b : {btnUndo, btnRedo}) {
          if (QWidget *pw = parentWidget()) {
              if (b->parentWidget() == this) {
                  QPoint g = mapToGlobal(b->pos());
                  QPoint lp = pw->mapFromGlobal(g);
                  b->setParent(pw);
                  b->move(lp);
                  b->setDrawFloatingBg(true);
                  b->show();
              }
              int targetX = (b == btnUndo) ? 20 : 70;
              int targetY = 20;
              if (animate) {
                  QPropertyAnimation* anim = new QPropertyAnimation(b, "pos");
                  anim->setDuration(200);
                  anim->setEndValue(QPoint(targetX, targetY));
                  anim->start(QAbstractAnimation::DeleteWhenStopped);
              } else {
                  b->move(targetX, targetY);
              }
          }
      }

      for (auto *b : m_buttons) {
        if (b == btnUndo || b == btnRedo) continue;
        if (!b->isVisible()) continue;
        int bx, by;
        if (m_orientation == Vertical) {
          bx = (w - btnS) / 2;
          by = currentPos;
        } else {
          bx = currentPos;
          by = (h - btnS) / 2;
        }
        currentPos += btnS + gap;
        if (animate) {
          QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
          anim->setDuration(200);
          anim->setEndValue(QPoint(bx, by));
          anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
          b->move(bx, by);
        }
        b->show();
      }
    }
  } else {
    int cx = width() / 2;
    int cy = height() / 2;
    int paintCx = cx;
    if (m_radialType == HalfEdge) {
        paintCx = m_isDockedLeft ? 0 : width();
    }
    int btnS = 40 * m_scale;
    
    QList<ToolbarBtn*> radialBtns = {
      btnPen, btnPencil, btnHighlighter, btnEraser,
      btnLasso, btnRuler, btnShape, btnStickyNote,
      btnText, btnImage, btnHand
    };
    
    // Undock floating buttons
    for (auto *b : {btnUndo, btnRedo}) {
        if (QWidget *pw = parentWidget()) {
            if (b->parentWidget() == this) {
                QPoint g = mapToGlobal(b->pos());
                QPoint lp = pw->mapFromGlobal(g);
                b->setParent(pw);
                b->move(lp);
                b->setDrawFloatingBg(true);
                b->show();
            }
            int targetX = (b == btnUndo) ? 20 : 70;
            int targetY = 20;
            if (animate) {
                QPropertyAnimation* anim = new QPropertyAnimation(b, "pos");
                anim->setDuration(200);
                anim->setEndValue(QPoint(targetX, targetY));
                anim->start(QAbstractAnimation::DeleteWhenStopped);
            } else {
                b->move(targetX, targetY);
            }
        }
    }
    
    ToolbarBtn *activeBtn = getButtonForMode(mode_);
    if (!activeBtn || !radialBtns.contains(activeBtn)) {
        activeBtn = btnPen;
    }
    
    for (auto *b : m_buttons) {
      if (!radialBtns.contains(b)) {
        b->hide();
      } else {
        b->setBtnSize(btnS);
      }
    }
    
    activeBtn->setBtnSize(btnS + 12);
    
    if (m_radialType == FullCircle) {
        if (animate) {
          QPropertyAnimation *anim = new QPropertyAnimation(activeBtn, "pos");
          anim->setDuration(200);
          anim->setEndValue(QPoint(cx - (btnS+12)/2, cy - (btnS+12)/2));
          anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
          activeBtn->move(cx - (btnS+12)/2, cy - (btnS+12)/2);
        }
        activeBtn->show();
    }

    // Absolute slot positioning reduces staggering layout animations by 90%
    if (m_radialType == HalfEdge) {
      int r = 110 * m_scale;
      for (int i = 0; i < radialBtns.size(); ++i) {
        auto *b = radialBtns[i];
        double angle = (m_isDockedLeft ? 0.0 : 180.0) + ((i - 2) * 35.0) + m_scrollAngle;
        double rad = angle * 3.14159 / 180.0;
        int currentSize = (b == activeBtn) ? (btnS + 12) : btnS;
        
        int targetX = paintCx + r * cos(rad) - currentSize / 2;
        int targetY = cy + r * sin(rad) - currentSize / 2;
        
        if (animate) {
          QPropertyAnimation *anim = new QPropertyAnimation(b, "pos");
          anim->setDuration(200);
          anim->setEndValue(QPoint(targetX, targetY));
          anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
          b->move(targetX, targetY);
        }
        b->setVisible(m_isDockedLeft ? cos(rad) > 0.05 : cos(rad) < -0.05);
      }
    } else {
      int r = 90 * m_scale;
      double angleStep = 360.0 / radialBtns.size();
      for (int i = 0; i < radialBtns.size(); ++i) {
        if (radialBtns[i] == activeBtn) continue;
        double rad = (-90.0 + i * angleStep) * 3.14159 / 180.0;
        int targetX = cx + r * cos(rad) - btnS / 2;
        int targetY = cy + r * sin(rad) - btnS / 2;
        
        if (animate) {
          QPropertyAnimation *anim = new QPropertyAnimation(radialBtns[i], "pos");
          anim->setDuration(200);
          anim->setEndValue(QPoint(targetX, targetY));
          anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
          radialBtns[i]->move(targetX, targetY);
        }
        radialBtns[i]->show();
      }
    }
    
    // Position the Setting Ring
    if (m_showRadialSettings) {
        int outerR = 155 * m_scale;
        
        if (m_radialType == HalfEdge) {
            double angleStep = 180.0 / (m_radialSettingsBtns.size() - 1);
            for (int i = 0; i < m_radialSettingsBtns.size(); ++i) {
                auto *btn = m_radialSettingsBtns[i];
                double angle = (m_isDockedLeft ? -90.0 : 90.0) + (i * angleStep);
                double rad = angle * 3.14159 / 180.0;
                int targetX = paintCx + outerR * cos(rad) - btn->width() / 2;
                int targetY = cy + outerR * sin(rad) - btn->height() / 2;
                
                if (animate) {
                    QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos");
                    anim->setDuration(250);
                    anim->setEasingCurve(QEasingCurve::OutBack);
                    if (btn->isHidden()) {
                        btn->move(paintCx - btn->width() / 2, cy - btn->height() / 2);
                    }
                    anim->setEndValue(QPoint(targetX, targetY));
                    anim->start(QAbstractAnimation::DeleteWhenStopped);
                } else {
                    btn->move(targetX, targetY);
                }
                btn->show();
                btn->raise();
                btn->setVisible(m_isDockedLeft ? cos(rad) > 0.05 : cos(rad) < -0.05);
            }
        } else {
            double outerAngleStep = 360.0 / m_radialSettingsBtns.size();
            for (int i = 0; i < m_radialSettingsBtns.size(); ++i) {
                auto *btn = m_radialSettingsBtns[i];
                double rad = (-90.0 + i * outerAngleStep) * 3.14159 / 180.0;
                int targetX = cx + outerR * cos(rad) - btn->width() / 2;
                int targetY = cy + outerR * sin(rad) - btn->height() / 2;
                
                if (animate) {
                    QPropertyAnimation *anim = new QPropertyAnimation(btn, "pos");
                    anim->setDuration(250);
                    anim->setEasingCurve(QEasingCurve::OutBack);
                    if (btn->isHidden()) {
                        btn->move(cx - btn->width() / 2, cy - btn->height() / 2);
                    }
                    anim->setEndValue(QPoint(targetX, targetY));
                    anim->start(QAbstractAnimation::DeleteWhenStopped);
                } else {
                    btn->move(targetX, targetY);
                }
                btn->show();
                btn->raise();
            }
        }
    } else {
        for (auto *btn : m_radialSettingsBtns) {
            btn->hide();
        }
    }
  }
}
