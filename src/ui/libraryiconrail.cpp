#include "libraryiconrail.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "moderntoolbar.h"
#include "uiscale.h"

#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
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

QIcon hamburgerGlyph(const QColor &fg, int px) {
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(fg, qMax(2, px / 10), Qt::SolidLine, Qt::RoundCap));
  const int x0 = px * 22 / 100;
  const int x1 = px - x0;
  const int y0 = px * 30 / 100;
  const int y1 = px / 2;
  const int y2 = px - y0;
  p.drawLine(x0, y0, x1, y0);
  p.drawLine(x0, y1, x1, y1);
  p.drawLine(x0, y2, x1, y2);
  return QIcon(pm);
}
} // namespace

LibraryIconRail::LibraryIconRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryIconRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(preferredWidth());
  m_accent = BlopTheme::accentPrimary();

  auto *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, UiScale::dp(8), 0, UiScale::dp(12));
  lay->setSpacing(UiScale::dp(2));

  const int tap = UiScale::dp(BlopStyle::touchTargetMinDp());
  m_menuBtn = new QToolButton(this);
  m_menuBtn->setObjectName(QStringLiteral("LibraryIconRailMenu"));
  m_menuBtn->setFixedSize(tap, tap);
  m_menuBtn->setIconSize(QSize(UiScale::dp(20), UiScale::dp(20)));
  m_menuBtn->setCursor(Qt::PointingHandCursor);
  m_menuBtn->setAutoRaise(true);
  m_menuBtn->setFocusPolicy(Qt::StrongFocus);
  m_menuBtn->setToolTip(QStringLiteral("Hauptmenü"));
  m_menuBtn->setIcon(hamburgerGlyph(QColor(0xB8, 0xBE, 0xC9), UiScale::dp(20)));
  connect(m_menuBtn, &QToolButton::clicked, this,
          &LibraryIconRail::menuToggled);
  lay->addWidget(m_menuBtn, 0, Qt::AlignHCenter);
  lay->addSpacing(UiScale::dp(4));

  m_logo = new QLabel(this);
  m_logo->setObjectName(QStringLiteral("LibraryIconRailLogo"));
  m_logo->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  m_logo->setAlignment(Qt::AlignCenter);
  m_logo->setScaledContents(false);
  lay->addWidget(m_logo, 0, Qt::AlignHCenter);
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
         QStringLiteral("Gedankenfäden — verknüpfte Notizen & Übersicht"), lay);
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

void LibraryIconRail::setBrandPixmap(const QPixmap &pm) {
  m_brand = pm;
  applyBrand();
}

void LibraryIconRail::applyBrand() {
  if (!m_logo)
    return;
  const int side = UiScale::dp(36);
  m_logo->setText(QString());
  if (m_brand.isNull()) {
    m_logo->setPixmap(QPixmap());
    m_logo->setText(QStringLiteral("B"));
    m_logo->setStyleSheet(QStringLiteral(
        "background: %1; color: #FFFFFF; border-radius: 8px;"
        "font-weight: 800; font-size: 13px;")
                              .arg(m_accent.name(QColor::HexRgb)));
    return;
  }
  QPixmap out(side, side);
  out.fill(Qt::transparent);
  const QPixmap scaled = m_brand.scaled(side, side, Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
  QPainter p(&out);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  const qreal radius = side * 0.28;
  QPainterPath clip;
  clip.addRoundedRect(QRectF(0, 0, side, side), radius, radius);
  p.setClipPath(clip);
  p.drawPixmap((side - scaled.width()) / 2, (side - scaled.height()) / 2,
               scaled);
  m_logo->setPixmap(out);
  m_logo->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
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
      "QToolButton#LibraryIconRailMenu, QToolButton#LibraryIconRailBtn {"
      "  background: transparent; border: none; border-radius: 10px;"
      "}"
      "QToolButton#LibraryIconRailMenu:hover, QToolButton#LibraryIconRailBtn:hover {"
      "  background: %2;"
      "}"
      "QToolButton#LibraryIconRailMenu:pressed, QToolButton#LibraryIconRailBtn:pressed {"
      "  background: %3;"
      "}")
                    .arg(nav, hover, onBg));
  if (m_menuBtn) {
    m_menuBtn->setIcon(
        hamburgerGlyph(QColor(0xB8, 0xBE, 0xC9), UiScale::dp(20)));
  }
  applyBrand();
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
