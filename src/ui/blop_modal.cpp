#include "blop_modal.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QApplication>
#include <QAbstractAnimation>
#include <QColor>
#include <QDialog>
#include <QEasingCurve>
#include <QEvent>
#include <QEventLoop>
#include <QFrame>
#include <QGraphicsBlurEffect>
#include <QGraphicsOpacityEffect>
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

QColor ownedSheetFill(QWidget *content) {
  if (content && content->property("blopForcePaper").toBool())
    return BlopStyle::paperSurface();
  if (BlopTheme::instance().isDark())
    return BlopTheme::surfaceElevated();
  return BlopStyle::paperSurface();
}

QColor ownedSheetBorder(QWidget *content) {
  if (content && content->property("blopForcePaper").toBool())
    return BlopStyle::paperBorder();
  if (BlopTheme::instance().isDark())
    return BlopTheme::borderDefault();
  return BlopStyle::paperBorder();
}
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
  // Grab the host *before* show so the soft-blur stage sees the real UI.
  if (modal->m_mode == Mode::Stage)
    modal->installBlurBackdrop();
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
  if (mode == Mode::Float ||
      (mode == Mode::Auto && modal)) {
    // Neue Notiz pick default; dialog may grow after format choice.
    if (preferredCardWidth > 0 && preferredCardWidth <= 600)
      modal->setPreferredCardHeightFrac(0.58);
  }

  int result = QDialog::Rejected;
  QEventLoop loop;
  bool dialogFinished = false;
  bool modalDismissed = false;

  QObject::connect(dlg, &QDialog::finished, &loop, [&](int code) {
    result = code;
    dialogFinished = true;
    loop.quit();
  });
  // User dismissed via backdrop / drag / ESC before clicking a dialog
  // button -> treat as Rejected, matches QDialog::exec() Esc behaviour.
  QObject::connect(modal, &BlopModal::dismissed, &loop, [&]() {
    modalDismissed = true;
    if (!dialogFinished)
      result = QDialog::Rejected;
    loop.quit();
  });

  // QDialog hides itself by default; force visible since we don't go
  // through exec() and the caller expects the dialog to render.
  dlg->show();
  loop.exec();

  // If the dialog itself reached finished() the modal is still open ->
  // dismiss it and wait for the dismiss animation to finish so the
  // caller's next setStyleSheet/show isn't racing with a stale backdrop.
  if (dialogFinished && modal && !modal->isHidden() && !modalDismissed) {
    QPointer<BlopModal> guard(modal);
    QEventLoop dismissLoop;
    QObject::connect(modal, &BlopModal::dismissed, &dismissLoop,
                     &QEventLoop::quit);
    modal->dismiss();
    // Safety timeout in case the dismiss animation never fires.
    QTimer::singleShot(600, &dismissLoop, &QEventLoop::quit);
    dismissLoop.exec();
    // Force-hide immediately so no stale backdrop can block the caller.
    if (guard && !guard->isHidden())
      guard->hide();
  }

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
  case Mode::Stage:
    cardObjName = QStringLiteral("BlopModalStage");
    break;
  case Mode::Float:
    cardObjName = QStringLiteral("BlopModalFloat");
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
  // Stage / sheets / float fill the host; compact cards stay content-sized.
  const int contentStretch =
      (m_mode == Mode::Stage || m_mode == Mode::BottomSheet ||
       m_mode == Mode::SideSheet || m_mode == Mode::Float)
          ? 1
          : 0;
  cardLay->addWidget(content, contentStretch);

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

void BlopModal::setPreferredCardHeightFrac(qreal frac) {
  m_preferredCardHeightFrac = qBound(0.35, frac, 0.95);
  layoutContent();
}

void BlopModal::preparePreferredSize(int widthPx, qreal heightFrac) {
  if (widthPx > 0)
    m_preferredCardWidth = widthPx;
  m_preferredCardHeightFrac = qBound(0.35, heightFrac, 0.95);
}

