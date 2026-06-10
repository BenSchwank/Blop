#include "blop_inwindow_menu.h"

#include "blop_theme.h"
#include "uiscale.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QWidget>

namespace BlopInWindowMenu {

namespace {

class Backdrop : public QWidget {
public:
  explicit Backdrop(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("BlopInWindowMenuBackdrop"));
    // v3.17.0: theme-aware scrim, ~25% feel. v3.18.2: painted manually so
    // the enter/exit fade can animate the alpha without stylesheet churn.
    QColor scrim = BlopTheme::scrimColor();
    scrim.setAlpha(qMin(scrim.alpha(), 64)); // ~25%
    m_scrim = scrim;
    setMouseTracking(true);
  }

  void setMenuFrame(QWidget *f) { m_frame = f; }
  void setOnDismiss(std::function<void()> cb) { m_onDismiss = std::move(cb); }

  // v3.18.2: backdrop fade-in (kFast) + frame slide-in (kStandard), exit
  // mirrored -- same vocabulary as BlopModal.
  void startEnterAnim() {
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

    if (m_frame) {
      const QPoint target = m_frame->pos();
      const QPoint start = target + QPoint(0, UiScale::dp(8));
      m_frame->move(start);
      auto *slide = new QPropertyAnimation(m_frame, "pos", this);
      slide->setDuration(BlopMotion::kStandard);
      slide->setStartValue(start);
      slide->setEndValue(target);
      slide->setEasingCurve(BlopMotion::kEaseStandard);
      slide->start(QAbstractAnimation::DeleteWhenStopped);
    }
  }

  void dismissAnimated() {
    if (m_dismissing) {
      return;
    }
    m_dismissing = true;
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    if (m_frame) {
      auto *slide = new QPropertyAnimation(m_frame, "pos", this);
      slide->setDuration(BlopMotion::kFast);
      slide->setStartValue(m_frame->pos());
      slide->setEndValue(m_frame->pos() + QPoint(0, UiScale::dp(6)));
      slide->setEasingCurve(QEasingCurve::InCubic);
      slide->start(QAbstractAnimation::DeleteWhenStopped);
    }

    auto *fade = new QVariantAnimation(this);
    fade->setDuration(BlopMotion::kFast);
    fade->setStartValue(m_progress);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InCubic);
    connect(fade, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &v) {
              m_progress = v.toReal();
              if (m_frame) {
                // Frame is an opaque child; hide it near the end of the
                // fade so the exit reads as one motion.
                m_frame->setVisible(m_progress > 0.25);
              }
              update();
            });
    connect(fade, &QVariantAnimation::finished, this, [this]() { close(); });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    QColor c = m_scrim;
    c.setAlphaF(c.alphaF() * m_progress);
    p.fillRect(rect(), c);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (m_frame && m_frame->geometry().contains(event->pos())) {
      // Click inside the frame: let the frame's child widgets handle it.
      event->ignore();
      return;
    }
    if (m_onDismiss) {
      auto cb = m_onDismiss;
      m_onDismiss = {};
      cb();
    }
    dismissAnimated();
  }

private:
  QPointer<QWidget> m_frame;
  std::function<void()> m_onDismiss;
  QColor m_scrim;
  qreal m_progress{0.0};
  bool m_dismissing{false};
};

QString itemStyle(bool /*destructive*/) {
  // v3.17.0: theme-aware -- reads tokens from BlopTheme so Light/Dark mode
  // reskins automatically. Geometry (padding/font-size/radius) matches the
  // historical look that v3.16.10 aligned to blopWebMenuStyleSheet(). The
  // destructive parameter is accepted for API stability but ignored --
  // Windows context menu uses one color for every item including Delete.
  auto rgba = [](const QColor &c) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
  };
  const QString text = BlopTheme::textPrimary().name(QColor::HexRgb);
  const QString press = rgba(BlopTheme::accentSubtle());
  const QString onAccent = BlopTheme::textOnAccent().name(QColor::HexRgb);
  // v3.18.2: hover tint (half the pressed alpha) for pointer devices.
  QColor hoverCol = BlopTheme::accentSubtle();
  hoverCol.setAlpha(hoverCol.alpha() / 2);
  const QString hover = rgba(hoverCol);
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent;"
             "  color: %1;"
             "  border: none;"
             "  text-align: left;"
             "  padding: 10px 22px;"
             "  font-size: 13px;"
             "  font-weight: 500;"
             "  border-radius: 8px;"
             "}"
             "QPushButton:hover { background: %4; }"
             "QPushButton:pressed {"
             "  background: %2;"
             "  color: %3;"
             "}"
             "QPushButton:focus { outline: none; }")
      .arg(text, press, onAccent, hover);
}

} // namespace

