#include "editoroverlays.h"

#include "PageItem.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "notechrome.h"
#include "notepreviewicon.h"
#include "overlayscrollindicator.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QIcon>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractButton>
#include <QAbstractAnimation>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QShowEvent>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QToolButton>
#include <QPair>
#include <QVector>
#include <QVBoxLayout>

#include <functional>
#include <memory>

namespace {

/// Kuratierte Blop-Palette (Productive Dark + Akzente), ergänzt um Zeichenfarben
static const QColor kBlopSwatches[] = {
    QColor("#0C0D17"), QColor("#1A1B2E"), QColor("#13141F"), QColor("#1C1D32"),
    QColor("#E6E4FF"), QColor("#FFFFFF"), QColor("#94A3B8"), QColor("#64748B"),
    QColor("#7C5CFC"), QColor("#6BA3F5"), QColor("#38BDF8"), QColor("#34D399"),
    QColor("#FBBF24"), QColor("#F97316"), QColor("#FB7185"), QColor("#A78BFA"),
    QColor("#000000"), QColor("#1E293B"), QColor("#F43F5E"), QColor("#22C55E"),
    QColor("#3B82F6"), QColor("#EAB308"), QColor("#C084FC"), QColor("#06B6D4"),
};

static QString blopSliderStyleSheet() {
  return BlopTheme::themed(QStringLiteral(
      "QSlider::groove:horizontal {"
      "  height: 7px; border-radius: 4px;"
      "  background: rgba(55, 58, 78, 0.95);"
      "  border: 1px solid rgba(120, 130, 160, 0.25);"
      "}"
      "QSlider::handle:horizontal {"
      "  width: 20px; height: 20px; margin: -7px 0;"
      "  border-radius: 10px;"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
      "    stop:0 #8EB8F8, stop:1 #6BA3F5);"
      "  border: 2px solid rgba(255,255,255,0.22);"
      "}"
      "QSlider::handle:horizontal:hover {"
      "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
      "    stop:0 #A8C8FF, stop:1 #7EB0F7);"
      "}"
      "QSlider::sub-page:horizontal {"
      "  height: 7px; border-radius: 4px;"
      "  background: rgba(107, 163, 245, 0.35);"
      "}"));
}

static QLabel *makeRgbLabel(const QString &text, QWidget *parent) {
  auto *l = new QLabel(text, parent);
  l->setFixedWidth(22);
  l->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: rgba(200, 205, 225, 0.9); font-weight: 700; font-size: 12px;")));
  return l;
}

class OverlayHost : public QWidget {
public:
  explicit OverlayHost(QWidget *anchor);
  void setCard(QWidget *c) { m_card = c; }
  std::function<void()> onDismiss;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void mousePressEvent(QMouseEvent *e) override;
  void showEvent(QShowEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void paintEvent(QPaintEvent *e) override;

private:
  void startEnterAnim();

  QWidget *m_anchor{nullptr};
  QWidget *m_card{nullptr};
  QColor m_scrim;
  qreal m_scrimProgress{0.0};
};

OverlayHost::OverlayHost(QWidget *anchor) : QWidget(anchor), m_anchor(anchor) {
  setObjectName(QStringLiteral("EditorOverlayHost"));
  setWindowFlags(Qt::Widget);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFocusPolicy(Qt::StrongFocus);
  // v3.18.2: scrim painted manually so enter fade can animate alpha without
  // stylesheet churn. Prefer BlopTheme::scrimColor so OverlayHost matches
  // BlopModal / Settings / All Pages.
#ifdef Q_OS_ANDROID
  m_scrim = BlopStyle::backdrop(/*forAndroid=*/true);
#else
  m_scrim = BlopTheme::scrimColor();
#endif
  setGeometry(anchor->rect());
  anchor->installEventFilter(this);
}

bool OverlayHost::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_anchor && event->type() == QEvent::Resize)
    setGeometry(m_anchor->rect());
  return false;
}

void OverlayHost::mousePressEvent(QMouseEvent *e) {
  if (m_card && !m_card->geometry().contains(e->pos())) {
    if (onDismiss)
      onDismiss();
    e->accept();
    return;
  }
  QWidget::mousePressEvent(e);
}

void OverlayHost::paintEvent(QPaintEvent *e) {
  Q_UNUSED(e);
  QPainter p(this);
  QColor c = m_scrim;
  c.setAlphaF(c.alphaF() * m_scrimProgress);
  p.fillRect(rect(), c);
}

void OverlayHost::startEnterAnim() {
  m_scrimProgress = 0.0;
  update();

  auto *scrimAnim = new QVariantAnimation(this);
  scrimAnim->setDuration(BlopMotion::kFast);
  scrimAnim->setStartValue(0.0);
  scrimAnim->setEndValue(1.0);
  scrimAnim->setEasingCurve(BlopMotion::kEaseStandard);
  connect(scrimAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant &v) {
            m_scrimProgress = v.toReal();
            update();
          });
  scrimAnim->start(QAbstractAnimation::DeleteWhenStopped);

  if (!m_card)
    return;
  m_card->setWindowOpacity(0.0);
  auto *cardAnim = new QPropertyAnimation(m_card, "windowOpacity", this);
  cardAnim->setDuration(BlopMotion::kStandard);
  cardAnim->setStartValue(0.0);
  cardAnim->setEndValue(1.0);
  cardAnim->setEasingCurve(BlopMotion::kEaseStandard);
  cardAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void OverlayHost::showEvent(QShowEvent *e) {
  QWidget::showEvent(e);
  if (m_anchor)
    setGeometry(m_anchor->rect());
  raise();
  setFocus(Qt::PopupFocusReason);
  startEnterAnim();
}

