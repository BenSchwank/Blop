#include "dashwidget.h"

#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "bloplocale.h"
#include "blopstyle.h"
#include "calendardayview.h"
#include "calendarservice.h"
#include "dashboardlayoutstore.h"
#include "dashdeskhero.h"
#include "editoroverlays.h"
#include "libraryorgstore.h"
#include "todostore.h"
#include "uiscale.h"

#include <QCheckBox>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <functional>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSizePolicy>
#include <QTime>
#include <QVBoxLayout>

namespace {
QString ink() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 220).name(QColor::HexArgb)
             : BlopStyle::paperInk().name(QColor::HexRgb);
}
QString muted() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 150).name(QColor::HexArgb)
             : BlopStyle::paperInkMuted().name(QColor::HexRgb);
}
/// Soft sheet — closer to desk than a floating admin card.
QString cardBg() {
  return BlopTheme::instance().isDark()
             ? BlopStyle::obsidianSheet().name(QColor::HexRgb)
             : QStringLiteral("#FFFFFF");
}
QString cardBorder() {
  return BlopTheme::instance().isDark()
             ? QColor(255, 255, 255, 14).name(QColor::HexArgb)
             : QColor(15, 23, 42, 18).name(QColor::HexArgb);
}
QString accent() { return BlopTheme::accentPrimary().name(QColor::HexRgb); }

QColor withAlpha(QColor c, int a) {
  c.setAlpha(a);
  return c;
}

QString hoverBorder() {
  return withAlpha(BlopTheme::accentPrimary(),
                   BlopTheme::instance().isDark() ? 90 : 110)
      .name(QColor::HexArgb);
}

QString checkBoxQss() {
  const int s = UiScale::dp(16);
  const int r = UiScale::dp(4);
  return QStringLiteral(
             "QCheckBox { spacing: %1px; background: transparent; }"
             "QCheckBox::indicator {"
             "  width: %2px; height: %2px;"
             "  border-radius: %3px;"
             "  border: 1.5px solid %4;"
             "  background: transparent;"
             "}"
             "QCheckBox::indicator:hover { border-color: %5; }"
             "QCheckBox::indicator:checked {"
             "  background: %5; border-color: %5;"
             "}")
      .arg(UiScale::dp(8))
      .arg(s)
      .arg(r)
      .arg(muted(), accent());
}

QLabel *makeMutedLine(const QString &text, QWidget *parent) {
  auto *l = new QLabel(text, parent);
  l->setWordWrap(true);
  l->setStyleSheet(QStringLiteral(
                       "color: %1; font-size: 13px; font-weight: 400;"
                       "background: transparent;")
                       .arg(muted()));
  return l;
}

QLabel *makeDueBadge(const QDateTime &due, QWidget *parent) {
  if (!due.isValid())
    return nullptr;
  const QDate today = QDate::currentDate();
  const QDate d = due.date();
  QString text;
  if (d == today)
    text = QStringLiteral("heute");
  else if (d == today.addDays(1))
    text = QStringLiteral("morgen");
  else
    text = d.toString(QStringLiteral("d.M."));
  auto *b = new QLabel(text, parent);
  b->setStyleSheet(QStringLiteral(
                       "color: %1; font-size: 11px; font-weight: 550;"
                       "background: %2; border-radius: 6px;"
                       "padding: 2px 7px;")
                       .arg(muted(),
                            BlopTheme::instance().isDark()
                                ? QStringLiteral("rgba(255,255,255,0.06)")
                                : QStringLiteral("rgba(15,23,42,0.05)")));
  return b;
}

void addHairline(QVBoxLayout *lay, QWidget *parent) {
  Q_UNUSED(lay);
  Q_UNUSED(parent);
  // Notion lists breathe via row padding — no dividers.
}

QString quietLinkQss() {
  return QStringLiteral(
             "QPushButton {"
             "  color: %1; font-size: 13px; font-weight: 450;"
             "  background: transparent; border: none;"
             "  text-align: left; padding: 2px 0;"
             "}"
             "QPushButton:hover { color: %2; }")
      .arg(muted(), accent());
}
} // namespace

