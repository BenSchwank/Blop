#include "androidphonetoolbar.h"

#include "blop_diag.h"
#include "blop_inwindow_menu.h"
#include "bloommenu.h"
#include "blop_theme.h"
#include "moderntoolbar.h"
#include "blopstyle.h"
#include "editoroverlays.h"
#include "tools/ToolUIBridge.h"
#include "uiscale.h"

#include <QAction>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QAbstractAnimation>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QSlider>
#include <QVBoxLayout>

namespace {

// Local copy of the Blop menu QSS so this class doesn't reach into mainwindow.cpp.
// Kept in sync with blopWebMenuStyleSheet() in src/ui/mainwindow.cpp ~171.
QString phoneMenuStyleSheet() {
  // v3.17.1: route through themed() so Light mode sees a white-on-light
  // menu surface instead of #14121F-on-light-bg (which would look like a
  // misplaced dark popover).
  return BlopTheme::themed(QString::fromUtf8(
      R"(QMenu { background-color: #14121F; border: 1px solid rgba(124, 92, 252, 0.42); border-radius: 12px; padding: 6px; }
QMenu::separator { height: 1px; background: rgba(255,255,255,0.08); margin: 6px 12px; }
QMenu::item { color: #E8E4FF; padding: 12px 22px; border-radius: 8px; font-size: 14px; font-weight: 500; }
QMenu::item:selected { background-color: rgba(124, 92, 252, 0.38); color: #FFFFFF; })"));
}

// Transparent full-window child that catches outside-taps for the brush sheet.
// The actual sheet is a SIBLING positioned above the backdrop in z-order, so
// taps on the sheet area reach the sheet first (slider, label) and never come
// down to the backdrop. Taps anywhere else hit the backdrop and dismiss both.
// Plain QWidget subclass (no Q_OBJECT) - AUTOMOC skips it cleanly.
class PhoneBrushBackdrop : public QWidget {
public:
  explicit PhoneBrushBackdrop(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
    m_scrim = BlopStyle::backdrop(/*forAndroid=*/true);
  }
  QPointer<QWidget> sheet;

protected:
  void paintEvent(QPaintEvent *e) override {
    Q_UNUSED(e);
    QPainter p(this);
    QColor c = m_scrim;
    c.setAlphaF(c.alphaF() * m_progress);
    p.fillRect(rect(), c);
  }

  void showEvent(QShowEvent *e) override {
    QWidget::showEvent(e);
    m_progress = 0.0;
    update();
    auto *fade = new QVariantAnimation(this);
    fade->setDuration(BlopMotion::kFast);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(BlopMotion::kEaseStandard);
    connect(fade, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) {
              m_progress = v.toReal();
              update();
            });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
  }

  void mousePressEvent(QMouseEvent *e) override {
    Q_UNUSED(e);
    if (sheet)
      sheet->deleteLater();
    close();
  }

private:
  QColor m_scrim;
  qreal m_progress{0.0};
};

} // namespace

// Blop Rail grip: small teardrop (round on three corners, pointed toward the
// screen corner) that shows the active tool glyph while the pill is collapsed.
class RailGrip : public QWidget {
  Q_OBJECT
  Q_PROPERTY(double appear READ appear WRITE setAppear)
public:
  explicit RailGrip(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    parent->installEventFilter(this);
  }

  void reposition() {
    QWidget *host = parentWidget();
    if (!host)
      return;
    move(host->width() - width() - UiScale::dp(10),
         host->height() - height() - UiScale::safeBottomPx(this) -
             UiScale::dp(8));
    raise();
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == parentWidget() && event->type() == QEvent::Resize)
      reposition();
    return QWidget::eventFilter(watched, event);
  }

  void setGlyph(const QString &name) {
    if (m_glyph == name)
      return;
    m_glyph = name;
    update();
  }
  void setAccent(const QColor &c) {
    m_accent = c;
    update();
  }

  double appear() const { return m_appear; }
  void setAppear(double v) {
    m_appear = v;
    update();
  }

signals:
  void tapped();

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setOpacity(qBound(0.0, m_appear, 1.0));

    const qreal s = 0.6 + 0.4 * qBound(0.0, m_appear, 1.0);
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(s, s);
    p.translate(-width() / 2.0, -height() / 2.0);

    const QRectF r = rect().adjusted(1.5, 1.5, -1.5, -1.5);
    const qreal big = r.height() * 0.5;
    const qreal tip = r.height() * 0.14;
    // Teardrop: three round corners, bottom-right pointed toward the corner.
    QPainterPath path;
    path.moveTo(r.left() + big, r.top());
    path.lineTo(r.right() - big, r.top());
    path.arcTo(QRectF(r.right() - 2 * big, r.top(), 2 * big, 2 * big), 90, -90);
    path.lineTo(r.right(), r.bottom() - tip);
    path.arcTo(QRectF(r.right() - 2 * tip, r.bottom() - 2 * tip, 2 * tip,
                      2 * tip),
               0, -90);
    path.lineTo(r.left() + big, r.bottom());
    path.arcTo(QRectF(r.left(), r.bottom() - 2 * big, 2 * big, 2 * big), 270,
               -90);
    path.lineTo(r.left(), r.top() + big);
    path.arcTo(QRectF(r.left(), r.top(), 2 * big, 2 * big), 180, -90);
    path.closeSubpath();

    p.fillPath(path, BlopStyle::surfaceBg());
    QColor border = m_accent;
    border.setAlphaF(0.55);
    p.setPen(QPen(border, 1.5));
    p.drawPath(path);

    const qreal glyphSize = r.height() * 0.52;
    QRectF glyphRect(r.center().x() - glyphSize / 2,
                     r.center().y() - glyphSize / 2, glyphSize, glyphSize);
    p.save();
    p.translate(glyphRect.topLeft());
    p.scale(glyphSize / 64.0, glyphSize / 64.0);
    blopDrawToolbarGlyph64(&p, m_glyph, m_accent);
    p.restore();
  }

  void mousePressEvent(QMouseEvent *e) override { e->accept(); }
  void mouseReleaseEvent(QMouseEvent *e) override {
    if (rect().contains(e->pos()))
      emit tapped();
    e->accept();
  }

private:
  QString m_glyph{QStringLiteral("pen")};
  QColor m_accent{QColor("#7C5CFC")};
  double m_appear{0.0};
};

AndroidPhoneToolbar::AndroidPhoneToolbar(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, false);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFocusPolicy(Qt::NoFocus);
  setupButtons();
  setToolMode(ToolMode::Pen);
}