void OverlayHost::keyPressEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Escape) {
    if (onDismiss)
      onDismiss();
    e->accept();
    return;
  }
  QWidget::keyPressEvent(e);
}

} // namespace

static void centerCardInOverlay(OverlayHost *overlay, QWidget *card, int w, int h) {
  auto *root = new QVBoxLayout(overlay);
  root->setContentsMargins(0, 0, 0, 0);
  root->addStretch(1);
  auto *row = new QHBoxLayout();
  row->addStretch(1);
  card->setFixedSize(w, h);
  row->addWidget(card, 0, Qt::AlignCenter);
  row->addStretch(1);
  root->addLayout(row);
  root->addStretch(1);
}

/// Kleine Top-Level-Fenster (z. B. Qt::Popup-Toolsettings) liefern mit window()
/// sich selbst — OverlayHost würde nur diese Mini-Fläche füllen. Dann zum
/// QObject-Elternfenster (z. B. QMainWindow) wechseln.
static QWidget *overlayAnchorForColorPicker(QWidget *parent) {
  if (!parent)
    return nullptr;
  QWidget *w = parent->window();
  if (!w)
    w = parent;
  if (w->isWindow() && (w->width() < 420 || w->height() < 380)) {
    if (QWidget *pw = w->parentWidget()) {
      QWidget *outer = pw->window();
      if (outer && outer != w)
        w = outer;
    }
  }
  return w;
}

