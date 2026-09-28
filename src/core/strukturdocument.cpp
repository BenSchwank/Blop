#include "strukturdocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>

StrukturDocument StrukturDocument::createEmpty(const QString &title) {
  StrukturDocument d;
  d.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  d.title = title.isEmpty() ? QStringLiteral("Neue Struktur") : title;
  d.version = 1;
  StrukturBlock para;
  para.type = StrukturBlock::Type::Paragraph;
  para.paragraph.text = QString();
  d.blocks.append(para);
  return d;
}

static QJsonObject cropToJson(const QRectF &r) {
  QJsonObject o;
  o.insert(QStringLiteral("x"), r.x());
  o.insert(QStringLiteral("y"), r.y());
  o.insert(QStringLiteral("w"), r.width());
  o.insert(QStringLiteral("h"), r.height());
  return o;
}

static std::optional<QRectF> cropFromJson(const QJsonValue &v) {
  if (!v.isObject())
    return std::nullopt;
  const QJsonObject o = v.toObject();
  if (!o.contains(QLatin1String("w")) || !o.contains(QLatin1String("h")))
    return std::nullopt;
  QRectF r(o.value(QStringLiteral("x")).toDouble(),
           o.value(QStringLiteral("y")).toDouble(),
           o.value(QStringLiteral("w")).toDouble(),
           o.value(QStringLiteral("h")).toDouble());
  if (r.width() <= 0.01 || r.height() <= 0.01)
    return std::nullopt;
  return r;
}

namespace StrukturGrid {

void clampItem(StrukturGridItem &item) {
  item.colSpan = qBound(1, item.colSpan, kColumns);
  item.rowSpan = qBound(1, item.rowSpan, kMaxRowSpan);
  item.col = qBound(0, item.col, kColumns - item.colSpan);
  item.row = qMax(0, item.row);
}

bool overlaps(const StrukturGridItem &a, const StrukturGridItem &b) {
  return a.col < b.col + b.colSpan && b.col < a.col + a.colSpan &&
         a.row < b.row + b.rowSpan && b.row < a.row + a.rowSpan;
}

void resolve(QVector<StrukturGridItem> &items, int pinned) {
  for (StrukturGridItem &it : items)
    clampItem(it);
  QVector<int> order;
  order.reserve(items.size());
  for (int i = 0; i < items.size(); ++i)
    if (i != pinned)
      order.append(i);
  std::stable_sort(order.begin(), order.end(), [&items](int a, int b) {
    if (items[a].row != items[b].row)
      return items[a].row < items[b].row;
    return items[a].col < items[b].col;
  });
  if (pinned >= 0 && pinned < items.size())
    order.prepend(pinned);

  // Push-down: later cards move below anything they collide with.
  QVector<int> placed;
  for (int idx : order) {
    StrukturGridItem &it = items[idx];
    bool moved = true;
    while (moved) {
      moved = false;
      for (int p : placed) {
        if (overlaps(it, items[p])) {
          it.row = items[p].row + items[p].rowSpan;
          moved = true;
        }
      }
    }
    placed.append(idx);
  }

  // Float up: close vertical gaps top-down so the grid stays compact.
  QVector<int> byRow(items.size());
  for (int i = 0; i < items.size(); ++i)
    byRow[i] = i;
  std::stable_sort(byRow.begin(), byRow.end(), [&items](int a, int b) {
    if (items[a].row != items[b].row)
      return items[a].row < items[b].row;
    return items[a].col < items[b].col;
  });
  QVector<int> settled;
  for (int idx : byRow) {
    StrukturGridItem &it = items[idx];
    while (it.row > 0) {
      StrukturGridItem probe = it;
      --probe.row;
      bool blocked = false;
      for (int s : settled)
        if (overlaps(probe, items[s])) {
          blocked = true;
          break;
        }
      if (blocked)
        break;
      it.row = probe.row;
    }
    settled.append(idx);
  }
}

int rowCount(const QVector<StrukturGridItem> &items) {
  int rows = 0;
  for (const StrukturGridItem &it : items)
    rows = qMax(rows, it.row + it.rowSpan);
  return rows;
}

} // namespace StrukturGrid

