#include "libraryiconrail.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "moderntoolbar.h"
#include "uiscale.h"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString accentRgba(const QColor &c, qreal a) {
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(QString::number(a, 'f', 2));
}

QIcon glyph(const QString &name, const QColor &fg, int px) {
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const qreal s = px / 64.0;
  p.scale(s, s);
  blopDrawToolbarGlyph64(&p, name, fg);
  return QIcon(pm);
}
} // namespace

LibraryIconRail::LibraryIconRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryIconRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(preferredWidth());
  m_accent = BlopTheme::accentPrimary();

  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, UiScale::dp(12), 0, UiScale::dp(12));
  lay->setSpacing(UiScale::dp(2));

  auto *logo = new QLabel(this);
  logo->setObjectName(QStringLiteral("LibraryIconRailLogo"));
  logo->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  logo->setAlignment(Qt::AlignCenter);
  logo->setText(QStringLiteral("B"));
  lay->addWidget(logo, 0, Qt::AlignHCenter);
  lay->addSpacing(UiScale::dp(8));

  // Dual-app switch first, then note utilities.
  addBtn(QStringLiteral("home"), QStringLiteral("home"),
         QStringLiteral("Dashboard"), lay);
  addBtn(QStringLiteral("library"), QStringLiteral("note"),
         QStringLiteral("Notizen"), lay);
  addBtn(QStringLiteral("new"), QStringLiteral("compose"),
         QStringLiteral("Neue Notiz"), lay);
  addBtn(QStringLiteral("favorites"), QStringLiteral("star"),
         QStringLiteral("Favoriten"), lay);
  addBtn(QStringLiteral("calendar"), QStringLiteral("calendar"),
         QStringLiteral("Kalender"), lay);
  addBtn(QStringLiteral("network"), QStringLiteral("network"),
         QStringLiteral("Gedankenfäden — Notizen verknüpfen"), lay);
  addBtn(QStringLiteral("apps"), QStringLiteral("apps"),
         QStringLiteral("Apps / Study"), lay);

  lay->addStretch(1);

  addBtn(QStringLiteral("settings"), QStringLiteral("settings"),
         QStringLiteral("Einstellungen"), lay);
  addBtn(QStringLiteral("help"), QStringLiteral("help"),
         QStringLiteral("Hilfe"), lay);
  addBtn(QStringLiteral("account"), QStringLiteral("person"),
         QStringLiteral("Konto"), lay);
  refreshStyles();
}

int LibraryIconRail::preferredWidth() const { return UiScale::dp(48); }

QToolButton *LibraryIconRail::addBtn(const QString &id, const QString &iconKey,
                                     const QString &tip, QVBoxLayout *lay) {
  auto *btn = new QToolButton(this);
  btn->setObjectName(QStringLiteral("LibraryIconRailBtn"));
  btn->setProperty("railId", id);
  btn->setProperty("iconKey", iconKey);
  btn->setToolTip(tip);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setAutoRaise(true);
  btn->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btn->setIconSize(QSize(UiScale::dp(20), UiScale::dp(20)));
  btn->setIcon(glyph(iconKey, QColor(200, 204, 214), UiScale::dp(20)));
  connect(btn, &QToolButton::clicked, this, [this, id]() {
    setActiveId(id);
    emit actionTriggered(id);
  });
  m_btns.insert(id, btn);
  lay->addWidget(btn, 0, Qt::AlignHCenter);
  return btn;
}

QToolButton *LibraryIconRail::buttonFor(const QString &id) const {
  return m_btns.value(id, nullptr);
}

void LibraryIconRail::setActiveId(const QString &id) {
  m_active = id;
  refreshStyles();
}

void LibraryIconRail::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  if (m_accent == color)
    return;
  m_accent = color;
  refreshStyles();
  if (m_avatar != QLatin1String("B"))
    setAvatarLetter(m_avatar);
}

void LibraryIconRail::setAvatarLetter(const QString &letter) {
  m_avatar = letter.trimmed().left(1).toUpper();
  if (m_avatar.isEmpty())
    m_avatar = QStringLiteral("B");
  if (QToolButton *b = m_btns.value(QStringLiteral("account"))) {
    QPixmap pm(UiScale::dp(28), UiScale::dp(28));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(m_accent);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, pm.width(), pm.height());
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(UiScale::dp(12));
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, m_avatar);
    b->setIcon(QIcon(pm));
  }
}

void LibraryIconRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), BlopStyle::obsidianNav());
  // Charcoal hairlines (never white) — top under logo / bottom above footer.
  p.setPen(QPen(QColor(255, 255, 255, 18), 1));
  const int midY = height() / 2;
  Q_UNUSED(midY);
  // Soft divider above the stretch footer cluster (settings/help/account).
  if (QToolButton *settings = m_btns.value(QStringLiteral("settings"))) {
    const int y = settings->geometry().top() - UiScale::dp(6);
    if (y > UiScale::dp(40) && y < height() - UiScale::dp(8))
      p.drawLine(UiScale::dp(10), y, width() - UiScale::dp(10), y);
  }
  if (QToolButton *home = m_btns.value(QStringLiteral("home"))) {
    const int y = home->geometry().top() - UiScale::dp(4);
    if (y > UiScale::dp(8))
      p.drawLine(UiScale::dp(10), y, width() - UiScale::dp(10), y);
  }
}

void LibraryIconRail::refreshStyles() {
  const QString nav = BlopStyle::obsidianNav().name(QColor::HexRgb);
  const QString acc = m_accent.name(QColor::HexRgb);
  const QString hover = accentRgba(m_accent, 0.16);
  const QString onBg = accentRgba(m_accent, 0.18);
  const QString onHover = accentRgba(m_accent, 0.28);
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryIconRail { background: %1; border: none; }"
      "QLabel#LibraryIconRailLogo {"
      "  background: %2; color: #FFFFFF; border-radius: 8px;"
      "  font-weight: 800; font-size: 13px;"
      "}"
      "QToolButton#LibraryIconRailBtn {"
      "  background: transparent; border: none; border-radius: 10px;"
      "}"
      "QToolButton#LibraryIconRailBtn:hover {"
      "  background: %3;"
      "}")
                    .arg(nav, acc, hover));
  for (auto it = m_btns.begin(); it != m_btns.end(); ++it) {
    QToolButton *btn = it.value();
    if (!btn)
      continue;
    const bool on = it.key() == m_active;
    const QString iconKey = btn->property("iconKey").toString();
    // Cool gray icons — never pure white (invisible on light hover washes).
    const QColor idleIcon(0xB8, 0xBE, 0xC9);
    if (it.key() == QLatin1String("account") && !m_avatar.isEmpty() &&
        m_avatar != QLatin1String("B")) {
      // Avatar icon set separately.
    } else {
      btn->setIcon(glyph(iconKey, on ? m_accent : idleIcon, UiScale::dp(20)));
    }
    if (on) {
      btn->setStyleSheet(QStringLiteral(
          "QToolButton#LibraryIconRailBtn {"
          "  background: %1; border: none; border-radius: 10px;"
          "}"
          "QToolButton#LibraryIconRailBtn:hover {"
          "  background: %2;"
          "}")
                             .arg(onBg, onHover));
    } else {
      btn->setStyleSheet(QString());
    }
  }
}