bool showColorPickerOverlay(QWidget *parent, QColor *color, const QString &title) {
  if (!parent || !color)
    return false;

  QWidget *anchor = overlayAnchorForColorPicker(parent);
  if (!anchor)
    return false;

  const int keepAlpha = color->alpha();

  auto *overlay = new OverlayHost(anchor);

  auto *card = new QFrame(overlay);
  card->setObjectName(QStringLiteral("ColorPickerCard"));
  // v3.16.1: card surface comes from BlopStyle. The local stylesheet only
  // styles the children (labels, line edits) so every overlay shares one
  // surface bg / border / radius set.
  card->setStyleSheet(
      BlopStyle::surfaceStyle(QStringLiteral("ColorPickerCard")) +
      BlopTheme::themed(QStringLiteral(
      "QLabel { color: rgba(235, 237, 245, 0.95); }"
      "QLineEdit {"
      "  background: rgba(22, 24, 36, 0.95);"
      "  border: 1px solid rgba(120, 130, 160, 0.35);"
      "  border-radius: 10px;"
      "  padding: 8px 12px;"
      "  color: #E8EAFF;"
      "  font-family: monospace;"
      "  font-size: 13px;"
      "  font-weight: 600;"
      "}"
      "QLineEdit:focus {"
      "  border: 1px solid rgba(107, 163, 245, 0.65);"
      "}")));

  QScreen *screen = nullptr;
  if (QWidget *ws = anchor->window())
    screen = ws->screen();
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
  const int maxW = qMax(300, avail.width() * 90 / 100);
  const int maxH = qMax(320, avail.height() * 88 / 100);
  int cardW = qMin(400, maxW);
  int cardH = qMin(500, maxH);
  QWidget *win = anchor->window();
  if (win) {
    cardW = qMin(cardW, qMax(280, win->width() - 40));
    cardH = qMin(cardH, qMax(300, win->height() - 40));
  }

  centerCardInOverlay(overlay, card, cardW, cardH);

  constexpr int kPreviewH = 72;
  constexpr int kSwatch = 32;

  auto *cardLay = new QVBoxLayout(card);
  cardLay->setContentsMargins(22, 18, 22, 18);
  cardLay->setSpacing(8);

  if (!title.isEmpty()) {
    auto *lbl = new QLabel(title, card);
    lbl->setStyleSheet(QStringLiteral(
        "font-weight: 700; font-size: 16px; letter-spacing: 0.2px;"));
    cardLay->addWidget(lbl);
  }

  auto *hint = new QLabel(QStringLiteral("Blop-Palette oder RGB"), card);
  hint->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: rgba(160, 168, 195, 0.88); font-size: 12px; font-weight: 500;")));
  cardLay->addWidget(hint);

  QColor cur = color->isValid() ? color->toRgb() : QColor(Qt::white);
  if (!cur.isValid())
    cur = Qt::white;

  auto *scroll = new QScrollArea(card);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setStyleSheet(QStringLiteral(
      "QScrollArea { background: transparent; border: none; }"));
  OverlayScrollIndicator::install(scroll);

  auto *inner = new QWidget(scroll);
  scroll->setWidget(inner);
  auto *lay = new QVBoxLayout(inner);
  lay->setContentsMargins(2, 0, 6, 0);
  lay->setSpacing(10);

  auto *preview = new QLabel(inner);
  preview->setMinimumHeight(kPreviewH);
  preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto *hexRow = new QHBoxLayout();
  hexRow->setSpacing(10);
  auto *hexLbl = new QLabel(QStringLiteral("HEX"), inner);
  hexLbl->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "color: rgba(180, 186, 210, 0.9); font-size: 11px; font-weight: 700;")));
  auto *hexEdit = new QLineEdit(inner);
  hexEdit->setMaxLength(7);
  hexEdit->setPlaceholderText(QStringLiteral("#RRGGBB"));
  hexRow->addWidget(hexLbl);
  hexRow->addWidget(hexEdit, 1);

  auto *swatchGrid = new QGridLayout();
  swatchGrid->setHorizontalSpacing(8);
  swatchGrid->setVerticalSpacing(8);
  constexpr int kCols = 6;
  const int n = static_cast<int>(sizeof(kBlopSwatches) / sizeof(kBlopSwatches[0]));
  QVector<QPushButton *> swatchBtns;
  swatchBtns.reserve(n);
  for (int i = 0; i < n; ++i) {
    const QColor sc = kBlopSwatches[i];
    auto *sw = new QPushButton(inner);
    sw->setFixedSize(kSwatch, kSwatch);
    sw->setCursor(Qt::PointingHandCursor);
    sw->setToolTip(sc.name(QColor::HexRgb));
    sw->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; border-radius: 8px;"
        "  border: 1px solid rgba(255,255,255,0.18); }"
        "QPushButton:hover { border: 2px solid #6BA3F5; }"
        "QPushButton:pressed { border: 2px solid #7C5CFC; }")
                          .arg(sc.name()));
    swatchGrid->addWidget(sw, i / kCols, i % kCols);
    swatchBtns.append(sw);
  }

  auto *sr = new QSlider(Qt::Horizontal, inner);
  auto *sg = new QSlider(Qt::Horizontal, inner);
  auto *sb = new QSlider(Qt::Horizontal, inner);
  for (auto *s : {sr, sg, sb}) {
    s->setRange(0, 255);
    s->setFixedHeight(26);
    s->setStyleSheet(blopSliderStyleSheet());
  }

  auto updatePreviewHex = [&]() {
    cur.setRgb(sr->value(), sg->value(), sb->value());
    preview->setStyleSheet(QStringLiteral(
        "background-color: %1; border-radius: 12px;"
        "border: 2px solid rgba(255,255,255,0.2); min-height: %2px;")
                               .arg(cur.name())
                               .arg(kPreviewH));
    hexEdit->blockSignals(true);
    hexEdit->setText(cur.name(QColor::HexRgb).toUpper());
    hexEdit->blockSignals(false);
  };

  sr->setValue(cur.red());
  sg->setValue(cur.green());
  sb->setValue(cur.blue());
  updatePreviewHex();

  QObject::connect(sr, &QSlider::valueChanged, card, [&](int) { updatePreviewHex(); });
  QObject::connect(sg, &QSlider::valueChanged, card, [&](int) { updatePreviewHex(); });
  QObject::connect(sb, &QSlider::valueChanged, card, [&](int) { updatePreviewHex(); });

  QObject::connect(hexEdit, &QLineEdit::editingFinished, card, [&]() {
    QString t = hexEdit->text().trimmed();
    if (t.startsWith(QLatin1Char('#')))
      t = t.mid(1);
    if (t.length() == 6) {
      bool ok = false;
      const int v = t.toInt(&ok, 16);
      if (ok) {
        cur.setRgb((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
        sr->blockSignals(true);
        sg->blockSignals(true);
        sb->blockSignals(true);
        sr->setValue(cur.red());
        sg->setValue(cur.green());
        sb->setValue(cur.blue());
        sr->blockSignals(false);
        sg->blockSignals(false);
        sb->blockSignals(false);
        updatePreviewHex();
      }
    }
  });

  for (int i = 0; i < swatchBtns.size(); ++i) {
    const QColor sc = kBlopSwatches[i];
    QObject::connect(swatchBtns[i], &QPushButton::clicked, card, [&, sc]() {
      cur = sc;
      sr->blockSignals(true);
      sg->blockSignals(true);
      sb->blockSignals(true);
      sr->setValue(cur.red());
      sg->setValue(cur.green());
      sb->setValue(cur.blue());
      sr->blockSignals(false);
      sg->blockSignals(false);
      sb->blockSignals(false);
      updatePreviewHex();
    });
  }

  lay->addWidget(preview);
  lay->addLayout(hexRow);

  auto *sep = new QFrame(inner);
  sep->setFrameShape(QFrame::HLine);
  sep->setFixedHeight(1);
  sep->setStyleSheet(QStringLiteral(
      "background: rgba(120, 130, 160, 0.22); border: none; max-height: 1px;"));
  lay->addWidget(sep);

  lay->addLayout(swatchGrid);

  auto *sep2 = new QFrame(inner);
  sep2->setFrameShape(QFrame::HLine);
  sep2->setFixedHeight(1);
  sep2->setStyleSheet(QStringLiteral(
      "background: rgba(120, 130, 160, 0.22); border: none; max-height: 1px;"));
  lay->addWidget(sep2);

  auto *rgbHead = new QLabel(QStringLiteral("RGB mischen"), inner);
  rgbHead->setStyleSheet(QStringLiteral(
      "color: rgba(160, 168, 195, 0.88); font-size: 12px; font-weight: 600;"));
  lay->addWidget(rgbHead);
  lay->addSpacing(4);

  auto addRgb = [&](const QString &letter, QSlider *s) {
    auto *row = new QHBoxLayout();
    row->setSpacing(10);
    row->addWidget(makeRgbLabel(letter, inner));
    row->addWidget(s, 1);
    lay->addLayout(row);
  };
  addRgb(QStringLiteral("R"), sr);
  addRgb(QStringLiteral("G"), sg);
  addRgb(QStringLiteral("B"), sb);

  lay->addStretch(0);

  cardLay->addWidget(scroll, 1);

  QEventLoop loop;
  bool accepted = false;

  auto finish = [&](bool ok) {
    accepted = ok;
    if (ok) {
      cur.setRgb(sr->value(), sg->value(), sb->value());
      *color = QColor(cur.red(), cur.green(), cur.blue(), keepAlpha);
    }
    loop.quit();
  };

  overlay->setCard(card);
  overlay->onDismiss = [&]() { finish(false); };

  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok,
                                     card);
  bbox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
  bbox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Übernehmen"));
  bbox->button(QDialogButtonBox::Cancel)->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QPushButton { color: #8EB8F8; border: none; background: transparent; "
      "font-weight:600; padding:10px 18px; }"
      "QPushButton:hover { color: #B8D4FF; background: rgba(255,255,255,0.06); "
      "border-radius: 10px; }")));
  bbox->button(QDialogButtonBox::Ok)->setStyleSheet(QStringLiteral(
      "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
      "  stop:0 #8EB8F8, stop:1 #6BA3F5); color: #0f172a; border: none; "
      "border-radius: 10px; padding: 12px 24px; font-weight: 600; }"
      "QPushButton:hover { background: #7EB0F7; }"
      "QPushButton:pressed { background: #5A94E8; }"));
  cardLay->addWidget(bbox, 0, Qt::AlignRight);

  QObject::connect(bbox, &QDialogButtonBox::accepted, overlay,
                     [&]() { finish(true); });
  QObject::connect(bbox, &QDialogButtonBox::rejected, overlay,
                     [&]() { finish(false); });

  overlay->show();
  overlay->raise();
  loop.exec();

  overlay->deleteLater();
  return accepted;
}

