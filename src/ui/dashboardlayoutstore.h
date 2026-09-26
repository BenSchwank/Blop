#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/// Discrete footprints on a 12-col grid: 3+9 / 6+6 / 12.
enum class DashSizeClass {
  S = 0,     // 3×2  (1/4)
  S3 = 1,    // 3×3
  S4 = 2,    // 3×4
  M = 3,     // 6×2  (1/2)
  Tall = 4,  // 6×3
  M4 = 5,    // 6×4
  M5 = 6,    // 6×5
  Q3 = 7,    // 9×2  (3/4)
  Q3T = 8,   // 9×3
  Q3H = 9,   // 9×4
  Q3X = 10,  // 9×5
  L = 11,    // 12×2 (full)
  XL = 12,   // 12×3
  Hero = 13, // 12×4
  L5 = 14,   // 12×5
};

struct DashboardWidgetSpec {
  QString id;
  bool visible{true};
  int order{0};
  int row{0};
  int col{0};
  DashSizeClass sizeClass{DashSizeClass::M};
};

class DashboardLayoutStore {
public:
  static QVector<DashboardWidgetSpec> defaults();
  static QVector<DashboardWidgetSpec> load();
  static void save(const QVector<DashboardWidgetSpec> &specs);
  static void reset();

  static QStringList knownIds();
  static QString displayName(const QString &id);
  static DashboardWidgetSpec defaultFor(const QString &id);

  static int colSpanFor(DashSizeClass c);
  static int rowSpanFor(DashSizeClass c);
  static DashSizeClass sizeClassFromString(const QString &s);
  static QString sizeClassToString(DashSizeClass c);
  static DashSizeClass fromSpans(int colSpan, int rowSpan);
  static int snapColSpan(int colSpan);
  static int snapRowSpan(int rowSpan);
};
