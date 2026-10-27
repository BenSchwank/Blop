#include "atomicfile.h"
#include "calendarservice.h"

#include "googleauthmanager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QDate>
#include <QTime>
#include <algorithm>

namespace {
QString localCalPath() {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/blop_calendar_local.json");
}
} // namespace

CalendarService &CalendarService::instance() {
  static CalendarService s;
  return s;
}

CalendarService::CalendarService(QObject *parent) : QObject(parent) {
  connect(&GoogleAuthManager::instance(), &GoogleAuthManager::authenticated,
          this, [this]() { refreshGoogle(); });
  connect(&GoogleAuthManager::instance(),
          &GoogleAuthManager::calendarTokenUpdated, this,
          [this]() { refreshGoogle(); });
}

QVector<CalendarEvent> CalendarService::loadLocal() const {
  QFile f(localCalPath());
  if (!f.open(QIODevice::ReadOnly))
    return {};
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isArray())
    return {};
  QVector<CalendarEvent> out;
  for (const QJsonValue &v : doc.array()) {
    const QJsonObject o = v.toObject();
    CalendarEvent e;
    e.id = o.value(QStringLiteral("id")).toString();
    e.title = o.value(QStringLiteral("title")).toString();
    e.start = QDateTime::fromString(
        o.value(QStringLiteral("start")).toString(), Qt::ISODate);
    e.end = QDateTime::fromString(o.value(QStringLiteral("end")).toString(),
                                  Qt::ISODate);
    e.allDay = o.value(QStringLiteral("allDay")).toBool(false);
    e.source = QStringLiteral("local");
    e.location = o.value(QStringLiteral("location")).toString();
    e.color = o.value(QStringLiteral("color")).toString();
    if (!e.id.isEmpty() && e.start.isValid())
      out.append(e);
  }
  return out;
}

void CalendarService::saveLocal(const QVector<CalendarEvent> &events) const {
  QJsonArray arr;
  for (const CalendarEvent &e : events) {
    if (e.source != QLatin1String("local"))
      continue;
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.id);
    o.insert(QStringLiteral("title"), e.title);
    o.insert(QStringLiteral("start"), e.start.toString(Qt::ISODate));
    o.insert(QStringLiteral("end"), e.end.toString(Qt::ISODate));
    o.insert(QStringLiteral("allDay"), e.allDay);
    o.insert(QStringLiteral("location"), e.location);
    if (!e.color.isEmpty())
      o.insert(QStringLiteral("color"), e.color);
    arr.append(o);
  }
  AtomicFile::write(localCalPath(),
                    QJsonDocument(arr).toJson(QJsonDocument::Compact),
                    QStringLiteral("Kalender"));
}

QVector<CalendarEvent> CalendarService::upcoming(int limit) const {
  QVector<CalendarEvent> all = merged();
  const QDateTime now = QDateTime::currentDateTime();
  all.erase(std::remove_if(all.begin(), all.end(),
                           [&](const CalendarEvent &e) {
                             return e.end.isValid() ? e.end < now
                                                    : e.start < now.addDays(-1);
                           }),
            all.end());
  std::sort(all.begin(), all.end(),
            [](const CalendarEvent &a, const CalendarEvent &b) {
              return a.start < b.start;
            });
  if (all.size() > limit)
    all.resize(limit);
  return all;
}

QVector<CalendarEvent> CalendarService::eventsForDay(const QDate &day) const {
  if (!day.isValid())
    return {};
  const QVector<CalendarEvent> all = merged();
  QVector<CalendarEvent> out;
  for (const CalendarEvent &e : all) {
    if (!e.start.isValid())
      continue;
    const QDate s = e.start.date();
    QDate last = e.end.isValid() ? e.end.date() : s;
    if (e.allDay || (e.end.isValid() && e.end.time() == QTime(0, 0) &&
                     e.end > e.start))
      last = last.addDays(-1);
    if (last < s)
      last = s;
    if (day >= s && day <= last)
      out.append(e);
  }
  std::sort(out.begin(), out.end(),
            [](const CalendarEvent &a, const CalendarEvent &b) {
              return a.start < b.start;
            });
  return out;
}

CalendarEvent CalendarService::eventById(const QString &id) const {
  if (id.isEmpty())
    return {};
  for (const CalendarEvent &e : merged()) {
    if (e.id == id)
      return e;
  }
  return {};
}