/// Library-style A4 miniature for the layout picker (lined/grid/dots/legal).
static QString a4LayoutCardQss(const QString &objectName, bool noteChrome) {
  if (noteChrome) {
    const QString accent = NoteChrome::accent().name(QColor::HexRgb);
    return NoteChrome::overlayCardStyle(objectName) + QStringLiteral(
        "QLabel { color: %1; }"
        "QToolButton {"
        "  border-radius: 12px; border: 1px solid %2; background: %3;"
        "  color: %1; font-size: 11px; font-weight: 600; padding: 6px 6px;"
        "}"
        "QToolButton:hover { background: %4; border-color: %5; }"
        "QToolButton:checked { border: 2px solid %5; background: %6; }"
        "QDialogButtonBox QPushButton {"
        "  min-height: 34px; min-width: 84px; border-radius: 10px; font-weight: 600;"
        "}")
        .arg(NoteChrome::textPrimary().name(QColor::HexRgb),
             NoteChrome::border().name(QColor::HexRgb),
             NoteChrome::toolbarFill().name(QColor::HexRgb),
             NoteChrome::panelBg().name(QColor::HexRgb), accent,
             NoteChrome::accentSoft().name(QColor::HexArgb));
  }
  return BlopStyle::surfaceStyle(objectName) +
         BlopTheme::themed(QStringLiteral(
             "QLabel { color: rgba(235, 237, 245, 0.95); }"
             "QToolButton {"
             "  border-radius: 12px;"
             "  border: 1px solid rgba(132, 144, 182, 0.36);"
             "  background: rgba(30, 34, 50, 0.92);"
             "  color: rgba(236, 239, 248, 0.96);"
             "  font-size: 11px; font-weight: 600; padding: 6px 6px;"
             "}"
             "QToolButton:hover {"
             "  background: rgba(44, 49, 72, 0.95);"
             "  border-color: rgba(156, 174, 226, 0.62);"
             "}"
             "QToolButton:checked {"
             "  border: 2px solid #7C5CFC;"
             "  background: rgba(124, 92, 252, 0.18);"
             "}"
             "QDialogButtonBox QPushButton {"
             "  min-height: 34px; min-width: 84px; border-radius: 10px; font-weight: 600;"
             "}"));
}

static QString a4TitleQss(bool noteChrome) {
  if (noteChrome)
    return QStringLiteral(
               "color: %1; font-size: 18px; font-weight: 700; letter-spacing: 0.2px; "
               "background: transparent;")
        .arg(NoteChrome::textPrimary().name(QColor::HexRgb));
  return BlopTheme::typeQss(BlopTheme::TextRole::HeadlineMedium) +
         QStringLiteral("letter-spacing: 0.2px;");
}

static QString a4MutedQss(bool noteChrome, int fontPx) {
  if (noteChrome)
    return QStringLiteral("color: %1; font-size: %2px; background: transparent;")
        .arg(NoteChrome::textSecondary().name(QColor::HexRgb))
        .arg(fontPx);
  return BlopTheme::themed(
      QStringLiteral("color: rgba(180,178,200,0.88); font-size: %1px;")
          .arg(fontPx));
}

