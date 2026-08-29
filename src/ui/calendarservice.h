#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

struct CalendarEvent {
  QString id;
  QString title;
  QDateTime start;
  QDateTime end;
  bool allDay{false};
  QString source; // "local" | "google"
  QString location;
};

/// Local Blop events + Google Calendar when an access token is available.
class CalendarService : public QObject {
  Q_OBJECT
public:
  static CalendarService &instance();

  QVector<CalendarEvent> upcoming(int limit = 12) const;
  QVector<CalendarEvent> eventsForDay(const QDate &day) const;
  CalendarEvent addLocal(const QString &title, const QDateTime &start,
                         const QDateTime &end, bool allDay = false);
  /// Creates on Google when access token exists; otherwise local.
  CalendarEvent createEvent(const QString &title, const QDateTime &start,
                            const QDateTime &end, bool allDay = false);
  bool removeLocal(const QString &id);
  /// Deletes local or Google event by id.
  bool removeEvent(const QString &id);

  bool hasGoogleAccess() const;
  void refreshGoogle();
  void connectGoogle(); // triggers OAuth with calendar scopes
  void disconnectGoogle();

signals:
  void eventsChanged();
  void googleAuthRequired();
  void googleSyncFailed(const QString &error);

private:
  explicit CalendarService(QObject *parent = nullptr);
  QVector<CalendarEvent> loadLocal() const;
  void saveLocal(const QVector<CalendarEvent> &events) const;
  QVector<CalendarEvent> m_googleCache;
};
