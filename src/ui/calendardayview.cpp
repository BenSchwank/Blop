#include "calendardayview.h"

#include "blop_dialogs.h"
#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include "bloplocale.h"
#include "blopstyle.h"
#include "calendareventeditor.h"
#include "uiscale.h"

#include <QCalendarWidget>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
constexpr int kDayStartHour = 6;
constexpr int kDayEndHour = 22;
constexpr int kHourPxCompact = 36;
constexpr int kHourPxFull = 52;

QString ink() { return BlopTheme::textPrimary().name(); }
QString muted() { return BlopTheme::textSecondary().name(); }
QString accent() { return BlopStyle::accent().name(); }
QString accentHover() { return BlopTheme::accentHover().name(QColor::HexRgb); }
QString cardBg() {
  const QColor c = BlopTheme::accentPrimary();
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(BlopTheme::instance().isDark() ? QStringLiteral("0.18")
                                          : QStringLiteral("0.10"));
}
QString hairline() {
  return BlopTheme::instance().isDark()
             ? QStringLiteral("rgba(255,255,255,0.10)")
             : QStringLiteral("rgba(55,53,47,0.09)");
}

QString formatWhen(const CalendarEvent &e) {
  if (e.allDay)
    return QStringLiteral("Ganztägig");
  const QString a = e.start.toString(QStringLiteral("HH:mm"));
  if (!e.end.isValid())
    return a;
  if (e.end.date() != e.start.date())
    return QStringLiteral("%1 → %2")
        .arg(e.start.toString(QStringLiteral("dd.MM. HH:mm")),
             e.end.toString(QStringLiteral("dd.MM. HH:mm")));
  return QStringLiteral("%1 – %2").arg(a, e.end.toString(QStringLiteral("HH:mm")));
}
} // namespace

