#include "libraryiconrail.h"

#include "moderntoolbar.h"
#include "uiscale.h"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
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

  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, UiScale::dp(12), 0, UiScale::dp(12));
  lay->setSpacing(UiScale::dp(2));

  auto *logo = new QLabel(this);
  logo->setObjectName(QStringLiteral("LibraryIconRailLogo"));
  logo->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  logo->setAlignment(Qt::AlignCenter);
  logo->setText(QStringLiteral("B"));
  logo->setStyleSheet(QStringLiteral(
      "QLabel#LibraryIconRailLogo {"
      "  background: #5B9DFF; color: #FFFFFF; border-radius: 8px;"
      "  font-weight: 800; font-size: 13px;"
      "}"));
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

void LibraryIconRail::setAvatarLetter(const QString &letter) {
  m_avatar = letter.trimmed().left(1).toUpper();
  if (m_avatar.isEmpty())
    m_avatar = QStringLiteral("B");
  if (QToolButton *b = m_btns.value(QStringLiteral("account"))) {
    QPixmap pm(UiScale::dp(28), UiScale::dp(28));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(0x5B, 0x9D, 0xFF));
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
  QWidget::paintEvent(event);
}

void LibraryIconRail::refreshStyles() {
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryIconRail { background: #16181E; border: none; }"
      "QToolButton#LibraryIconRailBtn {"
      "  background: transparent; border: none; border-radius: 10px;"
      "}"
      "QToolButton#LibraryIconRailBtn:hover {"
      "  background: rgba(255,255,255,0.06);"
      "}"));
  for (auto it = m_btns.begin(); it != m_btns.end(); ++it) {
    QToolButton *btn = it.value();
    if (!btn)
      continue;
    const bool on = it.key() == m_active;
    const QString iconKey = btn->property("iconKey").toString();
    if (it.key() == QLatin1String("account") && !m_avatar.isEmpty() &&
        m_avatar != QLatin1String("B")) {
      // Avatar icon set separately.
    } else {
      btn->setIcon(glyph(iconKey, on ? m_accent : QColor(200, 204, 214),
                         UiScale::dp(20)));
    }
    if (on) {
      btn->setStyleSheet(QStringLiteral(
          "QToolButton#LibraryIconRailBtn {"
          "  background: rgba(91,157,255,0.18); border: none; border-radius: 10px;"
          "}"
          "QToolButton#LibraryIconRailBtn:hover {"
          "  background: rgba(91,157,255,0.24);"
          "}"));
    } else {
      btn->setStyleSheet(QString());
    }
  }
}
