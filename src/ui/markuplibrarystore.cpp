#include "markuplibrarystore.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainterPath>
#include <QStandardPaths>
#include <QUuid>

namespace {

QJsonObject strokeToJson(const Stroke &s) {
  QJsonObject o;
  o.insert(QStringLiteral("width"), s.width);
  o.insert(QStringLiteral("color"), s.color.name(QColor::HexArgb));
  o.insert(QStringLiteral("eraser"), s.isEraser);
  o.insert(QStringLiteral("highlighter"), s.isHighlighter);
  QJsonArray pts;
  for (const QPointF &p : s.points) {
    QJsonObject pt;
    pt.insert(QStringLiteral("x"), p.x());
    pt.insert(QStringLiteral("y"), p.y());
    pts.append(pt);
  }
  o.insert(QStringLiteral("points"), pts);
  return o;
}

Stroke strokeFromJson(const QJsonObject &o) {
  Stroke s;
  s.width = o.value(QStringLiteral("width")).toDouble(2.0);
  s.color = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#ff000000")));
  s.isEraser = o.value(QStringLiteral("eraser")).toBool(false);
  s.isHighlighter = o.value(QStringLiteral("highlighter")).toBool(false);
  const QJsonArray pts = o.value(QStringLiteral("points")).toArray();
  QPainterPath path;
  for (int i = 0; i < pts.size(); ++i) {
    const QJsonObject pt = pts.at(i).toObject();
    const QPointF p(pt.value(QStringLiteral("x")).toDouble(),
                    pt.value(QStringLiteral("y")).toDouble());
    s.points.append(p);
    if (i == 0)
      path.moveTo(p);
    else
      path.lineTo(p);
  }
  s.path = path;
  return s;
}

} // namespace

QString MarkupLibraryStore::storePath() {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/markup_library.json");
}

QVector<MarkupLibraryItem> MarkupLibraryStore::load() {
  QVector<MarkupLibraryItem> out;
  QFile f(storePath());
  if (!f.open(QIODevice::ReadOnly))
    return out;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isArray())
    return out;
  for (const QJsonValue &v : doc.array()) {
    const QJsonObject o = v.toObject();
    MarkupLibraryItem item;
    item.id = o.value(QStringLiteral("id")).toString();
    item.name = o.value(QStringLiteral("name")).toString();
    item.created = QDateTime::fromString(
        o.value(QStringLiteral("created")).toString(), Qt::ISODate);
    item.previewColor =
        QColor(o.value(QStringLiteral("previewColor")).toString(QStringLiteral("#ff000000")));
    for (const QJsonValue &sv : o.value(QStringLiteral("strokes")).toArray())
      item.strokes.append(strokeFromJson(sv.toObject()));
    if (item.id.isEmpty())
      item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!item.strokes.isEmpty())
      out.append(item);
  }
  return out;
}

bool MarkupLibraryStore::save(const QVector<MarkupLibraryItem> &items) {
  QJsonArray arr;
  for (const MarkupLibraryItem &item : items) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), item.id);
    o.insert(QStringLiteral("name"), item.name);
    o.insert(QStringLiteral("created"),
             item.created.toString(Qt::ISODate));
    o.insert(QStringLiteral("previewColor"),
             item.previewColor.name(QColor::HexArgb));
    QJsonArray strokes;
    for (const Stroke &s : item.strokes)
      strokes.append(strokeToJson(s));
    o.insert(QStringLiteral("strokes"), strokes);
    arr.append(o);
  }
  QFile f(storePath());
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
  return true;
}

bool MarkupLibraryStore::addItem(const MarkupLibraryItem &item) {
  auto items = load();
  items.prepend(item);
  return save(items);
}

bool MarkupLibraryStore::removeItem(const QString &id) {
  auto items = load();
  const auto before = items.size();
  items.erase(std::remove_if(items.begin(), items.end(),
                             [&](const MarkupLibraryItem &it) {
                               return it.id == id;
                             }),
              items.end());
  if (items.size() == before)
    return false;
  return save(items);
}