void AndroidPhoneToolbar::setupButtons() {
  btnPen = new ToolbarBtn(QStringLiteral("pen"), this);
  btnEraser = new ToolbarBtn(QStringLiteral("eraser"), this);
  btnLasso = new ToolbarBtn(QStringLiteral("lasso"), this);
  btnUndo = new ToolbarBtn(QStringLiteral("undo"), this);
  btnRedo = new ToolbarBtn(QStringLiteral("redo"), this);
  btnColor = new ToolbarBtn(QStringLiteral("palette"), this);
  btnBrush = new ToolbarBtn(QStringLiteral("brush_size"), this);
  btnOverflow = new ToolbarBtn(QStringLiteral("more"), this);

  const int btnSize = UiScale::dp(32);
  for (auto *b : {btnPen, btnEraser, btnLasso, btnUndo, btnRedo, btnColor,
                  btnBrush, btnOverflow}) {
    b->setBtnSize(btnSize);
    b->setAccentColor(m_accentColor);
    b->setDrawFloatingBg(false);
  }

  m_toolButtons = {btnPen, btnEraser, btnLasso};

  connect(btnPen, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:pen")); selectTool(ToolMode::Pen); });
  connect(btnEraser, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:eraser")); selectTool(ToolMode::Eraser); });
  connect(btnLasso, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:lasso")); selectTool(ToolMode::Lasso); });
  connect(btnUndo, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:undo")); emit undoRequested(); });
  connect(btnRedo, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:redo")); emit redoRequested(); });
  connect(btnColor, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:color")); showColorPicker(); });
  connect(btnBrush, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:brush")); showBrushSizeSheet(); });
  connect(btnOverflow, &ToolbarBtn::clicked, this, [this]() { BlopDiag::recordUiAction(QStringLiteral("toolbar:overflow")); showOverflowMenu(); });

  // Blop Bloom: long-press on any visible tool button fans the full tool
  // palette out of the press point as an arc of squircle petals.
  for (ToolbarBtn *b : {btnPen, btnEraser, btnLasso}) {
    connect(b, &ToolbarBtn::longPressed, this, [this, b]() {
      BlopDiag::recordUiAction(QStringLiteral("toolbar:bloom"));
      showToolBloom(b);
    });
  }
}

void AndroidPhoneToolbar::showToolBloom(ToolbarBtn *anchorBtn) {
  QWidget *win = window();
  if (!win || !anchorBtn)
    return;

  const QVector<BloomItem> items = {
      {QStringLiteral("pen"), ToolMode::Pen},
      {QStringLiteral("pencil"), ToolMode::Pencil},
      {QStringLiteral("highlighter"), ToolMode::Highlighter},
      {QStringLiteral("eraser"), ToolMode::Eraser},
      {QStringLiteral("lasso"), ToolMode::Lasso},
      {QStringLiteral("ruler"), ToolMode::Ruler},
      {QStringLiteral("shape"), ToolMode::Shape},
      {QStringLiteral("stickynote"), ToolMode::StickyNote},
      {QStringLiteral("text"), ToolMode::Text},
      {QStringLiteral("image"), ToolMode::Image},
      {QStringLiteral("hand"), ToolMode::Hand},
  };

  const QPoint anchor = anchorBtn->mapTo(
      win, QPoint(anchorBtn->width() / 2, anchorBtn->height() / 2));
  BloomMenu *bloom = BloomMenu::popup(win, anchor, items, m_mode, m_accentColor);
  if (!bloom)
    return;
  QPointer<AndroidPhoneToolbar> safe(this);
  connect(bloom, &BloomMenu::toolSelected, this, [safe](ToolMode mode) {
    if (safe)
      safe->selectTool(mode);
  });
}

void AndroidPhoneToolbar::setToolMode(ToolMode mode) {
  const bool changed = (m_mode != mode);
  m_mode = mode;
  // Only "tool" buttons (Pen/Eraser/Lasso) carry the active highlight in the
  // visible row. Other modes (selected via overflow) are reflected on the
  // overflow button via a small accent dot in paintEvent.
  for (auto *b : m_toolButtons)
    b->setActive(false);
  ToolbarBtn *active = nullptr;
  if (mode == ToolMode::Pen)
    active = btnPen;
  else if (mode == ToolMode::Eraser)
    active = btnEraser;
  else if (mode == ToolMode::Lasso)
    active = btnLasso;
  if (active) {
    active->setActive(true);
    active->animateSelect();
  }
  if (changed)
    update();
  if (auto *grip = qobject_cast<RailGrip *>(m_grip.data()))
    grip->setGlyph(activeToolIconName());
}

QString AndroidPhoneToolbar::activeToolIconName() const {
  switch (m_mode) {
  case ToolMode::Pen: return QStringLiteral("pen");
  case ToolMode::Pencil: return QStringLiteral("pencil");
  case ToolMode::Highlighter: return QStringLiteral("highlighter");
  case ToolMode::Eraser: return QStringLiteral("eraser");
  case ToolMode::Lasso: return QStringLiteral("lasso");
  case ToolMode::Ruler: return QStringLiteral("ruler");
  case ToolMode::Shape: return QStringLiteral("shape");
  case ToolMode::StickyNote: return QStringLiteral("stickynote");
  case ToolMode::Text: return QStringLiteral("text");
  case ToolMode::Image: return QStringLiteral("image");
  case ToolMode::Hand: return QStringLiteral("hand");
  }
  return QStringLiteral("pen");
}

void AndroidPhoneToolbar::setCollapsed(bool collapsed) {
  if (m_collapsed == collapsed)
    return;
  QWidget *host = parentWidget();
  if (!host)
    return;
  m_collapsed = collapsed;

  if (collapsed) {
    BlopDiag::recordUiAction(QStringLiteral("toolbar:rail-collapse"));
    // Slide the pill below the screen edge, then hide it. The parent keeps
    // updating the (hidden) pill's geometry on resize, so expanding later
    // animates back to an always-correct position.
    const QPoint target(x(), host->height() + UiScale::dp(8));
    auto *slide = new QPropertyAnimation(this, "pos", this);
    slide->setDuration(BlopMotion::kStandard);
    slide->setStartValue(pos());
    slide->setEndValue(target);
    slide->setEasingCurve(BlopMotion::kEaseStandard);
    QPointer<AndroidPhoneToolbar> safe(this);
    connect(slide, &QPropertyAnimation::finished, this, [safe]() {
      if (safe && safe->m_collapsed)
        safe->hide();
    });
    slide->start(QAbstractAnimation::DeleteWhenStopped);

    auto *grip = new RailGrip(host);
    m_grip = grip;
    grip->setGlyph(activeToolIconName());
    grip->setAccent(m_accentColor);
    const int gs = UiScale::dp(46);
    grip->resize(gs, gs);
    grip->reposition();
    grip->show();
    grip->raise();
    connect(grip, &RailGrip::tapped, this,
            [this]() { setCollapsed(false); });

    auto *in = new QPropertyAnimation(grip, "appear", grip);
    in->setDuration(BlopMotion::kEmphasis);
    in->setStartValue(0.0);
    in->setEndValue(1.0);
    in->setEasingCurve(BlopMotion::kEaseOvershoot);
    in->start(QAbstractAnimation::DeleteWhenStopped);
  } else {
    BlopDiag::recordUiAction(QStringLiteral("toolbar:rail-expand"));
    if (QWidget *grip = m_grip.data()) {
      auto *out = new QPropertyAnimation(grip, "appear", grip);
      out->setDuration(BlopMotion::kFast);
      out->setStartValue(1.0);
      out->setEndValue(0.0);
      out->setEasingCurve(BlopMotion::kEaseStandard);
      connect(out, &QPropertyAnimation::finished, grip,
              &QWidget::deleteLater);
      out->start(QAbstractAnimation::DeleteWhenStopped);
      m_grip.clear();
    }

    const int h = preferredHeightPx();
    const QPoint target(x(), qMax(0, host->height() - h -
                                         UiScale::safeBottomPx(this) -
                                         UiScale::dp(8)));
    move(x(), host->height() + UiScale::dp(8));
    show();
    raise();
    auto *slide = new QPropertyAnimation(this, "pos", this);
    slide->setDuration(BlopMotion::kEmphasis);
    slide->setStartValue(pos());
    slide->setEndValue(target);
    slide->setEasingCurve(BlopMotion::kEaseOvershoot);
    slide->start(QAbstractAnimation::DeleteWhenStopped);
  }
}

void AndroidPhoneToolbar::mousePressEvent(QMouseEvent *e) {
  m_swipeTracking = true;
  m_swipeStart = e->pos();
  e->accept();
}

void AndroidPhoneToolbar::mouseMoveEvent(QMouseEvent *e) {
  if (m_swipeTracking && !m_collapsed &&
      e->pos().y() - m_swipeStart.y() > UiScale::dp(22)) {
    m_swipeTracking = false;
    setCollapsed(true);
  }
  e->accept();
}

void AndroidPhoneToolbar::mouseReleaseEvent(QMouseEvent *e) {
  m_swipeTracking = false;
  e->accept();
}

void AndroidPhoneToolbar::setAccentColor(const QColor &color) {
  if (!color.isValid() || m_accentColor == color)
    return;
  m_accentColor = color;
  for (QObject *child : children()) {
    if (auto *b = qobject_cast<ToolbarBtn *>(child)) {
      b->setAccentColor(m_accentColor);
      b->update();
    }
  }
  if (auto *grip = qobject_cast<RailGrip *>(m_grip.data()))
    grip->setAccent(m_accentColor);
  update();
}

int AndroidPhoneToolbar::preferredHeightPx() const {
  return UiScale::dp(44);
}

void AndroidPhoneToolbar::selectTool(ToolMode mode) {
  setToolMode(mode);
  emit toolChanged(mode);
}

void AndroidPhoneToolbar::emitPenConfig() {
  emit penConfigChanged(m_config.penColor, m_config.penWidth);
  ToolUIBridge::instance().setPenColor(m_config.penColor);
  ToolUIBridge::instance().setPenWidth(m_config.penWidth);
}

void AndroidPhoneToolbar::showOverflowMenu() {
  QPointer<AndroidPhoneToolbar> safe(this);

#ifdef Q_OS_ANDROID
  // QMenu allocates a top-level QWindow which triggers
  //   "Failed to acquire deadlock protector for QAndroidPlatformOpenGLWindow"
  // when a background accessibility service holds the EGL surface
  // (Qt 6.10 + Android 16). Use the in-window QFrame replacement instead.
  QList<BlopInWindowMenu::Item> items;
  auto addToolEntry = [&](const QString &label, ToolMode mode) {
    items.append({label, QIcon(), [safe, mode]() {
                    if (safe) safe->selectTool(mode);
                  }, false, false});
  };
  addToolEntry(QStringLiteral("Bleistift"), ToolMode::Pencil);
  addToolEntry(QStringLiteral("Marker"), ToolMode::Highlighter);
  addToolEntry(QStringLiteral("Lineal"), ToolMode::Ruler);
  addToolEntry(QStringLiteral("Form"), ToolMode::Shape);
  addToolEntry(QStringLiteral("Notizzettel"), ToolMode::StickyNote);
  addToolEntry(QStringLiteral("Text"), ToolMode::Text);
  addToolEntry(QStringLiteral("Bild"), ToolMode::Image);
  addToolEntry(QStringLiteral("Hand"), ToolMode::Hand);
  items.append({QString(), QIcon(), {}, false, true});
  items.append({QStringLiteral("Toolbar einklappen"), QIcon(), [safe]() {
                  if (safe) safe->setCollapsed(true);
                }, false, false});
  items.append({QStringLiteral("Zur Übersicht"), QIcon(), [safe]() {
                  if (safe) emit safe->backToOverviewRequested();
                }, false, false});

  // Anchor at the top-center of the overflow pill; helper clamps to window.
  const QPoint anchor =
      btnOverflow->mapToGlobal(QPoint(btnOverflow->width() / 2, 0));
  BlopInWindowMenu::show(this, anchor, items);
  return;
#else
  auto *menu = new QMenu(this);
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->setStyleSheet(phoneMenuStyleSheet());

  auto addToolEntry = [&](const QString &label, ToolMode mode) {
    QAction *a = menu->addAction(label);
    QObject::connect(a, &QAction::triggered, this, [safe, mode]() {
      if (safe)
        safe->selectTool(mode);
    });
  };

  addToolEntry(QStringLiteral("Bleistift"), ToolMode::Pencil);
  addToolEntry(QStringLiteral("Marker"), ToolMode::Highlighter);
  addToolEntry(QStringLiteral("Lineal"), ToolMode::Ruler);
  addToolEntry(QStringLiteral("Form"), ToolMode::Shape);
  addToolEntry(QStringLiteral("Notizzettel"), ToolMode::StickyNote);
  addToolEntry(QStringLiteral("Text"), ToolMode::Text);
  addToolEntry(QStringLiteral("Bild"), ToolMode::Image);
  addToolEntry(QStringLiteral("Hand"), ToolMode::Hand);
  menu->addSeparator();
  QAction *collapse = menu->addAction(QStringLiteral("Toolbar einklappen"));
  QObject::connect(collapse, &QAction::triggered, this, [safe]() {
    if (safe)
      safe->setCollapsed(true);
  });
  QAction *back = menu->addAction(QStringLiteral("Zur Übersicht"));
  QObject::connect(back, &QAction::triggered, this, [safe]() {
    if (safe)
      emit safe->backToOverviewRequested();
  });

  const QPoint anchor =
      btnOverflow->mapToGlobal(QPoint(btnOverflow->width() / 2, 0));
  menu->adjustSize();
  const QSize hint = menu->sizeHint();
  QPoint pos(anchor.x() - hint.width() / 2, anchor.y() - hint.height() - UiScale::dp(6));
  if (pos.x() < UiScale::dp(8))
    pos.setX(UiScale::dp(8));
  menu->popup(pos);
#endif
}

void AndroidPhoneToolbar::showColorPicker() {
  QColor c = m_config.penColor;
  QWidget *host = parentWidget() ? parentWidget() : window();
  if (showColorPickerOverlay(host, &c, QStringLiteral("Stiftfarbe"))) {
    m_config.penColor = c;
    emitPenConfig();
    update();
  }
}

void AndroidPhoneToolbar::showBrushSizeSheet() {
  // Lightweight in-window sheet with one slider 1..30 driving penWidth.
  // Anchored above the brush button, dismissed by tapping outside.
  //
  // Why in-window instead of QDialog(Qt::Popup): on Android (Qt 6.10 inproc,
  // single window surface) creating a top-level popup widget crashes the
  // process. Backdrop + sheet are both plain children of window().
  QWidget *win = window();
  if (!win)
    return;

  auto *backdrop = new PhoneBrushBackdrop(win);
  backdrop->resize(win->size());
  backdrop->move(0, 0);

  auto *sheet = new QFrame(win);
  sheet->setObjectName(QStringLiteral("BlopPhoneBrushSheet"));
  sheet->setAttribute(Qt::WA_DeleteOnClose);
  sheet->setAttribute(Qt::WA_StyledBackground, true);
  sheet->setStyleSheet(BlopTheme::themed(
      QStringLiteral(
          "QFrame#BlopPhoneBrushSheet { background-color: rgba(28,30,46,0.96); "
          "border: 1px solid rgba(124,92,252,0.42); border-radius: 14px; }"
          "QLabel { color: #E8E4FF; %1 }")
          .arg(BlopTheme::typeQss(BlopTheme::TextRole::LabelLarge))));
  backdrop->sheet = sheet;

  auto *layout = new QVBoxLayout(sheet);
  layout->setContentsMargins(UiScale::dp(14), UiScale::dp(12),
                             UiScale::dp(14), UiScale::dp(12));
  layout->setSpacing(UiScale::dp(6));
  auto *title = new QLabel(QStringLiteral("Strichstärke"), sheet);
  layout->addWidget(title);

  auto *valueLabel = new QLabel(QString::number(m_config.penWidth) +
                                    QStringLiteral(" px"),
                                sheet);
  valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  auto *slider = new QSlider(Qt::Horizontal, sheet);
  slider->setRange(1, 30);
  slider->setValue(m_config.penWidth);
  slider->setStyleSheet(BlopTheme::themed(QStringLiteral(
      "QSlider::groove:horizontal { border: 1px solid #333; height: 6px; "
      "background: #121212; margin: 2px 0; border-radius: 3px; }"
      "QSlider::handle:horizontal { background: #7C5CFC; border: 1px solid "
      "#7C5CFC; width: 18px; height: 18px; margin: -7px 0; border-radius: 9px; }"
      "QSlider::sub-page:horizontal { background: rgba(124,92,252,0.6); "
      "border-radius: 3px; }")));

  auto *row = new QHBoxLayout;
  row->addWidget(slider);
  row->addWidget(valueLabel);
  layout->addLayout(row);

  QPointer<AndroidPhoneToolbar> safe(this);
  QObject::connect(slider, &QSlider::valueChanged, sheet,
                   [safe, valueLabel](int v) {
                     valueLabel->setText(QString::number(v) +
                                         QStringLiteral(" px"));
                     if (safe) {
                       safe->m_config.penWidth = v;
                       safe->emitPenConfig();
                     }
                   });

  const int sheetW = UiScale::dp(260);
  const int sheetH = UiScale::dp(86);
  sheet->resize(sheetW, sheetH);

  // Position in window-local coordinates (mapTo(win, ...)) instead of global.
  const QPoint anchor =
      btnBrush->mapTo(win, QPoint(btnBrush->width() / 2, 0));
  QPoint pos(anchor.x() - sheetW / 2, anchor.y() - sheetH - UiScale::dp(8));
  const int margin = UiScale::dp(8);
  pos.setX(qBound(margin, pos.x(), win->width() - sheetW - margin));
  pos.setY(qBound(margin, pos.y(), win->height() - sheetH - margin));
  const QPoint targetPos = pos;
  sheet->move(targetPos + QPoint(0, UiScale::dp(12)));
  sheet->setWindowOpacity(0.0);

  backdrop->show();
  sheet->show();
  sheet->raise();

  auto *slide = new QPropertyAnimation(sheet, "pos", sheet);
  slide->setDuration(BlopMotion::kStandard);
  slide->setStartValue(sheet->pos());
  slide->setEndValue(targetPos);
  slide->setEasingCurve(BlopMotion::kEaseStandard);
  slide->start(QAbstractAnimation::DeleteWhenStopped);

  auto *fade = new QPropertyAnimation(sheet, "windowOpacity", sheet);
  fade->setDuration(BlopMotion::kStandard);
  fade->setStartValue(0.0);
  fade->setEndValue(1.0);
  fade->setEasingCurve(BlopMotion::kEaseStandard);
  fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void AndroidPhoneToolbar::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  // Bottom pill surface (matches BlopStyle::surfaceBg / surfaceBorder).
  const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  const int radius = UiScale::dp(BlopStyle::surfaceRadiusDp() - 4);
  QPainterPath path;
  path.addRoundedRect(r, radius, radius);
  p.fillPath(path, BlopStyle::surfaceBg());
  p.setPen(QPen(BlopStyle::surfaceBorder(), 1.0));
  p.drawPath(path);

  // Vertical separators between groups.
  p.setPen(QPen(QColor(255, 255, 255, 40), 1.0));
  const int sepTop = UiScale::dp(8);
  const int sepBot = height() - UiScale::dp(8);
  for (int x : m_separatorX) {
    p.drawLine(x, sepTop, x, sepBot);
  }
}

void AndroidPhoneToolbar::resizeEvent(QResizeEvent *) {
  // Reihenfolge: Pen Eraser Lasso | Undo Redo | Color Brush | Overflow
  // Buttons werden in 3 Gruppen + Overflow gerendert.
  const int btnSize = UiScale::dp(32);
  const int gap = UiScale::dp(4);
  const int sepPad = UiScale::dp(6);

  struct Group {
    QVector<ToolbarBtn *> btns;
    bool drawSepAfter;
  };
  const QVector<Group> groups = {
      {{btnPen, btnEraser, btnLasso}, true},
      {{btnUndo, btnRedo}, true},
      {{btnColor, btnBrush}, true},
      {{btnOverflow}, false},
  };

  int totalW = 0;
  for (int gi = 0; gi < groups.size(); ++gi) {
    const auto &g = groups[gi];
    totalW += g.btns.size() * btnSize + (g.btns.size() - 1) * gap;
    if (g.drawSepAfter)
      totalW += 2 * sepPad + 1; // padding around 1px separator
  }

  int x = (width() - totalW) / 2;
  if (x < UiScale::dp(8))
    x = UiScale::dp(8);
  const int y = (height() - btnSize) / 2;

  m_separatorX.clear();
  for (int gi = 0; gi < groups.size(); ++gi) {
    const auto &g = groups[gi];
    for (int i = 0; i < g.btns.size(); ++i) {
      ToolbarBtn *b = g.btns[i];
      b->move(x, y);
      x += btnSize;
      if (i < g.btns.size() - 1)
        x += gap;
    }
    if (g.drawSepAfter) {
      x += sepPad;
      m_separatorX.append(x);
      x += 1 + sepPad;
    }
  }
  update();
}

#include "androidphonetoolbar.moc"
