#include "calendardayview.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCalendarWidget>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kDayStartHour = 6;
constexpr int kDayEndHour = 22;
constexpr int kHourPxCompact = 36;
constexpr int kHourPxFull = 52;

QString ink() { return BlopTheme::textPrimary().name(); }
QString muted() { return BlopTheme::textSecondary().name(); }
QString accent() { return BlopStyle::accent().name(); }
QString cardBg() {
  return BlopTheme::instance().isDark()
             ? QStringLiteral("rgba(91,157,255,0.18)")
             : QStringLiteral("rgba(91,157,255,0.14)");
}
QString hairline() {
  return BlopTheme::instance().isDark()
             ? QStringLiteral("rgba(255,255,255,0.10)")
             : QStringLiteral("rgba(55,53,47,0.12)");
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
  root->setSpacing(UiScale::dp(8));

  // Mode chips: Liste | Tag | Woche | Monat (2×2 when compact/phone)
  m_modeGrid = new QGridLayout();
  m_modeGrid->setContentsMargins(0, 0, 0, 0);
  m_modeGrid->setHorizontalSpacing(UiScale::dp(4));
  m_modeGrid->setVerticalSpacing(UiScale::dp(4));
  m_modeGroup = new QButtonGroup(this);
  m_modeGroup->setExclusive(true);
  const struct {
    const char *label;
    Mode mode;
  } modes[] = {
      {"Liste", Mode::List},
      {"Tag", Mode::Day},
      {"Woche", Mode::Week},
      {"Monat", Mode::Month},
  };
  for (int i = 0; i < 4; ++i) {
    auto *b = new QPushButton(QString::fromUtf8(modes[i].label), this);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setMinimumHeight(UiScale::dp(32));
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setStyleSheet(BlopStyle::segmentQss());
    m_modeGroup->addButton(b, static_cast<int>(modes[i].mode));
    m_modeGrid->addWidget(b, 0, i);
  }
  root->addLayout(m_modeGrid);
  connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
    setMode(static_cast<Mode>(id));
  });
  relayoutModeChips();

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

  // --- Liste ---
  m_listScroll = new QScrollArea(m_stack);
  m_listScroll->setWidgetResizable(true);
  m_listScroll->setFrameShape(QFrame::NoFrame);
  m_listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listScroll->setStyleSheet(
      QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  m_listHost = new QWidget;
  m_listLay = new QVBoxLayout(m_listHost);
  m_listLay->setContentsMargins(0, 0, 0, 0);
  m_listLay->setSpacing(UiScale::dp(6));
  m_listLay->addStretch(1);
  m_listScroll->setWidget(m_listHost);
  m_stack->addWidget(m_listScroll);

  // --- Tag (painted timeline) ---
  m_dayScroll = new QScrollArea(m_stack);
  m_dayScroll->setWidgetResizable(false);
  m_dayScroll->setFrameShape(QFrame::NoFrame);
  m_dayScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_dayScroll->setStyleSheet(
      QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  m_timeline = new QWidget;
  m_timeline->setObjectName(QStringLiteral("CalDayTimeline"));
  m_timeline->setAttribute(Qt::WA_StyledBackground, true);
  m_timeline->setStyleSheet(
      QStringLiteral("QWidget#CalDayTimeline { background: transparent; }"));
  m_dayScroll->setWidget(m_timeline);
  m_dayScroll->viewport()->installEventFilter(this);
  m_stack->addWidget(m_dayScroll);

  // --- Woche ---
  m_weekScroll = new QScrollArea(m_stack);
  m_weekScroll->setWidgetResizable(true);
  m_weekScroll->setFrameShape(QFrame::NoFrame);
  m_weekScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_weekScroll->setStyleSheet(
      QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  m_weekHost = new QWidget;
  m_weekLay = new QVBoxLayout(m_weekHost);
  m_weekLay->setContentsMargins(0, 0, 0, 0);
  m_weekLay->setSpacing(UiScale::dp(8));
  m_weekLay->addStretch(1);
  m_weekScroll->setWidget(m_weekHost);
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

  connect(&CalendarService::instance(), &CalendarService::eventsChanged, this,
          &CalendarDayView::refresh);
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this,
          &CalendarDayView::refresh);

  if (auto *b = m_modeGroup->button(static_cast<int>(Mode::List)))
    b->setChecked(true);
  setMode(Mode::List);
}

void CalendarDayView::setCompact(bool on) {
  if (m_compact == on)
    return;
  m_compact = on;
  relayoutModeChips();
  rebuildAll();
}

void CalendarDayView::relayoutModeChips() {
  if (!m_modeGrid || !m_modeGroup)
    return;
  QList<QAbstractButton *> buttons;
  for (int id = 0; id < 4; ++id) {
    if (QAbstractButton *b = m_modeGroup->button(id))
      buttons.append(b);
  }
  // Clear cells without deleting widgets.
  while (m_modeGrid->count() > 0) {
    m_modeGrid->takeAt(0);
  }
  for (int i = 0; i < buttons.size(); ++i) {
    if (m_compact)
      m_modeGrid->addWidget(buttons[i], i / 2, i % 2);
    else
      m_modeGrid->addWidget(buttons[i], 0, i);
  }
}

void CalendarDayView::setMode(Mode mode) {
  m_mode = mode;
  if (m_modeGroup) {
    if (QAbstractButton *b = m_modeGroup->button(static_cast<int>(mode)))
      b->setChecked(true);
  }
  if (m_stack)
    m_stack->setCurrentIndex(static_cast<int>(mode));
  if (m_navBar)
    m_navBar->setVisible(mode != Mode::List);
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
  switch (m_mode) {
  case Mode::Week: {
    const QDate start = m_date.addDays(-(m_date.dayOfWeek() - 1));
    const QDate end = start.addDays(6);
    m_dateLabel->setText(
        QStringLiteral("%1 – %2")
            .arg(start.toString(QStringLiteral("d. MMM")),
                 end.toString(QStringLiteral("d. MMM yyyy"))));
    break;
  }
  case Mode::Month:
    m_dateLabel->setText(m_date.toString(QStringLiteral("MMMM yyyy")));
    break;
  case Mode::Day:
  case Mode::List:
  default:
    m_dateLabel->setText(m_date.toString(QStringLiteral("dddd, d. MMMM yyyy")));
    break;
  }
}

void CalendarDayView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (m_mode == Mode::Day)
    rebuildDay();
}