CalendarEvent CalendarService::addLocal(const QString &title,
                                        const QDateTime &start,
                                        const QDateTime &end, bool allDay,
                                        const QString &location,
                                        const QString &color) {
  auto items = loadLocal();
  CalendarEvent e;
  e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  e.title = title.trimmed().isEmpty() ? QStringLiteral("Termin")
                                      : title.trimmed();
  e.start = start;
  e.end = end.isValid() ? end : start.addSecs(3600);
  e.allDay = allDay;
  e.source = QStringLiteral("local");
  e.location = location.trimmed();
  e.color = color.trimmed();
  items.append(e);
  saveLocal(items);
  emit eventsChanged();
  return e;
}

QVector<CalendarEvent> CalendarService::merged() const {
  QVector<CalendarEvent> all = m_googleCache;
  const QVector<CalendarEvent> local = loadLocal();
  for (const CalendarEvent &l : local) {
    bool dup = false;
    for (const CalendarEvent &g : m_googleCache) {
      if (g.title == l.title && g.start == l.start && g.allDay == l.allDay) {
        dup = true;
        break;
      }
    }
    if (!dup)
      all.append(l);
  }
  return all;
}

void CalendarService::handleGoogleUnauthorized() {
  auto &auth = GoogleAuthManager::instance();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  // A refresh just failed to fix the 401 (or none exists): ask for login.
  if (!auth.hasRefreshToken() || now - m_lastRefreshAttemptMs < 60 * 1000) {
    auth.clearCalendarAccess();
    emit googleSyncFailed(
        QStringLiteral("Google-Token abgelaufen — bitte erneut verbinden"));
    emit googleAuthRequired();
    return;
  }
  m_lastRefreshAttemptMs = now;
  auto *c1 = new QMetaObject::Connection;
  auto *c2 = new QMetaObject::Connection;
  const auto cleanup = [c1, c2]() {
    QObject::disconnect(*c1);
    QObject::disconnect(*c2);
    delete c1;
    delete c2;
  };
  // accessTokenRefreshed -> persistAccessToken -> calendarTokenUpdated
  // already triggers refreshGoogle(); only failures need handling here.
  *c1 = connect(&auth, &GoogleAuthManager::accessTokenRefreshed, this,
                [cleanup]() { cleanup(); });
  *c2 = connect(&auth, &GoogleAuthManager::accessTokenRefreshFailed, this,
                [this, cleanup](const QString &reason) {
                  cleanup();
                  emit googleSyncFailed(
                      QStringLiteral("Google-Anmeldung abgelaufen (%1)")
                          .arg(reason));
                  emit googleAuthRequired();
                });
  auth.refreshAccessToken();
}

void CalendarService::googleRequest(
    const QByteArray &verb, const QUrl &url, const QByteArray &body,
    std::function<void(int, const QByteArray &)> done, bool retried) {
  const QString token = GoogleAuthManager::instance().accessToken();
  if (token.isEmpty()) {
    if (done)
      done(0, QByteArray());
    return;
  }
  auto *nam = new QNetworkAccessManager(this);
  QNetworkRequest req(url);
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setTransferTimeout(20000);
  if (!body.isEmpty())
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
  QNetworkReply *reply = nam->sendCustomRequest(req, verb, body);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, nam, verb, url, body, done, retried]() {
            const QByteArray raw = reply->readAll();
            const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                    .toInt();
            const QString err = reply->errorString();
            const bool netErr = reply->error() != QNetworkReply::NoError;
            reply->deleteLater();
            nam->deleteLater();
            if (status == 401 && !retried &&
                GoogleAuthManager::instance().hasRefreshToken()) {
              auto &auth = GoogleAuthManager::instance();
              auto *c1 = new QMetaObject::Connection;
              auto *c2 = new QMetaObject::Connection;
              const auto cleanup = [c1, c2]() {
                QObject::disconnect(*c1);
                QObject::disconnect(*c2);
                delete c1;
                delete c2;
              };
              *c1 = connect(&auth, &GoogleAuthManager::accessTokenRefreshed,
                            this, [this, cleanup, verb, url, body, done]() {
                              cleanup();
                              googleRequest(verb, url, body, done, true);
                            });
              *c2 = connect(&auth, &GoogleAuthManager::accessTokenRefreshFailed,
                            this, [this, cleanup, done](const QString &) {
                              cleanup();
                              emit googleAuthRequired();
                              if (done)
                                done(401, QByteArray());
                            });
              auth.refreshAccessToken();
              return;
            }
            if (netErr && status == 0)
              emit googleSyncFailed(err);
            if (done)
              done(status, raw);
          });
}