static QString a4SectionQss(bool noteChrome) {
  if (noteChrome)
    return QStringLiteral(
               "font-size: 13px; font-weight: 600; color: %1; background: transparent;")
        .arg(NoteChrome::textPrimary().name(QColor::HexRgb));
  return BlopTheme::themed(QStringLiteral(
      "font-size: 13px; font-weight: 600; color: rgba(225, 230, 246, 0.92);"));
}

static void styleA4ActionButtons(QAbstractButton *cancel, QAbstractButton *ok,
                                 bool noteChrome) {
  if (!cancel || !ok)
    return;
  if (noteChrome) {
    const QString accent = NoteChrome::accent().name(QColor::HexRgb);
    cancel->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; border: 1px solid %2; background: %3; "
        "border-radius: 12px; font-weight: 700; padding: 10px 18px; }"
        "QPushButton:hover { color: %4; background: %5; }")
                              .arg(NoteChrome::textSecondary().name(QColor::HexRgb),
                                   NoteChrome::border().name(QColor::HexRgb),
                                   NoteChrome::panelBg().name(QColor::HexRgb),
                                   NoteChrome::textPrimary().name(QColor::HexRgb),
                                   NoteChrome::toolbarFill().name(QColor::HexRgb)));
    ok->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: #0f172a; border: none; "
        "border-radius: 12px; padding: 12px 28px; font-weight: 700; }"
        "QPushButton:hover { background: %2; }")
                          .arg(accent, NoteChrome::accent().lighter(110).name(
                                           QColor::HexRgb)));
  } else {
    cancel->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { color: #8EB8F8; border: none; background: transparent; "
        "font-weight:600; padding:10px 18px; }"
        "QPushButton:hover { color: #B8D4FF; background: rgba(255,255,255,0.06); "
        "border-radius: 10px; }")));
    ok->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { background: #7C5CFC; color: white; border: none; "
        "border-radius: 10px; padding: 12px 28px; font-weight: 600; }"
        "QPushButton:hover { background: #8B70FF; }"
        "QPushButton:pressed { background: #6A4DE6; }")));
  }
}

