#include "librarytagstore.h"

#include <QHash>
#include <QSettings>
#include <QVariant>

namespace {
constexpr const char *kOrg = "Blop";
constexpr const char *kApp = "BlopApp";
constexpr const char *kCatalogKey = "library/tag_catalog";
constexpr const char *kMapKey = "library/note_tags";

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
  return uniqueSorted(loadMap(s).value(absolutePath));
}

void LibraryTagStore::setTagsForPath(const QString &absolutePath,
                                     const QStringList &tags) {
  if (absolutePath.isEmpty())
    return;
  QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
  auto map = loadMap(s);
  const QStringList cleaned = uniqueSorted(tags);
  if (cleaned.isEmpty())
    map.remove(absolutePath);
  else
    map.insert(absolutePath, cleaned);

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
  // Drop empty assignments
  for (auto it = map.begin(); it != map.end();) {
    if (it.value().isEmpty())
      it = map.erase(it);
    else
      ++it;
  }
  saveMap(s, map);
  return true;
}