static QJsonObject googleEventBody(const QString &title, const QDateTime &start,
                                   const QDateTime &end, bool allDay,
                                   const QString &location) {
  QJsonObject body;
  body.insert(QStringLiteral("summary"),
              title.trimmed().isEmpty() ? QStringLiteral("Termin")
                                        : title.trimmed());
  body.insert(QStringLiteral("location"), location.trimmed());
  QJsonObject startObj;
  QJsonObject endObj;
  if (allDay) {
    startObj.insert(QStringLiteral("date"), start.date().toString(Qt::ISODate));
    endObj.insert(QStringLiteral("date"),
                  (end.isValid() ? end : start.addDays(1))
                      .date()
                      .toString(Qt::ISODate));
  } else {
    startObj.insert(QStringLiteral("dateTime"),
                    start.toUTC().toString(Qt::ISODate));
    endObj.insert(QStringLiteral("dateTime"),
                  (end.isValid() ? end : start.addSecs(3600))
                      .toUTC()
                      .toString(Qt::ISODate));
  }
  body.insert(QStringLiteral("start"), startObj);
  body.insert(QStringLiteral("end"), endObj);
  return body;
}

CalendarEvent CalendarService::createEvent(const QString &title,
                                           const QDateTime &start,
                                           const QDateTime &end, bool allDay,
                                           const QString &location,
                                           const QString &color) {
  if (!hasGoogleAccess())
    return addLocal(title, start, end, allDay, location, color);

  // Local mirror keeps the event visible (and safe offline) until Google
  // confirms; on success it is removed so it is not shown twice.
  CalendarEvent pending = addLocal(title, start, end, allDay, location, color);
  const QString mirrorId = pending.id;
  const QJsonObject body = googleEventBody(title, start, end, allDay, location);
  googleRequest(
      "POST",
      QUrl(QStringLiteral(
          "https://www.googleapis.com/calendar/v3/calendars/primary/events")),
      QJsonDocument(body).toJson(QJsonDocument::Compact),
      [this, mirrorId](int status, const QByteArray &) {
        if (status >= 200 && status < 300) {
          removeLocal(mirrorId);
          refreshGoogle();
        } else {
          emit googleSyncFailed(
              QStringLiteral("Termin konnte nicht zu Google übertragen "
                             "werden (lokal gespeichert)"));
        }
      });
  return pending;
}

bool CalendarService::updateEvent(const CalendarEvent &e) {
  if (e.id.isEmpty())
    return false;
  auto items = loadLocal();
  for (CalendarEvent &it : items) {
    if (it.id != e.id)
      continue;
    it.title = e.title;
    it.start = e.start;
    it.end = e.end;
    it.allDay = e.allDay;
    it.location = e.location;
    it.color = e.color;
    saveLocal(items);
    emit eventsChanged();
    return true;
  }
  if (!hasGoogleAccess())
    return false;

  // Optimistic cache update, Google PATCH, then reconcile via refresh.
  for (CalendarEvent &g : m_googleCache) {
    if (g.id == e.id) {
      g.title = e.title;
      g.start = e.start;
      g.end = e.end;
      g.allDay = e.allDay;
      g.location = e.location;
    }
  }
  emit eventsChanged();
  const QJsonObject body =
      googleEventBody(e.title, e.start, e.end, e.allDay, e.location);
  googleRequest(
      "PATCH",
      QUrl(QStringLiteral(
               "https://www.googleapis.com/calendar/v3/calendars/primary/"
               "events/") +
           QString::fromLatin1(QUrl::toPercentEncoding(e.id))),
      QJsonDocument(body).toJson(QJsonDocument::Compact),
      [this](int status, const QByteArray &) {
        if (status < 200 || status >= 300)
          emit googleSyncFailed(
              QStringLiteral("Änderung konnte nicht zu Google übertragen "
                             "werden"));
        refreshGoogle();
      });
  return true;
}

bool CalendarService::removeLocal(const QString &id) {
  auto items = loadLocal();
  const int before = items.size();
  items.erase(std::remove_if(items.begin(), items.end(),
                             [&](const CalendarEvent &e) {
                               return e.id == id;
                             }),
              items.end());
  if (items.size() == before)
    return false;
  saveLocal(items);
  emit eventsChanged();
  return true;
}

bool CalendarService::removeEvent(const QString &id) {
  if (id.isEmpty())
    return false;
  if (removeLocal(id))
    return true;
  if (!hasGoogleAccess())
    return false;

  const int before = m_googleCache.size();
  m_googleCache.erase(std::remove_if(m_googleCache.begin(), m_googleCache.end(),
                                     [&](const CalendarEvent &e) {
                                       return e.id == id;
                                     }),
                      m_googleCache.end());
  if (m_googleCache.size() != before)
    emit eventsChanged();

  googleRequest(
      "DELETE",
      QUrl(QStringLiteral(
               "https://www.googleapis.com/calendar/v3/calendars/primary/"
               "events/") +
           QString::fromLatin1(QUrl::toPercentEncoding(id))),
      QByteArray(), [this](int status, const QByteArray &) {
        // 404/410: already gone remotely, fine.
        if (status >= 400 && status != 404 && status != 410)
          emit googleSyncFailed(
              QStringLiteral("Löschen bei Google fehlgeschlagen"));
        refreshGoogle();
      });
  return true;
}

