#include "blop_modal.h"

#include "blop_theme.h"
#include "uiscale.h"

#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QEasingCurve>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QLayout>
#include <QShowEvent>
#include <QTimer>
#include <QTouchEvent>
#include <QVBoxLayout>

namespace {
// v3.18.2: aligned to BlopMotion tokens.
constexpr int kBackdropFadeMs = BlopMotion::kFast;
constexpr int kCardEnterMs = BlopMotion::kStandard;
constexpr int kBackdropFadeOutMs = BlopMotion::kFast;
constexpr int kCardExitMs = BlopMotion::kFast;
constexpr int kDragDismissThresholdDp = 80;
constexpr int kDragHandleHeightDp = 28;
} // namespace

BlopModal *BlopModal::present(QWidget *parent, QWidget *content, Mode mode,
                              const QString &accessibleTitle,
                              int preferredCardWidth) {
  if (!parent || !content)
    return nullptr;
  auto *win = parent->window();
  auto *modal = new BlopModal(win ? win : parent, content, mode, accessibleTitle);
  if (preferredCardWidth > 0)
    modal->setPreferredCardWidth(preferredCardWidth);
  modal->show();
  modal->raise();
  modal->startOpenAnim();
  return modal;
}

int BlopModal::execBlocking(QWidget *parent, QDialog *dlg, Mode mode,
                            int preferredCardWidth) {
  if (!parent || !dlg)
    return QDialog::Rejected;

  // Strip the top-level QDialog window flags so reparenting into our card
  // doesn't try to spawn a separate QWindow. On Android, top-level
  // QWindow creation is the path that triggered the v3.16.x
  // QtAndroidAccessibility EGL deadlock; embedding the dialog as a
  // plain child widget keeps us off that path.
  dlg->setWindowFlags(Qt::Widget);
  dlg->setAttribute(Qt::WA_TranslucentBackground, false);
  dlg->setAttribute(Qt::WA_DeleteOnClose, false);

  auto *modal = present(parent, dlg, mode, QString(), preferredCardWidth);
  if (!modal)
    return QDialog::Rejected;

  int result = QDialog::Rejected;
  QEventLoop loop;
  bool dialogFinished = false;

  QObject::connect(dlg, &QDialog::finished, &loop, [&](int code) {
    result = code;
    dialogFinished = true;
    loop.quit();
  });
  // User dismissed via backdrop / drag / ESC before clicking a dialog
  // button -> treat as Rejected, matches QDialog::exec() Esc behaviour.
  QObject::connect(modal, &BlopModal::dismissed, &loop, [&]() {
    if (!dialogFinished)
      result = QDialog::Rejected;
    loop.quit();
  });

  // QDialog hides itself by default; force visible since we don't go
  // through exec() and the caller expects the dialog to render.
  dlg->show();
  loop.exec();

  // If the dialog itself reached finished() the modal is still open ->
  // animate it out so the caller's next setStyleSheet/show isn't racing
  // with a stale backdrop.
  if (dialogFinished && modal && !modal->isHidden())
    modal->dismiss();

  return result;
}

