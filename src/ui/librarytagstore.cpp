#include "librarytagstore.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QVariant>

QColor LibraryTagStore::tagColor(const QString &name) {
  static const QColor kDots[] = {
      QColor(0x25, 0x63, 0xEB), QColor(0xF9, 0x73, 0x16),
      QColor(0x16, 0xA3, 0x4A), QColor(0x7C, 0x3A, 0xED)};
  return kDots[qHash(name.toLower()) % 4];
}

namespace {
constexpr const char *kOrg = "Blop";
constexpr const char *kApp = "BlopApp";
constexpr const char *kCatalogKey = "library/tag_catalog";
constexpr const char *kMapKey = "library/note_tags";

/// Canonical map key so Windows `/` vs `\` (and relative vs absolute) lookups hit.
QString canonicalPathKey(const QString &path) {
  if (path.isEmpty())
    return {};
  const QFileInfo fi(path);
  QString abs = fi.absoluteFilePath();
  if (abs.isEmpty())
    abs = path;
  abs = QDir::cleanPath(abs);
  return QDir::toNativeSeparators(abs);
}

QString separatorFolded(QString path) {
  return path.replace(QLatin1Char('\\'), QLatin1Char('/')).toLower();
}

QHash<QString, QStringList> loadMap(QSettings &s) {
  QHash<QString, QStringList> map;
  const QVariantMap raw = s.value(QString::fromLatin1(kMapKey)).toMap();
  for (auto it = raw.constBegin(); it != raw.constEnd(); ++it)
    map.insert(it.key(), it.value().toStringList());
  return map;
}

void saveMap(QSettings &s, const QHash<QString, QStringList> &map) {
  QVariantMap raw;
  for (auto it = map.constBegin(); it != map.constEnd(); ++it)
    raw.insert(it.key(), it.value());
  s.setValue(QString::fromLatin1(kMapKey), raw);
}

QStringList uniqueSorted(QStringList tags) {
  tags.removeAll(QString());
  tags.removeDuplicates();
  tags.sort(Qt::CaseInsensitive);
  return tags;
}

QStringList lookupTags(const QHash<QString, QStringList> &map,
                       const QString &absolutePath) {
  const QString key = canonicalPathKey(absolutePath);
  if (key.isEmpty())
    return {};
  if (map.contains(key))
    return uniqueSorted(map.value(key));
  // Exact miss: tolerate older keys with mixed separators / casing.
  const QString folded = separatorFolded(key);
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    if (separatorFolded(it.key()) == folded)
      return uniqueSorted(it.value());
  }
  return {};
}
} // namespace

QString LibraryTagStore::normalize(const QString &raw) {
  QString t = raw.trimmed();
  if (t.startsWith(QStringLiteral("[[")) && t.endsWith(QStringLiteral("]]")) &&
      t.size() > 4)
    t = t.mid(2, t.size() - 4).trimmed();
  // Collapse internal whitespace
  t = t.simplified();
  return t;
}

QStringList LibraryTagStore::catalog() {
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  return uniqueSorted(s.value(QString::fromLatin1(kCatalogKey)).toStringList());
}

void LibraryTagStore::setCatalog(const QStringList &tags) {
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  s.setValue(QString::fromLatin1(kCatalogKey), uniqueSorted(tags));
}

QStringList LibraryTagStore::tagsForPath(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return {};
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  return lookupTags(loadMap(s), absolutePath);
}

void LibraryTagStore::setTagsForPath(const QString &absolutePath,
                                     const QStringList &tags) {
  const QString key = canonicalPathKey(absolutePath);
  if (key.isEmpty())
    return;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  auto map = loadMap(s);
  // Drop legacy aliases that fold to the same path so we don't keep duplicates.
  const QString folded = separatorFolded(key);
  for (auto it = map.begin(); it != map.end();) {
    if (it.key() != key && separatorFolded(it.key()) == folded)
      it = map.erase(it);
    else
      ++it;
  }
  const QStringList cleaned = uniqueSorted(tags);
  if (cleaned.isEmpty())
    map.remove(key);
  else
    map.insert(key, cleaned);

  // Keep catalog in sync with assigned tags.
  QStringList cat = uniqueSorted(s.value(QString::fromLatin1(kCatalogKey)).toStringList());
  for (const QString &t : cleaned) {
    if (!cat.contains(t, Qt::CaseInsensitive))
      cat.append(t);
  }
  s.setValue(QString::fromLatin1(kCatalogKey), uniqueSorted(cat));
  saveMap(s, map);
}