bool CalendarDayView::eventFilter(QObject *watched, QEvent *event) {
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
      emit createAt(QDateTime(m_date, t));
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

void CalendarDayView::showEventMenu(const CalendarEvent &e,
                                    const QPoint &globalPos) {
  QMenu menu(this);
  menu.addAction(QStringLiteral("Löschen"), this, [this, e]() {
    confirmDelete(e);
  });
  menu.exec(globalPos);
}

void CalendarDayView::confirmDelete(const CalendarEvent &e) {
  const auto ans = QMessageBox::question(
      this, QStringLiteral("Termin löschen"),
      QStringLiteral("„%1“ wirklich löschen?").arg(e.title),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (ans == QMessageBox::Yes)
    CalendarService::instance().removeEvent(e.id);
}

QWidget *CalendarDayView::makeEventRow(const CalendarEvent &e, QWidget *parent) {
  auto *row = new QFrame(parent);
  row->setObjectName(QStringLiteral("CalEventRow"));
  row->setAttribute(Qt::WA_StyledBackground, true);
  row->setCursor(Qt::PointingHandCursor);
  row->setStyleSheet(QStringLiteral(
                         "QFrame#CalEventRow {"
                         "  background: %1;"
                         "  border: 1px solid %2;"
                         "  border-left: 3px solid %3;"
                         "  border-radius: 8px;"
                         "}")
                         .arg(cardBg(), hairline(), accent()));
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
  rl->addLayout(textCol, 1);

  row->setProperty("eventId", e.id);
  row->setProperty("eventTitle", e.title);
  row->installEventFilter(this);
  return row;
}

void CalendarDayView::rebuildAll() {
  updateChrome();
  rebuildList();
  rebuildDay();
  rebuildWeek();
  rebuildMonthList();
  if (m_monthCal && m_monthCal->selectedDate() != m_date)
    m_monthCal->setSelectedDate(m_date);
}

void CalendarDayView::rebuildList() {
  if (!m_listLay)
    return;
  while (QLayoutItem *it = m_listLay->takeAt(0)) {
    if (it->widget())
      delete it->widget();
    delete it;
  }

  const int limit = m_compact ? 10 : 24;
  const auto events = CalendarService::instance().upcoming(limit);
  if (events.isEmpty()) {
    auto *empty = new QLabel(
        CalendarService::instance().hasGoogleAccess()
            ? QStringLiteral("Keine anstehenden Termine.")
            : QStringLiteral(
                  "Keine Termine. Verbinde Google oder lege einen Termin an."),
        m_listHost);
    empty->setWordWrap(true);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px; background: transparent;")
            .arg(muted()));
    m_listLay->addWidget(empty);
  } else {
    for (const CalendarEvent &e : events)
      m_listLay->addWidget(makeEventRow(e, m_listHost));
  }
  m_listLay->addStretch(1);
}

void CalendarDayView::rebuildDay() {
  if (!m_timeline || !m_dayScroll)
    return;

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
        d.toString(QStringLiteral("ddd, d. MMM")), m_weekHost);
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
