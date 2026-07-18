#pragma once

#include "Note.h"

#include <QString>
#include <QVector>
#include <QColor>
#include <QDateTime>

/// One reusable markup snippet (strokes relative to their bounding box origin).
struct MarkupLibraryItem {
  QString id;
  QString name;
  QDateTime created;
  QVector<Stroke> strokes; ///< page-local paths; pageIndex ignored on insert
  QColor previewColor{Qt::black};
};

/// Device-local Drawboard-style Markup Library (JSON under AppData).
class MarkupLibraryStore {
public:
  static QVector<MarkupLibraryItem> load();
  static bool save(const QVector<MarkupLibraryItem> &items);
  static bool addItem(const MarkupLibraryItem &item);
  static bool removeItem(const QString &id);
  static QString storePath();
};
