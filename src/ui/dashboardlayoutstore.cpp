#include "dashboardlayoutstore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace {
QString settingsKey() { return QStringLiteral("dashboard/layout_v6"); }

DashboardWidgetSpec make(const QString &id, int order, int row, int col,
                         DashSizeClass size) {
  DashboardWidgetSpec s;
  s.id = id;
  s.visible = true;
  s.order = order;
  s.row = row;
  s.col = col;
  s.sizeClass = size;
  return s;
}
} // namespace

int DashboardLayoutStore::snapColSpan(int colSpan) {
  // True quarters: 3 + 9 = 12, 6 + 6 = 12, 12 = full.
  if (colSpan <= 4)
    return 3;
  if (colSpan <= 7)
    return 6;
  if (colSpan <= 10)
    return 9;
  return 12;
}

int DashboardLayoutStore::snapRowSpan(int rowSpan) {
  return qBound(2, rowSpan, 5);
}

int DashboardLayoutStore::colSpanFor(DashSizeClass c) {
  switch (c) {
  case DashSizeClass::S:
  case DashSizeClass::S3:
  case DashSizeClass::S4:
    return 3;
  case DashSizeClass::M:
  case DashSizeClass::Tall:
  case DashSizeClass::M4:
  case DashSizeClass::M5:
    return 6;
  case DashSizeClass::Q3:
  case DashSizeClass::Q3T:
  case DashSizeClass::Q3H:
  case DashSizeClass::Q3X:
    return 9;
  case DashSizeClass::L:
  case DashSizeClass::XL:
  case DashSizeClass::Hero:
  case DashSizeClass::L5:
    return 12;
  }
  return 6;
}

int DashboardLayoutStore::rowSpanFor(DashSizeClass c) {
  switch (c) {
  case DashSizeClass::S:
  case DashSizeClass::M:
  case DashSizeClass::Q3:
  case DashSizeClass::L:
    return 2;
  case DashSizeClass::S3:
  case DashSizeClass::Tall:
  case DashSizeClass::Q3T:
  case DashSizeClass::XL:
    return 3;
  case DashSizeClass::S4:
  case DashSizeClass::M4:
  case DashSizeClass::Q3H:
  case DashSizeClass::Hero:
    return 4;
  case DashSizeClass::M5:
  case DashSizeClass::Q3X:
  case DashSizeClass::L5:
    return 5;
  }
  return 2;
}

DashSizeClass DashboardLayoutStore::fromSpans(int colSpan, int rowSpan) {
  const int cs = snapColSpan(colSpan);
  const int rs = snapRowSpan(rowSpan);
  if (cs == 3) {
    if (rs <= 2)
      return DashSizeClass::S;
    if (rs == 3)
      return DashSizeClass::S3;
    return DashSizeClass::S4;
  }
  if (cs == 6) {
    if (rs <= 2)
      return DashSizeClass::M;
    if (rs == 3)
      return DashSizeClass::Tall;
    if (rs == 4)
      return DashSizeClass::M4;
    return DashSizeClass::M5;
  }
  if (cs == 9) {
    if (rs <= 2)
      return DashSizeClass::Q3;
    if (rs == 3)
      return DashSizeClass::Q3T;
    if (rs == 4)
      return DashSizeClass::Q3H;
    return DashSizeClass::Q3X;
  }
  if (rs <= 2)
    return DashSizeClass::L;
  if (rs == 3)
    return DashSizeClass::XL;
  if (rs == 4)
    return DashSizeClass::Hero;
  return DashSizeClass::L5;
}

DashSizeClass DashboardLayoutStore::sizeClassFromString(const QString &s) {
  if (s == QLatin1String("S"))
    return DashSizeClass::S;
  if (s == QLatin1String("S3"))
    return DashSizeClass::S3;
  if (s == QLatin1String("S4"))
    return DashSizeClass::S4;
  if (s == QLatin1String("Tall"))
    return DashSizeClass::Tall;
  if (s == QLatin1String("M4"))
    return DashSizeClass::M4;
  if (s == QLatin1String("M5"))
    return DashSizeClass::M5;
  if (s == QLatin1String("Q3"))
    return DashSizeClass::Q3;
  if (s == QLatin1String("Q3T"))
    return DashSizeClass::Q3T;
  if (s == QLatin1String("Q3H"))
    return DashSizeClass::Q3H;
  if (s == QLatin1String("Q3X"))
    return DashSizeClass::Q3X;
  if (s == QLatin1String("L"))
    return DashSizeClass::L;
  if (s == QLatin1String("XL"))
    return DashSizeClass::XL;
  if (s == QLatin1String("Hero"))
    return DashSizeClass::Hero;
  if (s == QLatin1String("L5"))
    return DashSizeClass::L5;
  return DashSizeClass::M;
}

