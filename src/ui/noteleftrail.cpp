#include "noteleftrail.h"

#include "notechrome.h"
#include "uiscale.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QSize>
#include <QToolButton>
#include <QVBoxLayout>

NoteLeftRail::NoteLeftRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("NoteLeftRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(preferredWidth());

  m_lay = new QVBoxLayout(this);
  m_lay->setContentsMargins(UiScale::dp(6), UiScale::dp(12), UiScale::dp(6),
                            UiScale::dp(12));
  m_lay->setSpacing(UiScale::dp(2));

  makeBtn(QStringLiteral("pages"), QStringLiteral("Seitenleiste"));
  makeBtn(QStringLiteral("allpages"), QStringLiteral("Alle Seiten"));
  makeBtn(QStringLiteral("bookmarks"), QStringLiteral("Lesezeichen"));
  makeBtn(QStringLiteral("history"), QStringLiteral("Verlauf"));
  makeBtn(QStringLiteral("search"), QStringLiteral("Suche"));
  makeBtn(QStringLiteral("more"), QStringLiteral("Mehr"));
  m_lay->addSpacing(UiScale::dp(8));
  makeBtn(QStringLiteral("select"), QStringLiteral("Auswahl"));
  makeBtn(QStringLiteral("props"), QStringLiteral("Eigenschaften"));
  m_lay->addStretch(1);
  makeBtn(QStringLiteral("theme"), QStringLiteral("Editor Hell/Dunkel"));
  makeBtn(QStringLiteral("export"), QStringLiteral("Exportieren"));
  makeBtn(QStringLiteral("settings"), QStringLiteral("Einstellungen"));

  if (auto *pages = m_btns.value(QStringLiteral("pages"))) {
    pages->setCheckable(true);
    pages->setChecked(true);
    connect(pages, &QToolButton::clicked, this, [this](bool) {
      setPagesExpanded(!m_pagesExpanded);
      emit pagesToggled(m_pagesExpanded);
    });
  }
  if (auto *b = m_btns.value(QStringLiteral("allpages")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::allPagesClicked);
  if (auto *b = m_btns.value(QStringLiteral("bookmarks")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::bookmarksClicked);
  if (auto *b = m_btns.value(QStringLiteral("history")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::historyClicked);
  if (auto *b = m_btns.value(QStringLiteral("search")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::searchClicked);
  if (auto *b = m_btns.value(QStringLiteral("more")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::moreClicked);
  if (auto *b = m_btns.value(QStringLiteral("select")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::selectClicked);
  if (auto *b = m_btns.value(QStringLiteral("props")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::propertiesClicked);
  if (auto *b = m_btns.value(QStringLiteral("theme")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::themeToggleClicked);
  if (auto *b = m_btns.value(QStringLiteral("export")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::exportClicked);
  if (auto *b = m_btns.value(QStringLiteral("settings")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::settingsClicked);

  refreshStyles();
}

int NoteLeftRail::preferredWidth() const { return UiScale::dp(52); }

QToolButton *NoteLeftRail::makeBtn(const QString &id, const QString &tip) {
  auto *btn = new QToolButton(this);
  btn->setObjectName(QStringLiteral("NoteLeftRailBtn"));
  btn->setToolTip(tip);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setAutoRaise(true);
  btn->setFixedSize(UiScale::dp(38), UiScale::dp(38));
  btn->setIconSize(QSize(UiScale::dp(18), UiScale::dp(18)));
  m_btns.insert(id, btn);
  m_lay->addWidget(btn, 0, Qt::AlignHCenter);
  return btn;
}

void NoteLeftRail::setIcon(const QString &id, const QIcon &icon) {
  if (QToolButton *b = m_btns.value(id))
    b->setIcon(icon);
}

void NoteLeftRail::setAccentColor(const QColor &color) {
  if (!color.isValid())
    return;
  m_accent = color;
  refreshStyles();
  update();
}

void NoteLeftRail::setPagesExpanded(bool on) {
  m_pagesExpanded = on;
  if (QToolButton *pages = m_btns.value(QStringLiteral("pages")))
    pages->setChecked(on);
  refreshStyles();
}

void NoteLeftRail::refreshStyles() {
  const QColor a = NoteChrome::accent();
  setStyleSheet(QStringLiteral(
      "QToolButton#NoteLeftRailBtn {"
      "  background: transparent; border: none; border-radius: 8px; padding: 0;"
      "}"
      "QToolButton#NoteLeftRailBtn:hover { background: rgba(255,255,255,0.08); }"
      "QToolButton#NoteLeftRailBtn:checked {"
      "  background: rgba(%1,%2,%3,0.22);"
      "}")
                    .arg(a.red())
                    .arg(a.green())
                    .arg(a.blue()));
}

void NoteLeftRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), NoteChrome::panelBg());
  p.setPen(QPen(NoteChrome::borderSoft(), 1));
  p.drawLine(width() - 1, 0, width() - 1, height());
}
