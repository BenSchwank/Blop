#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QVector>
#include <optional>

struct StrukturEmbedBlock {
  QString notePath; // relative to struktur folder when possible
  int pageIndex{0};
  /// Normalized page crop [0,1]; nullopt = full A4.
  std::optional<QRectF> cropRect;
};

struct StrukturParagraphBlock {
  QString text;
};

struct StrukturBlock {
  enum class Type { Paragraph, Embed };
  Type type{Type::Paragraph};
  StrukturParagraphBlock paragraph;
  StrukturEmbedBlock embed;
};

struct StrukturDocument {
  QString id;
  QString title;
  int version{1};
  QVector<StrukturBlock> blocks;

  static StrukturDocument createEmpty(const QString &title);
  static bool load(const QString &path, StrukturDocument &out);
  static bool save(const StrukturDocument &doc, const QString &path);

  /// Resolve embed notePath against struktur file directory.
  static QString absoluteNotePath(const QString &strukturPath,
                                  const QString &storedPath);
  /// Store path relative to struktur dir when under same root.
  static QString storeNotePath(const QString &strukturPath,
                               const QString &absoluteNotePath);
};