BlopModal *BlopModal::hostOf(QWidget *content) {
  for (QWidget *w = content; w; w = w->parentWidget()) {
    if (auto *m = qobject_cast<BlopModal *>(w))
      return m;
  }
  return nullptr;
}

QRect BlopModal::preferredCardRect() const {
  if (!parentWidget() || !m_card)
    return {};
  const int W = width();
  const int H = height();
  if (m_mode == Mode::Float) {
    const int gap = UiScale::dp(24);
    int cardW = m_preferredCardWidth > 0 ? m_preferredCardWidth : int(W * 0.58);
    cardW = qBound(UiScale::dp(420), cardW, W - 2 * gap);
    const qreal frac =
        m_preferredCardHeightFrac > 0.0 ? m_preferredCardHeightFrac : 0.72;
    int cardH = int(H * frac);
    cardH = qBound(UiScale::dp(360), cardH, H - 2 * gap);
    return QRect((W - cardW) / 2, (H - cardH) / 2, cardW, cardH);
  }
  // Fallback: current geometry after a layout pass would be needed; return
  // existing card rect for non-Float modes.
  return m_card->geometry();
}

void BlopModal::animateCardToPreferred(int durationMs) {
  if (!m_card || m_dismissing)
    return;
  const QRect target = preferredCardRect();
  if (!target.isValid()) {
    layoutContent();
    return;
  }
  if (m_cardAnim) {
    m_cardAnim->stop();
    m_cardAnim->deleteLater();
    m_cardAnim = nullptr;
  }
  // Keep preferred values in sync so resizeEvent / layoutContent match.
  // Width/height already stored via setters; just animate geometry.
  const QRect start = m_card->geometry();
  if ((start.topLeft() - target.topLeft()).manhattanLength() < 2 &&
      qAbs(start.width() - target.width()) < 2 &&
      qAbs(start.height() - target.height()) < 2) {
    m_card->setGeometry(target);
    return;
  }
  m_cardAnim = new QPropertyAnimation(m_card, "geometry", this);
  m_cardAnim->setDuration(qMax(80, durationMs));
  m_cardAnim->setStartValue(start);
  m_cardAnim->setEndValue(target);
  m_cardAnim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_cardAnim, &QPropertyAnimation::finished, this, [this]() {
    m_cardAnim = nullptr;
    // Sync content without restarting another animation.
    if (m_card)
      layoutContent();
  });
  m_cardAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void BlopModal::applyTheme() {
  if (m_mode == Mode::Stage) {
    // Soft-blur stage: backdrop is the blurred snapshot + dim layer, not
    // the solid theme scrim (avoids double-darkening).
    setStyleSheet(QStringLiteral(
        "QWidget#BlopModalBackdrop { background: transparent; border: none; }"));
  } else {
    setStyleSheet(BlopTheme::scrimQss(QStringLiteral("BlopModalBackdrop")));
  }

  if (m_content) {
    if (m_mode == Mode::Card || m_mode == Mode::Auto || m_mode == Mode::Stage ||
        m_mode == Mode::SideSheet || m_mode == Mode::Float) {
      m_content->setAutoFillBackground(false);
      // Dialogs that paint their own surface must keep their stylesheet.
      if (!m_content->property("blopOwnsBackground").toBool()) {
        if (m_mode == Mode::SideSheet) {
          m_content->setStyleSheet(
              QStringLiteral("background-color: %1;")
                  .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb)));
        } else {
          m_content->setStyleSheet(QStringLiteral("background: transparent;"));
        }
      }
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
    // ownsBg + blopForcePaper → Notion paper; else follow app theme.
    const bool ownsBg =
        m_content && m_content->property("blopOwnsBackground").toBool();
    const QColor fill =
        ownsBg ? ownedSheetFill(m_content) : BlopTheme::surfaceElevated();
    const QColor border =
        ownsBg ? ownedSheetBorder(m_content) : BlopTheme::borderDefault();
    QString qss = QStringLiteral(
                      "QFrame#BlopModalSideSheet {"
                      "  background: %1;"
                      "  border: 1px solid %2;"
                      "  border-top-left-radius: %3px;"
                      "  border-bottom-left-radius: %3px;"
                      "  border-top-right-radius: 0px;"
                      "  border-bottom-right-radius: 0px;"
                      "}")
                      .arg(fill.name(QColor::HexRgb),
                           border.name(QColor::HexRgb),
                           QString::number(BlopTheme::r24));
    m_card->setStyleSheet(qss);
    m_card->setGraphicsEffect(nullptr);
  } else if (m_mode == Mode::Float) {
    m_card->setObjectName(QStringLiteral("BlopModalFloat"));
    m_card->setGraphicsEffect(nullptr);
    const bool forcePaper =
        m_content && m_content->property("blopForcePaper").toBool();
    if (forcePaper) {
      // Settings / paper dialogs: opaque floating sheet (same host as Deck).
      m_card->setAutoFillBackground(true);
      const int rad = UiScale::dp(BlopStyle::radiusLgDp() + 4);
      m_card->setStyleSheet(
          QStringLiteral("QFrame#BlopModalFloat {"
                         "  background: %1;"
                         "  border: 1px solid %2;"
                         "  border-radius: %3px;"
                         "}")
              .arg(ownedSheetFill(m_content).name(QColor::HexRgb),
                   ownedSheetBorder(m_content).name(QColor::HexRgb),
                   QString::number(rad)));
    } else {
      // Format-Deck: transparent chrome — cards paint themselves.
      m_card->setAutoFillBackground(false);
      m_card->setStyleSheet(QStringLiteral(
          "QFrame#BlopModalFloat { background: transparent; border: none; }"));
    }
  } else {
    // Centered card / stage + scrim.
    const QString obj = (m_mode == Mode::Stage)
                            ? QStringLiteral("BlopModalStage")
                            : QStringLiteral("BlopModalCard");
    m_card->setObjectName(obj);
    m_card->setAutoFillBackground(true);
    const bool ownsBg =
        m_content && m_content->property("blopOwnsBackground").toBool();
    const int rad = UiScale::dp(m_mode == Mode::Stage
                                    ? BlopStyle::radiusLgDp() + 2
                                    : BlopStyle::radiusLgDp());
    if (ownsBg) {
      m_card->setStyleSheet(
          QStringLiteral("QFrame#%1 {"
                         "  background: %2;"
                         "  border: 1px solid %3;"
                         "  border-radius: %4px;"
                         "}")
              .arg(obj, ownedSheetFill(m_content).name(QColor::HexRgb),
                   ownedSheetBorder(m_content).name(QColor::HexRgb),
                   QString::number(rad)));
    } else {
      m_card->setStyleSheet(BlopStyle::surfaceStyle(obj));
    }
    // Never use QGraphicsDropShadowEffect here. On Windows/MinGW it races
    // the software rasterizer (QWidgetEffectSourcePrivate::pixmap /
    // "Painter not active") and leaves dialogs visually ghosted + dead to
    // clicks (Neue Notiz / Einstellungen). Border + scrim is enough depth.
    m_card->setGraphicsEffect(nullptr);
  }
}