static StrukturEmbedBlock embedFromJson(const QJsonObject &bo) {
  StrukturEmbedBlock e;
  e.notePath = bo.value(QStringLiteral("notePath")).toString();
  e.pageIndex = bo.value(QStringLiteral("pageIndex")).toInt(0);
  e.cropRect = cropFromJson(bo.value(QStringLiteral("cropRect")));
  e.halfWidth = bo.value(QStringLiteral("halfWidth")).toBool(false);
  return e;
}

static QJsonObject embedToJson(const StrukturEmbedBlock &e) {
  QJsonObject bo;
  bo.insert(QStringLiteral("notePath"), e.notePath);
  bo.insert(QStringLiteral("pageIndex"), e.pageIndex);
  if (e.cropRect)
    bo.insert(QStringLiteral("cropRect"), cropToJson(*e.cropRect));
  else
    bo.insert(QStringLiteral("cropRect"), QJsonValue());
  if (e.halfWidth)
    bo.insert(QStringLiteral("halfWidth"), true);
  return bo;
}

bool StrukturDocument::load(const QString &path, StrukturDocument &out) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  f.close();
  if (!doc.isObject())
    return false;
  const QJsonObject root = doc.object();
  out.id = root.value(QStringLiteral("id")).toString();
  out.title = root.value(QStringLiteral("title")).toString();
  out.date = root.value(QStringLiteral("date")).toString();
  out.status = root.value(QStringLiteral("status")).toString();
  out.tags.clear();
  const QJsonArray tags = root.value(QStringLiteral("tags")).toArray();
  for (const QJsonValue &tv : tags) {
    const QString t = tv.toString().trimmed();
    if (!t.isEmpty())
      out.tags.append(t);
  }
  out.version = root.value(QStringLiteral("version")).toInt(1);
  out.blocks.clear();
  const QJsonArray blocks = root.value(QStringLiteral("blocks")).toArray();
  for (const QJsonValue &bv : blocks) {
    if (!bv.isObject())
      continue;
    const QJsonObject bo = bv.toObject();
    const QString type = bo.value(QStringLiteral("type")).toString();
    StrukturBlock block;
    if (type == QLatin1String("embed")) {
      // v1 single embed → one full-width card (or half when flagged).
      block.type = StrukturBlock::Type::EmbedGrid;
      StrukturGridItem it;
      it.embed = embedFromJson(bo);
      it.colSpan = it.embed.halfWidth ? 2 : StrukturGrid::kColumns;
      it.rowSpan = 2;
      block.items.append(it);
    } else if (type == QLatin1String("columns")) {
      // v2 side-by-side row → two half cards.
      block.type = StrukturBlock::Type::EmbedGrid;
      const QJsonArray items = bo.value(QStringLiteral("items")).toArray();
      for (const QJsonValue &iv : items) {
        if (!iv.isObject() || block.items.size() >= 2)
          continue;
        StrukturGridItem it;
        it.embed = embedFromJson(iv.toObject());
        it.col = block.items.size() * 2;
        it.colSpan = 2;
        it.rowSpan = 2;
        block.items.append(it);
      }
      if (block.items.isEmpty())
        continue;
    } else if (type == QLatin1String("grid")) {
      block.type = StrukturBlock::Type::EmbedGrid;
      const QJsonArray items = bo.value(QStringLiteral("items")).toArray();
      for (const QJsonValue &iv : items) {
        if (!iv.isObject())
          continue;
        const QJsonObject io = iv.toObject();
        StrukturGridItem it;
        it.embed = embedFromJson(io);
        it.col = io.value(QStringLiteral("col")).toInt(0);
        it.row = io.value(QStringLiteral("row")).toInt(0);
        it.colSpan = io.value(QStringLiteral("colSpan")).toInt(4);
        it.rowSpan = io.value(QStringLiteral("rowSpan")).toInt(2);
        block.items.append(it);
      }
      if (block.items.isEmpty())
        continue;
      StrukturGrid::resolve(block.items);
    } else {
      block.type = StrukturBlock::Type::Paragraph;
      block.paragraph.text = bo.value(QStringLiteral("text")).toString();
      block.paragraph.kind = bo.value(QStringLiteral("kind")).toString();
    }
    out.blocks.append(block);
  }
  if (out.blocks.isEmpty()) {
    StrukturBlock para;
    para.type = StrukturBlock::Type::Paragraph;
    out.blocks.append(para);
  }
  if (out.id.isEmpty())
    out.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  return true;
}

