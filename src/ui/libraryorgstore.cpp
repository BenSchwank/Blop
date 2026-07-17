#include "libraryorgstore.h"

#include <QDateTime>
#include <QHash>
#include <QSettings>
#include <QVariant>
#include <algorithm>

namespace {
constexpr const char *kOrg = "Blop";
constexpr const char *kApp = "BlopApp";
constexpr const char *kFavKey = "library/favorites";
constexpr const char *kColorKey = "library/color_labels";
constexpr const char *kRecentKey = "library/recent_opens";

QStringList loadStringList(QSettings &s, const char *key) {
  return s.value(QString::fromLatin1(key)).toStringList();
}

void saveStringList(QSettings &s, const char *key, QStringList list) {
  list.removeAll(QString());
  list.removeDuplicates();
  s.setValue(QString::fromLatin1(key), list);
}

QHash<QString, int> loadColorMap(QSettings &s) {
  QHash<QString, int> map;
  const QVariantMap raw = s.value(QString::fromLatin1(kColorKey)).toMap();
  for (auto it = raw.constBegin(); it != raw.constEnd(); ++it)
    map.insert(it.key(), it.value().toInt());
  return map;
}

void saveColorMap(QSettings &s, const QHash<QString, int> &map) {
  QVariantMap raw;
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    if (it.value() > 0)
      raw.insert(it.key(), it.value());
  }
  s.setValue(QString::fromLatin1(kColorKey), raw);
}

struct RecentEntry {
  QString path;
  qint64 ts{0};
};

QVector<RecentEntry> loadRecent(QSettings &s) {
  QVector<RecentEntry> out;
  const QVariantList raw = s.value(QString::fromLatin1(kRecentKey)).toList();
  for (const QVariant &v : raw) {
    const QVariantMap m = v.toMap();
    RecentEntry e;
    e.path = m.value(QStringLiteral("path")).toString();
    e.ts = m.value(QStringLiteral("ts")).toLongLong();
    if (!e.path.isEmpty())
      out.append(e);
  }
  return out;
}

void saveRecent(QSettings &s, const QVector<RecentEntry> &entries) {
  QVariantList raw;
  for (const RecentEntry &e : entries) {
    QVariantMap m;
    m.insert(QStringLiteral("path"), e.path);
    m.insert(QStringLiteral("ts"), e.ts);
    raw.append(m);
  }
  s.setValue(QString::fromLatin1(kRecentKey), raw);
}
} // namespace

bool LibraryOrgStore::isFavorite(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return false;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  return loadStringList(s, kFavKey).contains(absolutePath);
}

void LibraryOrgStore::setFavorite(const QString &absolutePath, bool favorite) {
  if (absolutePath.isEmpty())
    return;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  QStringList favs = loadStringList(s, kFavKey);
  favs.removeAll(absolutePath);
  if (favorite)
    favs.prepend(absolutePath);
  saveStringList(s, kFavKey, favs);
}

void LibraryOrgStore::toggleFavorite(const QString &absolutePath) {
  setFavorite(absolutePath, !isFavorite(absolutePath));
}

QStringList LibraryOrgStore::favoritePaths() {
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  return loadStringList(s, kFavKey);
}

LibraryOrgStore::ColorLabel LibraryOrgStore::colorLabel(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return ColorLabel::None;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  const int v = loadColorMap(s).value(absolutePath, 0);
  if (v < 0 || v > int(ColorLabel::Slate))
    return ColorLabel::None;
  return static_cast<ColorLabel>(v);
}

void LibraryOrgStore::setColorLabel(const QString &absolutePath, ColorLabel label) {
  if (absolutePath.isEmpty())
    return;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  auto map = loadColorMap(s);
  if (label == ColorLabel::None)
    map.remove(absolutePath);
  else
    map.insert(absolutePath, int(label));
  saveColorMap(s, map);
}

QColor LibraryOrgStore::colorForLabel(ColorLabel label) {
  switch (label) {
  case ColorLabel::Rose:
    return QColor(QStringLiteral("#F07178"));
  case ColorLabel::Amber:
    return QColor(QStringLiteral("#E6B450"));
  case ColorLabel::Lime:
    return QColor(QStringLiteral("#7FD962"));
  case ColorLabel::Sky:
    return QColor(QStringLiteral("#59C2FF"));
  case ColorLabel::Violet:
    return QColor(QStringLiteral("#C3A6FF"));
  case ColorLabel::Slate:
    return QColor(QStringLiteral("#8A9199"));
  case ColorLabel::None:
  default:
    return QColor();
  }
}

QString LibraryOrgStore::labelDisplayName(ColorLabel label) {
  switch (label) {
  case ColorLabel::Rose:
    return QStringLiteral("Rosa");
  case ColorLabel::Amber:
    return QStringLiteral("Amber");
  case ColorLabel::Lime:
    return QStringLiteral("Lime");
  case ColorLabel::Sky:
    return QStringLiteral("Himmel");
  case ColorLabel::Violet:
    return QStringLiteral("Violett");
  case ColorLabel::Slate:
    return QStringLiteral("Schiefer");
  case ColorLabel::None:
  default:
    return QStringLiteral("Keine");
  }
}

void LibraryOrgStore::touchRecent(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  auto entries = loadRecent(s);
  for (int i = entries.size() - 1; i >= 0; --i) {
    if (entries[i].path == absolutePath)
      entries.removeAt(i);
  }
  RecentEntry e;
  e.path = absolutePath;
  e.ts = QDateTime::currentMSecsSinceEpoch();
  entries.prepend(e);
  while (entries.size() > 48)
    entries.removeLast();
  saveRecent(s, entries);
}

QStringList LibraryOrgStore::recentPaths(int limit) {
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  auto entries = loadRecent(s);
  std::sort(entries.begin(), entries.end(),
            [](const RecentEntry &a, const RecentEntry &b) { return a.ts > b.ts; });
  QStringList out;
  for (const RecentEntry &e : entries) {
    out.append(e.path);
    if (out.size() >= limit)
      break;
  }
  return out;
}

void LibraryOrgStore::remapPath(const QString &fromPath, const QString &toPath) {
  if (fromPath.isEmpty() || toPath.isEmpty() || fromPath == toPath)
    return;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));

  QStringList favs = loadStringList(s, kFavKey);
  for (QString &p : favs) {
    if (p == fromPath)
      p = toPath;
    else if (p.startsWith(fromPath + QLatin1Char('/')))
      p = toPath + p.mid(fromPath.size());
  }
  saveStringList(s, kFavKey, favs);

  auto colors = loadColorMap(s);
  QHash<QString, int> nextColors;
  for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
    QString key = it.key();
    if (key == fromPath)
      key = toPath;
    else if (key.startsWith(fromPath + QLatin1Char('/')))
      key = toPath + key.mid(fromPath.size());
    nextColors.insert(key, it.value());
  }
  saveColorMap(s, nextColors);

  auto recent = loadRecent(s);
  for (RecentEntry &e : recent) {
    if (e.path == fromPath)
      e.path = toPath;
    else if (e.path.startsWith(fromPath + QLatin1Char('/')))
      e.path = toPath + e.path.mid(fromPath.size());
  }
  saveRecent(s, recent);
}
