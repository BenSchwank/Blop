#include "calendardayview.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kDayStartHour = 6;
constexpr int kDayEndHour = 22;
constexpr int kHourPxCompact = 36;
constexpr int kHourPxFull = 56;
} // namespace

CalendarDayView::CalendarDayView(QWidget *parent) : QWidget(parent) {
  m_date = QDate::currentDate();
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(UiScale::dp(8));

  auto *hdr = new QHBoxLayout();
  hdr->setSpacing(UiScale::dp(6));
  auto *btnPrev = new QPushButton(QStringLiteral("‹"), this);
  auto *btnNext = new QPushButton(QStringLiteral("›"), this);
  auto *btnToday = new QPushButton(QStringLiteral("Heute"), this);
  for (QPushButton *b : {btnPrev, btnNext, btnToday}) {
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(BlopStyle::quietIconButtonQss());
    b->setMinimumSize(UiScale::dp(36), UiScale::dp(32));
  }
  m_dateLabel = new QLabel(this);
  m_dateLabel->setAlignment(Qt::AlignCenter);
  m_dateLabel->setStyleSheet(QStringLiteral(
      "color: %1; font-size: 16px; font-weight: 650; background: transparent;")
                                 .arg(BlopTheme::textPrimary().name()));

  hdr->addWidget(btnPrev, 0);
  hdr->addWidget(m_dateLabel, 1);
  hdr->addWidget(btnToday, 0);
  hdr->addWidget(btnNext, 0);
  root->addLayout(hdr);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(false);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setStyleSheet(
      QStringLiteral("QScrollArea { background: transparent; border: none; }"));
  m_timeline = new QWidget;
  m_timeline->setObjectName(QStringLiteral("CalDayTimeline"));
  m_scroll->setWidget(m_timeline);
  m_scroll->viewport()->installEventFilter(this);
  root->addWidget(m_scroll, 1);

  connect(btnPrev, &QPushButton::clicked, this,
          [this]() { setDate(m_date.addDays(-1)); });
  connect(btnNext, &QPushButton::clicked, this,
          [this]() { setDate(m_date.addDays(1)); });
  connect(btnToday, &QPushButton::clicked, this,
          [this]() { setDate(QDate::currentDate()); });
  connect(&CalendarService::instance(), &CalendarService::eventsChanged, this,
          &CalendarDayView::refresh);

  refresh();
}

void CalendarDayView::setCompact(bool on) {
  if (m_compact == on)
    return;
  m_compact = on;
  rebuildTimeline();
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

void CalendarDayView::refresh() {
  m_dateLabel->setText(m_date.toString(QStringLiteral("dddd, d. MMMM yyyy")));
  rebuildTimeline();
}

void CalendarDayView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  rebuildTimeline();
}

bool CalendarDayView::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_scroll->viewport()) {
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
      const auto events = CalendarService::instance().eventsForDay(m_date);
      for (const CalendarEvent &e : events) {
        if (e.id == eventId) {
          showEventMenu(e, me->globalPosition().toPoint());
          return true;
        }
      }
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

void CalendarDayView::rebuildTimeline() {
  if (!m_timeline || !m_scroll)
    return;

  const QList<QWidget *> kids =
      m_timeline->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
  for (QWidget *c : kids)
    delete c;

  const int hourPx = UiScale::dp(m_compact ? kHourPxCompact : kHourPxFull);
  const int hours = kDayEndHour - kDayStartHour;
  const int labelW = UiScale::dp(44);
  const int width =
      qMax(m_scroll->viewport()->width(), UiScale::dp(m_compact ? 260 : 420));
  const int height = hours * hourPx;
  m_timeline->setFixedSize(width, height);

  const QString line = BlopTheme::instance().isDark()
                           ? QStringLiteral("rgba(255,255,255,0.08)")
                           : QStringLiteral("rgba(55,53,47,0.10)");
  const QString muted = BlopTheme::textSecondary().name();
  const QString ink = BlopTheme::textPrimary().name();
  const QString accent = BlopStyle::accent().name();

  for (int h = kDayStartHour; h < kDayEndHour; ++h) {
    const int y = (h - kDayStartHour) * hourPx;
    auto *hourLbl = new QLabel(
        QStringLiteral("%1:00").arg(h, 2, 10, QLatin1Char('0')), m_timeline);
    hourLbl->setGeometry(0, y, labelW - 4, UiScale::dp(18));
    hourLbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(muted));
    auto *rule = new QFrame(m_timeline);
    rule->setGeometry(labelW, y, width - labelW, 1);
    rule->setStyleSheet(QStringLiteral("background: %1;").arg(line));
  }

  auto *hit = new QWidget(m_timeline);
  hit->setGeometry(labelW, 0, width - labelW, height);
  hit->setCursor(Qt::PointingHandCursor);
  hit->setProperty("calHit", true);
  hit->installEventFilter(this);

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
    chip->setGeometry(labelW + 4, y, width - labelW - 10, hgt);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setStyleSheet(QStringLiteral(
                            "QFrame {"
                            "  background: %1;"
                            "  border: 1px solid %2;"
                            "  border-left: 3px solid %3;"
                            "  border-radius: 6px;"
                            "}")
                            .arg(BlopTheme::instance().isDark()
                                     ? QStringLiteral("rgba(91,157,255,0.18)")
                                     : QStringLiteral("rgba(91,157,255,0.14)"),
                                 line, accent));
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
            .arg(ink));
    chipLay->addWidget(title);
    if (!e.allDay && hgt > UiScale::dp(34)) {
      auto *when = new QLabel(QStringLiteral("%1 – %2")
                                  .arg(startT.toString(QStringLiteral("HH:mm")),
                                       endT.toString(QStringLiteral("HH:mm"))),
                              chip);
      when->setStyleSheet(
          QStringLiteral("color: %1; font-size: 11px; background: transparent;")
              .arg(muted));
      chipLay->addWidget(when);
    }
    chip->setProperty("eventId", e.id);
    chip->installEventFilter(this);
    chip->raise();
  }

  if (m_date == QDate::currentDate()) {
    const int nowMin =
        QTime::currentTime().hour() * 60 + QTime::currentTime().minute();
    const int y =
        qMax(0, ((nowMin - kDayStartHour * 60) * hourPx) / 60 - hourPx);
    QTimer::singleShot(0, this, [this, y]() {
      if (m_scroll)
        m_scroll->verticalScrollBar()->setValue(y);
    });
  }
}
