#pragma once
#include "AbstractTool.h"
#include "RulerItem.h"
#include "ToolManager.h"
#include "ToolSettings.h"

#include <QBrush>
#include <QColor>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QTransform>

class LassoTool : public AbstractTool {
  Q_OBJECT
public:
  using AbstractTool::AbstractTool;
  ToolMode mode() const override { return ToolMode::Lasso; }
  QString name() const override { return QStringLiteral("Lasso"); }
  QString iconName() const override { return QStringLiteral("lasso"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene)
      return false;

    QGraphicsItem *itemUnderMouse =
        scene->itemAt(event->scenePos(), QTransform());
    if (itemUnderMouse && itemUnderMouse->isSelected())
      return false; // let Qt move the selection

    // Drop a stale marquee from an aborted previous gesture.
    if (m_selectionItem && m_selectionItem->scene()) {
      m_selectionItem->scene()->removeItem(m_selectionItem);
      delete m_selectionItem;
    }
    m_selectionItem = nullptr;

    scene->clearSelection();
    m_startPos = event->scenePos();
    m_currentPath = QPainterPath();

    // Default: rectangle marquee (clean-sheet contract).
    if (m_config.lassoMode == LassoMode::Freehand) {
      m_currentPath.moveTo(m_startPos);
    } else {
      m_currentPath.addRect(QRectF(m_startPos, m_startPos));
    }

    ensureMarquee(scene);
    m_selectionItem->setPath(m_currentPath);
    m_selectionItem->show();
    return true;
  }

  bool handleMouseMove(QGraphicsSceneMouseEvent *event,
                       QGraphicsScene *scene) override {
    if (!m_selectionItem || !scene)
      return false;

    if (m_config.lassoMode == LassoMode::Freehand) {
      m_currentPath.lineTo(event->scenePos());
    } else {
      QRectF rect(m_startPos, event->scenePos());
      rect = rect.normalized();
      if (m_config.aspectLock) {
        const qreal w = rect.width();
        const qreal h = rect.height();
        const qreal size = qMax(qAbs(w), qAbs(h));
        const qreal sx = (event->scenePos().x() >= m_startPos.x()) ? 1.0 : -1.0;
        const qreal sy = (event->scenePos().y() >= m_startPos.y()) ? 1.0 : -1.0;
        rect = QRectF(m_startPos,
                      m_startPos + QPointF(size * sx, size * sy))
                   .normalized();
      }
      m_currentPath = QPainterPath();
      m_currentPath.addRect(rect);
    }

    QPainterPath shown = m_currentPath;
    if (m_config.lassoMode == LassoMode::Freehand)
      shown.closeSubpath();
    m_selectionItem->setPath(shown);
    scene->update();
    return true;
  }

  bool handleMouseRelease(QGraphicsSceneMouseEvent *event,
                          QGraphicsScene *scene) override {
    Q_UNUSED(event);
    if (!m_selectionItem || !scene)
      return false;

    if (m_config.lassoMode == LassoMode::Freehand) {
      m_currentPath.closeSubpath();
      m_selectionItem->setPath(m_currentPath);
    }

    scene->setSelectionArea(m_currentPath, Qt::ReplaceSelection,
                            Qt::IntersectsItemShape, QTransform());

    for (QGraphicsItem *item : scene->selectedItems()) {
      if (item && item->type() == RulerItem::Type)
        item->setSelected(false);
    }

    scene->removeItem(m_selectionItem);
    delete m_selectionItem;
    m_selectionItem = nullptr;
    scene->update();
    return true;
  }

  void onDeactivated() override {
    if (m_selectionItem && m_selectionItem->scene()) {
      m_selectionItem->scene()->removeItem(m_selectionItem);
      delete m_selectionItem;
    }
    m_selectionItem = nullptr;
  }

private:
  void ensureMarquee(QGraphicsScene *scene) {
    if (m_selectionItem)
      return;
    m_selectionItem = new QGraphicsPathItem();
    QPen pen(QColor(0x5B, 0x9D, 0xFF), 1.5, Qt::DashLine, Qt::RoundCap,
             Qt::RoundJoin);
    pen.setCosmetic(true);
    pen.setDashPattern({4.0, 3.0});
    m_selectionItem->setPen(pen);
    m_selectionItem->setBrush(QColor(91, 157, 255, 20)); // ~0.08 alpha
    m_selectionItem->setZValue(99999);
    scene->addItem(m_selectionItem);
  }

  QPointF m_startPos;
  QPainterPath m_currentPath;
  QGraphicsPathItem *m_selectionItem{nullptr};
};