bool CalendarService::hasGoogleAccess() const {
  return GoogleAuthManager::instance().hasCalendarAccess();
}

void CalendarService::connectGoogle() {
  GoogleAuthManager::instance().loginForCalendar();
}

void CalendarService::disconnectGoogle() {
  GoogleAuthManager::instance().clearCalendarAccess();
  m_googleCache.clear();
  emit eventsChanged();
}

void CalendarService::refreshGoogle() {
  if (m_googleInFlight)
    return;
  const QString token = GoogleAuthManager::instance().accessToken();
  if (token.isEmpty()) {
    // Access token gone but a refresh token may bring it back (app restart
    // after > 1 h): try that before treating the account as disconnected.
    auto &auth = GoogleAuthManager::instance();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (auth.hasRefreshToken() && now - m_lastRefreshAttemptMs > 60 * 1000) {
      m_lastRefreshAttemptMs = now;
      auth.refreshAccessToken();
      return;
    }
    m_googleCache.clear();
    emit eventsChanged();
    return;
  }

  QUrl url(QStringLiteral("https://www.googleapis.com/calendar/v3/calendars/"
                          "primary/events"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("maxResults"), QStringLiteral("100"));
  q.addQueryItem(QStringLiteral("singleEvents"), QStringLiteral("true"));
  q.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("startTime"));
  q.addQueryItem(QStringLiteral("timeMin"),
                 QDateTime(QDate::currentDate().addDays(-7), QTime(0, 0))
                     .toUTC()
                     .toString(Qt::ISODate));
  q.addQueryItem(QStringLiteral("timeMax"),
                 QDateTime(QDate::currentDate().addDays(60), QTime(23, 59))
                     .toUTC()
                     .toString(Qt::ISODate));
  url.setQuery(q);

  // A direct GET (not googleRequest): its 401 handling coordinates with the
  // calendarTokenUpdated -> refreshGoogle() loop through handleGoogleUnauthorized.
  auto *nam = new QNetworkAccessManager(this);
  QNetworkRequest req(url);
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setTransferTimeout(20000);
  m_googleInFlight = true;
  const int epoch = ++m_googleEpoch;
  QNetworkReply *reply = nam->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, nam, epoch]() {
    reply->deleteLater();
    nam->deleteLater();
    if (epoch != m_googleEpoch) {
      m_googleInFlight = false;
      return;
    }
    m_googleInFlight = false;
    if (reply->error() != QNetworkReply::NoError) {
      const int status =
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      if (status == 401)
        handleGoogleUnauthorized();
      else
        emit googleSyncFailed(reply->errorString());
      return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray items =
        doc.object().value(QStringLiteral("items")).toArray();
    QVector<CalendarEvent> cache;
    for (const QJsonValue &v : items) {
      const QJsonObject o = v.toObject();
      if (o.value(QStringLiteral("status")).toString() ==
          QLatin1String("cancelled"))
        continue;
      CalendarEvent e;
      e.id = o.value(QStringLiteral("id")).toString();
      e.title = o.value(QStringLiteral("summary"))
                    .toString(QStringLiteral("Ohne Titel"));
      e.source = QStringLiteral("google");
      e.location = o.value(QStringLiteral("location")).toString();
      const QJsonObject start = o.value(QStringLiteral("start")).toObject();
      const QJsonObject end = o.value(QStringLiteral("end")).toObject();
      if (start.contains(QStringLiteral("dateTime"))) {
        e.start = QDateTime::fromString(
            start.value(QStringLiteral("dateTime")).toString(), Qt::ISODate);
        e.end = QDateTime::fromString(
            end.value(QStringLiteral("dateTime")).toString(), Qt::ISODate);
      } else {
        e.allDay = true;
        e.start = QDateTime(QDate::fromString(
                                start.value(QStringLiteral("date")).toString(),
                                Qt::ISODate),
                            QTime(0, 0));
        e.end = QDateTime(
            QDate::fromString(end.value(QStringLiteral("date")).toString(),
                              Qt::ISODate),
            QTime(0, 0));
      }
      if (e.start.isValid())
        cache.append(e);
    }
    m_googleCache = cache;
    emit eventsChanged();
  });
}