#include "strukturdocument.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

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
      block.type = StrukturBlock::Type::Embed;
      block.embed.notePath = bo.value(QStringLiteral("notePath")).toString();
      block.embed.pageIndex = bo.value(QStringLiteral("pageIndex")).toInt(0);
      block.embed.cropRect = cropFromJson(bo.value(QStringLiteral("cropRect")));
    } else {
      block.type = StrukturBlock::Type::Paragraph;
      block.paragraph.text = bo.value(QStringLiteral("text")).toString();
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
  root.insert(QStringLiteral("version"), doc.version);
  QJsonArray blocks;
  for (const StrukturBlock &b : doc.blocks) {
    QJsonObject bo;
    if (b.type == StrukturBlock::Type::Embed) {
      bo.insert(QStringLiteral("type"), QStringLiteral("embed"));
      bo.insert(QStringLiteral("notePath"), b.embed.notePath);
      bo.insert(QStringLiteral("pageIndex"), b.embed.pageIndex);
      if (b.embed.cropRect)
        bo.insert(QStringLiteral("cropRect"), cropToJson(*b.embed.cropRect));
      else
        bo.insert(QStringLiteral("cropRect"), QJsonValue());
    } else {
      bo.insert(QStringLiteral("type"), QStringLiteral("paragraph"));
      bo.insert(QStringLiteral("text"), b.paragraph.text);
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
