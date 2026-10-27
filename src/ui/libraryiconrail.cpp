#include "libraryiconrail.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "moderntoolbar.h"
#include "uiscale.h"

#include <QHideEvent>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QSettings>
#include <QSizePolicy>
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

} // namespace

LibraryIconRail::LibraryIconRail(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LibraryIconRail"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(preferredWidth());
  m_accent = BlopTheme::accentPrimary();

  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  m_lay = new QVBoxLayout(this);
  m_lay->setContentsMargins(0, UiScale::dp(10), 0, UiScale::dp(10));
  m_lay->setSpacing(UiScale::dp(4));

  m_logo = new QLabel(this);
  m_logo->setObjectName(QStringLiteral("LibraryIconRailLogo"));
  m_logo->setFixedSize(UiScale::dp(36), UiScale::dp(36));
  m_logo->setAlignment(Qt::AlignCenter);
  m_logo->setScaledContents(false);
  m_logo->setCursor(Qt::PointingHandCursor);
  m_logo->setToolTip(QStringLiteral("Seitenleiste ein-/ausklappen"));
  m_logo->setAccessibleName(QStringLiteral("Seitenleiste"));
  m_logo->installEventFilter(this);
  m_lay->addWidget(m_logo, 0, Qt::AlignHCenter);
  m_lay->addSpacing(UiScale::dp(6));

  // Clusters: logo+home/library | mid tools | settings — gaps are draggable.
  addBtn(QStringLiteral("home"), QStringLiteral("home"),
         QStringLiteral("Dashboard"), m_lay);
  addBtn(QStringLiteral("library"), QStringLiteral("note"),
         QStringLiteral("Notizen"), m_lay);
  m_gapTop = addGap(m_lay, 0);
  addBtn(QStringLiteral("new"), QStringLiteral("add"),
         QStringLiteral("Neue Notiz"), m_lay);
  addBtn(QStringLiteral("favorites"), QStringLiteral("star"),
         QStringLiteral("Favoriten"), m_lay);
  addBtn(QStringLiteral("calendar"), QStringLiteral("calendar"),
         QStringLiteral("Kalender"), m_lay);
  addBtn(QStringLiteral("network"), QStringLiteral("network"),
         QStringLiteral("Gedankenfäden — verknüpfte Notizen & Übersicht"), m_lay);
  addBtn(QStringLiteral("apps"), QStringLiteral("apps"),
         QStringLiteral("Apps / Study"), m_lay);
  m_gapBot = addGap(m_lay, 1);

  addBtn(QStringLiteral("settings"), QStringLiteral("settings"),
         QStringLiteral("Einstellungen"), m_lay);
  addBtn(QStringLiteral("help"), QStringLiteral("help"),
         QStringLiteral("Hilfe"), m_lay);
  addBtn(QStringLiteral("account"), QStringLiteral("person"),
         QStringLiteral("Konto"), m_lay);
  loadGapRatio();
  applyGapRatio();
  refreshStyles();
}

QWidget *LibraryIconRail::addGap(QVBoxLayout *lay, int which) {
  auto *gap = new QWidget(this);
  gap->setObjectName(QStringLiteral("LibraryIconRailGap"));
  gap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  gap->setMinimumHeight(UiScale::dp(8));
  auto *gl = new QVBoxLayout(gap);
  gl->setContentsMargins(0, 0, 0, 0);
  gl->setSpacing(0);
  gl->addStretch(1);

  auto *handle = new QWidget(gap);
  handle->setObjectName(QStringLiteral("LibraryIconRailGapHandle"));
  handle->setFixedHeight(UiScale::dp(8));
  handle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  handle->setCursor(Qt::SizeVerCursor);
  handle->setToolTip(QStringLiteral("Ziehen — Cluster verschieben"));
  handle->setAttribute(Qt::WA_StyledBackground, true);
  handle->setStyleSheet(QStringLiteral("background: transparent;"));
  handle->installEventFilter(this);
  if (which == 0)
    m_handleTop = handle;
  else
    m_handleBot = handle;
  gl->addWidget(handle);
  gl->addStretch(1);

  lay->addWidget(gap, 1);
  return gap;
}

void LibraryIconRail::loadGapRatio() {
  const qreal saved =
      QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
          .value(QStringLiteral("ui/iconRailGapRatio"), 0.42)
          .toReal();
  m_gapRatio = qBound(0.12, saved, 0.88);
}

void LibraryIconRail::saveGapRatio() const {
  QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
      .setValue(QStringLiteral("ui/iconRailGapRatio"), m_gapRatio);
}

