#pragma once

#include "calendarservice.h"

#include <QDate>
#include <QWidget>

class QButtonGroup;
class QCalendarWidget;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

/// Dashboard/maximized calendar: Liste | Tag | Woche | Monat.
class CalendarDayView : public QWidget {
  Q_OBJECT
public:
  enum class Mode { List = 0, Day = 1, Week = 2, Month = 3 };

  explicit CalendarDayView(QWidget *parent = nullptr);

  void setDate(const QDate &date);
  QDate date() const { return m_date; }
  void setCompact(bool on);
  void setMode(Mode mode);
  Mode mode() const { return m_mode; }
  void refresh();

signals:
  void createAt(const QDateTime &start);
  void dateChanged(const QDate &date);

protected:
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void rebuildAll();
  void rebuildList();
  void rebuildDay();
  void rebuildWeek();
  void rebuildMonthList();
  void updateChrome();
  void relayoutModeChips();
  void showEventMenu(const CalendarEvent &e, const QPoint &globalPos);
  void confirmDelete(const CalendarEvent &e);
  QWidget *makeEventRow(const CalendarEvent &e, QWidget *parent);

  QDate m_date;
  bool m_compact{false};
  Mode m_mode{Mode::List};

  QLabel *m_dateLabel{nullptr};
  QWidget *m_navBar{nullptr};
  QButtonGroup *m_modeGroup{nullptr};
  QGridLayout *m_modeGrid{nullptr};
  QStackedWidget *m_stack{nullptr};

  QScrollArea *m_listScroll{nullptr};
  QWidget *m_listHost{nullptr};
  QVBoxLayout *m_listLay{nullptr};

  QScrollArea *m_dayScroll{nullptr};
  QWidget *m_timeline{nullptr};

  QScrollArea *m_weekScroll{nullptr};
  QWidget *m_weekHost{nullptr};
  QVBoxLayout *m_weekLay{nullptr};

  QCalendarWidget *m_monthCal{nullptr};
  QScrollArea *m_monthListScroll{nullptr};
  QWidget *m_monthListHost{nullptr};
  QVBoxLayout *m_monthListLay{nullptr};

  QPoint m_pressPos;
  bool m_swiping{false};
};