static QIcon makePageTemplateIcon(PageBackgroundType t, int w, int h,
                                  const QColor &paper = QColor(252, 250, 245)) {
  NotePreviewIcon::Spec spec;
  spec.kind = NotePreviewIcon::Kind::A4;
  spec.backgroundType = static_cast<int>(t);
  spec.paper = paper.isValid() ? paper : QColor(252, 250, 245);
  const int px = qMax(w, h);
  const QPixmap src = NotePreviewIcon::pixmap(spec, px);
  if (src.isNull())
    return {};
  if (src.size() == QSize(w, h))
    return QIcon(src);
  return QIcon(src.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

bool showA4LayoutOverlay(QWidget *parent, const QString &windowTitle,
                        const QString &subtitle, int initialType,
                        const QColor &initialPaper, A4LayoutDialogResult *out,
                        bool noteChrome) {
  if (!parent || !out)
    return false;

  out->accepted = false;

  auto *overlay = new OverlayHost(parent);

  auto *card = new QFrame(overlay);
  card->setObjectName(QStringLiteral("A4LayoutCard"));
  // v3.16.1: card surface from BlopStyle so the A4 layout overlay shares the
  // visual surface with all other overlays.
  card->setStyleSheet(a4LayoutCardQss(QStringLiteral("A4LayoutCard"), noteChrome));

  QScreen *screen = nullptr;
  if (QWidget *w = parent->window())
    screen = w->screen();
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
#ifdef Q_OS_ANDROID
  // Phones are narrower than the 780-px desktop card, so let the card
  // grow to fill almost the whole screen with just a small dp(12) margin
  // on each side. Account for the system top/bottom insets so we don't
  // overlap the status bar or gesture bar.
  const int marginH = UiScale::dp(12);
  const int safeTop = UiScale::safeTopPx(parent);
  const int safeBot = UiScale::safeBottomPx(parent);
  const int cardW = qMax(280, avail.width() - 2 * marginH);
  const int cardH =
      qMax(420, avail.height() - safeTop - safeBot - 2 * marginH);
#else
  // Compact centered card — no more full-width stretched overlay.
  const int cardW = qMin(440, qMax(360, avail.width() * 42 / 100));
  const int cardH = qMin(560, qMax(460, avail.height() * 72 / 100));
#endif
  centerCardInOverlay(overlay, card, cardW, cardH);

  auto *root = new QVBoxLayout(card);
#ifdef Q_OS_ANDROID
  // Tighter card padding on Android so the layout option grid still fits
  // comfortably even on a 360-dp wide phone.
  root->setContentsMargins(UiScale::dp(20), UiScale::dp(18),
                           UiScale::dp(20), UiScale::dp(16));
  root->setSpacing(UiScale::dp(14));
#else
  root->setContentsMargins(24, 22, 24, 18);
  root->setSpacing(14);
#endif

  auto *titleLbl = new QLabel(windowTitle, card);
  titleLbl->setStyleSheet(a4TitleQss(noteChrome));
  root->addWidget(titleLbl);

  if (!subtitle.isEmpty()) {
    auto *sub = new QLabel(subtitle, card);
    sub->setWordWrap(true);
    sub->setStyleSheet(a4MutedQss(noteChrome, 13));
    root->addWidget(sub);
  }

  auto *basics = new QWidget(card);
  auto *basicsRoot = new QVBoxLayout(basics);
  basicsRoot->setContentsMargins(0, 4, 0, 0);
  basicsRoot->setSpacing(12);

  auto *colorRow = new QHBoxLayout();
  colorRow->setSpacing(12);
  auto *lblColor = new QLabel(QStringLiteral("Seitenfarbe"), basics);
  lblColor->setStyleSheet(a4SectionQss(noteChrome));
  colorRow->addWidget(lblColor);
  QColor paperColor = initialPaper.isValid() ? initialPaper : QColor(Qt::white);
  auto *colorBtn = new QPushButton(basics);
  colorBtn->setFixedSize(UiScale::dp(44), UiScale::dp(32));
  colorBtn->setCursor(Qt::PointingHandCursor);
  auto refreshColorBtn = [colorBtn, &paperColor]() {
    colorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; border: 1px solid rgba(180,180,200,0.45); "
        "border-radius: 10px; }")
                                .arg(paperColor.name()));
  };
  refreshColorBtn();
  colorRow->addWidget(colorBtn);
  colorRow->addStretch();
  basicsRoot->addLayout(colorRow);

  auto *g = new QGridLayout();
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(10);
  g->setColumnStretch(0, 1);
  g->setColumnStretch(1, 1);
  g->setColumnStretch(2, 1);
  int chosen = qBound(0, initialType, static_cast<int>(PageBackgroundType::Legal));
  auto *tplGroup = new QButtonGroup(card);
  tplGroup->setExclusive(true);

  constexpr int kTplIconW = 72;
  constexpr int kTplIconH = 72;
  QVector<QPair<QToolButton *, PageBackgroundType>> tplBtns;
  auto refreshTplIcons = [&]() {
    for (const auto &pair : tplBtns)
      pair.first->setIcon(
          makePageTemplateIcon(pair.second, kTplIconW, kTplIconH, paperColor));
  };
  QObject::connect(colorBtn, &QPushButton::clicked, card, [&, overlay]() {
    QWidget *cpHost = parent->window();
    if (!cpHost)
      cpHost = parent;
    // Seitenlayout ausblenden, sonst liegen zwei Modale übereinander (Fertig + Farbe).
    overlay->hide();
    const bool ok = showColorPickerOverlay(cpHost, &paperColor,
                                           QStringLiteral("Seitenfarbe wählen"));
    overlay->show();
    overlay->raise();
    if (ok) {
      refreshColorBtn();
      refreshTplIcons();
    }
  });
  auto addTplBtn = [&](int row, int col, const QString &name, PageBackgroundType t) {
    auto *tb = new QToolButton(basics);
    tb->setText(name);
    tb->setIcon(makePageTemplateIcon(t, kTplIconW, kTplIconH, paperColor));
    tb->setIconSize(QSize(kTplIconW, kTplIconH));
    tb->setCheckable(true);
    tb->setFixedSize(112, 128);
    tb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    const int ti = static_cast<int>(t);
    QObject::connect(tb, &QToolButton::toggled, card, [ti, &chosen](bool on) {
      if (on)
        chosen = ti;
    });
    tplGroup->addButton(tb);
    tplBtns.append({tb, t});
    g->addWidget(tb, row, col, Qt::AlignCenter);
    return tb;
  };

  QToolButton *bBlank =
      addTplBtn(0, 0, QStringLiteral("Weiß / leer"), PageBackgroundType::Blank);
  QToolButton *bLined =
      addTplBtn(0, 1, QStringLiteral("Liniert"), PageBackgroundType::Lined);
  QToolButton *bGrid =
      addTplBtn(0, 2, QStringLiteral("Kariert"), PageBackgroundType::Grid);
  QToolButton *bDot =
      addTplBtn(1, 0, QStringLiteral("Punktiert"), PageBackgroundType::Dotted);
  QToolButton *bLeg =
      addTplBtn(1, 1, QStringLiteral("Legal"), PageBackgroundType::Legal);

  QToolButton *initialTpl = bGrid;
  switch (chosen) {
  case static_cast<int>(PageBackgroundType::Blank):
    initialTpl = bBlank;
    break;
  case static_cast<int>(PageBackgroundType::Lined):
    initialTpl = bLined;
    break;
  case static_cast<int>(PageBackgroundType::Grid):
    initialTpl = bGrid;
    break;
  case static_cast<int>(PageBackgroundType::Dotted):
    initialTpl = bDot;
    break;
  case static_cast<int>(PageBackgroundType::Legal):
    initialTpl = bLeg;
    break;
  default:
    break;
  }
  initialTpl->setChecked(true);

  basicsRoot->addLayout(g, 1);
  root->addWidget(basics, 1);

  auto *bbox =
      new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, card);
  bbox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
  bbox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Fertig"));
  styleA4ActionButtons(bbox->button(QDialogButtonBox::Cancel),
                       bbox->button(QDialogButtonBox::Ok), noteChrome);
  root->addSpacing(14);
  root->addWidget(bbox, 0, Qt::AlignRight);

  QEventLoop loop;
  bool accepted = false;

  auto finish = [&](bool ok) {
    accepted = ok;
    loop.quit();
  };

  overlay->setCard(card);
  overlay->onDismiss = [&]() { finish(false); };

  QObject::connect(bbox, &QDialogButtonBox::accepted, overlay,
                     [&]() { finish(true); });
  QObject::connect(bbox, &QDialogButtonBox::rejected, overlay,
                     [&]() { finish(false); });

  overlay->show();
  overlay->raise();
  loop.exec();

  if (accepted) {
    out->accepted = true;
    out->backgroundType =
        qBound(0, chosen, static_cast<int>(PageBackgroundType::Legal));
    out->paperColor = paperColor;
  }

  overlay->deleteLater();
  return out->accepted;
}