CalendarDayView::CalendarDayView(QWidget *parent) : QWidget(parent) {
  m_date = QDate::currentDate();
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(UiScale::dp(6));

  // Quiet mode row: current view + ⋯ (no fat segment chips).
  m_modeBar = new QWidget(this);
  auto *modeLay = new QHBoxLayout(m_modeBar);
  modeLay->setContentsMargins(0, 0, 0, 0);
  modeLay->setSpacing(UiScale::dp(4));

  m_modeLabel = new QLabel(modeLabel(Mode::Day), m_modeBar);
  m_modeLabel->setCursor(Qt::PointingHandCursor);
  m_modeLabel->setToolTip(QStringLiteral("Ansicht wechseln"));
  m_modeLabel->installEventFilter(this);
  m_modeLabel->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px; font-weight: 500;"
                     "background: transparent;")
          .arg(muted()));
  modeLay->addWidget(m_modeLabel, 0, Qt::AlignVCenter);

  m_btnAdd = new QPushButton(QStringLiteral("＋ Termin"), m_modeBar);
  m_btnAdd->setCursor(Qt::PointingHandCursor);
  m_btnAdd->setFlat(true);
  connect(m_btnAdd, &QPushButton::clicked, this, [this]() {
    requestCreate(QDateTime(m_date, QTime(9, 0)));
  });
  modeLay->addWidget(m_btnAdd, 0, Qt::AlignVCenter);
  modeLay->addStretch(1);

  // Full calendar only — compact board uses the mode label as the control.
  m_btnModeMore = new QPushButton(QStringLiteral("⋯"), m_modeBar);
  m_btnModeMore->setCursor(Qt::PointingHandCursor);
  m_btnModeMore->setFlat(true);
  m_btnModeMore->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  m_btnModeMore->setToolTip(QStringLiteral("Ansicht"));
  connect(m_btnModeMore, &QPushButton::clicked, this,
          &CalendarDayView::showModeMenu);
  modeLay->addWidget(m_btnModeMore, 0, Qt::AlignVCenter);
  root->addWidget(m_modeBar);

  m_btnGoogle = new QPushButton(this);
  m_btnGoogle->setCursor(Qt::PointingHandCursor);
  m_btnGoogle->setFlat(true);
  m_btnGoogle->setMinimumHeight(UiScale::dp(24));
  connect(m_btnGoogle, &QPushButton::clicked, this, [this]() {
    if (CalendarService::instance().hasGoogleAccess()) {
      CalendarService::instance().disconnectGoogle();
    } else {
      CalendarService::instance().connectGoogle();
    }
    refreshGoogleButton();
  });
  root->addWidget(m_btnGoogle, 0, Qt::AlignLeft);

  m_navBar = new QWidget(this);
  auto *hdr = new QHBoxLayout(m_navBar);
  hdr->setContentsMargins(0, 0, 0, 0);
  hdr->setSpacing(UiScale::dp(6));
  auto *btnPrev = new QPushButton(QStringLiteral("‹"), m_navBar);
  auto *btnNext = new QPushButton(QStringLiteral("›"), m_navBar);
  auto *btnToday = new QPushButton(QStringLiteral("Heute"), m_navBar);
  for (QPushButton *b : {btnPrev, btnNext, btnToday}) {
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(BlopStyle::quietIconButtonQss());
    b->setMinimumSize(UiScale::dp(36), UiScale::dp(32));
  }
  m_dateLabel = new QLabel(m_navBar);
  m_dateLabel->setAlignment(Qt::AlignCenter);
  m_dateLabel->setWordWrap(true);
  hdr->addWidget(btnPrev, 0);
  hdr->addWidget(m_dateLabel, 1);
  hdr->addWidget(btnToday, 0);
  hdr->addWidget(btnNext, 0);
  root->addWidget(m_navBar);

  connect(&CalendarService::instance(), &CalendarService::eventsChanged, this,
          [this]() {
            refreshGoogleButton();
            refresh();
          });
  refreshGoogleButton();

  connect(btnPrev, &QPushButton::clicked, this, [this]() {
    if (m_mode == Mode::Week)
      setDate(m_date.addDays(-7));
    else if (m_mode == Mode::Month)
      setDate(m_date.addMonths(-1));
    else
      setDate(m_date.addDays(-1));
  });
  connect(btnNext, &QPushButton::clicked, this, [this]() {
    if (m_mode == Mode::Week)
      setDate(m_date.addDays(7));
    else if (m_mode == Mode::Month)
      setDate(m_date.addMonths(1));
    else
      setDate(m_date.addDays(1));
  });
  connect(btnToday, &QPushButton::clicked, this,
          [this]() { setDate(QDate::currentDate()); });

  m_stack = new QStackedWidget(this);
  m_stack->setStyleSheet(
      QStringLiteral("QStackedWidget { background: transparent; border: none; }"));

  auto styleScroll = [](QScrollArea *sa, QWidget *host) {
    sa->setAttribute(Qt::WA_StyledBackground, true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"));
    if (sa->viewport()) {
      sa->viewport()->setAutoFillBackground(false);
      sa->viewport()->setStyleSheet(
          QStringLiteral("background: transparent;"));
    }
    if (host) {
      host->setAttribute(Qt::WA_StyledBackground, true);
      host->setStyleSheet(QStringLiteral("background: transparent;"));
    }
  };

  // --- Tag (agenda + timeline) ---
  m_dayPage = new QWidget(m_stack);
  auto *dayRoot = new QVBoxLayout(m_dayPage);
  dayRoot->setContentsMargins(0, 0, 0, 0);
  dayRoot->setSpacing(UiScale::dp(6));
  auto *agendaHost = new QWidget(m_dayPage);
  m_dayAgendaLay = new QVBoxLayout(agendaHost);
  m_dayAgendaLay->setContentsMargins(0, 0, 0, 0);
  m_dayAgendaLay->setSpacing(UiScale::dp(4));
  dayRoot->addWidget(agendaHost, 0);

  m_dayScroll = new QScrollArea(m_dayPage);
  m_dayScroll->setWidgetResizable(false);
  m_timeline = new QWidget;
  m_timeline->setObjectName(QStringLiteral("CalDayTimeline"));
  m_timeline->setAttribute(Qt::WA_StyledBackground, true);
  m_timeline->setStyleSheet(
      QStringLiteral("QWidget#CalDayTimeline { background: transparent; }"));
  m_dayScroll->setWidget(m_timeline);
  m_dayScroll->viewport()->installEventFilter(this);
  styleScroll(m_dayScroll, m_timeline);
  dayRoot->addWidget(m_dayScroll, 1);
  m_stack->addWidget(m_dayPage);

  // --- Woche ---
  m_weekScroll = new QScrollArea(m_stack);
  m_weekScroll->setWidgetResizable(true);
  m_weekHost = new QWidget;
  m_weekLay = new QVBoxLayout(m_weekHost);
  m_weekLay->setContentsMargins(0, 0, 0, 0);
  m_weekLay->setSpacing(UiScale::dp(8));
  m_weekLay->addStretch(1);
  m_weekScroll->setWidget(m_weekHost);
  styleScroll(m_weekScroll, m_weekHost);
  m_stack->addWidget(m_weekScroll);

  // --- Monat ---
  auto *monthPage = new QWidget(m_stack);
  auto *monthLay = new QVBoxLayout(monthPage);
  monthLay->setContentsMargins(0, 0, 0, 0);
  monthLay->setSpacing(UiScale::dp(8));
  m_monthCal = new QCalendarWidget(monthPage);
  m_monthCal->setGridVisible(false);
  m_monthCal->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
  m_monthCal->setNavigationBarVisible(false);
  m_monthCal->setSelectedDate(m_date);
  connect(m_monthCal, &QCalendarWidget::clicked, this,
          [this](const QDate &d) { setDate(d); });
  monthLay->addWidget(m_monthCal, 0);
  m_monthListScroll = new QScrollArea(monthPage);
  m_monthListScroll->setWidgetResizable(true);
  m_monthListScroll->setFrameShape(QFrame::NoFrame);
  m_monthListScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_monthListScroll->setStyleSheet(
      QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  m_monthListHost = new QWidget;
  m_monthListLay = new QVBoxLayout(m_monthListHost);
  m_monthListLay->setContentsMargins(0, 0, 0, 0);
  m_monthListLay->setSpacing(UiScale::dp(6));
  m_monthListLay->addStretch(1);
  m_monthListScroll->setWidget(m_monthListHost);
  monthLay->addWidget(m_monthListScroll, 1);
  m_stack->addWidget(monthPage);

  root->addWidget(m_stack, 1);

  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          &CalendarDayView::refresh);

  setMode(Mode::Day);
  applyCompactChrome();
}

