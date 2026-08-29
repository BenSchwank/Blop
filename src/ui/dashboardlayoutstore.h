#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct DashboardWidgetSpec {
  QString id;
  bool visible{true};
  int order{0};
  /// 12-column grid placement (Notion-style blocks).
  int row{0};
  int col{0};
  int colSpan{6};
  int rowSpan{1};
  /// Per-widget content density (0 = default).
  int itemLimit{0};
};

/// Persists dashboard block visibility, grid layout, and content limits.
class DashboardLayoutStore {
public:
  static QVector<DashboardWidgetSpec> defaults();
  static QVector<DashboardWidgetSpec> load();
  static void save(const QVector<DashboardWidgetSpec> &specs);
  static void reset();

  static QStringList knownIds();
  static QString displayName(const QString &id);
  static DashboardWidgetSpec defaultFor(const QString &id);

  static int itemLimitFor(const QString &id, int fallback);
};
