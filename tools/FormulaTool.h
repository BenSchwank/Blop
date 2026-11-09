#pragma once
#include "AbstractTool.h"
#include "GraphCanvasItem.h"
#include "ToolManager.h"

#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QPen>

/// Math dock tile: drag a rect (default click = 420×340) to place a GraphCanvasItem.
class FormulaTool : public AbstractTool {
public:
  using AbstractTool::AbstractTool;

  ToolMode mode() const override { return ToolMode::Formula; }
  QString name() const override { return QStringLiteral("Formel"); }
  QString iconName() const override { return QStringLiteral("pi"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene || !event)
      return false;
    m_start = event->scenePos();
    m_preview = scene->addRect(QRectF(m_start, m_start),
                               QPen(QColor(91, 157, 255), 1.5, Qt::DashLine),
                               QBrush(QColor(91, 157, 255, 28)));
    m_preview->setZValue(1000);
    return true;
  }

  bool handleMouseMove(QGraphicsSceneMouseEvent *event,
                       QGraphicsScene *scene) override {
    Q_UNUSED(scene);
    if (!m_preview || !event)
      return false;
    m_preview->setRect(QRectF(m_start, event->scenePos()).normalized());
    return true;
  }

  bool handleMouseRelease(QGraphicsSceneMouseEvent *event,
                          QGraphicsScene *scene) override {
    if (!scene) {
      m_preview = nullptr;
      return false;
    }
    QRectF bb;
    if (m_preview) {
      bb = m_preview->rect().normalized();
      scene->removeItem(m_preview);
      delete m_preview;
      m_preview = nullptr;
    }
    if (bb.width() < 24.0 || bb.height() < 24.0) {
      // Click / tiny drag → default graph size from plan.
      const QPointF c = event ? event->scenePos() : m_start;
      bb = QRectF(c.x() - 210.0, c.y() - 170.0, 420.0, 340.0);
    }
    auto *graphItem = new GraphCanvasItem(bb);
    GraphObject d;
    d.rect = bb;
    d.functions.clear();
    d.selectedFunction = -1;
    graphItem->fromData(d);
    graphItem->setZValue(4.0);
    scene->addItem(graphItem);
    m_lastCompletedItem = graphItem;
    emit contentModified();
    return true;
  }

private:
  QPointF m_start;
  QGraphicsRectItem *m_preview{nullptr};
};
