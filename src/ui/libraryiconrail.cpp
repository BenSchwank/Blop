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
  lay->setContentsMargins(UiScale::dp(6), UiScale::dp(12), UiScale::dp(6),
                          UiScale::dp(12));
  lay->setSpacing(UiScale::dp(2));

  // K rail: brand mark, then primary destinations, then utilities.
  auto *logo = new QLabel(this);
  logo->setObjectName(QStringLiteral("LibraryIconRailLogo"));
  logo->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  logo->setAlignment(Qt::AlignCenter);
  logo->setText(QStringLiteral("B"));
  logo->setStyleSheet(QStringLiteral(
      "QLabel#LibraryIconRailLogo {"
      "  background: #2A2D36; color: #E8EAF0; border-radius: 8px;"
      "  font-weight: 800; font-size: 13px;"
      "}"));
  lay->addWidget(logo, 0, Qt::AlignHCenter);
  lay->addSpacing(UiScale::dp(8));

  addBtn(QStringLiteral("new"), QStringLiteral("add"),
         QStringLiteral("Neue Notiz"), lay);
  addBtn(QStringLiteral("library"), QStringLiteral("home"),
         QStringLiteral("Bibliothek"), lay);
  addBtn(QStringLiteral("folders"), QStringLiteral("folder"),
         QStringLiteral("Ordner"), lay);
  addBtn(QStringLiteral("favorites"), QStringLiteral("star"),
         QStringLiteral("Favoriten"), lay);
  addBtn(QStringLiteral("search"), QStringLiteral("search"),
         QStringLiteral("Suchen"), lay);

  lay->addStretch(1);

  addBtn(QStringLiteral("settings"), QStringLiteral("settings"),
         QStringLiteral("Einstellungen"), lay);
  addBtn(QStringLiteral("help"), QStringLiteral("help"),
         QStringLiteral("Hilfe"), lay);
  addBtn(QStringLiteral("account"), QStringLiteral("person"),
         QStringLiteral("Konto"), lay);
  refreshStyles();
}

int LibraryIconRail::preferredWidth() const { return UiScale::dp(52); }

QToolButton *LibraryIconRail::addBtn(const QString &id, const QString &iconKey,
                                     const QString &tip, QVBoxLayout *lay) {
  auto *btn = new QToolButton(this);
  btn->setObjectName(QStringLiteral("LibraryIconRailBtn"));
  btn->setProperty("railId", id);
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
    p.drawEllipse(pm.rect());
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(UiScale::sp(11));
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, m_avatar);
    b->setIcon(QIcon(pm));
  }
}

void LibraryIconRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), QColor(0x16, 0x18, 0x1E));
}

void LibraryIconRail::refreshStyles() {
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryIconRail { background: #16181E; border: none; }"
      "QToolButton#LibraryIconRailBtn {"
      "  background: transparent; border: none; border-radius: 10px; padding: 0;"
      "}"
      "QToolButton#LibraryIconRailBtn:hover {"
      "  background: rgba(255,255,255,0.08);"
      "}"));
  for (auto it = m_btns.begin(); it != m_btns.end(); ++it) {
    QToolButton *b = it.value();
    if (!b)
      continue;
    const bool on = (it.key() == m_active);
    if (on) {
      b->setStyleSheet(QStringLiteral(
          "QToolButton#LibraryIconRailBtn {"
          "  background: rgba(91,157,255,0.18); border: none; border-radius: 10px;"
          "}"
          "QToolButton#LibraryIconRailBtn:hover {"
          "  background: rgba(91,157,255,0.26);"
          "}"));
    } else {
      b->setStyleSheet(QString());
    }
  }
}
