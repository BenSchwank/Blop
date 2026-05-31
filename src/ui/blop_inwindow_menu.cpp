#include "blop_inwindow_menu.h"

#include "uiscale.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace BlopInWindowMenu {

namespace {

class Backdrop : public QWidget {
public:
  explicit Backdrop(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("BlopInWindowMenuBackdrop"));
    setStyleSheet(QStringLiteral(
        "QWidget#BlopInWindowMenuBackdrop { background: rgba(0,0,0,0.25); }"));
    setMouseTracking(true);
  }

  void setMenuFrame(QWidget *f) { m_frame = f; }
  void setOnDismiss(std::function<void()> cb) { m_onDismiss = std::move(cb); }

protected:
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
    close();
  }

private:
  QPointer<QWidget> m_frame;
  std::function<void()> m_onDismiss;
};

QString itemStyle(bool destructive) {
  const QString color =
      destructive ? QStringLiteral("#FF6B6B") : QStringLiteral("#E8E4FF");
  // Match the QMenu styling used elsewhere in the app: dark surface with the
  // Blop purple accent on press, slightly tinted text.
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent;"
             "  color: %1;"
             "  border: none;"
             "  text-align: left;"
             "  padding: 12px 18px;"
             "  font-size: 14px;"
             "  font-weight: 500;"
             "  border-radius: 8px;"
             "}"
             "QPushButton:pressed {"
             "  background: rgba(124,92,252,0.38);"
             "  color: #FFFFFF;"
             "}"
             "QPushButton:focus { outline: none; }")
      .arg(color);
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
  // Mirror the BlopWebMenuStyleSheet QMenu appearance for visual consistency
  // with the desktop right-click menu.
  frame->setStyleSheet(QStringLiteral(
      "QFrame#BlopInWindowMenuFrame {"
      "  background: #14121F;"
      "  border: 1px solid rgba(124,92,252,0.40);"
      "  border-radius: 12px;"
      "}"));

  auto *vlay = new QVBoxLayout(frame);
  vlay->setContentsMargins(UiScale::dp(6), UiScale::dp(6), UiScale::dp(6),
                           UiScale::dp(6));
  vlay->setSpacing(UiScale::dp(2));

  QPointer<Backdrop> safeBackdrop(backdrop);

  auto closeBackdrop = [safeBackdrop]() {
    if (safeBackdrop) {
      safeBackdrop->close();
    }
  };
  backdrop->setMenuFrame(frame);
  backdrop->setOnDismiss(closeBackdrop);

  int totalContentHeight = vlay->contentsMargins().top() +
                           vlay->contentsMargins().bottom();
  const int separatorH = UiScale::dp(1) + UiScale::dp(8); // line + margins
  const int itemH = UiScale::dp(44);
  int filledRows = 0;

  for (const Item &it : items) {
    if (it.separator) {
      auto *sep = new QFrame(frame);
      sep->setFrameShape(QFrame::HLine);
      sep->setStyleSheet(QStringLiteral(
          "QFrame { color: rgba(255,255,255,0.08); background: "
          "rgba(255,255,255,0.08); border: none; min-height: 1px; max-height: "
          "1px; margin: 4px 12px; }"));
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
                         safeBackdrop->close();
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

  // Size the frame. Prefer the natural width but cap to fit inside the window
  // with a small safety margin.
  frame->adjustSize();
  int targetW = qMax(frame->sizeHint().width(), UiScale::dp(220));
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
}

} // namespace BlopInWindowMenu