void LibraryIconRail::applyGapRatio() {
  if (!m_lay || !m_gapTop || !m_gapBot)
    return;
  const int top = qMax(1, qRound(m_gapRatio * 100.0));
  const int bot = qMax(1, 100 - top);
  m_lay->setStretchFactor(m_gapTop, top);
  m_lay->setStretchFactor(m_gapBot, bot);
}

int LibraryIconRail::preferredWidth() const { return UiScale::dp(48); }

QToolButton *LibraryIconRail::addBtn(const QString &id, const QString &iconKey,
                                     const QString &tip, QVBoxLayout *lay) {
  auto *btn = new QToolButton(this);
  btn->setObjectName(QStringLiteral("LibraryIconRailBtn"));
  btn->setProperty("railId", id);
  btn->setProperty("iconKey", iconKey);
  btn->setProperty("railTip", tip);
  btn->setAccessibleName(tip);
  btn->setToolTip(QString());
  btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setAutoRaise(true);
  btn->setFixedSize(UiScale::dp(40), UiScale::dp(40));
  btn->setIconSize(QSize(UiScale::dp(20), UiScale::dp(20)));
  btn->setIcon(glyph(iconKey, QColor(200, 204, 214), UiScale::dp(20)));
  btn->installEventFilter(this);
  connect(btn, &QToolButton::clicked, this, [this, id]() {
    if (id != QLatin1String("new"))
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
  if (!m_avatar.isEmpty())
    setAvatarLetter(m_avatar);
}

void LibraryIconRail::setAvatarLetter(const QString &letter) {
  m_avatar = letter.trimmed().left(1).toUpper();
  if (QToolButton *b = m_btns.value(QStringLiteral("account"))) {
    if (m_avatar.isEmpty()) {
      refreshStyles();
      return;
    }
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

void LibraryIconRail::setLogoToolTip(const QString &tip) {
  if (m_logo)
    m_logo->setToolTip(tip);
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

void LibraryIconRail::hideEvent(QHideEvent *event) {
  hideFlyout();
  QWidget::hideEvent(event);
}

bool LibraryIconRail::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_logo && event->type() == QEvent::MouseButtonPress) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
      emit logoActivated();
      return true;
    }
  }

  const bool isTop = watched == m_handleTop;
  const bool isBot = watched == m_handleBot;
  if (isTop || isBot) {
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton) {
        m_dragWhich = isTop ? 0 : 1;
        m_dragStartY = me->globalPosition().toPoint().y();
        m_dragStartRatio = m_gapRatio;
        return true;
      }
    }
    if (event->type() == QEvent::MouseMove && m_dragWhich >= 0) {
      auto *me = static_cast<QMouseEvent *>(event);
      const int freeH =
          (m_gapTop ? m_gapTop->height() : 0) +
          (m_gapBot ? m_gapBot->height() : 0);
      if (freeH > UiScale::dp(16)) {
        const int dy = me->globalPosition().toPoint().y() - m_dragStartY;
        // Dragging either handle down grows the upper gap (mid cluster moves down).
        m_gapRatio =
            qBound(0.12, m_dragStartRatio + (qreal)dy / (qreal)freeH, 0.88);
        applyGapRatio();
        update();
      }
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && m_dragWhich >= 0) {
      m_dragWhich = -1;
      saveGapRatio();
      return true;
    }
  }

  auto *btn = qobject_cast<QToolButton *>(watched);
  if (!btn || !m_btns.values().contains(btn))
    return QWidget::eventFilter(watched, event);
  if (event->type() == QEvent::Enter) {
    showFlyout(btn);
  } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
    hideFlyout();
  }
  return QWidget::eventFilter(watched, event);
}

void LibraryIconRail::showFlyout(QToolButton *btn) {
  if (!btn)
    return;
  QWidget *host = window();
  if (!host)
    return;
  const QString tip = btn->property("railTip").toString();
  if (tip.isEmpty())
    return;
  if (!m_flyout) {
    m_flyout = new QLabel(host);
    m_flyout->setObjectName(QStringLiteral("LibraryIconRailTip"));
    m_flyout->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_flyout->setAttribute(Qt::WA_ShowWithoutActivating, true);
  } else if (m_flyout->parentWidget() != host) {
    m_flyout->setParent(host);
  }
  const bool dark = BlopTheme::instance().isDark();
  const QString tipBg =
      dark ? BlopStyle::obsidianSheet().name(QColor::HexRgb)
           : BlopStyle::paperSurface().name(QColor::HexRgb);
  const QString tipFg =
      dark ? BlopStyle::obsidianText().name(QColor::HexRgb)
           : BlopStyle::paperInk().name(QColor::HexRgb);
  const QString tipBd =
      dark ? QStringLiteral("rgba(255,255,255,0.14)")
           : BlopStyle::paperBorder().name(QColor::HexRgb);
  m_flyout->setStyleSheet(QStringLiteral(
      "QLabel#LibraryIconRailTip {"
      "  background: %1; color: %2;"
      "  border: 1px solid %3;"
      "  border-radius: %4px;"
      "  padding: %5px %6px;"
      "  font-size: 12px; font-weight: 650;"
      "}")
                              .arg(tipBg, tipFg, tipBd,
                                   QString::number(UiScale::dp(8)),
                                   QString::number(UiScale::dp(4)),
                                   QString::number(UiScale::dp(10))));
  m_flyout->setText(tip);
  m_flyout->adjustSize();
  const QPoint global = btn->mapToGlobal(
      QPoint(btn->width() + UiScale::dp(8),
             (btn->height() - m_flyout->height()) / 2));
  QPoint local = host->mapFromGlobal(global);
  const int maxX = qMax(0, host->width() - m_flyout->width() - UiScale::dp(8));
  if (local.x() > maxX)
    local.setX(maxX);
  if (local.x() < 0)
    local.setX(0);
  m_flyout->move(local);
  m_flyout->show();
  m_flyout->raise();
}

