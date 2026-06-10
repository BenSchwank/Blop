#include "blop_modal.h"

#include "blop_theme.h"
#include "uiscale.h"

#include <QApplication>
#include <QDialog>
#include <QEasingCurve>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {
constexpr int kBackdropFadeMs = 180;
constexpr int kCardEnterMs = 220;
constexpr int kBackdropFadeOutMs = 140;
constexpr int kCardExitMs = 180;
constexpr int kDragDismissThresholdDp = 80;
constexpr int kDragHandleHeightDp = 28;
} // namespace

BlopModal *BlopModal::present(QWidget *parent, QWidget *content, Mode mode,
                              const QString &accessibleTitle) {
  if (!parent || !content)
    return nullptr;
  auto *win = parent->window();
  auto *modal = new BlopModal(win ? win : parent, content, mode, accessibleTitle);
  modal->show();
  modal->raise();
  modal->startOpenAnim();
  return modal;
}

int BlopModal::execBlocking(QWidget *parent, QDialog *dlg, Mode mode) {
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

  auto *modal = present(parent, dlg, mode);
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
  cardLay->addWidget(content, 1);

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

  if (m_mode == Mode::BottomSheet) {
    QString qss = BlopTheme::bottomSheetQss(QStringLiteral("BlopModalSheet"));
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
    if (BlopTheme::instance().isLight()) {
      auto *shadow = new QGraphicsDropShadowEffect(m_card);
      shadow->setBlurRadius(28);
      shadow->setOffset(-6, 0);
      shadow->setColor(QColor(0, 0, 0, 60));
      m_card->setGraphicsEffect(shadow);
    } else {
      m_card->setGraphicsEffect(nullptr);
    }
  } else {
    QString qss = BlopTheme::cardQss(QStringLiteral("BlopModalCard"));
    m_card->setStyleSheet(qss);
    if (BlopTheme::instance().isLight()) {
      auto *shadow = new QGraphicsDropShadowEffect(m_card);
      shadow->setBlurRadius(28);
      shadow->setOffset(0, 6);
      shadow->setColor(QColor(0, 0, 0, 60));
      m_card->setGraphicsEffect(shadow);
    } else {
      m_card->setGraphicsEffect(nullptr);
    }
  }
}

void BlopModal::layoutContent() {
  if (!parentWidget() || !m_card)
    return;
  const int W = width();
  const int H = height();
  const int pad = UiScale::dp(16);

  if (m_mode == Mode::BottomSheet) {
    const int sheetMaxH = H - UiScale::dp(48);
    const int sheetH = qMin(sheetMaxH, qMax(UiScale::dp(280),
                                            int(H * 0.92)));
    const int sheetW = W;
    m_card->setGeometry(0, H - sheetH, sheetW, sheetH);
  } else if (m_mode == Mode::SideSheet) {
    const int sheetW = qBound(UiScale::dp(360),
                              qMin(m_preferredCardWidth, int(W * 0.55)),
                              qMin(W - pad, UiScale::dp(560)));
    m_card->setGeometry(W - sheetW, 0, sheetW, H);
  } else {
    const int cardW = qBound(UiScale::dp(320),
                             qMin(m_preferredCardWidth, W - 2 * pad),
                             qMin(W - 2 * pad, UiScale::dp(720)));
    const int cardH = qBound(UiScale::dp(220), int(H * 0.85),
                             H - 2 * pad);
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
  setWindowOpacity(0.0);
  m_backdropAnim = new QPropertyAnimation(this, "windowOpacity", this);
  m_backdropAnim->setDuration(kBackdropFadeMs);
  m_backdropAnim->setStartValue(0.0);
  m_backdropAnim->setEndValue(1.0);
  m_backdropAnim->setEasingCurve(QEasingCurve::OutCubic);
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
    m_cardAnim->setEasingCurve(m_mode == Mode::Card ? QEasingCurve::OutBack
                                                    : QEasingCurve::OutCubic);
    m_cardAnim->start(QAbstractAnimation::DeleteWhenStopped);
  }
}

void BlopModal::dismiss() {
  if (m_dismissing)
    return;
  m_dismissing = true;
  emit aboutToDismiss();
  startDismissAnim();
}

void BlopModal::startDismissAnim() {
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

  connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
    emit dismissed();
    close();
  });
  fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

bool BlopModal::eventFilter(QObject *watched, QEvent *event) {
  if (m_parentFilterTarget == watched && event->type() == QEvent::Resize) {
    if (parentWidget())
      setGeometry(parentWidget()->rect());
    layoutContent();
  }
  return QWidget::eventFilter(watched, event);
}

void BlopModal::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
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
  // Click outside the card area dismisses.
  if (!m_card->geometry().contains(p)) {
    dismiss();
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