void showA4LayoutOverlayAsync(
    QWidget *parent, const QString &windowTitle, const QString &subtitle,
    int initialType, const QColor &initialPaper,
    std::function<void(const A4LayoutDialogResult &)> onDone, bool noteChrome) {
  if (!parent) {
    if (onDone)
      onDone(A4LayoutDialogResult{});
    return;
  }

  auto *overlay = new OverlayHost(parent);
  overlay->setObjectName(QStringLiteral("AndroidTransientOverlay"));
  auto *card = new QFrame(overlay);
  card->setObjectName(QStringLiteral("A4LayoutCardAsync"));
  card->setStyleSheet(a4LayoutCardQss(QStringLiteral("A4LayoutCardAsync"), noteChrome));

  int cardW = 420;
  int cardH = 520;
#ifdef Q_OS_ANDROID
  cardW = qBound(UiScale::dp(300), int(qreal(qMax(1, parent->width())) * 0.94),
                 UiScale::dp(540));
  cardH = qBound(UiScale::dp(360), int(qreal(qMax(1, parent->height())) * 0.82),
                 UiScale::dp(680));
#endif
  centerCardInOverlay(overlay, card, cardW, cardH);

  auto *root = new QVBoxLayout(card);
#ifdef Q_OS_ANDROID
  root->setContentsMargins(UiScale::dp(20), UiScale::dp(18), UiScale::dp(20),
                           UiScale::dp(16));
  root->setSpacing(UiScale::dp(10));
#else
  root->setContentsMargins(22, 20, 22, 16);
  root->setSpacing(12);
#endif

  auto *titleLbl = new QLabel(windowTitle, card);
  titleLbl->setStyleSheet(a4TitleQss(noteChrome));
  root->addWidget(titleLbl);
  if (!subtitle.isEmpty()) {
    auto *sub = new QLabel(subtitle, card);
    sub->setWordWrap(true);
    sub->setStyleSheet(a4MutedQss(noteChrome, 14));
    root->addWidget(sub);
  }

  auto chosen = std::make_shared<int>(
      qBound(0, initialType, static_cast<int>(PageBackgroundType::Legal)));
  auto paperColor = std::make_shared<QColor>(
      initialPaper.isValid() ? initialPaper : QColor(Qt::white));

  auto *colorRow = new QHBoxLayout();
  colorRow->setSpacing(12);
  auto *lblColor = new QLabel(QStringLiteral("Seitenfarbe"), card);
  lblColor->setStyleSheet(a4SectionQss(noteChrome));
  colorRow->addWidget(lblColor);
  auto *colorBtn = new QPushButton(card);
  colorBtn->setFixedSize(UiScale::dp(44), UiScale::dp(32));
  colorBtn->setCursor(Qt::PointingHandCursor);
  auto refreshColorBtn = [colorBtn, paperColor]() {
    colorBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; border: 1px solid rgba(180,180,200,0.45); "
        "border-radius: 10px; }")
                                .arg(paperColor->name()));
  };
  refreshColorBtn();
  colorRow->addWidget(colorBtn);
  colorRow->addStretch();
  root->addLayout(colorRow);

  auto *grid = new QGridLayout();
  grid->setHorizontalSpacing(10);
  grid->setVerticalSpacing(10);
  auto *tplGroup = new QButtonGroup(card);
  tplGroup->setExclusive(true);
  int kTplIconW = 72;
  int kTplIconH = 72;
  QSize tplButtonSize(112, 128);
#ifdef Q_OS_ANDROID
  kTplIconW = UiScale::dp(84);
  kTplIconH = UiScale::dp(84);
  tplButtonSize = QSize(UiScale::dp(118), UiScale::dp(148));
