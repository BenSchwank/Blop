#pragma once

#include <QObject>
#include <QString>

struct WeatherSnapshot {
  QString placeLabel;
  double latitude{0};
  double longitude{0};
  double tempC{0};
  int weatherCode{-1};
  QString summary;
  bool valid{false};
  QString error;
};

/// Open-Meteo forecast + geocoding (no API key).
class WeatherService : public QObject {
  Q_OBJECT
public:
  static WeatherService &instance();

  WeatherSnapshot current() const { return m_snap; }
  bool hasLocation() const { return m_hasLocation; }

  void setLocation(const QString &label, double lat, double lon);
  void clearLocation();
  void refresh(bool force = false);
  void searchPlace(const QString &query);

signals:
  void weatherChanged();
  void searchFailed(const QString &message);

private:
  explicit WeatherService(QObject *parent = nullptr);
  void loadSettings();
  void saveSettings() const;
  void fetchForecast();
  static QString summaryForCode(int code);

  WeatherSnapshot m_snap;
  bool m_hasLocation{false};
  qint64 m_fetchedAtMs{0};
  class QNetworkAccessManager *m_nam{nullptr};
};