QString CalendarDayView::modeLabel(Mode mode) {
  switch (mode) {
  case Mode::Week:
    return QStringLiteral("Woche");
  case Mode::Month:
    return QStringLiteral("Monat");
  case Mode::Day:
  default:
    return QStringLiteral("Tag");
  }
}

void CalendarDayView::syncModeLabel() {
  if (m_modeLabel) {
    // Compact: label is the only view control — hint with a quiet caret.
    const QString base = modeLabel(m_mode);
    m_modeLabel->setText(m_compact ? base + QStringLiteral(" ▾") : base);
  }
  applyCompactChrome();
}

void CalendarDayView::showModeMenu() {
  QWidget *anchor = m_btnModeMore && m_btnModeMore->isVisible()
                        ? static_cast<QWidget *>(m_btnModeMore)
                        : static_cast<QWidget *>(m_modeLabel);
  if (!anchor)
    return;
  QList<BlopInWindowMenu::Item> items;
  const Mode modes[] = {Mode::Day, Mode::Week, Mode::Month};
  for (Mode m : modes) {
    const QString mark =
        (m == m_mode) ? QStringLiteral("✓ ") : QStringLiteral("   ");
    items.push_back({mark + modeLabel(m), QIcon(), [this, m]() { setMode(m); }});
  }
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  items.push_back(sep);
  const bool linked = CalendarService::instance().hasGoogleAccess();
  items.push_back(
      {linked ? QStringLiteral("Google trennen")
              : QStringLiteral("Google verbinden"),
       QIcon(), [this]() {
         if (CalendarService::instance().hasGoogleAccess())
           CalendarService::instance().disconnectGoogle();
         else
           CalendarService::instance().connectGoogle();
         refreshGoogleButton();
       }});
  BlopInWindowMenu::show(
      this, anchor->mapToGlobal(QPoint(0, anchor->height())), items);
}

void CalendarDayView::requestCreate(const QDateTime &presetStart) {
  CalendarEvent draft;
  if (!CalendarEventEditor::promptNew(this, presetStart, &draft))
    return;
  CalendarService::instance().createEvent(draft.title, draft.start, draft.end,
                                          draft.allDay, draft.location,
                                          draft.color);
  refresh();
}

