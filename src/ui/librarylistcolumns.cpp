#include "librarylistcolumns.h"

#include "cloudstoragestore.h"
#include "storageprefs.h"
#include "uiscale.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>

namespace LibraryListColumns {
namespace {

struct CacheEntry {
  qint64 mtime{0};
  qint64 size{-1};
  int pages{-1};
  NotePreviewIcon::Kind kind{NotePreviewIcon::Kind::A4};
};

QMutex &cacheMutex() {
  static QMutex m;
  return m;
}

QHash<QString, CacheEntry> &metaCache() {
  static QHash<QString, CacheEntry> c;
  return c;
}

int peekPageCount(const QString &path, qint64 mtime, qint64 size) {
  QMutexLocker lock(&cacheMutex());
  auto &cache = metaCache();
  auto it = cache.find(path);
  if (it != cache.end() && it->mtime == mtime && it->size == size)
    return it->pages;

  CacheEntry e;
  e.mtime = mtime;
  e.size = size;
  e.pages = -1;
  const NotePreviewIcon::Spec spec =
      NotePreviewIcon::specForPath(path, false);
  e.kind = spec.kind;

  if (path.endsWith(QLatin1String(".bnote"), Qt::CaseInsensitive) &&
      size > 0 && size < 32 * 1024 * 1024) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
      const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
      const QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
      e.pages = pages.isEmpty() ? 1 : pages.size();
    }
  } else if (spec.kind == NotePreviewIcon::Kind::Infinite) {
    e.pages = -2; // sentinel for infinite
  } else if (spec.kind == NotePreviewIcon::Kind::Struktur) {
    e.pages = -1;
  }

  cache.insert(path, e);
  // Soft cap so the library paint path cannot grow without bound.
  if (cache.size() > 512) {
    auto drop = cache.begin();
    cache.erase(drop);
  }
  return e.pages;
}

QString folderLeaf(const QString &path, const QString &root) {
  QString rel = path;
  if (!root.isEmpty() && path.startsWith(root, Qt::CaseInsensitive)) {
    rel = path.mid(root.size());
    while (rel.startsWith(QLatin1Char('/')) || rel.startsWith(QLatin1Char('\\')))
      rel.remove(0, 1);
  }
  const QFileInfo fi(path);
  if (fi.isFile()) {
    const QString parent = fi.absolutePath();
    if (!root.isEmpty() &&
        (parent.compare(root, Qt::CaseInsensitive) == 0 ||
         QFileInfo(parent).canonicalFilePath().compare(
             QFileInfo(root).canonicalFilePath(), Qt::CaseInsensitive) == 0))
      return QStringLiteral("Standard");
    return QFileInfo(parent).fileName();
  }
  return fi.fileName().isEmpty() ? QStringLiteral("Standard") : fi.fileName();
}

} // namespace

int rowPad() { return UiScale::dp(12); }
int selectColWidth() { return UiScale::dp(56); }
int placeColWidth() { return UiScale::dp(160); }
int extentColWidth() { return UiScale::dp(120); }
int modifiedColWidth() { return UiScale::dp(150); }
int nameMinWidth() { return UiScale::dp(220); }
int nameMaxWidth() { return UiScale::dp(340); }
int tagsMinWidth() { return UiScale::dp(220); }

std::array<QRect, Count> columnRects(const QRect &row) {
  std::array<QRect, Count> out{};
  const int pad = rowPad();
  const int innerW = qMax(0, row.width() - 2 * pad);
  const int y = row.top();
  const int h = row.height();

  const int w0 = selectColWidth();
  const int w3 = placeColWidth();
  const int w4 = extentColWidth();
  const int w5 = modifiedColWidth();
  const int fixed = w0 + w3 + w4 + w5;
  int flex = qMax(0, innerW - fixed);
  // stretch 2 : 3 for Name : Tags — Name capped so TAGS sits closer.
  int w1 = (flex * 2) / 5;
  int w2 = flex - w1;
  if (w1 < nameMinWidth()) {
    const int need = nameMinWidth() - w1;
    w1 = nameMinWidth();
    w2 = qMax(0, w2 - need);
  }
  if (w1 > nameMaxWidth()) {
    w2 += w1 - nameMaxWidth();
    w1 = nameMaxWidth();
  }
  if (w2 < tagsMinWidth() && flex >= nameMinWidth() + tagsMinWidth()) {
    const int need = tagsMinWidth() - w2;
    w2 = tagsMinWidth();
    w1 = qMax(nameMinWidth(), w1 - need);
    if (w1 > nameMaxWidth()) {
      w2 += w1 - nameMaxWidth();
      w1 = nameMaxWidth();
    }
  }

  int x = row.left() + pad;
  out[Select] = QRect(x, y, w0, h);
  x += w0;
  out[Name] = QRect(x, y, w1, h);
  x += w1;
  out[Tags] = QRect(x, y, w2, h);
  x += w2;
  out[Place] = QRect(x, y, w3, h);
  x += w3;
  out[Extent] = QRect(x, y, w4, h);
  x += w4;
  out[Modified] = QRect(x, y, w5, h);
  return out;
}