void LibraryIconRail::hideFlyout() {
  if (m_flyout)
    m_flyout->hide();
}

void LibraryIconRail::setInnerEdge(bool on) {
  if (m_innerEdge == on)
    return;
  m_innerEdge = on;
  update();
}

void LibraryIconRail::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.fillRect(rect(), BlopStyle::libraryIconRail());
  if (m_innerEdge) {
    // Start under the title/header band so the L-leg reads as one piece.
    const int y = UiScale::dp(44);
    p.setPen(QPen(QColor(0x4A, 0x4E, 0x58), 1));
    p.drawLine(width() - 1, y, width() - 1, height() - 1);
  }
  // Hairlines at the draggable gap-handle midpoints.
  const bool lightRail = BlopStyle::libraryIconRail().lightness() > 140;
  p.setPen(QPen(lightRail ? QColor(0, 0, 0, 28) : QColor(255, 255, 255, 22), 1));
  auto drawHandleLine = [&](QWidget *handle) {
    if (!handle || !handle->isVisible())
      return;
    const QPoint c = handle->mapTo(this, QPoint(0, handle->height() / 2));
    if (c.y() > UiScale::dp(8) && c.y() < height() - UiScale::dp(8))
      p.drawLine(UiScale::dp(10), c.y(), width() - UiScale::dp(10), c.y());
  };
  drawHandleLine(m_handleTop);
  drawHandleLine(m_handleBot);
}

void LibraryIconRail::refreshStyles() {
  const QString nav = BlopStyle::libraryIconRail().name(QColor::HexRgb);
  const QString hover = accentRgba(m_accent, 0.16);
  const QString onBg = accentRgba(m_accent, 0.18);
  const QString onHover = accentRgba(m_accent, 0.28);
  setStyleSheet(QStringLiteral(
      "QWidget#LibraryIconRail { background: %1; border: none; }"
      "QToolButton#LibraryIconRailBtn {"
      "  background: transparent; border: none; border-radius: 10px;"
      "}"
      "QToolButton#LibraryIconRailBtn:hover {"
      "  background: %2;"
      "}"
      "QToolButton#LibraryIconRailBtn:pressed {"
      "  background: %3;"
      "}")
                    .arg(nav, hover, onBg));
  applyBrand();
  for (auto it = m_btns.begin(); it != m_btns.end(); ++it) {
    QToolButton *btn = it.value();
    if (!btn)
      continue;
    const bool isNew = it.key() == QLatin1String("new");
    const bool on = !isNew && it.key() == m_active;
    const QString iconKey = btn->property("iconKey").toString();
    const bool lightRail = BlopStyle::libraryIconRail().lightness() > 140;
    const QColor idleIcon =
        lightRail ? BlopStyle::paperInkMuted() : QColor(0xB8, 0xBE, 0xC9);
    if (it.key() == QLatin1String("account") && !m_avatar.isEmpty()) {
      // Avatar icon set separately.
    } else {
      btn->setIcon(glyph(iconKey, (on || isNew) ? m_accent : idleIcon,
                         UiScale::dp(20)));
    }
    if (isNew) {
      btn->setStyleSheet(QStringLiteral(
          "QToolButton#LibraryIconRailBtn {"
          "  background: transparent; border: 1px solid %1;"
          "  border-radius: 10px; padding: 0;"
          "}"
          "QToolButton#LibraryIconRailBtn:hover {"
          "  background: %2; border: 1px solid %1;"
          "}"
          "QToolButton#LibraryIconRailBtn:pressed {"
          "  background: %3; border: 1px solid %1;"
          "}")
                             .arg(accentRgba(m_accent, 0.70), hover, onBg));
    } else if (on) {
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