QString DashboardLayoutStore::sizeClassToString(DashSizeClass c) {
  switch (c) {
  case DashSizeClass::S:
    return QStringLiteral("S");
  case DashSizeClass::S3:
    return QStringLiteral("S3");
  case DashSizeClass::S4:
    return QStringLiteral("S4");
  case DashSizeClass::M:
    return QStringLiteral("M");
  case DashSizeClass::Tall:
    return QStringLiteral("Tall");
  case DashSizeClass::M4:
    return QStringLiteral("M4");
  case DashSizeClass::M5:
    return QStringLiteral("M5");
  case DashSizeClass::Q3:
    return QStringLiteral("Q3");
  case DashSizeClass::Q3T:
    return QStringLiteral("Q3T");
  case DashSizeClass::Q3H:
    return QStringLiteral("Q3H");
  case DashSizeClass::Q3X:
    return QStringLiteral("Q3X");
  case DashSizeClass::L:
    return QStringLiteral("L");
  case DashSizeClass::XL:
    return QStringLiteral("XL");
  case DashSizeClass::Hero:
    return QStringLiteral("Hero");
  case DashSizeClass::L5:
    return QStringLiteral("L5");
  }
  return QStringLiteral("M");
}

QStringList DashboardLayoutStore::knownIds() {
  return {QStringLiteral("today"), QStringLiteral("todos"),
          QStringLiteral("calendar"), QStringLiteral("recent"),
          QStringLiteral("shortcuts")};
}

QString DashboardLayoutStore::displayName(const QString &id) {
  if (id == QLatin1String("today"))
    return QStringLiteral("Heute");
  if (id == QLatin1String("todos"))
    return QStringLiteral("Aufgaben");
  if (id == QLatin1String("calendar"))
    return QStringLiteral("Kalender");
  if (id == QLatin1String("recent"))
    return QStringLiteral("Zuletzt");
  if (id == QLatin1String("shortcuts"))
    return QStringLiteral("Schnellzugriff");
  return id;
}

DashboardWidgetSpec DashboardLayoutStore::defaultFor(const QString &id) {
  for (const auto &s : defaults()) {
    if (s.id == id)
      return s;
  }
  return make(id, 99, 0, 0, DashSizeClass::M);
}

QVector<DashboardWidgetSpec> DashboardLayoutStore::defaults() {
  return {
      make(QStringLiteral("today"), 0, 0, 0, DashSizeClass::M),
      make(QStringLiteral("todos"), 1, 0, 6, DashSizeClass::M),
      make(QStringLiteral("calendar"), 2, 2, 0, DashSizeClass::Tall),
      make(QStringLiteral("recent"), 3, 2, 6, DashSizeClass::Tall),
      make(QStringLiteral("shortcuts"), 4, 5, 0, DashSizeClass::L),
  };
}

QVector<DashboardWidgetSpec> DashboardLayoutStore::load() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QByteArray raw = st.value(settingsKey()).toByteArray();
  if (raw.isEmpty())
    return defaults();

  const QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isArray())
    return defaults();

  QVector<DashboardWidgetSpec> out;
  for (const QJsonValue &v : doc.array()) {
    const QJsonObject o = v.toObject();
    DashboardWidgetSpec s;
    s.id = o.value(QStringLiteral("id")).toString();
    s.visible = o.value(QStringLiteral("visible")).toBool(true);
    s.order = o.value(QStringLiteral("order")).toInt(out.size());
    s.row = qBound(0, o.value(QStringLiteral("row")).toInt(0), 24);
    s.col = qBound(0, o.value(QStringLiteral("col")).toInt(0), 11);
    s.sizeClass = sizeClassFromString(
        o.value(QStringLiteral("sizeClass")).toString(QStringLiteral("M")));
    const int cs = colSpanFor(s.sizeClass);
    if (s.col + cs > 12)
      s.col = qMax(0, 12 - cs);
    if (!s.id.isEmpty() && knownIds().contains(s.id))
      out.append(s);
  }

  for (const QString &id : knownIds()) {
    bool found = false;
    for (const auto &s : out) {
      if (s.id == id) {
        found = true;
        break;
      }
    }
    if (!found) {
      auto s = defaultFor(id);
      s.visible = false;
      int maxBottom = 0;
      for (const auto &x : out)
        maxBottom = qMax(maxBottom, x.row + rowSpanFor(x.sizeClass));
      s.row = maxBottom;
      s.col = 0;
      out.append(s);
    }
  }

  std::sort(out.begin(), out.end(),
            [](const DashboardWidgetSpec &a, const DashboardWidgetSpec &b) {
              if (a.row != b.row)
                return a.row < b.row;
              if (a.col != b.col)
                return a.col < b.col;
              return a.order < b.order;
            });
  return out;
}

void DashboardLayoutStore::save(const QVector<DashboardWidgetSpec> &specs) {
  QJsonArray arr;
  for (int i = 0; i < specs.size(); ++i) {
    const auto &s = specs[i];
    QJsonObject o;
    o.insert(QStringLiteral("id"), s.id);
    o.insert(QStringLiteral("visible"), s.visible);
    o.insert(QStringLiteral("order"), i);
    o.insert(QStringLiteral("row"), s.row);
    o.insert(QStringLiteral("col"), s.col);
    o.insert(QStringLiteral("sizeClass"), sizeClassToString(s.sizeClass));
    arr.append(o);
  }
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(settingsKey(),
              QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void DashboardLayoutStore::reset() { save(defaults()); }