QRect checkboxRect(const QRect &selectCol) {
  const int s = UiScale::dp(16);
  return QRect(selectCol.left() + UiScale::dp(12),
               selectCol.center().y() - s / 2, s, s);
}

QRect starRect(const QRect &selectCol) {
  const int s = UiScale::dp(16);
  return QRect(selectCol.left() + UiScale::dp(22),
               selectCol.center().y() - s / 2, s, s);
}

QRect menuRect(const QRect &modifiedCol) {
  const int s = UiScale::dp(28);
  return QRect(modifiedCol.right() - s - UiScale::dp(2),
               modifiedCol.center().y() - s / 2, s, s);
}

QRect tagGhostRect(const QRect &tagsCol) {
  return tagsCol.adjusted(0, UiScale::dp(6), -UiScale::dp(4), -UiScale::dp(6));
}

QString humanSize(qint64 bytes) {
  if (bytes < 0)
    return QStringLiteral("—");
  if (bytes < 1024)
    return QStringLiteral("%1 B").arg(bytes);
  if (bytes < 1024 * 1024)
    return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f',
                                       bytes < 10 * 1024 ? 1 : 0);
  return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

QString formatModified(const QString &path) {
  if (path.isEmpty())
    return QStringLiteral("—");
  const QDateTime dt = QFileInfo(path).lastModified();
  if (!dt.isValid())
    return QStringLiteral("—");
  const QDate d = dt.date();
  const QDate today = QDate::currentDate();
  if (d == today)
    return QStringLiteral("Heute, %1").arg(dt.toString(QStringLiteral("HH:mm")));
  if (d == today.addDays(-1))
    return QStringLiteral("Gestern, %1").arg(dt.toString(QStringLiteral("HH:mm")));
  return QLocale().toString(d, QStringLiteral("d. MMM")) +
         QStringLiteral(", ") + dt.toString(QStringLiteral("HH:mm"));
}

Meta peekMeta(const QString &path, bool isFolder, const QString &localRoot) {
  Meta m;
  m.modifiedLabel = formatModified(path);
  if (path.isEmpty()) {
    m.extentLabel = QStringLiteral("—");
    m.placeLabel = QStringLiteral("—");
    return m;
  }

  const QFileInfo fi(path);
  m.fileBytes = fi.exists() ? fi.size() : -1;

  // Place
  QString kindPlace = QStringLiteral("Lokal");
  QString root = localRoot;
  const QString canon = fi.canonicalFilePath();
  const QString probe = canon.isEmpty() ? path : canon;
  const QString cloud = StoragePrefs::primaryLinkedCloudPath();
  if (!cloud.isEmpty()) {
    const QString c = QFileInfo(cloud).canonicalFilePath();
    const QString cloudRoot = c.isEmpty() ? cloud : c;
    if (probe.startsWith(cloudRoot, Qt::CaseInsensitive)) {
      kindPlace = QStringLiteral("Cloud");
      root = cloudRoot;
    }
  }
  if (kindPlace != QLatin1String("Cloud")) {
    for (const CloudStorageEntry &e : CloudStorageStore::load()) {
      if (e.path.isEmpty() || !StoragePrefs::isUsableFilesystemDir(e.path))
        continue;
      const QString c = QFileInfo(e.path).canonicalFilePath();
      const QString r = c.isEmpty() ? e.path : c;
      if (probe.startsWith(r, Qt::CaseInsensitive)) {
        kindPlace = QStringLiteral("Cloud");
        root = r;
        break;
      }
    }
  }
  const QString leaf = folderLeaf(probe, root);
  m.placeLabel = kindPlace + QStringLiteral(" · ") + leaf;

  if (isFolder) {
    m.kind = NotePreviewIcon::Kind::Folder;
    m.extentLabel = QStringLiteral("—");
    return m;
  }

  const NotePreviewIcon::Spec spec =
      NotePreviewIcon::specForPath(path, false);
  m.kind = spec.kind;
  switch (spec.kind) {
  case NotePreviewIcon::Kind::Infinite:
    m.formatBadge = QStringLiteral("Unendlich");
    break;
  case NotePreviewIcon::Kind::Struktur:
    m.formatBadge = QStringLiteral("Struktur");
    break;
  case NotePreviewIcon::Kind::A4:
    m.formatBadge = QStringLiteral("DIN A4");
    break;
  case NotePreviewIcon::Kind::Folder:
  default:
    break;
  }

  const int pages =
      peekPageCount(path, fi.lastModified().toMSecsSinceEpoch(), m.fileBytes);
  m.pageCount = pages;
  const QString sizePart = humanSize(m.fileBytes);
  if (spec.kind == NotePreviewIcon::Kind::Infinite || pages == -2)
    m.extentLabel = QStringLiteral("∞ · %1").arg(sizePart);
  else if (pages >= 0)
    m.extentLabel =
        QStringLiteral("%1 S. · %2").arg(pages).arg(sizePart);
  else
    m.extentLabel = sizePart;

  return m;
}

} // namespace LibraryListColumns
