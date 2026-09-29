#include "dashboardlayoutstore.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace {
QString settingsKey() { return QStringLiteral("dashboard/layout_v7"); }

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
  return qBound(1, rowSpan, 5);
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
  case DashSizeClass::Title:
    return 12;
  }
  return 6;
}

int DashboardLayoutStore::rowSpanFor(DashSizeClass c) {
  switch (c) {
  case DashSizeClass::Title:
    return 1;
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
  if (rs <= 1)
    return DashSizeClass::Title;
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
  if (s == QLatin1String("Title"))
    return DashSizeClass::Title;
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
  case DashSizeClass::Title:
    return QStringLiteral("Title");
  }
  return QStringLiteral("M");
}

QStringList DashboardLayoutStore::knownIds() {
  return {QStringLiteral("intro"), QStringLiteral("today"),
          QStringLiteral("todos"), QStringLiteral("calendar"),
          QStringLiteral("recent"), QStringLiteral("shortcuts")};
}

bool DashboardLayoutStore::isBannerId(const QString &id) {
  return id == QLatin1String("banner") || id.startsWith(QLatin1String("banner_"));
}

bool DashboardLayoutStore::isKnownBlockId(const QString &id) {
  return knownIds().contains(id) || isBannerId(id);
}

QString DashboardLayoutStore::allocateBannerId(
    const QVector<DashboardWidgetSpec> &specs) {
  auto used = [&](const QString &id) {
    for (const auto &s : specs) {
      if (s.id == id)
        return true;
    }
    return false;
  };
  if (!used(QStringLiteral("banner")))
    return QStringLiteral("banner");
  for (int n = 2; n < 64; ++n) {
    const QString id = QStringLiteral("banner_%1").arg(n);
    if (!used(id))
      return id;
  }
  return QStringLiteral("banner_%1").arg(QDateTime::currentMSecsSinceEpoch());
}

QString DashboardLayoutStore::displayName(const QString &id) {
  if (id == QLatin1String("intro"))
    return QStringLiteral("Begrüßung");
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
  if (isBannerId(id))
    return QStringLiteral("Banner");
  return id;
}

DashboardWidgetSpec DashboardLayoutStore::defaultFor(const QString &id) {
  for (const auto &s : defaults()) {
    if (s.id == id)
      return s;
  }
  if (isBannerId(id))
    return make(id, 99, 0, 0, DashSizeClass::L);
  return make(id, 99, 0, 0, DashSizeClass::M);
}

QVector<DashboardWidgetSpec> DashboardLayoutStore::defaults() {
  // Cover → full-width title → aligned 6+6 content row → tasks.
  auto today = make(QStringLiteral("today"), 99, 0, 0, DashSizeClass::M);
  today.visible = false;
  auto shortcuts = make(QStringLiteral("shortcuts"), 99, 0, 0, DashSizeClass::L);
  shortcuts.visible = false;
  return {
      make(QStringLiteral("banner"), 0, 0, 0, DashSizeClass::L),
      make(QStringLiteral("intro"), 1, 2, 0, DashSizeClass::Title),
      today,
      make(QStringLiteral("calendar"), 2, 3, 0, DashSizeClass::Tall),
      make(QStringLiteral("recent"), 3, 3, 6, DashSizeClass::Tall),
      make(QStringLiteral("todos"), 4, 6, 0, DashSizeClass::M),
      shortcuts,
  };
}