DashWidget::DashWidget(const QString &id, QWidget *parent)
    : QFrame(parent), m_id(id) {
  setObjectName(QStringLiteral("DashWidget"));
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_Hover, true);
  setProperty("dashBlockId", id);

  m_root = new QVBoxLayout(this);
  m_root->setContentsMargins(UiScale::dp(16), UiScale::dp(14), UiScale::dp(16),
                             UiScale::dp(12));
  m_root->setSpacing(UiScale::dp(8));

  m_header = new QWidget(this);
  auto *hdrLay = new QHBoxLayout(m_header);
  hdrLay->setContentsMargins(0, 0, 0, 0);
  hdrLay->setSpacing(UiScale::dp(8));

  m_grip = new QLabel(QStringLiteral("⋮⋮"), m_header);
  m_grip->setObjectName(QStringLiteral("DashDragGrip"));
  m_grip->setCursor(Qt::SizeAllCursor);
  m_grip->setVisible(false);
  m_grip->installEventFilter(this);
  m_grip->setStyleSheet(QStringLiteral(
                            "color: %1; font-size: 14px; font-weight: 700;"
                            "background: transparent; padding: 2px 4px;")
                            .arg(muted()));
  hdrLay->addWidget(m_grip, 0);

  m_title = new QLabel(DashboardLayoutStore::displayName(id), m_header);
  m_title->setObjectName(QStringLiteral("DashWidgetTitle"));
  m_title->setMinimumWidth(UiScale::dp(24));
  m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_title->setStyleSheet(QStringLiteral(
                             "color: %1; font-size: 15px; font-weight: 600;"
                             "letter-spacing: -0.3px; background: transparent;")
                             .arg(ink()));
  hdrLay->addWidget(m_title, 1);

  // Size comes from the corner resize handle only — no duplicate S/M/L chips.

  m_btnStyle = new QPushButton(QStringLiteral("Stil"), m_header);
  m_btnStyle->setCursor(Qt::PointingHandCursor);
  m_btnStyle->setFlat(true);
  m_btnStyle->setVisible(false);
  m_btnStyle->setFixedHeight(UiScale::dp(24));
  m_btnStyle->setStyleSheet(BlopStyle::quietIconButtonQss());
  m_btnStyle->setToolTip(QStringLiteral("Hintergrund & Rahmen"));
  connect(m_btnStyle, &QPushButton::clicked, this, &DashWidget::showStyleMenu);
  hdrLay->addWidget(m_btnStyle, 0);

  m_root->addWidget(m_header, 0);

  m_bodyHost = new QWidget(this);
  m_bodyHost->setObjectName(QStringLiteral("DashWidgetBody"));
  m_bodyLay = new QVBoxLayout(m_bodyHost);
  m_bodyLay->setContentsMargins(0, 0, 0, 0);
  m_bodyLay->setSpacing(0);
  m_root->addWidget(m_bodyHost, 1);

  m_resizeHandle = new QWidget(this);
  m_resizeHandle->setObjectName(QStringLiteral("DashResizeHandle"));
  m_resizeHandle->setCursor(Qt::SizeFDiagCursor);
  m_resizeHandle->setVisible(false);
  m_resizeHandle->installEventFilter(this);
  m_resizeHandle->setStyleSheet(QStringLiteral(
      "QWidget#DashResizeHandle {"
      "  background: %1;"
      "  border: 1px solid %2;"
      "  border-radius: %3px;"
      "}"
      "QWidget#DashResizeHandle:hover { background: %4; }")
                                    .arg(accent(),
                                         QColor(255, 255, 255, 200).name(QColor::HexArgb),
                                         QString::number(UiScale::dp(4)),
                                         BlopTheme::accentHover().name(
                                             QColor::HexRgb)));

  applyChrome();
  rebuildBody();
  updateEditAffordances();
}

DashWidget *DashWidget::create(const QString &id, QWidget *parent) {
  return new DashWidget(id, parent);
}

void DashWidget::applyChrome() {
  const bool intro = (m_id == QLatin1String("intro"));
  const bool banner = DashboardLayoutStore::isBannerId(m_id);
  // Banner always stays page-chrome (wash/cover) — never a gray card plate,
  // including in edit mode (edit lives on DashDeskHero buttons).
  const bool chromeless = banner;
  const int r = UiScale::dp(cardRadiusDp());
  const bool compact = isCompact();

  QString border = QStringLiteral("transparent");
  if (m_lifted) {
    border = withAlpha(BlopTheme::accentPrimary(), 170).name(QColor::HexArgb);
  } else if (m_editMode && !banner) {
    border = withAlpha(BlopTheme::accentPrimary(), 70).name(QColor::HexArgb);
  } else if (!chromeless && m_borderEnabled) {
    border = m_borderColor.isEmpty() ? cardBorder() : m_borderColor;
  }

  QString bg = QStringLiteral("transparent");
  if (m_lifted) {
    bg = BlopTheme::instance().isDark() ? QStringLiteral("#303440")
                                        : QStringLiteral("#FFFFFF");
  } else if (!chromeless && m_bgEnabled) {
    bg = m_bgColor.isEmpty() ? cardBg() : m_bgColor;
  }

  setStyleSheet(QStringLiteral(
                    "QFrame#DashWidget {"
                    "  background: %1;"
                    "  border: 1px solid %2;"
                    "  border-radius: %3px;"
                    "}"
                    "QFrame#DashWidget:hover {"
                    "  border-color: %4;"
                    "}")
                    .arg(bg, border, QString::number(r),
                         m_editMode ? accent()
                                    : (chromeless || !m_borderEnabled
                                           ? border
                                           : hoverBorder())));
  setGraphicsEffect(nullptr);
  if (m_title) {
    const int titlePx = compact ? 13 : 15;
    m_title->setStyleSheet(
        QStringLiteral("color: %1; font-size: %2px; font-weight: 600;"
                       "letter-spacing: -0.3px; background: transparent;")
            .arg(ink())
            .arg(titlePx));
  }
  if (m_root) {
    if (banner) {
      // Full-bleed cover — never switch to card insets (that read as a gray bar).
      m_root->setContentsMargins(0, 0, 0, 0);
      m_root->setSpacing(0);
    } else if (intro && !m_editMode) {
      // Slightly tighter greeting inset — chrome plate still applies above.
      m_root->setContentsMargins(UiScale::dp(14), UiScale::dp(12),
                                 UiScale::dp(14), UiScale::dp(10));
      m_root->setSpacing(UiScale::dp(6));
    } else if (compact) {
      m_root->setContentsMargins(UiScale::dp(10), UiScale::dp(8),
                                 UiScale::dp(10), UiScale::dp(8));
      m_root->setSpacing(UiScale::dp(4));
    } else {
      m_root->setContentsMargins(UiScale::dp(16), UiScale::dp(14),
                                 UiScale::dp(16), UiScale::dp(12));
      m_root->setSpacing(UiScale::dp(8));
    }
  }
  adaptHeaderDensity();
}

