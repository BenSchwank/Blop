#include "dashboardlayoutstore.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace {
QString settingsKey() { return QStringLiteral("dashboard/layout_v5"); }

DashboardWidgetSpec make(const QString &id, int order, int row, int col,
                         int colSpan, int rowSpan, int itemLimit = 0) {
  DashboardWidgetSpec s;
  s.id = id;
  s.visible = true;
  s.order = order;
  s.row = row;
  s.col = col;
  s.colSpan = colSpan;
  s.rowSpan = rowSpan;
  s.itemLimit = itemLimit;
  return s;
}

QVector<DashboardWidgetSpec> migrateFromV4() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QJsonDocument doc = QJsonDocument::fromJson(
      st.value(QStringLiteral("dashboard/layout_v4")).toByteArray());
  if (!doc.isArray())
    return {};
  auto out = DashboardLayoutStore::defaults();
  for (const QJsonValue &value : doc.array()) {
    const QJsonObject old = value.toObject();
    const QString id = old.value(QStringLiteral("id")).toString();
    for (auto &spec : out) {
      if (spec.id != id)
        continue;
      spec.visible = old.value(QStringLiteral("visible")).toBool(spec.visible);
      spec.itemLimit = old.value(QStringLiteral("itemLimit")).toInt(spec.itemLimit);
      break;
    }
  }
  return out;
}

QVector<DashboardWidgetSpec> migrateFromV1() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QByteArray raw =
      st.value(QStringLiteral("dashboard/layout_v1")).toByteArray();
  if (raw.isEmpty())
    return {};
  const QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isArray())
    return {};

  QVector<DashboardWidgetSpec> out = DashboardLayoutStore::defaults();
  QHash<QString, bool> vis;
  QHash<QString, int> ord;
  for (const QJsonValue &v : doc.array()) {
    const QJsonObject o = v.toObject();
    const QString id = o.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
      continue;
    vis.insert(id, o.value(QStringLiteral("visible")).toBool(true));
    ord.insert(id, o.value(QStringLiteral("order")).toInt(ord.size()));
  }
  for (auto &s : out) {
    if (vis.contains(s.id))
      s.visible = vis.value(s.id);
    if (ord.contains(s.id))
      s.order = ord.value(s.id);
  }
  std::sort(out.begin(), out.end(),
            [](const DashboardWidgetSpec &a, const DashboardWidgetSpec &b) {
              return a.order < b.order;
            });
  int row = 0;
  for (auto &s : out) {
    if (!s.visible)
      continue;
    s.row = row++;
    s.col = 0;
    s.colSpan = 12;
    s.rowSpan = 1;
  }
  return out;
}
} // namespace

QStringList DashboardLayoutStore::knownIds() {
  return {QStringLiteral("greeting"), QStringLiteral("today"),
          QStringLiteral("capture"),  QStringLiteral("projects"),
          QStringLiteral("clock"),    QStringLiteral("todos"),
          QStringLiteral("calendar"), QStringLiteral("recent"),
          QStringLiteral("shortcuts"), QStringLiteral("actions")};
}

QString DashboardLayoutStore::displayName(const QString &id) {
  if (id == QLatin1String("greeting"))
    return QStringLiteral("Begrüßung");
  if (id == QLatin1String("today"))
    return QStringLiteral("Heute im Fokus");
  if (id == QLatin1String("capture"))
    return QStringLiteral("Schnellerfassung");
  if (id == QLatin1String("projects"))
    return QStringLiteral("Projekte");
  if (id == QLatin1String("clock"))
    return QStringLiteral("Uhr");
  if (id == QLatin1String("todos"))
    return QStringLiteral("Aufgaben");
  if (id == QLatin1String("calendar"))
    return QStringLiteral("Kalender");
  if (id == QLatin1String("recent"))
    return QStringLiteral("Zuletzt");
  if (id == QLatin1String("shortcuts"))
    return QStringLiteral("Schnellzugriff");
  if (id == QLatin1String("actions"))
    return QStringLiteral("Schnellaktionen");
  return id;
}

DashboardWidgetSpec DashboardLayoutStore::defaultFor(const QString &id) {
  for (const auto &s : defaults()) {
    if (s.id == id)
      return s;
  }
  return make(id, 99, 0, 0, 12, 1);
}