BlopModal::BlopModal(QWidget *parent, QWidget *content, Mode mode,
                     const QString &accessibleTitle)
    : QWidget(parent), m_content(content), m_mode(resolveMode(mode)) {
  setObjectName(QStringLiteral("BlopModalBackdrop"));
  setAttribute(Qt::WA_DeleteOnClose);
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_AcceptTouchEvents, true);
  setFocusPolicy(Qt::StrongFocus);
  if (!accessibleTitle.isEmpty())
    setAccessibleName(accessibleTitle);

  // Cover the parent window completely.
  if (parent)
    setGeometry(parent->rect());

  // Card frame.
  m_card = new QFrame(this);
  QString cardObjName;
  switch (m_mode) {
  case Mode::BottomSheet:
    cardObjName = QStringLiteral("BlopModalSheet");
    break;
  case Mode::SideSheet:
    cardObjName = QStringLiteral("BlopModalSideSheet");
    break;
  case Mode::Card:
  case Mode::Auto:
  default:
    cardObjName = QStringLiteral("BlopModalCard");
    break;
  }
  m_card->setObjectName(cardObjName);
  m_card->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLay = new QVBoxLayout(m_card);
  cardLay->setContentsMargins(0, 0, 0, 0);
  cardLay->setSpacing(0);

  if (m_mode == Mode::BottomSheet) {
    m_dragHandle = new QWidget(m_card);
    m_dragHandle->setObjectName(QStringLiteral("BlopModalDragHandle"));
    m_dragHandle->setFixedHeight(UiScale::dp(kDragHandleHeightDp));
    m_dragHandle->setCursor(Qt::SizeVerCursor);
    auto *handleLay = new QHBoxLayout(m_dragHandle);
    handleLay->setContentsMargins(0, UiScale::dp(10), 0, UiScale::dp(6));
    handleLay->addStretch(1);
    auto *grip = new QFrame(m_dragHandle);
    grip->setObjectName(QStringLiteral("BlopModalDragHandleGrip"));
    grip->setFixedSize(UiScale::dp(40), UiScale::dp(4));
    handleLay->addWidget(grip);
    handleLay->addStretch(1);
    cardLay->addWidget(m_dragHandle, 0);
  }

  content->setParent(m_card);
  // Don't stretch short dialogs to fill 85% of the window height.
  cardLay->addWidget(content, 0);

  applyTheme();
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          &BlopModal::applyTheme);

  // Watch the parent for resize so we can keep the modal full-bleed.
  if (parent) {
    parent->installEventFilter(this);
    m_parentFilterTarget = parent;
  }

  // Make the card opaque to clicks (otherwise clicks would fall through to
  // the backdrop and dismiss).
  m_card->setMouseTracking(true);

  layoutContent();
}

BlopModal::Mode BlopModal::resolveMode(Mode requested) const {
  if (requested != Mode::Auto)
    return requested;
#ifdef Q_OS_ANDROID
  return UiScale::isAndroidTablet(parentWidget()) ? Mode::Card
                                                  : Mode::BottomSheet;
#else
  return Mode::Card;
#endif
}

void BlopModal::setPreferredCardWidth(int px) {
  m_preferredCardWidth = px;
  layoutContent();
}

void BlopModal::applyTheme() {
  setStyleSheet(BlopTheme::scrimQss(QStringLiteral("BlopModalBackdrop")));

  if (m_content) {
    if (m_mode == Mode::Card || m_mode == Mode::Auto) {
      m_content->setAutoFillBackground(false);
      m_content->setStyleSheet(QStringLiteral("background: transparent;"));
    } else {
      m_content->setStyleSheet(
          QStringLiteral("background-color: %1;")
              .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb)));
    }
  }

  if (m_mode == Mode::BottomSheet) {
    QString qss = BlopTheme::bottomSheetQss(QStringLiteral("BlopModalSheet"));
    m_card->setAutoFillBackground(true);
    qss += QStringLiteral(
               "QFrame#BlopModalDragHandleGrip {"
               "  background: %1;"
               "  border-radius: %2px;"
               "  border: none;"
               "}")
               .arg(BlopTheme::borderStrong().name(QColor::HexArgb),
                    QString::number(UiScale::dp(2)));
    m_card->setStyleSheet(qss);
  } else if (m_mode == Mode::SideSheet) {
    // Side sheet: rounded left corners only, full-height right pane.
    QString qss = QStringLiteral(
                      "QFrame#BlopModalSideSheet {"
                      "  background: %1;"
                      "  border: 1px solid %2;"
                      "  border-top-left-radius: %3px;"
                      "  border-bottom-left-radius: %3px;"
                      "  border-top-right-radius: 0px;"
                      "  border-bottom-right-radius: 0px;"
                      "}")
                      .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb),
                           QStringLiteral("rgba(%1,%2,%3,%4)")
                               .arg(BlopTheme::borderDefault().red())
                               .arg(BlopTheme::borderDefault().green())
                               .arg(BlopTheme::borderDefault().blue())
                               .arg(QString::number(
                                   BlopTheme::borderDefault().alphaF(), 'f', 3)),
                           QString::number(BlopTheme::r24));
    m_card->setStyleSheet(qss);
    m_card->setGraphicsEffect(nullptr);
  } else {
    QString qss = QStringLiteral(
                     "QWidget#BlopModalCard {"
                     "  background: %1;"
                     "  border: 1px solid %2;"
                     "  border-radius: %3px;"
                     "}")
                     .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb),
                          QStringLiteral("rgba(%1,%2,%3,%4)")
                              .arg(BlopTheme::borderDefault().red())
                              .arg(BlopTheme::borderDefault().green())
                              .arg(BlopTheme::borderDefault().blue())
                              .arg(QString::number(
                                  BlopTheme::borderDefault().alphaF(), 'f', 3)),
                          QString::number(BlopTheme::r24));
    m_card->setStyleSheet(qss);
    m_card->setAutoFillBackground(true);
    // QGraphicsDropShadowEffect + rounded rect paints an L-shaped black
    // corner on the software rasterizer (Windows light mode / this VM).
    // The overlay scrim already separates the card; keep a clean border.
    m_card->setGraphicsEffect(nullptr);
  }
}