bool StrukturDocument::save(const StrukturDocument &doc, const QString &path) {
  QJsonObject root;
  root.insert(QStringLiteral("id"), doc.id);
  root.insert(QStringLiteral("title"), doc.title);
  root.insert(QStringLiteral("date"), doc.date);
  root.insert(QStringLiteral("status"), doc.status);
  QJsonArray tags;
  for (const QString &t : doc.tags)
    tags.append(t);
  root.insert(QStringLiteral("tags"), tags);
  root.insert(QStringLiteral("version"), doc.version);
  QJsonArray blocks;
  for (const StrukturBlock &b : doc.blocks) {
    QJsonObject bo;
    if (b.type == StrukturBlock::Type::Embed ||
        b.type == StrukturBlock::Type::Columns ||
        b.type == StrukturBlock::Type::EmbedGrid) {
      QVector<StrukturGridItem> gridItems = b.items;
      if (b.type == StrukturBlock::Type::Embed) {
        StrukturGridItem it;
        it.embed = b.embed;
        it.colSpan = b.embed.halfWidth ? 2 : StrukturGrid::kColumns;
        gridItems = {it};
      } else if (b.type == StrukturBlock::Type::Columns) {
        gridItems.clear();
        for (const StrukturEmbedBlock &e : b.columns) {
          StrukturGridItem it;
          it.embed = e;
          it.col = gridItems.size() * 2;
          it.colSpan = 2;
          gridItems.append(it);
        }
      }
      if (gridItems.isEmpty())
        continue;
      bo.insert(QStringLiteral("type"), QStringLiteral("grid"));
      QJsonArray items;
      for (const StrukturGridItem &it : gridItems) {
        QJsonObject io = embedToJson(it.embed);
        io.remove(QStringLiteral("halfWidth"));
        io.insert(QStringLiteral("col"), it.col);
        io.insert(QStringLiteral("row"), it.row);
        io.insert(QStringLiteral("colSpan"), it.colSpan);
        io.insert(QStringLiteral("rowSpan"), it.rowSpan);
        items.append(io);
      }
      bo.insert(QStringLiteral("items"), items);
    } else {
      bo.insert(QStringLiteral("type"), QStringLiteral("paragraph"));
      bo.insert(QStringLiteral("text"), b.paragraph.text);
      if (!b.paragraph.kind.isEmpty())
        bo.insert(QStringLiteral("kind"), b.paragraph.kind);
    }
    blocks.append(bo);
  }
  root.insert(QStringLiteral("blocks"), blocks);

  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly))
    return false;
  f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return f.commit();
}

QString StrukturDocument::absoluteNotePath(const QString &strukturPath,
                                           const QString &storedPath) {
  if (storedPath.isEmpty())
    return QString();
  const QFileInfo stored(storedPath);
  if (stored.isAbsolute())
    return QFileInfo(storedPath).absoluteFilePath();
  const QDir dir = QFileInfo(strukturPath).absoluteDir();
  return QFileInfo(dir.filePath(storedPath)).absoluteFilePath();
}

QString StrukturDocument::storeNotePath(const QString &strukturPath,
                                        const QString &absoluteNotePath) {
  const QFileInfo noteFi(absoluteNotePath);
  const QDir structDir = QFileInfo(strukturPath).absoluteDir();
  const QString rel = structDir.relativeFilePath(noteFi.absoluteFilePath());
  if (rel.startsWith(QLatin1String("..")))
    return noteFi.absoluteFilePath();
  return rel;
}
