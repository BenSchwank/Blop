#include "noteleftrail.h"

#include "uiscale.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>

NoteLeftRail::NoteLeftRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("NoteLeftRail"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFixedWidth(preferredWidth());

  m_lay = new QVBoxLayout(this);
  m_lay->setContentsMargins(UiScale::dp(6), UiScale::dp(10), UiScale::dp(6),
                            UiScale::dp(10));
  m_lay->setSpacing(UiScale::dp(4));

  makeBtn(QStringLiteral("pages"), QStringLiteral("Seiten"));
  makeBtn(QStringLiteral("search"), QStringLiteral("Suche"));
  makeBtn(QStringLiteral("layers"), QStringLiteral("Seitenmanager"));
  makeBtn(QStringLiteral("more"), QStringLiteral("Mehr"));
  m_lay->addSpacing(UiScale::dp(8));
  makeBtn(QStringLiteral("select"), QStringLiteral("Auswahl"));
  m_lay->addStretch(1);
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
  if (auto *b = m_btns.value(QStringLiteral("search")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::searchClicked);
  if (auto *b = m_btns.value(QStringLiteral("layers")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::pageManagerClicked);
  if (auto *b = m_btns.value(QStringLiteral("more")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::moreClicked);
  if (auto *b = m_btns.value(QStringLiteral("select")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::selectClicked);
  if (auto *b = m_btns.value(QStringLiteral("export")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::exportClicked);
  if (auto *b = m_btns.value(QStringLiteral("settings")))
    connect(b, &QToolButton::clicked, this, &NoteLeftRail::settingsClicked);

  refreshStyles();
}

int NoteLeftRail::preferredWidth() const { return UiScale::dp(48); }

QToolButton *NoteLeftRail::makeBtn(const QString &id, const QString &tip) {
  auto *btn = new QToolButton(this);
  btn->setObjectName(QStringLiteral("NoteLeftRailBtn"));
  btn->setToolTip(tip);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setAutoRaise(true);
  btn->setFixedSize(UiScale::dp(36), UiScale::dp(36));
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
  setStyleSheet(QStringLiteral(
      "QToolButton#NoteLeftRailBtn {"
      "  background: transparent; border: none; border-radius: 10px;"
      "  padding: 0;"
      "}"
      "QToolButton#NoteLeftRailBtn:hover {"
      "  background: rgba(255,255,255,0.08);"
      "}"
      "QToolButton#NoteLeftRailBtn:checked {"
      "  background: rgba(%1,%2,%3,0.28);"
      "}")
                    .arg(m_accent.red())
                    .arg(m_accent.green())
                    .arg(m_accent.blue()));
}

void NoteLeftRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
  const qreal radius = 18.0;

  // Soft drop shadow
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 70));
  p.drawRoundedRect(r.translated(0, 2), radius, radius);

  // Blop dark floating plate
  p.setBrush(QColor(14, 12, 24, 245));
  QColor border = m_accent;
  border.setAlpha(75);
  p.setPen(QPen(border, 1.0));
  p.drawRoundedRect(r, radius, radius);
}
