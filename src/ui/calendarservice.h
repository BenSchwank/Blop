#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

#include <functional>

struct CalendarEvent {
  QString id;
  QString title;
  QDateTime start;
  QDateTime end;
  bool allDay{false};
  QString source; // "local" | "google"
  QString location;
  QString color; // optional #RRGGBB accent
};

/// Local Blop events + Google Calendar when an access token is available.
class CalendarService : public QObject {
  Q_OBJECT
public:
  static CalendarService &instance();

  QVector<CalendarEvent> upcoming(int limit = 12) const;
  QVector<CalendarEvent> eventsForDay(const QDate &day) const;
  CalendarEvent eventById(const QString &id) const;
  CalendarEvent addLocal(const QString &title, const QDateTime &start,
                         const QDateTime &end, bool allDay = false,
                         const QString &location = QString(),
                         const QString &color = QString());
  /// Creates on Google when access token exists; otherwise local.
  CalendarEvent createEvent(const QString &title, const QDateTime &start,
                            const QDateTime &end, bool allDay = false,
                            const QString &location = QString(),
                            const QString &color = QString());
  /// Updates a local or Google event (matched by `e.id`).
  bool updateEvent(const CalendarEvent &e);
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
  /// Local + Google, without local mirrors that Google already returned.
  QVector<CalendarEvent> merged() const;
  /// Authorized Google call; on 401 refreshes the token once and retries.
  void googleRequest(const QByteArray &verb, const QUrl &url,
                     const QByteArray &body,
                     std::function<void(int status, const QByteArray &)> done,
                     bool retried = false);
  void handleGoogleUnauthorized();
  QVector<CalendarEvent> m_googleCache;
  qint64 m_lastRefreshAttemptMs{0};
  bool m_googleInFlight{false};
  int m_googleEpoch{0};
};
