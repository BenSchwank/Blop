#pragma once

#include <QDate>
#include <QLocale>
#include <QObject>
#include <QString>

/// App UI/date locale — chosen in Settings (Darstellung), never mixed per widget.
class BlopLocale : public QObject {
  Q_OBJECT
public:
  enum class Pref { System = 0, German = 1, English = 2 };

  static BlopLocale &instance();

  Pref preference() const { return m_pref; }
  void setPreference(Pref p);

  /// Resolved locale used for dates and bilingual UI helpers.
  QLocale locale() const;
  bool isGerman() const;

  QString formatDate(const QDate &d, const QString &format) const;
  QString goodMorning() const;
  QString goodAfternoon() const;
  QString goodEvening() const;

  static Pref prefFromCode(const QString &code);
  static QString codeFromPref(Pref p);

  /// Load from QSettings and apply QLocale::setDefault.
  void applyFromSettings();

signals:
  void localeChanged();

private:
  explicit BlopLocale(QObject *parent = nullptr);
  Pref m_pref{Pref::System};
};
