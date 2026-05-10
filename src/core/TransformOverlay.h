#pragma once
#include <QGraphicsObject>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QVector2D>
#include <cmath>
#include <QCursor>
#include <QTransform>
#include <QGraphicsItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>

static inline void bakeTransform(QGraphicsItem *item) {
  if (!item)
    return;
  QTransform localToParent = item->sceneTransform();
  if (item->parentItem()) {
    localToParent =
        localToParent * item->parentItem()->sceneTransform().inverted();
  }
  item->setTransform(localToParent);
  item->setPos(0, 0);
  item->setRotation(0);
  item->setScale(1.0);
}

static inline QCursor getRotatedCursor(Qt::CursorShape shape, qreal angle) {
  qreal baseAngle = 0;
  bool isDoubleArrow = false;

  if (shape == Qt::SizeVerCursor) {
    baseAngle = 90;
    isDoubleArrow = true;
  } else if (shape == Qt::SizeHorCursor) {
    baseAngle = 0;
    isDoubleArrow = true;
  } else if (shape == Qt::SizeBDiagCursor) {
    baseAngle = 45;
    isDoubleArrow = true;
  } else if (shape == Qt::SizeFDiagCursor) {
    baseAngle = 135;
    isDoubleArrow = true;
  }

  if (!isDoubleArrow)
    return QCursor(shape);

  qreal totalAngle = baseAngle + angle;
  int size = 32;
  QPixmap pix(size, size);
  pix.fill(Qt::transparent);
  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(size / 2, size / 2);
  p.rotate(totalAngle);
  QPen whitePen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap);
  QPen blackPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap);
  auto drawArrow = [&](QPainter &p) {
    p.drawLine(-9, 0, 9, 0);
    p.drawLine(-9, 0, -5, -4);
    p.drawLine(-9, 0, -5, 4);
    p.drawLine(9, 0, 5, -4);
    p.drawLine(9, 0, 5, 4);
  };
  p.setPen(whitePen);
  drawArrow(p);
  p.setPen(blackPen);
  drawArrow(p);
  return QCursor(pix, size / 2, size / 2);
}

class TransformOverlay : public QGraphicsObject {
    Q_OBJECT

public:
  enum Handle {
    None,
    TopLeft,
    Top,
    TopRight,
    Left,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
    Rotate,
    Center
  };
  TransformOverlay(QGraphicsItem *targetItem) : m_target(targetItem) {
    setFlag(ItemIsSelectable, false);
    setAcceptHoverEvents(true);
    sync();
  }
  QGraphicsItem *target() const { return m_target; }
  QRectF boundingRect() const override {
    return m_target->boundingRect().adjusted(-150, -150, 150, 150);
  }
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
      Q_UNUSED(option);
      Q_UNUSED(widget);
      
      painter->save();
      painter->setPen(QPen(QColor(108, 92, 231, 255), 2, Qt::DashLine));
      painter->setBrush(QColor(108, 92, 231, 20));
      
      QRectF r = m_target->boundingRect();
      painter->drawRect(r);

      // Robust scale extraction (rotation-invariant)
      qreal m11 = painter->transform().m11();
      qreal m12 = painter->transform().m12();
      qreal m21 = painter->transform().m21();
      qreal m22 = painter->transform().m22();
      qreal sx = std::sqrt(m11*m11 + m12*m12);
      qreal sy = std::sqrt(m21*m21 + m22*m22);
      qreal scaleBase = std::sqrt(sx*sx + sy*sy);
      if (scaleBase < 0.001) scaleBase = 1.0;
      
      qreal handleRadius = 6.0 / scaleBase;

      painter->setPen(QPen(QColor(50, 50, 50), 1.0 / scaleBase));
      painter->setBrush(Qt::white);