void DashWidget::applyChromePrefs(bool bgEnabled, bool borderEnabled,
                                  const QString &bgColor,
                                  const QString &borderColor) {
  m_bgEnabled = bgEnabled;
  m_borderEnabled = borderEnabled;
  m_bgColor = bgColor;
  m_borderColor = borderColor;
  applyChrome();
}

void DashWidget::setEditMode(bool on) {
  const bool changed = (m_editMode != on);
  m_editMode = on;
  updateEditAffordances();
  applyChrome();
  // Intro edit affordance + calendar chrome depend on edit/size state.
  if (changed && (m_id == QLatin1String("intro") ||
                  m_id == QLatin1String("calendar")))
    rebuildBody();
}

void DashWidget::setPhoneMode(bool phone) {
  if (m_phone == phone)
    return;
  m_phone = phone;
  updateEditAffordances();
}

void DashWidget::updateEditAffordances() {
  const bool showChrome = m_editMode;
  // On phone the board is a stack — move/resize handles do nothing.
  const bool showHandles = m_editMode && !m_phone;
  if (m_grip)
    m_grip->setVisible(showHandles);
  if (m_btnStyle)
    m_btnStyle->setVisible(showChrome &&
                           !DashboardLayoutStore::isBannerId(m_id));
  if (m_resizeHandle) {
    m_resizeHandle->setVisible(showHandles);
    if (showHandles)
      layoutResizeHandle();
  }
  // Intro / banner read as page chrome — hide card title outside edit.
  // Banner keeps chromeless even while editing (hero owns cover/wash controls).
  if (m_title) {
    if (DashboardLayoutStore::isBannerId(m_id))
      m_title->setVisible(false);
    else {
      const bool chromeless = (m_id == QLatin1String("intro"));
      m_title->setVisible(!chromeless || m_editMode);
    }
  }
  if (m_header) {
    if (DashboardLayoutStore::isBannerId(m_id))
      m_header->setVisible(false);
    else {
      const bool chromeless = (m_id == QLatin1String("intro"));
      m_header->setVisible(m_editMode || !chromeless);
    }
  }
  if (m_btnStyle && DashboardLayoutStore::isBannerId(m_id))
    m_btnStyle->setVisible(false);
  if (m_grip && DashboardLayoutStore::isBannerId(m_id))
    m_grip->setVisible(false);
  if (auto *hero = findChild<DashDeskHero *>())
    hero->setEditMode(m_editMode);
  adaptHeaderDensity();
}

void DashWidget::setLifted(bool lifted) {
  if (m_lifted == lifted)
    return;
  m_lifted = lifted;
  applyChrome();
}

void DashWidget::setSizeClass(DashSizeClass sizeClass) {
  if (m_sizeClass == sizeClass)
    return;
  const int wasCs = DashboardLayoutStore::colSpanFor(m_sizeClass);
  const int wasRs = DashboardLayoutStore::rowSpanFor(m_sizeClass);
  m_sizeClass = sizeClass;
  const int isCs = DashboardLayoutStore::colSpanFor(m_sizeClass);
  const int isRs = DashboardLayoutStore::rowSpanFor(m_sizeClass);
  const bool wasTiny = wasCs <= 3 || wasRs <= 2;
  const bool isTiny = isCs <= 3 || isRs <= 2;
  if (wasCs != isCs || wasRs != isRs || wasTiny != isTiny ||
      m_id == QLatin1String("calendar") || m_id == QLatin1String("shortcuts") ||
      m_id == QLatin1String("recent") || m_id == QLatin1String("todos") ||
      m_id == QLatin1String("today"))
    rebuildBody();
  adaptHeaderDensity();
  applyChrome();
}

int DashWidget::cardRadiusDp() { return BlopStyle::radiusMdDp(); }

void DashWidget::layoutResizeHandle() {
  if (!m_resizeHandle)
    return;
  const int grip = UiScale::dp(16);
  const int inset = UiScale::dp(6);
  m_resizeHandle->setGeometry(width() - grip - inset, height() - grip - inset,
                              grip, grip);
  m_resizeHandle->raise();
}