void BlopModal::layoutContent() {
  if (!parentWidget() || !m_card)
    return;
  const int W = width();
  const int H = height();
  const int pad = UiScale::dp(16);

  if (m_mode == Mode::BottomSheet) {
    const int lift = UiScale::safeBottomPx(parentWidget());
    const int sheetMaxH = H - qMax(UiScale::safeTopPx(parentWidget()),
                                   UiScale::dp(24));
    const int sheetH = qMin(sheetMaxH, qMax(UiScale::dp(280),
                                            int(H * 0.96)));
    const int sheetW = W;
    m_card->setGeometry(0, H - sheetH - lift, sheetW, sheetH);
    if (m_content) {
      m_content->setMinimumSize(0, 0);
      m_content->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
      m_content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      if (auto *lay = m_content->layout()) {
        lay->setSizeConstraint(QLayout::SetNoConstraint);
        lay->activate();
      }
    }
    if (auto *cardLay = qobject_cast<QVBoxLayout *>(m_card->layout())) {
      const int idx = cardLay->indexOf(m_content);
      if (idx >= 0)
        cardLay->setStretch(idx, 1);
    }
  } else if (m_mode == Mode::SideSheet) {
    // Desktop/tablet preference panel: width follows preferredCardWidth but
    // may grow with the window. Never cap at a phone-like 760dp — that made
    // Settings look like a skinny centered handset card on large monitors.
    int preferred =
        m_preferredCardWidth > 0 ? m_preferredCardWidth : int(W * 0.58);
    if (preferred < UiScale::dp(420))
      preferred = UiScale::dp(420);
    const int peek = UiScale::dp(48); // strip of underlying UI stays visible
    const int maxW = qMax(UiScale::dp(360), W - peek);
    const int sheetW = qBound(UiScale::dp(360), preferred, maxW);
    m_card->setGeometry(W - sheetW, 0, sheetW, H);
    if (m_content) {
      m_content->setMinimumSize(0, 0);
      m_content->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
      m_content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      if (auto *lay = m_content->layout()) {
        lay->setSizeConstraint(QLayout::SetNoConstraint);
        lay->activate();
      }
    }
  } else {
    const int preferred =
        m_preferredCardWidth > 0 ? m_preferredCardWidth : UiScale::dp(420);
    // Wider overlays (Neue Notiz) stay a centered card sized to the
    // preferred width + content height — never a near-fullscreen sheet.
    const bool sizedCard = preferred >= 700;
    if (sizedCard) {
      const int gap = UiScale::dp(16);
      const int maxW = qMin(int(W * 0.92), qMax(UiScale::dp(320), W - 2 * gap));
      const int minW = qMin(UiScale::dp(640), maxW);
      const int cardW = qBound(minW, preferred, maxW);
      const int maxH = qMin(int(H * 0.94), H - 2 * gap);
      const int cardH = qMax(UiScale::dp(480), maxH);
      m_card->setGeometry((W - cardW) / 2, (H - cardH) / 2, cardW, cardH);
      if (m_content) {
        m_content->setMinimumSize(0, 0);
        m_content->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        if (auto *lay = m_content->layout()) {
          lay->setSizeConstraint(QLayout::SetNoConstraint);
          lay->activate();
        }
      }
      if (auto *cardLay = qobject_cast<QVBoxLayout *>(m_card->layout())) {
        const int idx = cardLay->indexOf(m_content);
        if (idx >= 0)
          cardLay->setStretch(idx, 1);
      }
      return;
    }
    // Compact centered card. Measure height *after* giving content a real
    // width — word-wrapped QLabels otherwise report a skyscraper sizeHint
    // (one glyph per line) and overlays look "extrem lang gestreckt".
    const int cardW = qBound(UiScale::dp(320), preferred, W - 2 * pad);
    int contentH = UiScale::dp(140);
    if (m_content) {
      m_content->setMaximumWidth(cardW);
      m_content->setMinimumWidth(qMin(cardW, UiScale::dp(280)));
      if (auto *lay = m_content->layout())
        lay->activate();
      m_content->adjustSize();
      const QSize hint = m_content->sizeHint().isValid()
                             ? m_content->sizeHint()
                             : m_content->minimumSizeHint();
      // Prefer layout's heightForWidth when available (dialogs with wrap).
      int measured = hint.height();
      if (m_content->hasHeightForWidth())
        measured = qMax(measured, m_content->heightForWidth(cardW));
      contentH = qMax(UiScale::dp(120), measured + UiScale::dp(8));
    }
    const qreal heightFrac = preferred >= UiScale::dp(560) ? 0.92 : 0.72;
    const int maxH = qMin(int(H * heightFrac), H - 2 * pad);
    const int cardH = qBound(UiScale::dp(120), contentH, maxH);
    const int x = (W - cardW) / 2;
    const int y = (H - cardH) / 2;
    m_card->setGeometry(x, y, cardW, cardH);
  }
}