      QList<QPointF> handles = {
          r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight(),
          QPointF(r.center().x(), r.top()), QPointF(r.center().x(), r.bottom()),
          QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y())
      };
      
      for (const QPointF& p : handles) {
          painter->drawEllipse(p, handleRadius, handleRadius);
      }

      // Rotate Handle
      qreal rotateDist = 60.0 / scaleBase;
      QPointF rotHandle(r.center().x(), r.top() - rotateDist);
      
      painter->setPen(QPen(QColor(108, 92, 231, 255), 2.0 / scaleBase, Qt::SolidLine));
      painter->drawLine(QPointF(r.center().x(), r.top()), rotHandle);
      
      painter->setBrush(QColor(108, 92, 231, 255));
      painter->setPen(QPen(Qt::white, 1.5 / scaleBase));
      painter->drawEllipse(rotHandle, handleRadius * 1.2, handleRadius * 1.2);
      
      painter->restore();
  }
  void sync() {
    prepareGeometryChange();
    setPos(m_target->pos());
    setRotation(m_target->rotation());
    setScale(m_target->scale());
    setTransform(m_target->transform());
    setTransformOriginPoint(m_target->transformOriginPoint());
    update();
    emit transformChanged();
  }

signals:
  void transformChanged();
  void interactionStarted();
  void interactionEnded();

public:
  Handle handleAt(QPointF scenePos) {
    if (!scene() || scene()->views().isEmpty())
      return None;
    QGraphicsView *view = scene()->views().first();
    QTransform viewTx = view->viewportTransform();
    QTransform itemToView = sceneTransform() * viewTx;
    qreal sy = std::sqrt(itemToView.m21() * itemToView.m21() +
                         itemToView.m22() * itemToView.m22());
    if (sy < 0.001)
      sy = 1.0;
    QPointF mouseP(view->mapFromScene(scenePos));
    QRectF r = m_target->boundingRect();
    double scaleThreshold = 25.0;
    double rotateThreshold = 40.0;
    qreal localDist = 60.0 / sy;
    QPointF topCenterLocal(r.center().x(), r.top());
    QPointF rotHandleLocal(topCenterLocal.x(), topCenterLocal.y() - localDist);
    QPointF rotHandleView = itemToView.map(rotHandleLocal);
    if (QVector2D(mouseP - rotHandleView).length() < rotateThreshold)
      return Rotate;
    auto check = [&](QPointF localP) {
      QPointF handleView = itemToView.map(localP);
      return QVector2D(mouseP - handleView).length() < scaleThreshold;
    };
    if (check(r.topLeft()))
      return TopLeft;
    if (check(r.topRight()))
      return TopRight;
    if (check(r.bottomLeft()))
      return BottomLeft;
    if (check(r.bottomRight()))
      return BottomRight;
    if (check(QPointF(r.center().x(), r.top())))
      return Top;
    if (check(QPointF(r.center().x(), r.bottom())))
      return Bottom;
    if (check(QPointF(r.left(), r.center().y())))
      return Left;
    if (check(QPointF(r.right(), r.center().y())))
      return Right;
    QPointF localMouse = sceneTransform().inverted().map(scenePos);
    if (r.contains(localMouse))
      return Center;
    return None;
  }

  bool isHandleAt(QPointF scenePos) {
      return handleAt(scenePos) != None;
  }

