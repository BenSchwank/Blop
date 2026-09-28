#pragma once

#include "calendarservice.h"

#include <QDate>
#include <QWidget>

class QCalendarWidget;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

/// Dashboard/maximized calendar: Tag | Woche | Monat via quiet ⋯ menu.
class CalendarDayView : public QWidget {
  Q_OBJECT
public:
  enum class Mode { Day = 0, Week = 1, Month = 2 };

  explicit CalendarDayView(QWidget *parent = nullptr);

  void setDate(const QDate &date);
  QDate date() const { return m_date; }
  void setCompact(bool on);
  /// Ultra-small dashboard tiles: day agenda, no mode chrome / nav.
  void setMinimal(bool on);
  void setMode(Mode mode);
  Mode mode() const { return m_mode; }
  void refresh();

signals:
  void dateChanged(const QDate &date);

protected:
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void rebuildAll();
  void rebuildDay();
  void rebuildWeek();
  void rebuildMonthList();
  void updateChrome();
  void refreshGoogleButton();
  void applyCompactChrome();
  void syncModeLabel();
  void showModeMenu();
  void requestCreate(const QDateTime &presetStart);
  void showEventMenu(const CalendarEvent &e, const QPoint &globalPos);
  void confirmDelete(const CalendarEvent &e);
  QWidget *makeEventRow(const CalendarEvent &e, QWidget *parent);
  static QString modeLabel(Mode mode);

  QDate m_date;
  bool m_compact{false};
  bool m_minimal{false};
  Mode m_mode{Mode::Day};

  QLabel *m_dateLabel{nullptr};
  QWidget *m_navBar{nullptr};
  QWidget *m_modeBar{nullptr};
  QLabel *m_modeLabel{nullptr};
  QPushButton *m_btnModeMore{nullptr};
  QPushButton *m_btnAdd{nullptr};
  QPushButton *m_btnGoogle{nullptr};
  QStackedWidget *m_stack{nullptr};

  QScrollArea *m_dayScroll{nullptr};
  QWidget *m_timeline{nullptr};
  QWidget *m_dayPage{nullptr};
  QVBoxLayout *m_dayAgendaLay{nullptr};

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
