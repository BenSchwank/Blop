#pragma once

#include <QPointF>
#include <Qt>
#include <QtGlobal>

class QGraphicsScene;
class QTabletEvent;
class AbstractTool;

/// Single scene-pointer funnel for note drawing tools.
/// Views keep bypasses (pan/crop/graph/text) outside; all tool ink goes here.
class ToolEngine {
public:
  enum class Device { Mouse, Tablet, Touch };

  struct Pointer {
    QPointF scenePos;
    Qt::MouseButton button{Qt::LeftButton};
    Qt::MouseButtons buttons{Qt::NoButton};
    Qt::KeyboardModifiers modifiers{Qt::NoModifier};
    Device device{Device::Mouse};
    qreal pressure{1.0};
  };

  /// Dispatch press to the active ToolManager tool. Returns true if handled.
  static bool begin(QGraphicsScene *scene, const Pointer &p);

  /// Dispatch move. Returns true if handled.
  static bool move(QGraphicsScene *scene, const Pointer &p);

  /// Dispatch release. Returns true if handled.
  static bool end(QGraphicsScene *scene, const Pointer &p);

  /// Tablet path (pressure-aware). Uses AbstractTool::handleTabletEvent.
  static bool tablet(QGraphicsScene *scene, QTabletEvent *event,
                     const QPointF &scenePos);

  static AbstractTool *activeTool();
};
