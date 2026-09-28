#include "bloplocale.h"

#include <QDate>
#include <QSettings>

namespace {
constexpr const char *kKey = "ui/language";
}

BlopLocale &BlopLocale::instance() {
  static BlopLocale s;
  return s;
}

BlopLocale::BlopLocale(QObject *parent) : QObject(parent) {
  applyFromSettings();
}

BlopLocale::Pref BlopLocale::prefFromCode(const QString &code) {
  if (code == QLatin1String("de"))
    return Pref::German;
  if (code == QLatin1String("en"))
    return Pref::English;
  return Pref::System;
}

QString BlopLocale::codeFromPref(Pref p) {
  switch (p) {
  case Pref::German:
    return QStringLiteral("de");
  case Pref::English:
    return QStringLiteral("en");
  case Pref::System:
  default:
    return QStringLiteral("system");
  }
}

void BlopLocale::applyFromSettings() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  m_pref = prefFromCode(st.value(QLatin1String(kKey), QStringLiteral("system"))
                            .toString());
  QLocale::setDefault(locale());
}

void BlopLocale::setPreference(Pref p) {
  if (m_pref == p)
    return;
  m_pref = p;
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(QLatin1String(kKey), codeFromPref(p));
  QLocale::setDefault(locale());
  emit localeChanged();
}

QLocale BlopLocale::locale() const {
  switch (m_pref) {
  case Pref::German:
    return QLocale(QLocale::German, QLocale::Germany);
  case Pref::English:
    return QLocale(QLocale::English, QLocale::UnitedStates);
  case Pref::System:
  default:
    return QLocale::system();
  }
}

bool BlopLocale::isGerman() const {
  return locale().language() == QLocale::German;
}

QString BlopLocale::formatDate(const QDate &d, const QString &format) const {
  if (!d.isValid())
    return {};
  return locale().toString(d, format);
}

QString BlopLocale::goodMorning() const {
  return isGerman() ? QStringLiteral("Guten Morgen")
                    : QStringLiteral("Good morning");
}

QString BlopLocale::goodAfternoon() const {
  return isGerman() ? QStringLiteral("Guten Tag")
                    : QStringLiteral("Good afternoon");
}

QString BlopLocale::goodEvening() const {
  return isGerman() ? QStringLiteral("Guten Abend")
                    : QStringLiteral("Good evening");
}