void BlopModal::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  layoutContent();
}

void BlopModal::startOpenAnim() {
#ifdef Q_OS_ANDROID
  // Child-widget windowOpacity + off-screen BottomSheet slides are unreliable
  // on Android and left only the black scrim visible ("glass pane"). Land on
  // the final on-screen layout immediately.
  setWindowOpacity(1.0);
  layoutContent();
  if (m_card) {
    m_card->show();
    m_card->raise();
  }
  QTimer::singleShot(0, this, [this]() {
    if (m_dismissing || !m_card)
      return;
    layoutContent();
    m_card->raise();
  });
  return;
#else
  setWindowOpacity(0.0);
  m_backdropAnim = new QPropertyAnimation(this, "windowOpacity", this);
  m_backdropAnim->setDuration(kBackdropFadeMs);
  m_backdropAnim->setStartValue(0.0);
  m_backdropAnim->setEndValue(1.0);
  m_backdropAnim->setEasingCurve(BlopMotion::kEaseStandard);
  m_backdropAnim->start(QAbstractAnimation::DeleteWhenStopped);

  if (m_card) {
    const QRect endGeom = m_card->geometry();
    QRect startGeom = endGeom;
    if (m_mode == Mode::BottomSheet) {
      startGeom.translate(0, endGeom.height());
    } else if (m_mode == Mode::SideSheet) {
      startGeom.translate(endGeom.width(), 0);
    } else {
      const int shrink = qMin(endGeom.width(), endGeom.height()) / 12;
      startGeom.adjust(shrink, shrink, -shrink, -shrink);
    }
    m_card->setGeometry(startGeom);
    m_cardAnim = new QPropertyAnimation(m_card, "geometry", this);
    m_cardAnim->setDuration(kCardEnterMs);
    m_cardAnim->setStartValue(startGeom);
    m_cardAnim->setEndValue(endGeom);
    m_cardAnim->setEasingCurve(m_mode == Mode::Card ? BlopMotion::kEaseOvershoot
                                                    : BlopMotion::kEaseStandard);
    m_cardAnim->start(QAbstractAnimation::DeleteWhenStopped);
  }
#endif
}

void BlopModal::dismiss() {
  if (m_dismissing)
    return;
  m_dismissing = true;
  emit aboutToDismiss();
  startDismissAnim();
}