bool LibraryTagStore::addTagToCatalog(const QString &tag) {
  const QString n = normalize(tag);
  if (n.isEmpty())
    return false;
  QStringList cat = catalog();
  for (const QString &existing : cat) {
    if (existing.compare(n, Qt::CaseInsensitive) == 0)
      return false;
  }
  cat.append(n);
  setCatalog(cat);
  return true;
}

bool LibraryTagStore::renameTag(const QString &from, const QString &to) {
  const QString a = normalize(from);
  const QString b = normalize(to);
  if (a.isEmpty() || b.isEmpty() || a.compare(b, Qt::CaseInsensitive) == 0)
    return false;

  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  QStringList cat = uniqueSorted(s.value(QString::fromLatin1(kCatalogKey)).toStringList());
  bool found = false;
  for (QString &t : cat) {
    if (t.compare(a, Qt::CaseInsensitive) == 0) {
      t = b;
      found = true;
    }
  }
  if (!found)
    cat.append(b);
  s.setValue(QString::fromLatin1(kCatalogKey), uniqueSorted(cat));

  auto map = loadMap(s);
  for (auto it = map.begin(); it != map.end(); ++it) {
    QStringList tags = it.value();
    bool changed = false;
    for (QString &t : tags) {
      if (t.compare(a, Qt::CaseInsensitive) == 0) {
        t = b;
        changed = true;
      }
    }
    if (changed)
      it.value() = uniqueSorted(tags);
  }
  saveMap(s, map);
  return true;
}

bool LibraryTagStore::removeTag(const QString &tag) {
  const QString n = normalize(tag);
  if (n.isEmpty())
    return false;

  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  QStringList cat = uniqueSorted(s.value(QString::fromLatin1(kCatalogKey)).toStringList());
  QStringList next;
  for (const QString &t : cat) {
    if (t.compare(n, Qt::CaseInsensitive) != 0)
      next.append(t);
  }
  s.setValue(QString::fromLatin1(kCatalogKey), next);

  auto map = loadMap(s);
  for (auto it = map.begin(); it != map.end(); ++it) {
    QStringList tags = it.value();
    QStringList kept;
    for (const QString &t : tags) {
      if (t.compare(n, Qt::CaseInsensitive) != 0)
        kept.append(t);
    }
    it.value() = kept;
  }
  saveMap(s, map);
  return true;
}

int LibraryTagStore::usageCount(const QString &tag) {
  const QString n = normalize(tag);
  if (n.isEmpty())
    return 0;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  const auto map = loadMap(s);
  int count = 0;
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    for (const QString &t : it.value()) {
      if (t.compare(n, Qt::CaseInsensitive) == 0) {
        ++count;
        break;
      }
    }
  }
  return count;
}

QHash<QString, int> LibraryTagStore::usageCounts() {
  QHash<QString, int> raw;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  const auto map = loadMap(s);
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    for (const QString &t : it.value()) {
      const QString n = normalize(t);
      if (!n.isEmpty())
        raw[n.toLower()] = raw.value(n.toLower()) + 1;
    }
  }
  QHash<QString, int> keyed;
  for (const QString &cat : catalog())
    keyed.insert(cat, raw.value(cat.toLower(), 0));
  return keyed;
}

void LibraryTagStore::remapPath(const QString &fromPath, const QString &toPath) {
  const QString from = canonicalPathKey(fromPath);
  const QString to = canonicalPathKey(toPath);
  if (from.isEmpty() || to.isEmpty() || from == to)
    return;
  const QString fromFold = separatorFolded(from);
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  auto map = loadMap(s);
  QHash<QString, QStringList> next;
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    const QString canon = canonicalPathKey(it.key());
    const QString keyFold = separatorFolded(canon);
    QString outKey = canon;
    if (keyFold == fromFold) {
      outKey = to;
    } else if (keyFold.startsWith(fromFold + QLatin1Char('/'))) {
      const QString remFold = keyFold.mid(fromFold.size()); // starts with '/'
      outKey = to + QDir::toNativeSeparators(remFold);
    }
    next.insert(canonicalPathKey(outKey), it.value());
  }
  saveMap(s, next);
}
