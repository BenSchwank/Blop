#include "ToolEngine.h"

#include "AbstractTool.h"
#include "ToolManager.h"

#include <QEvent>
#include <QGraphicsSceneMouseEvent>
#include <QTabletEvent>

AbstractTool *ToolEngine::activeTool() {
  return ToolManager::instance().activeTool();
}

static void fillSceneMouse(QGraphicsSceneMouseEvent *ev,
                           const ToolEngine::Pointer &p) {
  ev->setScenePos(p.scenePos);
  ev->setPos(p.scenePos);
  ev->setButton(p.button);
  ev->setButtons(p.buttons);
  ev->setModifiers(p.modifiers);
}

bool ToolEngine::begin(QGraphicsScene *scene, const Pointer &p) {
  AbstractTool *tool = activeTool();
  if (!tool || !scene)
    return false;
  if (p.button != Qt::LeftButton && p.device == Device::Mouse)
    return false;

  QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMousePress);
  fillSceneMouse(&ev, p);
  return tool->handleMousePress(&ev, scene);
}

bool ToolEngine::move(QGraphicsScene *scene, const Pointer &p) {
  AbstractTool *tool = activeTool();
  if (!tool || !scene)
    return false;

  QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMouseMove);
  fillSceneMouse(&ev, p);
  return tool->handleMouseMove(&ev, scene);
}

bool ToolEngine::end(QGraphicsScene *scene, const Pointer &p) {
  AbstractTool *tool = activeTool();
  if (!tool || !scene)
    return false;

  QGraphicsSceneMouseEvent ev(QEvent::GraphicsSceneMouseRelease);
  fillSceneMouse(&ev, p);
  return tool->handleMouseRelease(&ev, scene);
}

bool ToolEngine::tablet(QGraphicsScene *scene, QTabletEvent *event,
                        const QPointF &scenePos) {
  AbstractTool *tool = activeTool();
  if (!tool || !scene || !event)
    return false;

  tool->setStrokeSceneForTablet(scene);
  return tool->handleTabletEvent(event, scenePos);
}
