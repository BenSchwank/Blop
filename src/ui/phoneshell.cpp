#include "phoneshell.h"

#include "androidicons.h"
#include "blop_theme.h"
#include "blopstyle.h"
#include "phonechrome.h"
#include "uiscale.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QToolButton>

namespace {
QIcon studyIcon(const QColor &color) {
  const int px = UiScale::dp(24);
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  const qreal m = px * 0.18;
  QRectF r(m, m * 0.9, px - 2 * m, px - 1.8 * m);
  p.drawRoundedRect(r, 2, 2);
  p.drawLine(QPointF(px / 2.0, r.top()), QPointF(px / 2.0, r.bottom()));
  return QIcon(pm);
}
} // namespace

PhoneShell::PhoneShell(QWidget *host) : QWidget(host), m_host(host) {
  setObjectName(QStringLiteral("PhoneShell"));
  setAttribute(Qt::WA_StyledBackground, false);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFocusPolicy(Qt::NoFocus);

  m_bar = new QWidget(this);
  m_bar->setObjectName(QStringLiteral("PhoneShellNav"));
  auto *lay = new QHBoxLayout(m_bar);
  lay->setContentsMargins(UiScale::dp(4), UiScale::dp(2), UiScale::dp(4),
                          UiScale::dp(2));
  lay->setSpacing(0);

  m_btnDash = makeItem(QStringLiteral("dashboard"), QStringLiteral("Home"),
                       QStringLiteral("home"));
  m_btnNotes = makeItem(QStringLiteral("notes"), QStringLiteral("Notizen"),
                        QStringLiteral("pages"));
  m_btnStudy = makeItem(QStringLiteral("study"), QStringLiteral("Study"),
                        QString());
  m_btnMore = makeItem(QStringLiteral("more"), QStringLiteral("Mehr"),
                       QStringLiteral("menu"));

  connect(m_btnDash, &QToolButton::clicked, this, [this]() {
    emit destinationChosen(Dest::Dashboard);
  });
  connect(m_btnNotes, &QToolButton::clicked, this, [this]() {
    emit destinationChosen(Dest::Notes);
  });
  connect(m_btnStudy, &QToolButton::clicked, this, [this]() {
    emit destinationChosen(Dest::Study);
  });
  connect(m_btnMore, &QToolButton::clicked, this, &PhoneShell::moreRequested);

  lay->addWidget(m_btnDash, 1);
  lay->addWidget(m_btnNotes, 1);
  lay->addWidget(m_btnStudy, 1);
  lay->addWidget(m_btnMore, 1);

  if (host) {
    host->installEventFilter(this);
    if (QWidget *win = host->window())
      win->installEventFilter(this);
  }
  rebuildStyles();
  syncGeometry();
  hide();
}

QToolButton *PhoneShell::makeItem(const QString &id, const QString &label,
                                  const QString &iconName) {
  auto *b = new QToolButton(m_bar);
  b->setObjectName(QStringLiteral("PhoneShellItem_") + id);
  b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  b->setCursor(Qt::PointingHandCursor);
  b->setAutoRaise(true);
  b->setFocusPolicy(Qt::NoFocus);
  b->setText(label);
  const int iconPx = UiScale::dp(22);
  const QColor idle = BlopTheme::instance().isDark()
                          ? QColor(QStringLiteral("#9AA3BB"))
                          : BlopStyle::paperInkMuted();
  if (iconName.isEmpty())
    b->setIcon(studyIcon(idle));
  else
    b->setIcon(AndroidIcons::icon(iconName, idle, 22));
  b->setIconSize(QSize(iconPx, iconPx));
  b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  return b;
}

void PhoneShell::rebuildStyles() {
  if (m_bar)
    m_bar->setStyleSheet(PhoneChrome::bottomNavQss());
  const auto tint = [this](QToolButton *b, bool on, const QString &iconName) {
    if (!b)
      return;
    b->setStyleSheet(PhoneChrome::navItemQss(on));
    const QColor c =
        on ? QColor(0x5B, 0x9D, 0xFF)
           : (BlopTheme::instance().isDark() ? QColor(QStringLiteral("#9AA3BB"))
                                             : BlopStyle::paperInkMuted());
    if (iconName.isEmpty())
      b->setIcon(studyIcon(c));
    else
      b->setIcon(AndroidIcons::icon(iconName, c, 22));
  };
  tint(m_btnDash, m_dest == Dest::Dashboard, QStringLiteral("home"));
  tint(m_btnNotes, m_dest == Dest::Notes, QStringLiteral("pages"));
  tint(m_btnStudy, m_dest == Dest::Study, QString());
  tint(m_btnMore, false, QStringLiteral("menu"));
}

void PhoneShell::setDestination(Dest dest) {
  if (m_dest == dest) {
    rebuildStyles();
    return;
  }
  m_dest = dest;
  rebuildStyles();
}

void PhoneShell::setShellVisible(bool on) {
  if (on) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setEnabled(true);
    setVisible(true);
    rebuildStyles();
    syncGeometry();
    raise();
  } else {
    setVisible(false);
    setEnabled(false);
  }
}

void PhoneShell::syncGeometry() {
  QWidget *host = parentWidget() ? parentWidget() : m_host;
  if (!host)
    return;
  const int h = PhoneChrome::overlayHeightPx(host);
  setGeometry(0, qMax(0, host->height() - h), host->width(), h);
  if (m_bar) {
    const int side = PhoneChrome::sidePadPx(host);
    m_bar->setGeometry(0, 0, width(), h);
    if (auto *lay = qobject_cast<QHBoxLayout *>(m_bar->layout())) {
      lay->setContentsMargins(side, UiScale::dp(4),
                              side + UiScale::dp(PhoneChrome::rightReserveDp()),
                              UiScale::safeBottomPx(host) + UiScale::dp(4));
    }
  }
  raise();
}

bool PhoneShell::eventFilter(QObject *watched, QEvent *event) {
  if (event && (event->type() == QEvent::Resize || event->type() == QEvent::Show) &&
      isVisible())
    syncGeometry();
  Q_UNUSED(watched);
  return QWidget::eventFilter(watched, event);
}
