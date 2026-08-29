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
    arr.append(o);
  }
  QFile f(localCalPath());
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<CalendarEvent> CalendarService::upcoming(int limit) const {
  QVector<CalendarEvent> all = loadLocal();
  all += m_googleCache;
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
  QVector<CalendarEvent> all = loadLocal();
  all += m_googleCache;
  QVector<CalendarEvent> out;
  for (const CalendarEvent &e : all) {
    if (!e.start.isValid())
      continue;
    const QDate s = e.start.date();
    const QDate en = e.end.isValid() ? e.end.date() : s;
    if (day >= s && day <= en)
      out.append(e);
  }
  std::sort(out.begin(), out.end(),
            [](const CalendarEvent &a, const CalendarEvent &b) {
              return a.start < b.start;
            });
  return out;
}

CalendarEvent CalendarService::addLocal(const QString &title,
                                        const QDateTime &start,
                                        const QDateTime &end, bool allDay) {
  auto items = loadLocal();
  CalendarEvent e;
  e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  e.title = title.trimmed().isEmpty() ? QStringLiteral("Termin")
                                      : title.trimmed();
  e.start = start;
  e.end = end.isValid() ? end : start.addSecs(3600);
  e.allDay = allDay;
  e.source = QStringLiteral("local");
  items.append(e);
  saveLocal(items);
  emit eventsChanged();
  return e;
}

CalendarEvent CalendarService::createEvent(const QString &title,
                                           const QDateTime &start,
                                           const QDateTime &end, bool allDay) {
  const QString token = GoogleAuthManager::instance().accessToken();
  if (token.isEmpty())
    return addLocal(title, start, end, allDay);

  auto *nam = new QNetworkAccessManager(this);
  QUrl url(QStringLiteral(
      "https://www.googleapis.com/calendar/v3/calendars/primary/events"));
  QNetworkRequest req(url);
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                QStringLiteral("application/json"));

  QJsonObject body;
  body.insert(QStringLiteral("summary"),
              title.trimmed().isEmpty() ? QStringLiteral("Termin")
                                        : title.trimmed());
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

  // Optimistic local mirror until refresh returns.
  CalendarEvent pending = addLocal(title, start, end, allDay);
  pending.source = QStringLiteral("google");

  QNetworkReply *reply =
      nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
    reply->deleteLater();
    nam->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit googleSyncFailed(reply->errorString());
      return;
    }
    refreshGoogle();
  });
  return pending;
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

  // Google event (or unknown id still try Google delete).
  const QString token = GoogleAuthManager::instance().accessToken();
  if (token.isEmpty())
    return false;

  // Drop from cache immediately for snappy UI.
  const int before = m_googleCache.size();
  m_googleCache.erase(std::remove_if(m_googleCache.begin(), m_googleCache.end(),
                                     [&](const CalendarEvent &e) {
                                       return e.id == id;
                                     }),
                      m_googleCache.end());
  if (m_googleCache.size() != before)
    emit eventsChanged();

  auto *nam = new QNetworkAccessManager(this);
  QUrl url(QStringLiteral(
               "https://www.googleapis.com/calendar/v3/calendars/primary/events/") +
           QUrl::toPercentEncoding(id));
  QNetworkRequest req(url);
  req.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
  QNetworkReply *reply = nam->deleteResource(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
    reply->deleteLater();
    nam->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit googleSyncFailed(reply->errorString());
      refreshGoogle();
      return;
    }
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
  const QString token = GoogleAuthManager::instance().accessToken();
  if (token.isEmpty()) {
    m_googleCache.clear();
    emit eventsChanged();
    return;
  }

  auto *nam = new QNetworkAccessManager(this);
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

  QNetworkRequest req(url);
  req.setRawHeader("Authorization",
                   QByteArray("Bearer ") + token.toUtf8());
  QNetworkReply *reply = nam->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
    reply->deleteLater();
    nam->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit googleSyncFailed(reply->errorString());
      return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray items =
        doc.object().value(QStringLiteral("items")).toArray();
    QVector<CalendarEvent> cache;
    for (const QJsonValue &v : items) {
      const QJsonObject o = v.toObject();
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