void CalendarDayView::setCompact(bool on) {
  if (m_compact == on) {
    applyCompactChrome();
    return;
  }
  m_compact = on;
  applyCompactChrome();
  rebuildAll();
}

void CalendarDayView::applyCompactChrome() {
  if (m_modeLabel) {
    m_modeLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: %2px; font-weight: 500;"
                       "background: transparent;")
            .arg(muted())
            .arg(m_compact ? 12 : 13));
  }
  if (m_btnAdd) {
    m_btnAdd->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  color: %1; font-size: 12px; font-weight: 500;"
                       "  background: transparent; border: none;"
                       "  padding: 2px 4px;"
                       "}"
                       "QPushButton:hover { color: %2; }")
            .arg(muted(), accent()));
  }
  if (m_btnModeMore) {
    // Board tiles: no second ⋯ — mode label opens the menu.
    m_btnModeMore->setVisible(!m_compact && !m_minimal);
    const int s = UiScale::dp(m_compact ? 26 : 30);
    m_btnModeMore->setFixedSize(s, s);
    m_btnModeMore->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  color: %1; font-size: 16px; font-weight: 600;"
                       "  background: transparent; border: none;"
                       "  border-radius: %2px; padding: 0;"
                       "}"
                       "QPushButton:hover { color: %3; background: %4; }")
            .arg(muted(), QString::number(UiScale::dp(6)), ink(),
                 BlopTheme::instance().isDark()
                     ? QStringLiteral("rgba(255,255,255,0.06)")
                     : QStringLiteral("rgba(15,23,42,0.05)")));
  }
  if (m_btnGoogle) {
    m_btnGoogle->setMinimumHeight(UiScale::dp(m_compact ? 22 : 28));
    refreshGoogleButton();
  }

  // Compact board tiles must not trap wheel / finger scroll — otherwise the
  // dashboard page and rubber-band bounce feel "dead" over the calendar card.
  auto tuneNested = [this](QScrollArea *sa) {
    if (!sa)
      return;
    if (m_compact) {
      sa->setProperty("blopFitContents", true);
      sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      if (auto *bar = sa->verticalScrollBar())
        bar->setEnabled(false);
    } else {
      sa->setProperty("blopFitContents", false);
      sa->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
      if (auto *bar = sa->verticalScrollBar())
        bar->setEnabled(true);
    }
  };
  tuneNested(m_dayScroll);
  tuneNested(m_weekScroll);
  tuneNested(m_monthListScroll);
}

void CalendarDayView::setMinimal(bool on) {
  if (m_minimal == on)
    return;
  m_minimal = on;
  if (m_minimal)
    setMode(Mode::Day);
  if (m_modeBar)
    m_modeBar->setVisible(!m_minimal);
  if (m_navBar)
    m_navBar->setVisible(!m_minimal);
  if (m_dayScroll)
    m_dayScroll->setVisible(!m_minimal); // tiny tiles: agenda only
  rebuildAll();
}

void CalendarDayView::setMode(Mode mode) {
  if (m_minimal)
    mode = Mode::Day;
  m_mode = mode;
  syncModeLabel();
  if (m_stack)
    m_stack->setCurrentIndex(static_cast<int>(mode));
  if (m_navBar)
    m_navBar->setVisible(!m_minimal);
  rebuildAll();
}

void CalendarDayView::setDate(const QDate &date) {
  if (!date.isValid())
    return;
  if (date == m_date) {
    refresh();
    return;
  }
  m_date = date;
  emit dateChanged(m_date);
  refresh();
}

void CalendarDayView::refresh() { rebuildAll(); }