void DashWidget::setBody(QWidget *body) {
  while (QLayoutItem *it = m_bodyLay->takeAt(0)) {
    if (it->widget())
      delete it->widget();
    delete it;
  }
  if (body) {
    body->setParent(m_bodyHost);
    m_bodyLay->addWidget(body, 1);
  }
}

void DashWidget::refreshContent() {
  // Keep the banner widget alive across board refreshes so an open file dialog
  // / in-progress crop cannot leave a stale hero on screen without its cover.
  if (DashboardLayoutStore::isBannerId(m_id)) {
    if (auto *hero = findChild<DashDeskHero *>()) {
      hero->reloadCover();
      hero->setEditMode(m_editMode);
      return;
    }
  }
  rebuildBody();
}

void DashWidget::rebuildBody() {
  QWidget *body = nullptr;
  if (m_id == QLatin1String("intro"))
    body = buildIntro();
  else if (DashboardLayoutStore::isBannerId(m_id))
    body = buildBanner();
  else if (m_id == QLatin1String("today"))
    body = buildToday();
  else if (m_id == QLatin1String("todos"))
    body = buildTodos();
  else if (m_id == QLatin1String("calendar"))
    body = buildCalendar();
  else if (m_id == QLatin1String("recent"))
    body = buildRecent();
  else if (m_id == QLatin1String("shortcuts"))
    body = buildShortcuts();
  setBody(body);
}

QWidget *DashWidget::buildIntro() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(4));

  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QString custom =
      st.value(QStringLiteral("dashboard/intro.customGreeting")).toString().trimmed();
  QString hello = custom;
  if (hello.isEmpty()) {
    const int h = QTime::currentTime().hour();
    if (h < 11)
      hello = BlopLocale::instance().goodMorning();
    else if (h < 17)
      hello = BlopLocale::instance().goodAfternoon();
    else
      hello = BlopLocale::instance().goodEvening();
  }

  auto *helloLbl = new QLabel(hello, body);
  helloLbl->setWordWrap(true);
  helloLbl->setStyleSheet(
      QStringLiteral("color: %1; font-size: %2px; font-weight: 700;"
                     "letter-spacing: -1.0px; background: transparent;")
          .arg(ink())
          .arg(m_sizeClass == DashSizeClass::Title ? 26 : 30));
  lay->addWidget(helloLbl);

  int open = 0;
  for (const auto &t : TodoStore::load()) {
    if (!t.done)
      ++open;
  }
  const int events =
      CalendarService::instance().eventsForDay(QDate::currentDate()).size();
  const QString dateStr = BlopLocale::instance().formatDate(
      QDate::currentDate(), QStringLiteral("dddd, d. MMMM"));

  QString focus;
  if (events == 0 && open == 0) {
    focus = BlopLocale::instance().isGerman()
                ? QStringLiteral("ruhiger Tag")
                : QStringLiteral("quiet day");
  } else {
    QStringList bits;
    if (events > 0) {
      if (BlopLocale::instance().isGerman())
        bits << (events == 1 ? QStringLiteral("1 Termin")
                             : QStringLiteral("%1 Termine").arg(events));
      else
        bits << (events == 1 ? QStringLiteral("1 event")
                             : QStringLiteral("%1 events").arg(events));
    }
    if (open > 0) {
      if (BlopLocale::instance().isGerman())
        bits << (open == 1 ? QStringLiteral("1 Aufgabe")
                           : QStringLiteral("%1 Aufgaben").arg(open));
      else
        bits << (open == 1 ? QStringLiteral("1 task")
                           : QStringLiteral("%1 tasks").arg(open));
    }
    focus = bits.join(QStringLiteral(" · "));
  }

  auto *meta = new QLabel(
      QStringLiteral("%1  ·  %2").arg(dateStr, focus), body);
  meta->setWordWrap(true);
  meta->setStyleSheet(
      QStringLiteral("color: %1; font-size: 13px; font-weight: 450;"
                     "background: transparent;")
          .arg(muted()));
  lay->addWidget(meta);

  // Quick actions fill the empty title strip — Notion-page feeling.
  auto *actions = new QWidget(body);
  auto *actLay = new QHBoxLayout(actions);
  actLay->setContentsMargins(0, UiScale::dp(6), 0, 0);
  actLay->setSpacing(UiScale::dp(8));
  struct Quick {
    const char *de;
    const char *en;
    std::function<void()> fire;
  };
  const QList<Quick> quicks = {
      {"Neue Notiz", "New note", [this]() { emit newNoteRequested(); }},
      {"Bibliothek", "Library", [this]() { emit snapToNotesRequested(); }},
      {"Lernen", "Study", [this]() { emit studyRequested(); }},
  };
  const QString chipBg =
      BlopTheme::instance().isDark() ? QStringLiteral("rgba(255,255,255,0.05)")
                                     : QStringLiteral("rgba(15,23,42,0.04)");
  const QString chipHover =
      BlopTheme::instance().isDark() ? QStringLiteral("rgba(255,255,255,0.09)")
                                     : QStringLiteral("rgba(15,23,42,0.07)");
  for (const auto &q : quicks) {
    auto *btn = new QPushButton(
        BlopLocale::instance().isGerman() ? QString::fromUtf8(q.de)
                                          : QString::fromUtf8(q.en),
        actions);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setMinimumHeight(UiScale::dp(30));
    btn->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background: %1; border: none; border-radius: %2px;"
                       "  color: %3; font-size: 12px; font-weight: 550;"
                       "  padding: 4px 12px;"
                       "}"
                       "QPushButton:hover { background: %4; }")
            .arg(chipBg)
            .arg(UiScale::dp(BlopStyle::radiusMdDp()))
            .arg(ink())
            .arg(chipHover));
    connect(btn, &QPushButton::clicked, this, q.fire);
    actLay->addWidget(btn, 0);
  }
  actLay->addStretch(1);
  lay->addWidget(actions);

  if (m_editMode) {
    auto *edit = new QPushButton(
        BlopLocale::instance().isGerman() ? QStringLiteral("Text anpassen…")
                                          : QStringLiteral("Edit text…"),
        body);
    edit->setCursor(Qt::PointingHandCursor);
    edit->setFlat(true);
    edit->setStyleSheet(
        QStringLiteral("QPushButton { color: %1; font-size: 12px; font-weight: 550;"
                       "  background: transparent; border: none; text-align: left;"
                       "  padding: 4px 0; }"
                       "QPushButton:hover { color: %2; }")
            .arg(accent(), BlopTheme::accentHover().name(QColor::HexRgb)));
    connect(edit, &QPushButton::clicked, this, [this, edit]() {
      QList<BlopInWindowMenu::Item> items;
      items.push_back(
          {QStringLiteral("Text ändern…"), QIcon(), [this]() {
             QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
             const QString cur =
                 st.value(QStringLiteral("dashboard/intro.customGreeting"))
                     .toString();
             const QString next = BlopDialogs::promptText(
                 this, QStringLiteral("Begrüßung"),
                 QStringLiteral("Eigener Begrüßungstext:"), cur);
             // Cancel returns empty — keep existing custom text.
             if (next.isEmpty())
               return;
             st.setValue(QStringLiteral("dashboard/intro.customGreeting"),
                         next.trimmed());
             emit contentChanged();
             rebuildBody();
           }});
      items.push_back(
          {QStringLiteral("Automatisch (Tageszeit)"), QIcon(), [this]() {
             QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
             st.remove(QStringLiteral("dashboard/intro.customGreeting"));
             emit contentChanged();
             rebuildBody();
           }});
      BlopInWindowMenu::show(
          this, edit->mapToGlobal(QPoint(0, edit->height())), items);
    });
    lay->addWidget(edit, 0, Qt::AlignLeft);
  }

  return body;
}

