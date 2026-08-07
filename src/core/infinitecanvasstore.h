#pragma once

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QSet>
#include <QString>

/// Binary infinite-canvas document format (V1–V5).
/// Extracted from CanvasView so round-trips can be tested headlessly.
namespace InfiniteCanvasStore {

constexpr quint32 kMagicV1 = 0xB10B0001;
constexpr quint32 kMagicV2 = 0xB10B0002;
constexpr quint32 kMagicV3 = 0xB10B0003;
constexpr quint32 kMagicV4 = 0xB10B0004;
constexpr quint32 kMagicV5 = 0xB10B0005;

/// Serialize scene content. `exclude` skips overlay items (lasso/crop/transform).
bool saveToFile(const QString &path, QGraphicsScene *scene, bool isInfinite,
                const QSet<QGraphicsItem *> &exclude = {});

/// Clear `scene` and load content. Writes `isInfinite` when non-null.
bool loadFromFile(const QString &path, QGraphicsScene *scene,
                  bool *isInfinite = nullptr);

} // namespace InfiniteCanvasStore