QVector<DashboardWidgetSpec> DashboardLayoutStore::load() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  QByteArray raw = st.value(settingsKey()).toByteArray();
  // One-shot migrate from v6 if v7 empty.
  if (raw.isEmpty())
    raw = st.value(QStringLiteral("dashboard/layout_v6")).toByteArray();
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
    s.bgEnabled = o.value(QStringLiteral("bgEnabled")).toBool(true);
    s.borderEnabled = o.value(QStringLiteral("borderEnabled")).toBool(true);
    s.bgColor = o.value(QStringLiteral("bgColor")).toString();
    s.borderColor = o.value(QStringLiteral("borderColor")).toString();
    const int cs = colSpanFor(s.sizeClass);
    if (s.col + cs > 12)
      s.col = qMax(0, 12 - cs);
    if (!s.id.isEmpty() && isKnownBlockId(s.id))
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
      if (id == QLatin1String("intro")) {
        // Prefer defaults placement (under cover); fall back to top strip.
        s = defaultFor(QStringLiteral("intro"));
        s.visible = true;
        bool hasBanner = false;
        for (const auto &x : out) {
          if (isBannerId(x.id) && x.visible) {
            hasBanner = true;
            break;
          }
        }
        if (!hasBanner) {
          s.row = 0;
          s.col = 0;
          s.sizeClass = DashSizeClass::Title;
          const int shift = rowSpanFor(s.sizeClass);
          for (auto &x : out)
            x.row += shift;
          out.prepend(s);
        } else {
          out.append(s);
        }
      } else {
        s.visible = false;
        int maxBottom = 0;
        for (const auto &x : out)
          maxBottom = qMax(maxBottom, x.row + rowSpanFor(x.sizeClass));
        s.row = maxBottom;
        s.col = 0;
        out.append(s);
      }
    }
  }

  // One-shot: Notion stack — cover → title → dense content (fixes plump vertical stack).
  {
    QSettings mig(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    constexpr const char *kNotionStack = "dashboard/layout.notionStackV2";
    // Stamp legacy banner migrations so they never re-inflate the cover.
    mig.setValue(QStringLiteral("dashboard/layout.ensureBannerV1"), true);
    mig.setValue(QStringLiteral("dashboard/layout.bannerHeroV1"), true);
    mig.setValue(QStringLiteral("dashboard/layout.bannerShorterV1"), true);

    if (!mig.value(QLatin1String(kNotionStack)).toBool()) {
      mig.setValue(QLatin1String(kNotionStack), true);

      QHash<QString, bool> visibility;
      for (const auto &s : out)
        visibility.insert(s.id, s.visible);

      QVector<DashboardWidgetSpec> extras;
      for (const auto &s : out) {
        if (isBannerId(s.id) && s.id != QLatin1String("banner"))
          extras.append(s);
      }

      out = defaults();
      for (auto &s : out) {
        if (visibility.contains(s.id))
          s.visible = visibility.value(s.id);
        if (s.id == QLatin1String("intro") || s.id == QLatin1String("banner"))
          s.visible = true;
      }
      int maxBottom = 0;
      for (const auto &x : out)
        maxBottom = qMax(maxBottom, x.row + rowSpanFor(x.sizeClass));
      for (DashboardWidgetSpec extra : extras) {
        extra.row = maxBottom;
        extra.col = 0;
        out.append(extra);
        maxBottom += rowSpanFor(extra.sizeClass);
      }

      QJsonArray arr;
      for (int i = 0; i < out.size(); ++i) {
        const auto &s = out[i];
        QJsonObject o;
        o.insert(QStringLiteral("id"), s.id);
        o.insert(QStringLiteral("visible"), s.visible);
        o.insert(QStringLiteral("order"), i);
        o.insert(QStringLiteral("row"), s.row);
        o.insert(QStringLiteral("col"), s.col);
        o.insert(QStringLiteral("sizeClass"), sizeClassToString(s.sizeClass));
        o.insert(QStringLiteral("bgEnabled"), s.bgEnabled);
        o.insert(QStringLiteral("borderEnabled"), s.borderEnabled);
        if (!s.bgColor.isEmpty())
          o.insert(QStringLiteral("bgColor"), s.bgColor);
        if (!s.borderColor.isEmpty())
          o.insert(QStringLiteral("borderColor"), s.borderColor);
        arr.append(o);
      }
      mig.setValue(settingsKey(),
                   QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }
  }

  // One-shot: hide redundant Heute when Kalender or Aufgaben already on board.
  {
    QSettings mig(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    constexpr const char *kHideToday = "dashboard/layout.hideTodayV1";
    if (!mig.value(QLatin1String(kHideToday)).toBool()) {
      mig.setValue(QLatin1String(kHideToday), true);
      bool hasCalOrTodos = false;
      for (const auto &s : out) {
        if (s.visible && (s.id == QLatin1String("calendar") ||
                          s.id == QLatin1String("todos"))) {
          hasCalOrTodos = true;
          break;
        }
      }
      if (hasCalOrTodos) {
        for (auto &s : out) {
          if (s.id == QLatin1String("today"))
            s.visible = false;
        }
        QJsonArray arr;
        for (int i = 0; i < out.size(); ++i) {
          const auto &s = out[i];
          QJsonObject o;
          o.insert(QStringLiteral("id"), s.id);
          o.insert(QStringLiteral("visible"), s.visible);
          o.insert(QStringLiteral("order"), i);
          o.insert(QStringLiteral("row"), s.row);
          o.insert(QStringLiteral("col"), s.col);
          o.insert(QStringLiteral("sizeClass"), sizeClassToString(s.sizeClass));
          o.insert(QStringLiteral("bgEnabled"), s.bgEnabled);
          o.insert(QStringLiteral("borderEnabled"), s.borderEnabled);
          if (!s.bgColor.isEmpty())
            o.insert(QStringLiteral("bgColor"), s.bgColor);
          if (!s.borderColor.isEmpty())
            o.insert(QStringLiteral("borderColor"), s.borderColor);
          arr.append(o);
        }
        mig.setValue(settingsKey(),
                     QJsonDocument(arr).toJson(QJsonDocument::Compact));
      }
    }
  }

  // Stale one-shot — do NOT hide the cover banner (that was a mistaken change).
  // Keep the key stamped so older builds don't re-run a destructive hide.
  {
    QSettings mig(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    mig.setValue(QStringLiteral("dashboard/layout.hideBannerStripV1"), true);
  }

  // Undo mistaken hideBannerStripV1 — restore cover banner for users who lost it.
  {
    QSettings mig(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    constexpr const char *kRestore =
        "dashboard/layout.restoreBannerAfterHideV1";
    if (!mig.value(QLatin1String(kRestore)).toBool()) {
      mig.setValue(QLatin1String(kRestore), true);
      bool hasVisibleBanner = false;
      DashboardWidgetSpec *primary = nullptr;
      for (auto &s : out) {
        if (!isBannerId(s.id))
          continue;
        if (s.id == QLatin1String("banner"))
          primary = &s;
        if (s.visible)
          hasVisibleBanner = true;
      }
      if (!hasVisibleBanner) {
        if (!primary) {
          auto s = defaultFor(QStringLiteral("banner"));
          s.visible = true;
          s.row = 0;
          s.col = 0;
          const int shift = rowSpanFor(s.sizeClass);
          for (auto &x : out)
            x.row += shift;
          out.prepend(s);
        } else {
          primary->visible = true;
          primary->row = 0;
          primary->col = 0;
          primary->sizeClass = DashSizeClass::L;
          const int shift = rowSpanFor(primary->sizeClass);
          for (auto &x : out) {
            if (&x == primary)
              continue;
            if (x.visible)
              x.row += shift;
          }
        }
        QJsonArray arr;
        for (int i = 0; i < out.size(); ++i) {
          const auto &s = out[i];
          QJsonObject o;
          o.insert(QStringLiteral("id"), s.id);
          o.insert(QStringLiteral("visible"), s.visible);
          o.insert(QStringLiteral("order"), i);
          o.insert(QStringLiteral("row"), s.row);
          o.insert(QStringLiteral("col"), s.col);
          o.insert(QStringLiteral("sizeClass"), sizeClassToString(s.sizeClass));
          o.insert(QStringLiteral("bgEnabled"), s.bgEnabled);
          o.insert(QStringLiteral("borderEnabled"), s.borderEnabled);
          if (!s.bgColor.isEmpty())
            o.insert(QStringLiteral("bgColor"), s.bgColor);
          if (!s.borderColor.isEmpty())
            o.insert(QStringLiteral("borderColor"), s.borderColor);
          arr.append(o);
        }
        mig.setValue(settingsKey(),
                     QJsonDocument(arr).toJson(QJsonDocument::Compact));
      }
    }
  }

  // One-shot: Notion rhythm — intro full width, calendar+recent as 6+6.
  {
    QSettings mig(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    constexpr const char *kRhythm = "dashboard/layout.gridRhythmV1";
    if (!mig.value(QLatin1String(kRhythm)).toBool()) {
      mig.setValue(QLatin1String(kRhythm), true);
      QHash<QString, bool> visibility;
      QHash<QString, DashboardWidgetSpec> chrome;
      for (const auto &s : out) {
        visibility.insert(s.id, s.visible);
        chrome.insert(s.id, s);
      }
      QVector<DashboardWidgetSpec> extras;
      for (const auto &s : out) {
        if (isBannerId(s.id) && s.id != QLatin1String("banner"))
          extras.append(s);
      }
      out = defaults();
      for (auto &s : out) {
        if (visibility.contains(s.id))
          s.visible = visibility.value(s.id);
        if (chrome.contains(s.id)) {
          const auto &c = chrome.value(s.id);
          s.bgEnabled = c.bgEnabled;
          s.borderEnabled = c.borderEnabled;
          s.bgColor = c.bgColor;
          s.borderColor = c.borderColor;
        }
        if (s.id == QLatin1String("intro") || s.id == QLatin1String("banner"))
          s.visible = true;
        if (s.id == QLatin1String("today") || s.id == QLatin1String("shortcuts"))
          s.visible = false;
      }
      int maxBottom = 0;
      for (const auto &x : out) {
        if (!x.visible)
          continue;
        maxBottom = qMax(maxBottom, x.row + rowSpanFor(x.sizeClass));
      }
      for (DashboardWidgetSpec extra : extras) {
        extra.row = maxBottom;
        extra.col = 0;
        extra.visible = true;
        out.append(extra);
        maxBottom += rowSpanFor(extra.sizeClass);
      }
      QJsonArray arr;
      for (int i = 0; i < out.size(); ++i) {
        const auto &s = out[i];
        QJsonObject o;
        o.insert(QStringLiteral("id"), s.id);
        o.insert(QStringLiteral("visible"), s.visible);
        o.insert(QStringLiteral("order"), i);
        o.insert(QStringLiteral("row"), s.row);
        o.insert(QStringLiteral("col"), s.col);
        o.insert(QStringLiteral("sizeClass"), sizeClassToString(s.sizeClass));
        o.insert(QStringLiteral("bgEnabled"), s.bgEnabled);
        o.insert(QStringLiteral("borderEnabled"), s.borderEnabled);
        if (!s.bgColor.isEmpty())
          o.insert(QStringLiteral("bgColor"), s.bgColor);
        if (!s.borderColor.isEmpty())
          o.insert(QStringLiteral("borderColor"), s.borderColor);
        arr.append(o);
      }
      mig.setValue(settingsKey(),
                   QJsonDocument(arr).toJson(QJsonDocument::Compact));
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
    o.insert(QStringLiteral("bgEnabled"), s.bgEnabled);
    o.insert(QStringLiteral("borderEnabled"), s.borderEnabled);
    if (!s.bgColor.isEmpty())
      o.insert(QStringLiteral("bgColor"), s.bgColor);
    if (!s.borderColor.isEmpty())
      o.insert(QStringLiteral("borderColor"), s.borderColor);
    arr.append(o);
  }
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  st.setValue(settingsKey(),
              QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void DashboardLayoutStore::reset() { save(defaults()); }