QWidget *DashWidget::buildBanner() {
  auto *hero = new DashDeskHero(m_id);
  hero->setEditMode(m_editMode);
  connect(hero, &DashDeskHero::removeRequested, this, [this]() {
    emit removeBannerRequested(m_id);
  });
  return hero;
}

bool DashWidget::eventFilter(QObject *watched, QEvent *event) {
  if (m_editMode && event->type() == QEvent::MouseButtonPress) {
    const auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
      if (watched == m_grip) {
        emit dragHandlePressed(me->globalPosition().toPoint());
        return true;
      }
      if (watched == m_resizeHandle) {
        emit resizeHandlePressed(me->globalPosition().toPoint());
        return true;
      }
    }
  }
  return QFrame::eventFilter(watched, event);
}

void DashWidget::resizeEvent(QResizeEvent *event) {
  QFrame::resizeEvent(event);
  if (m_editMode)
    layoutResizeHandle();
  adaptHeaderDensity();
}

bool DashWidget::isCompact() const {
  const int cs = DashboardLayoutStore::colSpanFor(m_sizeClass);
  const int rs = DashboardLayoutStore::rowSpanFor(m_sizeClass);
  return cs <= 3 || rs <= 2 || width() < UiScale::dp(220);
}

int DashWidget::maxListItems() const {
  const int rs = DashboardLayoutStore::rowSpanFor(m_sizeClass);
  const int cs = DashboardLayoutStore::colSpanFor(m_sizeClass);
  if (rs <= 2 || cs <= 3)
    return 2;
  if (rs <= 3)
    return 4;
  return 6;
}

QString DashWidget::elideForWidth(const QString &text, int widthPx,
                                  int fontPx) const {
  QFont f = font();
  f.setPixelSize(fontPx);
  const QFontMetrics fm(f);
  return fm.elidedText(text, Qt::ElideRight, qMax(UiScale::dp(40), widthPx));
}

void DashWidget::adaptHeaderDensity() {
  if (!m_title)
    return;
  const QString full = DashboardLayoutStore::displayName(m_id);
  const int fontPx = isCompact() ? 13 : 15;
  // Leave room for grip + Stil in edit mode.
  const int budget =
      qMax(UiScale::dp(48), width() - UiScale::dp(m_editMode ? 96 : 32));
  m_title->setText(elideForWidth(full, budget, fontPx));
  m_title->setToolTip(full);
}