protected:
  void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override {
    Handle h = handleAt(event->scenePos());
    QTransform t = m_target->transform();
    qreal rot = std::atan2(t.m12(), t.m11()) * 180.0 / M_PI;
    if (h == Rotate)
      setCursor(Qt::PointingHandCursor);
    else if (h == Center)
      setCursor(Qt::SizeAllCursor);
    else if (h == None)
      setCursor(Qt::ArrowCursor);
    else {
      Qt::CursorShape shape = Qt::ArrowCursor;
      if (h == Top || h == Bottom)
        shape = Qt::SizeVerCursor;
      else if (h == Left || h == Right)
        shape = Qt::SizeHorCursor;
      else if (h == TopLeft || h == BottomRight)
        shape = Qt::SizeFDiagCursor;
      else if (h == TopRight || h == BottomLeft)
        shape = Qt::SizeBDiagCursor;
      setCursor(getRotatedCursor(shape, rot));
    }
    QGraphicsObject::hoverMoveEvent(event);
  }
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
    m_dragHandle = handleAt(event->scenePos());
    if (m_dragHandle != None && m_dragHandle != Center) {
      bakeTransform(m_target);
      sync();
    }
    m_startScenePos = event->scenePos();
    m_initialSceneTransform = m_target->sceneTransform();
    m_initialTransform = m_target->transform();
    m_initialBoundingRect = m_target->boundingRect();
    if (m_dragHandle == None)
      m_dragHandle = Center;
    emit interactionStarted();
  }
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
    Q_UNUSED(event);
    m_dragHandle = None;
    sync();
    emit interactionEnded();
  }
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
    if (m_dragHandle == Center) {
      QPointF delta = event->scenePos() - m_startScenePos;
      m_target->moveBy(delta.x(), delta.y());
      m_startScenePos = event->scenePos();
      sync();
      return;
    }
    if (m_dragHandle == Rotate) {
      QPointF sceneCenter =
          m_initialSceneTransform.map(m_initialBoundingRect.center());
      QLineF startLine(sceneCenter, m_startScenePos);
      QLineF currentLine(sceneCenter, event->scenePos());
      qreal angleDiff = startLine.angleTo(currentLine);
      qreal initialRot = std::atan2(m_initialSceneTransform.m12(),
                                    m_initialSceneTransform.m11()) *
                         180.0 / M_PI;
      qreal targetRot = initialRot - angleDiff;
      qreal quotient = targetRot / 90.0;
      qreal nearest90 = std::round(quotient) * 90.0;
      if (std::abs(targetRot - nearest90) < 5.0) {
        targetRot = nearest90;
        angleDiff = initialRot - targetRot;
      }
      QTransform rotMatrix;
      rotMatrix.translate(sceneCenter.x(), sceneCenter.y());
      rotMatrix.rotate(-angleDiff);
      rotMatrix.translate(-sceneCenter.x(), -sceneCenter.y());
      QTransform newSceneTransform = m_initialSceneTransform * rotMatrix;
      if (m_target->parentItem())
        newSceneTransform = newSceneTransform *
                            m_target->parentItem()->sceneTransform().inverted();
      m_target->setTransform(newSceneTransform);
      sync();
      return;
    }
    QPointF localPos =
        m_initialSceneTransform.inverted().map(event->scenePos());
    QRectF r = m_initialBoundingRect;
    QPointF pivot;
    qreal dsx = 1.0;
    qreal dsy = 1.0;
    if (m_dragHandle == Left || m_dragHandle == TopLeft ||
        m_dragHandle == BottomLeft) {
      pivot.setX(r.right());
      qreal oldWidth = r.width();
      qreal newWidth = r.right() - localPos.x();
      if (oldWidth > 0.1)
        dsx = newWidth / oldWidth;
    } else if (m_dragHandle == Right || m_dragHandle == TopRight ||
               m_dragHandle == BottomRight) {
      pivot.setX(r.left());
      qreal oldWidth = r.width();
      qreal newWidth = localPos.x() - r.left();
      if (oldWidth > 0.1)
        dsx = newWidth / oldWidth;
    } else {
      pivot.setX(r.center().x());
    }
    if (m_dragHandle == Top || m_dragHandle == TopLeft ||
        m_dragHandle == TopRight) {
      pivot.setY(r.bottom());
      qreal oldHeight = r.height();
      qreal newHeight = r.bottom() - localPos.y();
      if (oldHeight > 0.1)
        dsy = newHeight / oldHeight;
    } else if (m_dragHandle == Bottom || m_dragHandle == BottomLeft ||
               m_dragHandle == BottomRight) {
      pivot.setY(r.top());
      qreal oldHeight = r.height();
      qreal newHeight = localPos.y() - r.top();
      if (oldHeight > 0.1)
        dsy = newHeight / oldHeight;
    } else {
      pivot.setY(r.center().y());
    }
    if (dsx < 0.01)
      dsx = 0.01;
    if (dsy < 0.01)
      dsy = 0.01;
    QTransform scaleMatrix;
    scaleMatrix.translate(pivot.x(), pivot.y());
    scaleMatrix.scale(dsx, dsy);
    scaleMatrix.translate(-pivot.x(), -pivot.y());
    m_target->setTransform(scaleMatrix * m_initialTransform);
    sync();
  }

private:
  QGraphicsItem *m_target;
  QPointF m_startScenePos;
  Handle m_dragHandle;
  QTransform m_initialSceneTransform;
  QTransform m_initialTransform;
  QRectF m_initialBoundingRect;
};
