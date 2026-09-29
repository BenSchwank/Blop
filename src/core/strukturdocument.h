#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

struct StrukturEmbedBlock {
  QString notePath; // relative to struktur folder when possible
  int pageIndex{0};
  /// Normalized page crop [0,1]; nullopt = full A4.
  std::optional<QRectF> cropRect;
  /// When true, prefer sharing a column row with a neighbour (half width).
  bool halfWidth{false};
};

/// One card in a 4-column embed grid. col 0–3, colSpan 1–4, rowSpan 1–4.
struct StrukturGridItem {
  StrukturEmbedBlock embed;
  int col{0};
  int row{0};
  int colSpan{4};
  int rowSpan{2};
};

struct StrukturParagraphBlock {
  QString text;
  /// "code", "todo", or empty for body text.
  QString kind;
};

struct StrukturBlock {
  /// Embed / Columns only exist in old files; load() migrates them to
  /// EmbedGrid and save() writes only "grid".
  enum class Type { Paragraph, Embed, Columns, EmbedGrid };
  Type type{Type::Paragraph};
  StrukturParagraphBlock paragraph;
  StrukturEmbedBlock embed;
  QVector<StrukturEmbedBlock> columns;
  /// Cards of an EmbedGrid block.
  QVector<StrukturGridItem> items;
};

namespace StrukturGrid {
constexpr int kColumns = 4;
constexpr int kMaxRowSpan = 4;
/// Clamp spans/positions into the 4-column grid.
void clampItem(StrukturGridItem &item);
bool overlaps(const StrukturGridItem &a, const StrukturGridItem &b);
/// Resolve collisions by pushing cards down, then float everything up.
/// `pinned` (if >= 0) keeps priority at its requested cell.
void resolve(QVector<StrukturGridItem> &items, int pinned = -1);
/// Total rows occupied.
int rowCount(const QVector<StrukturGridItem> &items);
} // namespace StrukturGrid

struct StrukturDocument {
  QString id;
  QString title;
  QString date;
  QStringList tags;
  QString status;
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

  /// One-shot: move root-level "Eingebettete Notiz*.bnote" into .blop-embeds/
  /// and rewrite matching notePath entries in sibling .struct files.
  /// Returns number of notes moved. Idempotent.
  static int migrateLegacyRootEmbeds(const QString &libraryRoot);
};