QVector<DashboardWidgetSpec> DashboardLayoutStore::defaults() {
  // Notion-style home: two tall columns, then clock + recent, then shortcuts.
  return {
      make(QStringLiteral("greeting"), 0, 0, 0, 12, 1),
      make(QStringLiteral("today"), 1, 0, 0, 8, 2),
      make(QStringLiteral("capture"), 2, 0, 8, 4, 2),
      make(QStringLiteral("todos"), 3, 2, 0, 6, 3),
      make(QStringLiteral("calendar"), 4, 2, 6, 6, 3, 8),
      make(QStringLiteral("projects"), 5, 5, 0, 5, 2),
      make(QStringLiteral("recent"), 6, 5, 5, 7, 2, 5),
      make(QStringLiteral("shortcuts"), 7, 7, 0, 12, 2),
      make(QStringLiteral("clock"), 8, 9, 0, 4, 2),
  };
}

int DashboardLayoutStore::itemLimitFor(const QString &id, int fallback) {
  for (const auto &s : load()) {
    if (s.id == id && s.itemLimit > 0)
      return s.itemLimit;
  }
  return fallback;
}

QVector<DashboardWidgetSpec> DashboardLayoutStore::load() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QByteArray raw = st.value(settingsKey()).toByteArray();
  if (raw.isEmpty()) {
    auto migrated = migrateFromV4();
    if (migrated.isEmpty())
      migrated = migrateFromV1();
    if (!migrated.isEmpty()) {
      save(migrated);
      return migrated;
    }
    return defaults();
  }

  const QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isArray())
    return defaults();

  QVector<DashboardWidgetSpec> out;
  const QJsonArray arr = doc.array();
  for (const QJsonValue &v : arr) {
    const QJsonObject o = v.toObject();
    DashboardWidgetSpec s;
    s.id = o.value(QStringLiteral("id")).toString();
    s.visible = o.value(QStringLiteral("visible")).toBool(true);
    s.order = o.value(QStringLiteral("order")).toInt(out.size());
    s.row = qBound(0, o.value(QStringLiteral("row")).toInt(0), 24);
    s.col = qBound(0, o.value(QStringLiteral("col")).toInt(0), 11);
    s.colSpan = qBound(1, o.value(QStringLiteral("colSpan")).toInt(6), 12);
    s.rowSpan = qBound(1, o.value(QStringLiteral("rowSpan")).toInt(1), 4);
    if (s.col + s.colSpan > 12)
      s.col = qMax(0, 12 - s.colSpan);
    s.itemLimit = o.value(QStringLiteral("itemLimit")).toInt(0);
    if (!s.id.isEmpty())
      out.append(s);
  }

  const QStringList known = knownIds();
  for (const QString &id : known) {
    bool found = false;
    for (const auto &s : out) {
      if (s.id == id) {
        found = true;
        break;
      }
    }
    if (!found) {
      DashboardWidgetSpec s = defaultFor(id);
      int maxBottom = 0;
      for (const auto &x : out)
        maxBottom = qMax(maxBottom, x.row + x.rowSpan);
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
  int firstGridRow = 24;
  for (const auto &s : out) {
    if (s.visible && s.id != QLatin1String("greeting") &&
        s.id != QLatin1String("actions"))
      firstGridRow = qMin(firstGridRow, s.row);
  }
  if (firstGridRow > 0 && firstGridRow < 24) {
    for (auto &s : out) {
      if (s.id != QLatin1String("greeting") && s.id != QLatin1String("actions"))
        s.row = qMax(0, s.row - firstGridRow);
    }
  }
  for (auto &s : out) {
    if (s.id == QLatin1String("actions"))
      s.visible = false;
    if (s.id == QLatin1String("greeting"))
      s.visible = true;
  }
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
    o.insert(QStringLiteral("colSpan"), s.colSpan);
    o.insert(QStringLiteral("rowSpan"), s.rowSpan);
    o.insert(QStringLiteral("itemLimit"), s.itemLimit);
    arr.append(o);
  }
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(settingsKey(),
              QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void DashboardLayoutStore::reset() { save(defaults()); }