void CalendarDayView::updateChrome() {
  if (!m_dateLabel)
    return;
  m_dateLabel->setStyleSheet(
      QStringLiteral(
          "color: %1; font-size: 15px; font-weight: 650; background: transparent;")
          .arg(ink()));
  refreshGoogleButton();
  auto &loc = BlopLocale::instance();
  switch (m_mode) {
  case Mode::Week: {
    const QDate start = m_date.addDays(-(m_date.dayOfWeek() - 1));
    const QDate end = start.addDays(6);
    m_dateLabel->setText(
        QStringLiteral("%1 – %2")
            .arg(loc.formatDate(start, QStringLiteral("d. MMM")),
                 loc.formatDate(end, QStringLiteral("d. MMM yyyy"))));
    break;
  }
  case Mode::Month:
    m_dateLabel->setText(loc.formatDate(m_date, QStringLiteral("MMMM yyyy")));
    break;
  case Mode::Day:
  default:
    m_dateLabel->setText(
        loc.formatDate(m_date, QStringLiteral("dddd, d. MMMM yyyy")));
    break;
  }
}

void CalendarDayView::refreshGoogleButton() {
  if (!m_btnGoogle)
    return;
  const bool linked = CalendarService::instance().hasGoogleAccess();
  m_btnGoogle->setText(linked ? QStringLiteral("Google trennen")
                              : QStringLiteral("Google verbinden"));
  m_btnGoogle->setStyleSheet(
      QStringLiteral("QPushButton {"
                     "  color: %1; font-size: 12px; font-weight: 600;"
                     "  background: transparent; border: none; padding: 4px 6px;"
                     "}"
                     "QPushButton:hover { color: %2; }")
          .arg(accent(), accentHover()));
  // Compact/minimal board tiles: Google lives in the quiet ⋯ menu — no admin row.
  if (m_minimal || m_compact) {
    m_btnGoogle->setVisible(false);
  } else {
    m_btnGoogle->setVisible(true);
  }
}

void CalendarDayView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (m_mode == Mode::Day)
    rebuildDay();
}