void show(QWidget *anchor, const QPoint &anchorGlobal,
          const QList<Item> &items) {
  if (!anchor || items.isEmpty()) {
    return;
  }
  QWidget *win = anchor->window();
  if (!win) {
    return;
  }

  auto *backdrop = new Backdrop(win);
  backdrop->setGeometry(win->rect());

  auto *frame = new QFrame(backdrop);
  frame->setObjectName(QStringLiteral("BlopInWindowMenuFrame"));
  frame->setAttribute(Qt::WA_StyledBackground, true);
  // v3.17.0: theme-aware. Mirrors blopWebMenuStyleSheet() (desktop QMenu)
  // by reading the same BlopTheme tokens.
  {
    auto rgba = [](const QColor &c) {
      return QStringLiteral("rgba(%1,%2,%3,%4)")
          .arg(c.red()).arg(c.green()).arg(c.blue())
          .arg(QString::number(c.alphaF(), 'f', 3));
    };
    const QString bg = BlopTheme::surfaceElevated().name(QColor::HexRgb);
    const QString border = rgba(BlopTheme::accentBorder());
    frame->setStyleSheet(QStringLiteral(
        "QFrame#BlopInWindowMenuFrame {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}").arg(bg, border));
  }

  auto *vlay = new QVBoxLayout(frame);
  vlay->setContentsMargins(UiScale::dp(6), UiScale::dp(6), UiScale::dp(6),
                           UiScale::dp(6));
  vlay->setSpacing(UiScale::dp(2));

  QPointer<Backdrop> safeBackdrop(backdrop);

  auto closeBackdrop = [safeBackdrop]() {
    if (safeBackdrop) {
      safeBackdrop->dismissAnimated();
    }
  };
  backdrop->setMenuFrame(frame);
  backdrop->setOnDismiss(closeBackdrop);

  int totalContentHeight = vlay->contentsMargins().top() +
                           vlay->contentsMargins().bottom();
  const int separatorH = UiScale::dp(1) + UiScale::dp(12); // 1px line + 6px top/bottom margin (matches blopWebMenuStyleSheet)
  const int itemH = UiScale::dp(44);
  int filledRows = 0;

  for (const Item &it : items) {
    if (it.separator) {
      auto *sep = new QFrame(frame);
      sep->setFrameShape(QFrame::HLine);
      {
        auto rgba = [](const QColor &c) {
          return QStringLiteral("rgba(%1,%2,%3,%4)")
              .arg(c.red()).arg(c.green()).arg(c.blue())
              .arg(QString::number(c.alphaF(), 'f', 3));
        };
        const QString sepCol = rgba(BlopTheme::borderDefault());
        sep->setStyleSheet(QStringLiteral(
            "QFrame { color: %1; background: %1; border: none; min-height: "
            "1px; max-height: 1px; margin: 6px 12px; }").arg(sepCol));
      }
      vlay->addWidget(sep);
      totalContentHeight += separatorH;
      continue;
    }

    auto *btn = new QPushButton(it.label, frame);
    if (!it.icon.isNull()) {
      btn->setIcon(it.icon);
      btn->setIconSize(QSize(UiScale::dp(18), UiScale::dp(18)));
    }
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(itemH);
    btn->setStyleSheet(itemStyle(it.destructive));
    btn->setFocusPolicy(Qt::NoFocus);

    auto handler = it.handler;
    QObject::connect(btn, &QPushButton::clicked, frame,
                     [safeBackdrop, handler]() {
                       // Close menu first so the handler runs without the
                       // backdrop covering anything it might open.
                       if (safeBackdrop) {
                         safeBackdrop->dismissAnimated();
                       }
                       if (handler) {
                         // Defer so we return to the event loop before doing
                         // any heavy work that may itself want to show UI.
                         QTimer::singleShot(0, [handler]() { handler(); });
                       }
                     });
    vlay->addWidget(btn);
    totalContentHeight += itemH + vlay->spacing();
    ++filledRows;
  }

  if (filledRows == 0) {
    backdrop->close();
    return;
  }

  // Use the natural sizeHint -- Windows QMenu sizes itself tightly to the
  // longest item, no artificial minimum. The window-rect cap remains so the
  // menu can't bleed outside the parent on small screens.
  frame->adjustSize();
  int targetW = frame->sizeHint().width();
  const int margin = UiScale::dp(12);
  targetW = qMin(targetW, win->width() - 2 * margin);
  int targetH = qMin(totalContentHeight, win->height() - 2 * margin);

  // Anchor: open below-right of anchorGlobal, clamp to window rect.
  QPoint local = win->mapFromGlobal(anchorGlobal);
  int x = local.x();
  int y = local.y();
  if (x + targetW > win->width() - margin) {
    x = win->width() - targetW - margin;
  }
  if (x < margin) x = margin;
  if (y + targetH > win->height() - margin) {
    // Flip above the anchor if it'd overflow the bottom.
    y = local.y() - targetH;
  }
  if (y < margin) y = margin;

  frame->setGeometry(x, y, targetW, targetH);

  backdrop->show();
  backdrop->raise();
  frame->raise();
  backdrop->startEnterAnim();
}

} // namespace BlopInWindowMenu