void BlopModal::installBlurBackdrop() {
  if (m_mode != Mode::Stage || !parentWidget())
    return;

  QPixmap snap = parentWidget()->grab();
  if (snap.isNull())
    return;

  if (!m_blurLayer) {
    m_blurLayer = new QLabel(this);
    m_blurLayer->setObjectName(QStringLiteral("BlopModalBlurLayer"));
    m_blurLayer->setScaledContents(true);
    m_blurLayer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  }
  m_blurLayer->setPixmap(snap);

  auto *blur = qobject_cast<QGraphicsBlurEffect *>(m_blurLayer->graphicsEffect());
  if (!blur) {
    blur = new QGraphicsBlurEffect(m_blurLayer);
    m_blurLayer->setGraphicsEffect(blur);
  }
  blur->setBlurRadius(10);
  blur->setBlurHints(QGraphicsBlurEffect::PerformanceHint);

  if (!m_dimLayer) {
    m_dimLayer = new QWidget(this);
    m_dimLayer->setObjectName(QStringLiteral("BlopModalDimLayer"));
    m_dimLayer->setAttribute(Qt::WA_StyledBackground, true);
    m_dimLayer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  }
  const QColor scrim = BlopTheme::scrimColor();
  m_dimLayer->setStyleSheet(
      QStringLiteral("QWidget#BlopModalDimLayer {"
                     "  background: rgba(%1,%2,%3,0.38);"
                     "  border: none;"
                     "}")
          .arg(scrim.red())
          .arg(scrim.green())
          .arg(scrim.blue()));

  layoutBlurLayers();
}