bool CalendarDayView::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_modeLabel &&
      event->type() == QEvent::MouseButtonRelease) {
    showModeMenu();
    return true;
  }

  // Compact tiles: bubble wheel to the dashboard page scroller so nested
  // day/week lists cannot swallow page scroll + bounce.
  if (m_compact && event->type() == QEvent::Wheel) {
    for (QWidget *p = parentWidget(); p; p = p->parentWidget()) {
      if (auto *sa = qobject_cast<QScrollArea *>(p)) {
        if (sa->property("blopFitContents").toBool())
          continue;
        if (QScrollBar *bar = sa->verticalScrollBar()) {
          if (bar->isEnabled() && bar->maximum() > bar->minimum()) {
            QCoreApplication::sendEvent(sa->viewport(), event);
            return true;
          }
        }
      }
    }
  }

  if (m_dayScroll && watched == m_dayScroll->viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      m_pressPos = me->pos();
      m_swiping = false;
    } else if (event->type() == QEvent::MouseMove) {
      auto *me = static_cast<QMouseEvent *>(event);
      if ((me->buttons() & Qt::LeftButton) &&
          qAbs(me->pos().x() - m_pressPos.x()) > UiScale::dp(48))
        m_swiping = true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
      auto *me = static_cast<QMouseEvent *>(event);
      const int dx = me->pos().x() - m_pressPos.x();
      if (m_swiping && qAbs(dx) > UiScale::dp(64)) {
        setDate(m_date.addDays(dx < 0 ? 1 : -1));
        m_swiping = false;
        return true;
      }
      m_swiping = false;
    }
    return QWidget::eventFilter(watched, event);
  }

  auto *w = qobject_cast<QWidget *>(watched);
  if (!w)
    return QWidget::eventFilter(watched, event);

  if (event->type() == QEvent::MouseButtonRelease && !m_swiping) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() != Qt::LeftButton)
      return QWidget::eventFilter(watched, event);

    const QString eventId = w->property("eventId").toString();
    if (!eventId.isEmpty()) {
      CalendarEvent found;
      found.id = eventId;
      found.title = w->property("eventTitle").toString();
      if (found.title.isEmpty())
        found.title = QStringLiteral("Termin");
      showEventMenu(found, me->globalPosition().toPoint());
      return true;
    }
    if (w->property("calHit").toBool()) {
      const int hourPx = UiScale::dp(m_compact ? kHourPxCompact : kHourPxFull);
      const int y = me->pos().y();
      const int minutesFromStart = (y * 60) / qMax(1, hourPx);
      int totalMin = kDayStartHour * 60 + minutesFromStart;
      totalMin = (totalMin / 15) * 15;
      QTime t(totalMin / 60, totalMin % 60);
      if (!t.isValid())
        t = QTime(9, 0);
      requestCreate(QDateTime(m_date, t));
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void CalendarDayView::showEventMenu(const CalendarEvent &e,
                                    const QPoint &globalPos) {
  QList<BlopInWindowMenu::Item> items;
  BlopInWindowMenu::Item del;
  del.label = QStringLiteral("Löschen");
  del.destructive = true;
  del.handler = [this, e]() { confirmDelete(e); };
  items.push_back(del);
  BlopInWindowMenu::show(this, globalPos, items);
}

void CalendarDayView::confirmDelete(const CalendarEvent &e) {
  if (!BlopDialogs::confirm(
          this, QStringLiteral("Termin löschen"),
          QStringLiteral("„%1“ wirklich löschen?").arg(e.title),
          QStringLiteral("Löschen"), QStringLiteral("Abbrechen")))
    return;
  CalendarService::instance().removeEvent(e.id);
}

QWidget *CalendarDayView::makeEventRow(const CalendarEvent &e, QWidget *parent) {
  auto *row = new QFrame(parent);
  row->setObjectName(QStringLiteral("CalEventRow"));
  row->setAttribute(Qt::WA_StyledBackground, true);
  row->setCursor(Qt::PointingHandCursor);
  const QString stripe =
      (!e.color.isEmpty() && QColor(e.color).isValid()) ? e.color : accent();
  row->setStyleSheet(QStringLiteral(
                         "QFrame#CalEventRow {"
                         "  background: %1;"
                         "  border: 1px solid %2;"
                         "  border-left: 3px solid %3;"
                         "  border-radius: 8px;"
                         "}")
                         .arg(cardBg(), hairline(), stripe));
  auto *rl = new QHBoxLayout(row);
  rl->setContentsMargins(UiScale::dp(10), UiScale::dp(8), UiScale::dp(10),
                         UiScale::dp(8));
  rl->setSpacing(UiScale::dp(8));

  auto *textCol = new QVBoxLayout();
  textCol->setContentsMargins(0, 0, 0, 0);
  textCol->setSpacing(2);
  auto *title = new QLabel(e.title.isEmpty() ? QStringLiteral("Termin") : e.title,
                           row);
  title->setWordWrap(true);
  title->setStyleSheet(
      QStringLiteral(
          "color: %1; font-size: 13px; font-weight: 650; background: transparent;")
          .arg(ink()));
  auto *when = new QLabel(formatWhen(e), row);
  when->setStyleSheet(
      QStringLiteral("color: %1; font-size: 11px; background: transparent;")
          .arg(muted()));
  textCol->addWidget(title);
  textCol->addWidget(when);
  if (!e.location.isEmpty()) {
    auto *loc = new QLabel(e.location, row);
    loc->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(muted()));
    textCol->addWidget(loc);
  }
  rl->addLayout(textCol, 1);

  row->setProperty("eventId", e.id);
  row->setProperty("eventTitle", e.title);
  row->installEventFilter(this);
  return row;
}

void CalendarDayView::rebuildAll() {
  updateChrome();
  rebuildDay();
  rebuildWeek();
  rebuildMonthList();
  if (m_monthCal && m_monthCal->selectedDate() != m_date)
    m_monthCal->setSelectedDate(m_date);
}