void DashWidget::showStyleMenu() {
  QList<BlopInWindowMenu::Item> items;

  BlopInWindowMenu::Item bgToggle;
  bgToggle.label = m_bgEnabled ? QStringLiteral("Hintergrund aus")
                               : QStringLiteral("Hintergrund an");
  bgToggle.handler = [this]() {
    m_bgEnabled = !m_bgEnabled;
    applyChrome();
    emit chromePrefsChanged(m_bgEnabled, m_borderEnabled, m_bgColor,
                            m_borderColor);
  };
  items.push_back(bgToggle);

  BlopInWindowMenu::Item bgColor;
  bgColor.label = QStringLiteral("Hintergrundfarbe…");
  bgColor.handler = [this]() {
    QColor start = m_bgColor.isEmpty() ? QColor(cardBg()) : QColor(m_bgColor);
    if (!start.isValid())
      start = BlopTheme::instance().isDark() ? BlopStyle::obsidianSheet()
                                             : QColor(Qt::white);
    QColor c = start;
    if (!showColorPickerOverlay(window() ? window() : this, &c,
                                QStringLiteral("Hintergrundfarbe")))
      return;
    m_bgEnabled = true;
    m_bgColor = c.name(QColor::HexRgb);
    applyChrome();
    emit chromePrefsChanged(m_bgEnabled, m_borderEnabled, m_bgColor,
                            m_borderColor);
  };
  items.push_back(bgColor);

  BlopInWindowMenu::Item bgReset;
  bgReset.label = QStringLiteral("Hintergrund zurücksetzen");
  bgReset.handler = [this]() {
    m_bgColor.clear();
    m_bgEnabled = true;
    applyChrome();
    emit chromePrefsChanged(m_bgEnabled, m_borderEnabled, m_bgColor,
                            m_borderColor);
  };
  items.push_back(bgReset);

  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);

  BlopInWindowMenu::Item borderToggle;
  borderToggle.label = m_borderEnabled ? QStringLiteral("Rahmen aus")
                                       : QStringLiteral("Rahmen an");
  borderToggle.handler = [this]() {
    m_borderEnabled = !m_borderEnabled;
    applyChrome();
    emit chromePrefsChanged(m_bgEnabled, m_borderEnabled, m_bgColor,
                            m_borderColor);
  };
  items.push_back(borderToggle);

  BlopInWindowMenu::Item borderColor;
  borderColor.label = QStringLiteral("Rahmenfarbe…");
  borderColor.handler = [this]() {
    QColor start =
        m_borderColor.isEmpty() ? QColor(cardBorder()) : QColor(m_borderColor);
    if (!start.isValid())
      start = BlopTheme::accentPrimary();
    QColor c = start;
    if (!showColorPickerOverlay(window() ? window() : this, &c,
                                QStringLiteral("Rahmenfarbe")))
      return;
    m_borderEnabled = true;
    m_borderColor = c.name(QColor::HexRgb);
    applyChrome();
    emit chromePrefsChanged(m_bgEnabled, m_borderEnabled, m_bgColor,
                            m_borderColor);
  };
  items.push_back(borderColor);

  BlopInWindowMenu::Item borderReset;
  borderReset.label = QStringLiteral("Rahmen zurücksetzen");
  borderReset.handler = [this]() {
    m_borderColor.clear();
    m_borderEnabled = true;
    applyChrome();
    emit chromePrefsChanged(m_bgEnabled, m_borderEnabled, m_bgColor,
                            m_borderColor);
  };
  items.push_back(borderReset);

  const QPoint g =
      m_btnStyle ? m_btnStyle->mapToGlobal(QPoint(0, m_btnStyle->height()))
                 : mapToGlobal(QPoint(width() - UiScale::dp(40), 0));
  BlopInWindowMenu::show(this, g, items);
}

