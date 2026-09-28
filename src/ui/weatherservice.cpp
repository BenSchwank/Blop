#include "weatherservice.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr const char *kLat = "dashboard/weather.lat";
constexpr const char *kLon = "dashboard/weather.lon";
constexpr const char *kLabel = "dashboard/weather.placeLabel";
constexpr qint64 kCacheMs = 20 * 60 * 1000;
} // namespace

WeatherService &WeatherService::instance() {
  static WeatherService s;
  return s;
}

WeatherService::WeatherService(QObject *parent) : QObject(parent) {
  m_nam = new QNetworkAccessManager(this);
  loadSettings();
  if (m_hasLocation)
    refresh(true);
}

void WeatherService::loadSettings() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  if (!st.contains(QLatin1String(kLat)) || !st.contains(QLatin1String(kLon))) {
    m_hasLocation = false;
    return;
  }
  m_snap.latitude = st.value(QLatin1String(kLat)).toDouble();
  m_snap.longitude = st.value(QLatin1String(kLon)).toDouble();
  m_snap.placeLabel = st.value(QLatin1String(kLabel), QStringLiteral("Ort"))
                          .toString();
  m_hasLocation = true;
}

void WeatherService::saveSettings() const {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  if (!m_hasLocation) {
    st.remove(QLatin1String(kLat));
    st.remove(QLatin1String(kLon));
    st.remove(QLatin1String(kLabel));
    return;
  }
  st.setValue(QLatin1String(kLat), m_snap.latitude);
  st.setValue(QLatin1String(kLon), m_snap.longitude);
  st.setValue(QLatin1String(kLabel), m_snap.placeLabel);
}

void WeatherService::setLocation(const QString &label, double lat, double lon) {
  m_snap.placeLabel = label;
  m_snap.latitude = lat;
  m_snap.longitude = lon;
  m_hasLocation = true;
  m_snap.valid = false;
  m_snap.error.clear();
  saveSettings();
  m_fetchedAtMs = 0;
  fetchForecast();
}

void WeatherService::clearLocation() {
  m_hasLocation = false;
  m_snap = WeatherSnapshot{};
  m_fetchedAtMs = 0;
  saveSettings();
  emit weatherChanged();
}

void WeatherService::refresh(bool force) {
  if (!m_hasLocation)
    return;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (!force && m_snap.valid && (now - m_fetchedAtMs) < kCacheMs)
    return;
  fetchForecast();
}

QString WeatherService::summaryForCode(int code) {
  if (code == 0)
    return QStringLiteral("Klar");
  if (code <= 3)
    return QStringLiteral("Teilweise bewölkt");
  if (code <= 48)
    return QStringLiteral("Nebel");
  if (code <= 57)
    return QStringLiteral("Nieselregen");
  if (code <= 67)
    return QStringLiteral("Regen");
  if (code <= 77)
    return QStringLiteral("Schnee");
  if (code <= 82)
    return QStringLiteral("Schauer");
  if (code <= 86)
    return QStringLiteral("Schneeschauer");
  if (code <= 99)
    return QStringLiteral("Gewitter");
  return QStringLiteral("Wetter");
}

void WeatherService::fetchForecast() {
  if (!m_hasLocation || !m_nam)
    return;
  QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("latitude"),
                 QString::number(m_snap.latitude, 'f', 4));
  q.addQueryItem(QStringLiteral("longitude"),
                 QString::number(m_snap.longitude, 'f', 4));
  q.addQueryItem(QStringLiteral("current"),
                 QStringLiteral("temperature_2m,weather_code"));
  q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
  url.setQuery(q);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("BlopDashboard/1.0"));
  QNetworkReply *reply = m_nam->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      m_snap.valid = false;
      m_snap.error = QStringLiteral("Wetter nicht erreichbar");
      emit weatherChanged();
      return;
    }
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject cur =
        doc.object().value(QStringLiteral("current")).toObject();
    if (cur.isEmpty()) {
      m_snap.valid = false;
      m_snap.error = QStringLiteral("Keine Wetterdaten");
      emit weatherChanged();
      return;
    }
    m_snap.tempC = cur.value(QStringLiteral("temperature_2m")).toDouble();
    m_snap.weatherCode = cur.value(QStringLiteral("weather_code")).toInt();
    m_snap.summary = summaryForCode(m_snap.weatherCode);
    m_snap.valid = true;
    m_snap.error.clear();
    m_fetchedAtMs = QDateTime::currentMSecsSinceEpoch();
    emit weatherChanged();
  });
}

void WeatherService::searchPlace(const QString &query) {
  const QString qtrim = query.trimmed();
  if (qtrim.isEmpty() || !m_nam) {
    emit searchFailed(QStringLiteral("Ort eingeben"));
    return;
  }
  QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("name"), qtrim);
  q.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
  q.addQueryItem(QStringLiteral("language"), QStringLiteral("de"));
  q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
  url.setQuery(q);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("BlopDashboard/1.0"));
  QNetworkReply *reply = m_nam->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, qtrim]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit searchFailed(QStringLiteral("Ortssuche fehlgeschlagen"));
      return;
    }
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray results =
        doc.object().value(QStringLiteral("results")).toArray();
    if (results.isEmpty()) {
      emit searchFailed(QStringLiteral("Kein Ort gefunden"));
      return;
    }
    const QJsonObject place = results.first().toObject();
    const QString name = place.value(QStringLiteral("name")).toString(qtrim);
    const QString admin =
        place.value(QStringLiteral("admin1")).toString();
    const QString country =
        place.value(QStringLiteral("country_code")).toString();
    QString label = name;
    if (!admin.isEmpty())
      label += QStringLiteral(", ") + admin;
    else if (!country.isEmpty())
      label += QStringLiteral(", ") + country;
    setLocation(label, place.value(QStringLiteral("latitude")).toDouble(),
                place.value(QStringLiteral("longitude")).toDouble());
  });
}