void CalendarDayView::rebuildDay() {
  if (m_dayAgendaLay) {
    while (QLayoutItem *it = m_dayAgendaLay->takeAt(0)) {
      if (it->widget())
        delete it->widget();
      delete it;
    }
    const auto dayEvents = CalendarService::instance().eventsForDay(m_date);
    if (dayEvents.isEmpty()) {
      auto *empty = new QLabel(QStringLiteral("Keine Termine an diesem Tag."),
                               m_dayAgendaLay->parentWidget());
      empty->setWordWrap(true);
      empty->setStyleSheet(
          QStringLiteral("color: %1; font-size: 13px; background: transparent;")
              .arg(muted()));
      m_dayAgendaLay->addWidget(empty);
      if (m_compact || m_minimal) {
        auto *add = new QPushButton(QStringLiteral("＋ Termin"),
                                    m_dayAgendaLay->parentWidget());
        add->setFlat(true);
        add->setCursor(Qt::PointingHandCursor);
        add->setStyleSheet(
            QStringLiteral("QPushButton { color: %1; font-size: 12px;"
                           "  background: transparent; border: none;"
                           "  text-align: left; padding: 2px 0; }"
                           "QPushButton:hover { color: %2; }")
                .arg(muted(), accent()));
        connect(add, &QPushButton::clicked, this, [this]() {
          requestCreate(QDateTime(m_date, QTime(9, 0)));
        });
        m_dayAgendaLay->addWidget(add, 0, Qt::AlignLeft);
      }
    } else {
      const int limit = m_minimal ? 4 : 8;
      int n = 0;
      for (const CalendarEvent &e : dayEvents) {
        if (n++ >= limit)
          break;
        m_dayAgendaLay->addWidget(
            makeEventRow(e, m_dayAgendaLay->parentWidget()));
      }
    }
  }

  if (!m_timeline || !m_dayScroll || m_minimal)
    return;

  const auto dayEvents = CalendarService::instance().eventsForDay(m_date);
  // Compact board tile with nothing on the day: skip the empty hour grid.
  if (m_compact && dayEvents.isEmpty()) {
    m_timeline->hide();
    m_dayScroll->hide();
    return;
  }
  m_dayScroll->show();
  m_timeline->show();

  const QList<QWidget *> kids =
      m_timeline->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
  for (QWidget *c : kids)
    delete c;

  const int hourPx = UiScale::dp(m_compact ? kHourPxCompact : kHourPxFull);
  const int hours = kDayEndHour - kDayStartHour;
  const int labelW = UiScale::dp(44);
  int timelineW = m_dayScroll->viewport()->width();
  if (timelineW < UiScale::dp(120))
    timelineW = qMax(UiScale::dp(200), this->width());
  const int height = hours * hourPx;
  m_timeline->setFixedSize(timelineW, height);
  m_timeline->show();

  for (int h = kDayStartHour; h < kDayEndHour; ++h) {
    const int y = (h - kDayStartHour) * hourPx;
    auto *hourLbl = new QLabel(
        QStringLiteral("%1:00").arg(h, 2, 10, QLatin1Char('0')), m_timeline);
    hourLbl->setGeometry(0, y, labelW - 4, UiScale::dp(18));
    hourLbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(muted()));
    hourLbl->show();
    auto *rule = new QFrame(m_timeline);
    rule->setGeometry(labelW, y, qMax(1, timelineW - labelW), 1);
    rule->setStyleSheet(QStringLiteral("background: %1;").arg(hairline()));
    rule->show();
  }

  auto *hit = new QWidget(m_timeline);
  hit->setGeometry(labelW, 0, qMax(1, timelineW - labelW), height);
  hit->setAttribute(Qt::WA_TranslucentBackground, true);
  hit->setStyleSheet(QStringLiteral("background: transparent;"));
  hit->setCursor(Qt::PointingHandCursor);
  hit->setProperty("calHit", true);
  hit->installEventFilter(this);
  hit->show();

  const auto events = CalendarService::instance().eventsForDay(m_date);
  for (const CalendarEvent &e : events) {
    QTime startT = e.allDay ? QTime(kDayStartHour, 0) : e.start.time();
    QTime endT = e.allDay ? QTime(kDayEndHour, 0)
                          : (e.end.isValid() ? e.end.time() : startT.addSecs(3600));
    if (e.start.date() < m_date)
      startT = QTime(kDayStartHour, 0);
    if (e.end.isValid() && e.end.date() > m_date)
      endT = QTime(kDayEndHour, 0);

    int startMin = startT.hour() * 60 + startT.minute();
    int endMin = endT.hour() * 60 + endT.minute();
    const int dayStartMin = kDayStartHour * 60;
    const int dayEndMin = kDayEndHour * 60;
    startMin = qBound(dayStartMin, startMin, dayEndMin - 15);
    endMin = qBound(startMin + 20, endMin, dayEndMin);
    const int y = ((startMin - dayStartMin) * hourPx) / 60;
    const int hgt =
        qMax(UiScale::dp(22), ((endMin - startMin) * hourPx) / 60 - 2);

    auto *chip = new QFrame(m_timeline);
    chip->setGeometry(labelW + 4, y, qMax(1, timelineW - labelW - 10), hgt);
    chip->setAttribute(Qt::WA_StyledBackground, true);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setStyleSheet(QStringLiteral(
                            "QFrame {"
                            "  background: %1;"
                            "  border: 1px solid %2;"
                            "  border-left: 3px solid %3;"
                            "  border-radius: 6px;"
                            "}")
                            .arg(cardBg(), hairline(), accent()));
    auto *chipLay = new QVBoxLayout(chip);
    chipLay->setContentsMargins(UiScale::dp(8), UiScale::dp(4), UiScale::dp(8),
                                UiScale::dp(4));
    chipLay->setSpacing(0);
    auto *title = new QLabel(
        e.title.isEmpty() ? QStringLiteral("Termin") : e.title, chip);
    title->setWordWrap(true);
    title->setStyleSheet(
        QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 650; background: transparent;")
            .arg(ink()));
    chipLay->addWidget(title);
    if (!e.allDay && hgt > UiScale::dp(34)) {
      auto *when = new QLabel(formatWhen(e), chip);
      when->setStyleSheet(
          QStringLiteral("color: %1; font-size: 11px; background: transparent;")
              .arg(muted()));
      chipLay->addWidget(when);
    }
    chip->setProperty("eventId", e.id);
    chip->setProperty("eventTitle", e.title);
    chip->installEventFilter(this);
    chip->show();
    chip->raise();
  }

  if (m_date == QDate::currentDate()) {
    const int nowMin =
        QTime::currentTime().hour() * 60 + QTime::currentTime().minute();
    const int y =
        qMax(0, ((nowMin - kDayStartHour * 60) * hourPx) / 60 - hourPx);
    QTimer::singleShot(0, this, [this, y]() {
      if (m_dayScroll)
        m_dayScroll->verticalScrollBar()->setValue(y);
    });
  }
}