QWidget *DashWidget::buildToday() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(2));

  const QDate today = QDate::currentDate();
  const auto todos = TodoStore::load();
  const auto events = CalendarService::instance().eventsForDay(today);

  int overdue = 0;
  for (const auto &t : todos) {
    if (!t.done && t.due.isValid() && t.due.date() < today)
      ++overdue;
  }
  if (overdue > 0) {
    auto *warn = new QLabel(
        QStringLiteral("%1 überfällig").arg(overdue), body);
    warn->setStyleSheet(QStringLiteral(
                            "color: #E06C75; font-size: 12px; font-weight: 600;"
                            "background: transparent; padding: 2px 0 6px 0;"));
    lay->addWidget(warn);
  }

  int evShown = 0;
  const bool compact = isCompact();
  const int fontPx = compact ? 12 : 14;
  const int maxEv = compact ? 1 : 2;
  const int maxTodo = compact ? 2 : 4;
  const int textBudget =
      qMax(UiScale::dp(56),
           (DashboardLayoutStore::colSpanFor(m_sizeClass) * UiScale::dp(28)) -
               UiScale::dp(24));
  for (const auto &ev : events) {
    if (evShown >= maxEv)
      break;
    QString line = ev.title;
    if (!ev.allDay && ev.start.isValid())
      line = QStringLiteral("%1 · %2")
                 .arg(ev.start.time().toString(QStringLiteral("HH:mm")),
                      ev.title);
    line = elideForWidth(line, textBudget, fontPx);
    auto *lbl = new QLabel(line, body);
    lbl->setToolTip(ev.title);
    lbl->setWordWrap(false);
    lbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: %2px; font-weight: 500;"
                       "background: transparent; padding: %3px 0;")
            .arg(ink())
            .arg(fontPx)
            .arg(compact ? 3 : 6));
    lay->addWidget(lbl);
    if (evShown == 0)
      addHairline(lay, body);
    ++evShown;
  }

  int shown = 0;
  for (const auto &t : todos) {
    if (t.done || shown >= maxTodo)
      continue;
    if (t.due.isValid() && t.due.date() > today)
      continue;
    if (shown > 0 || evShown > 0)
      addHairline(lay, body);
    auto *row = new QWidget(body);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, UiScale::dp(6), 0, UiScale::dp(6));
    rowLay->setSpacing(UiScale::dp(8));
    auto *cb = new QCheckBox(row);
    cb->setChecked(false);
    cb->setStyleSheet(checkBoxQss());
    const QString tid = t.id;
    connect(cb, &QCheckBox::toggled, this, [this, tid](bool on) {
      if (on) {
        TodoStore::setDone(tid, true);
        emit contentChanged();
      }
    });
    rowLay->addWidget(cb, 0);
    auto *lbl = new QLabel(t.title, row);
    lbl->setStyleSheet(QStringLiteral(
                           "color: %1; font-size: 14px; background: transparent;")
                           .arg(ink()));
    lbl->setWordWrap(true);
    rowLay->addWidget(lbl, 1);
    if (QLabel *due = makeDueBadge(t.due, row))
      rowLay->addWidget(due, 0);
    lay->addWidget(row);
    ++shown;
  }
  if (shown == 0 && evShown == 0 && overdue == 0)
    lay->addWidget(makeMutedLine(QStringLiteral("Nichts Dringendes."), body));
  lay->addStretch(1);
  return body;
}

QWidget *DashWidget::buildTodos() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  const bool compact = isCompact();
  const int fontPx = compact ? 12 : 14;
  const int padY = compact ? 4 : 8;
  const int maxItems = maxListItems();
  const int textBudget =
      qMax(UiScale::dp(56),
           (DashboardLayoutStore::colSpanFor(m_sizeClass) * UiScale::dp(28)) -
               UiScale::dp(compact ? 48 : 64));

  const auto todos = TodoStore::load();
  int shown = 0;
  int openCount = 0;
  for (const auto &t : todos) {
    if (t.done)
      continue;
    ++openCount;
    if (shown >= maxItems)
      continue;
    if (shown > 0)
      addHairline(lay, body);
    auto *row = new QWidget(body);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, UiScale::dp(padY), 0, UiScale::dp(padY));
    rowLay->setSpacing(UiScale::dp(compact ? 6 : 8));
    auto *cb = new QCheckBox(row);
    cb->setStyleSheet(checkBoxQss());
    const QString tid = t.id;
    connect(cb, &QCheckBox::toggled, this, [this, tid](bool on) {
      TodoStore::setDone(tid, on);
      emit contentChanged();
    });
    rowLay->addWidget(cb, 0);
    const QString shownTitle = elideForWidth(t.title, textBudget, fontPx);
    auto *lbl = new QLabel(shownTitle, row);
    lbl->setToolTip(t.title);
    lbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: %2px; background: transparent;")
            .arg(ink())
            .arg(fontPx));
    lbl->setWordWrap(false);
    rowLay->addWidget(lbl, 1);
    if (!compact) {
      if (QLabel *due = makeDueBadge(t.due, row))
        rowLay->addWidget(due, 0);
    }
    lay->addWidget(row);
    ++shown;
  }
  if (shown == 0)
    lay->addWidget(
        makeMutedLine(QStringLiteral("Keine offenen Aufgaben."), body));
  else if (openCount > shown)
    lay->addWidget(makeMutedLine(
        QStringLiteral("+%1 weitere").arg(openCount - shown), body));
  lay->addStretch(1);

  if (!compact) {
    auto *add = new QPushButton(QStringLiteral("Neue Aufgabe"), body);
    add->setCursor(Qt::PointingHandCursor);
    add->setFlat(true);
    add->setStyleSheet(quietLinkQss());
    connect(add, &QPushButton::clicked, this, [this]() {
      const QString title = BlopDialogs::promptText(
          this, QStringLiteral("Neue Aufgabe"), QStringLiteral("Titel:"));
      const QString trimmed = title.trimmed();
      if (trimmed.isEmpty())
        return;
      TodoStore::add(trimmed);
      emit contentChanged();
    });
    lay->addWidget(add, 0, Qt::AlignLeft);
  }
  return body;
}

