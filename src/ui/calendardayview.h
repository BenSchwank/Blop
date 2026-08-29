#pragma once

#include "calendarservice.h"

#include <QDate>
#include <QWidget>

class QLabel;
class QScrollArea;
class QVBoxLayout;

/// Google-Calendar-style single-day timeline with prev/next day navigation.
class CalendarDayView : public QWidget {
  Q_OBJECT
public:
  explicit CalendarDayView(QWidget *parent = nullptr);

  void setDate(const QDate &date);
  QDate date() const { return m_date; }
  void setCompact(bool on);
  void refresh();

signals:
  void createAt(const QDateTime &start);
  void dateChanged(const QDate &date);

protected:
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void rebuildTimeline();
  void showEventMenu(const CalendarEvent &e, const QPoint &globalPos);
  void confirmDelete(const CalendarEvent &e);

  QDate m_date;
  bool m_compact{false};
  QLabel *m_dateLabel{nullptr};
  QScrollArea *m_scroll{nullptr};
  QWidget *m_timeline{nullptr};
  QPoint m_pressPos;
  bool m_swiping{false};
};
