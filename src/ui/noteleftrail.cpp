#include "noteleftrail.h"

#include "notechrome.h"
#include "uiscale.h"

#include <QFrame>
#include <QLinearGradient>
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
  m_lay->setContentsMargins(UiScale::dp(6), UiScale::dp(10), UiScale::dp(6),
                            UiScale::dp(10));
  m_lay->setSpacing(UiScale::dp(2));

  makeBtn(QStringLiteral("pages"), QStringLiteral("Seitenleiste"));
  makeBtn(QStringLiteral("allpages"), QStringLiteral("Alle Seiten"));
  makeBtn(QStringLiteral("bookmarks"), QStringLiteral("Lesezeichen"));
  makeBtn(QStringLiteral("history"), QStringLiteral("Verlauf"));
  makeBtn(QStringLiteral("search"), QStringLiteral("Suche"));
  makeBtn(QStringLiteral("more"), QStringLiteral("Mehr"));
  addGroupSeparator();
  makeBtn(QStringLiteral("select"), QStringLiteral("Auswahl"));
  makeBtn(QStringLiteral("props"), QStringLiteral("Eigenschaften"));
  m_lay->addStretch(1);
  addGroupSeparator();
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

void NoteLeftRail::addGroupSeparator() {
  auto *sep = new QFrame(this);
  sep->setFixedHeight(1);
  sep->setStyleSheet(QStringLiteral("background: %1; border: none;")
                         .arg(NoteChrome::borderSoft().name(QColor::HexRgb)));
  m_lay->addSpacing(UiScale::dp(4));
  m_lay->addWidget(sep);
  m_lay->addSpacing(UiScale::dp(4));
  m_separators.append(sep);
}

QToolButton *NoteLeftRail::makeBtn(const QString &id, const QString &tip) {
  auto *btn = new QToolButton(this);
  btn->setObjectName(QStringLiteral("NoteLeftRailBtn"));
  btn->setToolTip(tip);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setAutoRaise(true);
  btn->setFixedSize(UiScale::dp(40), UiScale::dp(40));
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
  for (QFrame *sep : m_separators) {
    if (sep)
      sep->setStyleSheet(
          QStringLiteral("background: %1; border: none;")
              .arg(NoteChrome::borderSoft().name(QColor::HexRgb)));
  }
  setStyleSheet(QStringLiteral(
      "QToolButton#NoteLeftRailBtn {"
      "  background: transparent; border: none; border-radius: 6px; padding: 0;"
      "}"
      "QToolButton#NoteLeftRailBtn:hover {"
      "  background: rgba(255,255,255,%1);"
      "}"
      "QToolButton#NoteLeftRailBtn:checked {"
      "  background: rgba(0,0,0,%2);"
      "}")
                    .arg(NoteChrome::isDark() ? QStringLiteral("0.10")
                                              : QStringLiteral("0.14"),
                         NoteChrome::isDark() ? QStringLiteral("0.55")
                                              : QStringLiteral("0.12")));
  // Accent edge for the active pages button — painted via stylesheet border.
  if (QToolButton *pages = m_btns.value(QStringLiteral("pages"))) {
    if (pages->isChecked()) {
      pages->setStyleSheet(QStringLiteral(
          "QToolButton#NoteLeftRailBtn {"
          "  background: rgba(0,0,0,%1); border: none; border-radius: 6px;"
          "  border-left: 3px solid %2; padding: 0;"
          "}"
          "QToolButton#NoteLeftRailBtn:hover {"
          "  background: rgba(255,255,255,%3);"
          "}")
                               .arg(NoteChrome::isDark() ? QStringLiteral("0.55")
                                                         : QStringLiteral("0.12"),
                                    a.name(QColor::HexRgb),
                                    NoteChrome::isDark() ? QStringLiteral("0.10")
                                                         : QStringLiteral("0.14")));
    } else {
      pages->setStyleSheet(QString());
    }
  }
}

void NoteLeftRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  QLinearGradient grad(0, 0, 0, height());
  grad.setColorAt(0, NoteChrome::toolbarFill());
  grad.setColorAt(1, NoteChrome::toolbarFillEnd());
  p.fillRect(rect(), grad);
  p.setPen(QPen(NoteChrome::borderSoft(), 1));
  p.drawLine(width() - 1, 0, width() - 1, height());
}