QWidget *DashWidget::buildCalendar() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(4));

  const int cs = DashboardLayoutStore::colSpanFor(m_sizeClass);
  const int rs = DashboardLayoutStore::rowSpanFor(m_sizeClass);
  const bool tiny = cs <= 3 || rs <= 2;

  auto *day = new CalendarDayView(body);
  day->setCompact(true);
  // Tiny tiles stay agenda-only; normal/tall tiles keep Tag/Woche/Monat + create.
  day->setMinimal(tiny && !m_editMode);
  day->setDate(QDate::currentDate());
  day->refresh();
  lay->addWidget(day, 1);

  if (cs > 3) {
    auto *open = new QPushButton(QStringLiteral("Vollbild"), body);
    open->setCursor(Qt::PointingHandCursor);
    open->setFlat(true);
    open->setStyleSheet(quietLinkQss());
    connect(open, &QPushButton::clicked, this,
            &DashWidget::maximizeCalendarRequested);
    lay->addWidget(open, 0, Qt::AlignLeft);
  }
  return body;
}

QWidget *DashWidget::buildRecent() {
  auto *body = new QWidget();
  auto *lay = new QVBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  const bool compact = isCompact();
  const int fontPx = compact ? 12 : 14;
  const int padY = compact ? 5 : 9;
  const int maxItems = maxListItems();
  const QString hoverFill =
      BlopTheme::instance().isDark()
          ? QStringLiteral("rgba(255,255,255,0.05)")
          : BlopStyle::paperHover().name(QColor::HexRgb);

  const QStringList paths = LibraryOrgStore::recentPaths(maxItems);
  if (paths.isEmpty()) {
    lay->addWidget(
        makeMutedLine(QStringLiteral("Noch keine kürzlich geöffneten Notizen."),
                      body));
    lay->addStretch(1);
    return body;
  }
  // Approximate text budget from board footprint (widget may not be laid out yet).
  const int textBudget =
      qMax(UiScale::dp(64),
           (DashboardLayoutStore::colSpanFor(m_sizeClass) * UiScale::dp(28)) -
               UiScale::dp(compact ? 16 : 24));

  for (int i = 0; i < paths.size(); ++i) {
    if (i > 0)
      addHairline(lay, body);
    const QString &path = paths.at(i);
    const QFileInfo fi(path);
    const QString fullName = fi.completeBaseName();
    const QString shown = elideForWidth(fullName, textBudget, fontPx);
    auto *btn = new QPushButton(shown, body);
    btn->setToolTip(fullName);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setStyleSheet(QStringLiteral(
                           "QPushButton {"
                           "  text-align: left; padding: %1px 4px;"
                           "  color: %2; font-size: %3px; font-weight: 500;"
                           "  background: transparent; border: none;"
                           "  border-radius: %5px;"
                           "}"
                           "QPushButton:hover { background: %4; }")
                           .arg(padY)
                           .arg(ink())
                           .arg(fontPx)
                           .arg(hoverFill)
                           .arg(UiScale::dp(cardRadiusDp() - 2)));
    connect(btn, &QPushButton::clicked, this, [this, path]() {
      emit openNotePath(path);
    });
    lay->addWidget(btn, 0);
  }
  lay->addStretch(1);
  return body;
}

QWidget *DashWidget::buildShortcuts() {
  auto *body = new QWidget();
  const bool narrow = DashboardLayoutStore::colSpanFor(m_sizeClass) <= 3;
  QBoxLayout *lay = nullptr;
  if (narrow)
    lay = new QVBoxLayout(body);
  else
    lay = new QHBoxLayout(body);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(narrow ? 6 : 10));

  struct Item {
    const char *label;
    std::function<void()> fire;
  };
  const QList<Item> items = {
      {"Neue Notiz", [this]() { emit newNoteRequested(); }},
      {"Bibliothek", [this]() { emit snapToNotesRequested(); }},
      {"Lernen", [this]() { emit studyRequested(); }},
  };

  const QString sheetBg =
      BlopTheme::instance().isDark() ? QStringLiteral("rgba(255,255,255,0.04)")
                                     : QStringLiteral("rgba(15,23,42,0.03)");
  const QString sheetHover =
      BlopTheme::instance().isDark() ? QStringLiteral("rgba(255,255,255,0.07)")
                                     : QStringLiteral("rgba(15,23,42,0.06)");

  for (const auto &it : items) {
    auto *card = new QPushButton(QString::fromUtf8(it.label), body);
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(UiScale::dp(narrow ? 36 : 52));
    card->setStyleSheet(QStringLiteral(
                            "QPushButton {"
                            "  background: %1;"
                            "  border: none;"
                            "  border-radius: %2px;"
                            "  color: %3; font-size: 14px; font-weight: 550;"
                            "  padding: %4px;"
                            "}"
                            "QPushButton:hover { background: %5; }")
                            .arg(sheetBg)
                            .arg(UiScale::dp(cardRadiusDp()))
                            .arg(ink())
                            .arg(QString::number(narrow ? 8 : 12))
                            .arg(sheetHover));
    connect(card, &QPushButton::clicked, this, it.fire);
    lay->addWidget(card, 1);
  }
  return body;
}