void BlopModal::startDismissAnim() {
#ifdef Q_OS_ANDROID
  emit dismissed();
  close();
  return;
#else
  auto *fadeOut = new QPropertyAnimation(this, "windowOpacity", this);
  fadeOut->setDuration(kBackdropFadeOutMs);
  fadeOut->setStartValue(windowOpacity());
  fadeOut->setEndValue(0.0);
  fadeOut->setEasingCurve(QEasingCurve::InCubic);

  if (m_card) {
    const QRect startGeom = m_card->geometry();
    QRect endGeom = startGeom;
    if (m_mode == Mode::BottomSheet) {
      endGeom.translate(0, startGeom.height());
    } else if (m_mode == Mode::SideSheet) {
      endGeom.translate(startGeom.width(), 0);
    } else {
      const int shrink = qMin(startGeom.width(), startGeom.height()) / 14;
      endGeom.adjust(shrink, shrink, -shrink, -shrink);
    }
    auto *cardAnim = new QPropertyAnimation(m_card, "geometry", this);
    cardAnim->setDuration(kCardExitMs);
    cardAnim->setStartValue(startGeom);
    cardAnim->setEndValue(endGeom);
    cardAnim->setEasingCurve(QEasingCurve::InCubic);
    cardAnim->start(QAbstractAnimation::DeleteWhenStopped);
  }

  QPointer<BlopModal> self(this);
  auto finish = [self]() {
    if (!self || self->isHidden())
      return;
    emit self->dismissed();
    self->close();
  };
  connect(fadeOut, &QPropertyAnimation::finished, this, finish);
  QTimer::singleShot(kBackdropFadeOutMs + 120, this, finish);
  fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
#endif
}

bool BlopModal::eventFilter(QObject *watched, QEvent *event) {
  if (m_parentFilterTarget == watched && event->type() == QEvent::Resize) {
    if (parentWidget())
      setGeometry(parentWidget()->rect());
    layoutContent();
  }
  return QWidget::eventFilter(watched, event);
}

void BlopModal::dismissFromOutsideTap(const QPoint &pos) {
  if (!m_card || m_dismissing)
    return;
  if (!m_card->geometry().contains(pos))
    dismiss();
}

bool BlopModal::event(QEvent *event) {
  if (event->type() == QEvent::TouchBegin) {
    auto *te = static_cast<QTouchEvent *>(event);
    if (!te->points().isEmpty()) {
      const QPoint pos = te->points().first().position().toPoint();
      if (m_card && !m_card->geometry().contains(pos)) {
        dismissFromOutsideTap(pos);
        event->accept();
        return true;
      }
    }
  }
  return QWidget::event(event);
}

void BlopModal::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Back) {
    dismiss();
    return;
  }
  QWidget::keyPressEvent(event);
}

void BlopModal::mousePressEvent(QMouseEvent *event) {
  if (!m_card) {
    QWidget::mousePressEvent(event);
    return;
  }
  const QPoint p = event->pos();
  if (!m_card->geometry().contains(p)) {
    dismissFromOutsideTap(p);
    event->accept();
    return;
  }
  // Drag handle press in BottomSheet mode begins drag-to-dismiss.
  if (m_mode == Mode::BottomSheet && m_dragHandle) {
    const QPoint handlePos = m_dragHandle->mapFrom(this, p);
    if (m_dragHandle->rect().contains(handlePos)) {
      m_dragging = true;
      m_dragStart = p;
      m_dragOffset = 0;
      event->accept();
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

void BlopModal::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragging && m_card) {
    const int dy = event->pos().y() - m_dragStart.y();
    m_dragOffset = qMax(0, dy);
    QRect g = m_card->geometry();
    // Reset to base then translate (avoid drift on repeated moves).
    layoutContent();
    g = m_card->geometry();
    g.translate(0, m_dragOffset);
    m_card->setGeometry(g);
    // Fade backdrop proportional to drag distance.
    const qreal frac = qBound(0.0, m_dragOffset / qreal(m_card->height()), 1.0);
    setWindowOpacity(1.0 - frac * 0.6);
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void BlopModal::mouseReleaseEvent(QMouseEvent *event) {
  if (m_dragging) {
    m_dragging = false;
    if (m_dragOffset > UiScale::dp(kDragDismissThresholdDp)) {
      dismiss();
    } else {
      // Snap back.
      layoutContent();
      setWindowOpacity(1.0);
    }
    m_dragOffset = 0;
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void BlopModal::onParentResized() {
  if (parentWidget())
    setGeometry(parentWidget()->rect());
  layoutContent();
}