void CalendarDayView::rebuildWeek() {
  if (!m_weekLay)
    return;
  while (QLayoutItem *it = m_weekLay->takeAt(0)) {
    if (it->widget())
      delete it->widget();
    delete it;
  }

  const QDate start = m_date.addDays(-(m_date.dayOfWeek() - 1));
  bool any = false;
  for (int i = 0; i < 7; ++i) {
    const QDate d = start.addDays(i);
    const auto events = CalendarService::instance().eventsForDay(d);
    auto *dayHdr = new QLabel(
        BlopLocale::instance().formatDate(d, QStringLiteral("ddd, d. MMM")),
        m_weekHost);
    dayHdr->setStyleSheet(
        QStringLiteral(
            "color: %1; font-size: 12px; font-weight: 700; background: transparent;")
            .arg(d == QDate::currentDate() ? accent() : muted()));
    m_weekLay->addWidget(dayHdr);
    if (events.isEmpty()) {
      auto *none = new QLabel(QStringLiteral("—"), m_weekHost);
      none->setStyleSheet(
          QStringLiteral("color: %1; font-size: 12px; background: transparent;")
              .arg(muted()));
      m_weekLay->addWidget(none);
    } else {
      any = true;
      for (const CalendarEvent &e : events)
        m_weekLay->addWidget(makeEventRow(e, m_weekHost));
    }
  }
  if (!any) {
    // keep structure; optional hint at top already covered per-day dashes
  }
  m_weekLay->addStretch(1);
}

void CalendarDayView::rebuildMonthList() {
  if (!m_monthListLay)
    return;
  while (QLayoutItem *it = m_monthListLay->takeAt(0)) {
    if (it->widget())
      delete it->widget();
    delete it;
  }
  const auto events = CalendarService::instance().eventsForDay(m_date);
  if (events.isEmpty()) {
    auto *empty =
        new QLabel(QStringLiteral("Keine Termine an diesem Tag."), m_monthListHost);
    empty->setWordWrap(true);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px; background: transparent;")
            .arg(muted()));
    m_monthListLay->addWidget(empty);
  } else {
    for (const CalendarEvent &e : events)
      m_monthListLay->addWidget(makeEventRow(e, m_monthListHost));
  }
  m_monthListLay->addStretch(1);
}