void BlopModal::layoutBlurLayers() {
  if (m_blurLayer)
    m_blurLayer->setGeometry(rect());
  if (m_dimLayer)
    m_dimLayer->setGeometry(rect());
  if (m_blurLayer)
    m_blurLayer->lower();
  if (m_dimLayer && m_blurLayer)
    m_dimLayer->stackUnder(m_card);
  else if (m_dimLayer)
    m_dimLayer->lower();
  if (m_card)
    m_card->raise();
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
  } else if (m_mode == Mode::Stage) {
    // Variante A: centered ~60% stage. Floors keep readability on large
    // monitors; caps keep breathing room on small laptop windows.
    const int maxW = qMax(UiScale::dp(320), int(W * 0.88));
    const int maxH = qMax(UiScale::dp(280), int(H * 0.88));
    int cardW = int(W * 0.60);
    int cardH = int(H * 0.60);
    cardW = qBound(qMin(UiScale::dp(720), maxW), cardW, maxW);
    cardH = qBound(qMin(UiScale::dp(520), maxH), cardH, maxH);
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
    layoutBlurLayers();
  } else if (m_mode == Mode::Float) {
    // Format-Deck / Neue Notiz: size driven by preferred width + height frac.
    const QRect r = preferredCardRect();
    m_card->setGeometry(r);
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
  } else {
    const int preferred =
        m_preferredCardWidth > 0 ? m_preferredCardWidth : UiScale::dp(420);
    // Wider overlays stay a centered card sized to the preferred width +
    // content height — never a near-fullscreen sheet.
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
      startGeom.translate(0, UiScale::dp(12));
    }
    m_card->setGeometry(startGeom);
    m_cardAnim = new QPropertyAnimation(m_card, "geometry", this);
    m_cardAnim->setDuration(kCardEnterMs);
    m_cardAnim->setStartValue(startGeom);
    m_cardAnim->setEndValue(endGeom);
    m_cardAnim->setEasingCurve(BlopMotion::kEaseStandard);
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
  // windowOpacity only works for top-level windows; BlopModal is a child
  // widget, so we use a QGraphicsOpacityEffect to actually fade the backdrop.
  auto *opacity = new QGraphicsOpacityEffect(this);
  opacity->setOpacity(1.0);
  setGraphicsEffect(opacity);
  auto *fadeOut = new QPropertyAnimation(opacity, "opacity", this);
  fadeOut->setDuration(kBackdropFadeOutMs);
  fadeOut->setStartValue(1.0);
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
      endGeom.translate(0, UiScale::dp(10));
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
  if (event->type() == QEvent::TouchBegin && m_card) {
    auto *te = static_cast<QTouchEvent *>(event);
    if (!te->points().isEmpty()) {
      // Unaccepted touches bubble up from the content widgets, so compare in
      // global coordinates — local ones belong to the original target.
      const QPoint global = te->points().first().globalPosition().toPoint();
      const QRect cardGlobal(m_card->mapToGlobal(QPoint(0, 0)), m_card->size());
      if (!cardGlobal.contains(global)) {
        dismiss();
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
