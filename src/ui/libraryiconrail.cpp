#include "libraryiconrail.h"

#include "blop_theme.h"
#include "moderntoolbar.h"
#include "uiscale.h"

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
  lay->setContentsMargins(UiScale::dp(6), UiScale::dp(10), UiScale::dp(6),
                          UiScale::dp(10));
  lay->setSpacing(UiScale::dp(4));

  addBtn(QStringLiteral("home"), QStringLiteral("home"),
         QStringLiteral("Bibliothek"), lay);
  addBtn(QStringLiteral("new"), QStringLiteral("add"),
         QStringLiteral("Neue Notiz"), lay);
  addBtn(QStringLiteral("folders"), QStringLiteral("folder"),
         QStringLiteral("Ordner"), lay);
  addBtn(QStringLiteral("favorites"), QStringLiteral("star"),
         QStringLiteral("Favoriten"), lay);
  addBtn(QStringLiteral("search"), QStringLiteral("search"),
         QStringLiteral("Suchen"), lay);
  lay->addStretch(1);
  addBtn(QStringLiteral("settings"), QStringLiteral("settings"),
         QStringLiteral("Einstellungen"), lay);
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
  btn->setIcon(glyph(iconKey, QColor(220, 224, 232), UiScale::dp(20)));
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
  Q_UNUSED(letter);
}

void LibraryIconRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), QColor(0x1C, 0x1E, 0x24));
}

void LibraryIconRail::refreshStyles() {
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryIconRail { background: #1C1E24; border: none; }"
      "QToolButton#LibraryIconRailBtn {"
      "  background: transparent; border: none; border-radius: 8px; padding: 0;"
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
          "  background: rgba(91,157,255,0.18); border: none; border-radius: 8px;"
          "  border-left: 3px solid %1;"
          "}"
          "QToolButton#LibraryIconRailBtn:hover { background: rgba(91,157,255,0.24); }")
                           .arg(m_accent.name(QColor::HexRgb)));
    } else {
      b->setStyleSheet(QString());
    }
  }
}