#endif
  auto tplBtns =
      std::make_shared<QVector<QPair<QToolButton *, PageBackgroundType>>>();
  auto refreshTplIcons = [tplBtns, paperColor, kTplIconW, kTplIconH]() {
    for (const auto &pair : *tplBtns)
      pair.first->setIcon(makePageTemplateIcon(pair.second, kTplIconW, kTplIconH,
                                               *paperColor));
  };
  QObject::connect(colorBtn, &QPushButton::clicked, card,
                   [parent, overlay, paperColor, refreshColorBtn,
                    refreshTplIcons]() {
                     QWidget *cpHost = parent->window();
                     if (!cpHost)
                       cpHost = parent;
                     overlay->hide();
                     const bool ok = showColorPickerOverlay(
                         cpHost, paperColor.get(),
                         QStringLiteral("Seitenfarbe wählen"));
                     overlay->show();
                     overlay->raise();
                     if (ok) {
                       refreshColorBtn();
                       refreshTplIcons();
                     }
                   });
  auto addTplBtn = [&](int row, int col, const QString &name, PageBackgroundType t) {
    auto *tb = new QToolButton(card);
    tb->setText(name);
    tb->setIcon(makePageTemplateIcon(t, kTplIconW, kTplIconH, *paperColor));
    tb->setIconSize(QSize(kTplIconW, kTplIconH));
    tb->setCheckable(true);
    tb->setFixedSize(tplButtonSize);
    tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    const int ti = static_cast<int>(t);
    QObject::connect(tb, &QToolButton::toggled, card, [ti, chosen](bool on) {
      if (on)
        *chosen = ti;
    });
    tplGroup->addButton(tb);
    tplBtns->append({tb, t});
    grid->addWidget(tb, row, col);
    return tb;
  };

  QToolButton *bBlank =
      addTplBtn(0, 0, QStringLiteral("Weiß / leer"), PageBackgroundType::Blank);
  QToolButton *bLined =
      addTplBtn(0, 1, QStringLiteral("Liniert"), PageBackgroundType::Lined);
  QToolButton *bGrid =
      addTplBtn(0, 2, QStringLiteral("Kariert"), PageBackgroundType::Grid);
  QToolButton *bDot =
      addTplBtn(1, 0, QStringLiteral("Punktiert"), PageBackgroundType::Dotted);
  QToolButton *bLeg =
      addTplBtn(1, 1, QStringLiteral("Legal"), PageBackgroundType::Legal);
  QToolButton *initialTpl = bGrid;
  switch (*chosen) {
  case static_cast<int>(PageBackgroundType::Blank):
    initialTpl = bBlank;
    break;
  case static_cast<int>(PageBackgroundType::Lined):
    initialTpl = bLined;
    break;
  case static_cast<int>(PageBackgroundType::Grid):
    initialTpl = bGrid;
    break;
  case static_cast<int>(PageBackgroundType::Dotted):
    initialTpl = bDot;
    break;
  case static_cast<int>(PageBackgroundType::Legal):
    initialTpl = bLeg;
    break;
  default:
    break;
  }
  initialTpl->setChecked(true);
  root->addLayout(grid, 1);

  auto finish = [overlay, onDone](const A4LayoutDialogResult &res) {
    if (onDone)
      onDone(res);
    overlay->deleteLater();
  };

#ifdef Q_OS_ANDROID
  auto *actions = new QHBoxLayout();
  actions->setSpacing(UiScale::dp(10));
  auto *btnCancel = new QPushButton(QStringLiteral("Abbrechen"), card);
  btnCancel->setMinimumHeight(UiScale::dp(48));
  btnCancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *btnDone = new QPushButton(QStringLiteral("Fertig"), card);
  btnDone->setMinimumHeight(UiScale::dp(48));
  btnDone->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  if (noteChrome) {
    styleA4ActionButtons(btnCancel, btnDone, true);
  } else {
    btnCancel->setStyleSheet(BlopTheme::themed(
        "QPushButton { background: #262237; color: #E0DBFF; border: 1px solid #3A3550; border-radius: 12px; font-weight: 700; font-size: 15px; padding: 10px 12px; }"
        "QPushButton:hover { background: #312C45; }"));
    btnDone->setStyleSheet(BlopTheme::themed(
        "QPushButton { background: #7C5CFC; color: white; border: none; border-radius: 12px; font-weight: 700; font-size: 15px; padding: 10px 12px; }"
        "QPushButton:hover { background: #957AFF; }"));
  }
  actions->addWidget(btnCancel);
  actions->addWidget(btnDone);
  root->addLayout(actions);
#else
  auto *bbox =
      new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, card);
  bbox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
  bbox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Fertig"));
  styleA4ActionButtons(bbox->button(QDialogButtonBox::Cancel),
                       bbox->button(QDialogButtonBox::Ok), noteChrome);
  root->addWidget(bbox, 0, Qt::AlignRight);
#endif

  overlay->setCard(card);
  overlay->onDismiss = [finish]() { finish(A4LayoutDialogResult{}); };
#ifdef Q_OS_ANDROID
  QObject::connect(btnCancel, &QPushButton::clicked, overlay,
                   [finish]() { finish(A4LayoutDialogResult{}); });
  QObject::connect(btnDone, &QPushButton::clicked, overlay,
                   [finish, chosen, paperColor]() {
                     A4LayoutDialogResult r;
                     r.accepted = true;
                     r.backgroundType =
                         qBound(0, *chosen, static_cast<int>(PageBackgroundType::Legal));
                     r.paperColor = *paperColor;
                     finish(r);
                   });
#else
  QObject::connect(bbox, &QDialogButtonBox::rejected, overlay,
                   [finish]() { finish(A4LayoutDialogResult{}); });
  QObject::connect(bbox, &QDialogButtonBox::accepted, overlay,
                   [finish, chosen, paperColor]() {
                     A4LayoutDialogResult r;
                     r.accepted = true;
                     r.backgroundType =
                         qBound(0, *chosen, static_cast<int>(PageBackgroundType::Legal));
                     r.paperColor = *paperColor;
                     finish(r);
                   });
#endif

  overlay->show();
  overlay->raise();
}

