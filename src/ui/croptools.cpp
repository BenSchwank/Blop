#include "croptools.h"

#include <QButtonGroup>
#include <QCursor>
#include <QFrame>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QVector2D>

// ============================================================================
// CropMenu
// ============================================================================

CropMenu::CropMenu(QWidget *parent, bool showModeButtons) : QWidget(parent) {
  setStyleSheet(
      "QWidget { background-color: #1E1E1E; border-radius: 8px; border: 1px "
      "solid #6c5ce7; }"
      "QPushButton { background: #333; border: none; color: #DDD; "
      "border-radius: 4px; padding: 6px 12px; font-size: 12px; }"
      "QPushButton:checked { background-color: #6c5ce7; color: white; }"
      "QPushButton:hover { background-color: #444; }"
      "QPushButton#ApplyBtn { background-color: #2E7D32; color: white; }"
      "QPushButton#CancelBtn { background-color: #C62828; color: white; }");
  setAttribute(Qt::WA_StyledBackground);
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);
  btnRect = new QPushButton("Rechteck", this);
  btnRect->setCheckable(true);
  btnFree = new QPushButton("Freihand", this);
  btnFree->setCheckable(true);
  QButtonGroup *grp = new QButtonGroup(this);
  grp->addButton(btnRect);
  grp->addButton(btnFree);
  connect(btnRect, &QPushButton::clicked, this, &CropMenu::rectRequested);
  connect(btnFree, &QPushButton::clicked, this, &CropMenu::freehandRequested);
  layout->addWidget(btnRect);
  layout->addWidget(btnFree);
  QFrame *line = new QFrame;
  line->setFrameShape(QFrame::VLine);
  line->setStyleSheet("color: #555;");
  layout->addWidget(line);
  if (!showModeButtons) {
    btnRect->hide();
    btnFree->hide();
    line->hide();
  }
  QPushButton *btnApply = new QPushButton("✔", this);
  btnApply->setObjectName("ApplyBtn");
  QPushButton *btnCancel = new QPushButton("✕", this);
  btnCancel->setObjectName("CancelBtn");
  connect(btnApply, &QPushButton::clicked, this, &CropMenu::applyRequested);
  connect(btnCancel, &QPushButton::clicked, this, &CropMenu::cancelRequested);
  layout->addWidget(btnApply);
  layout->addWidget(btnCancel);
  adjustSize();
  hide();
}

void CropMenu::setMode(bool rect) {
  if (rect)
    btnRect->setChecked(true);
  else
    btnFree->setChecked(true);
}

// ============================================================================
// CropResizer
// ============================================================================

CropResizer::CropResizer(QRectF rect) : m_rect(rect) {
  setFlags(ItemIsSelectable | ItemIsMovable);
  setAcceptHoverEvents(true);
}

QRectF CropResizer::boundingRect() const {
  return m_rect.adjusted(-20, -20, 20, 20);
}

void CropResizer::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                        QWidget *) {
  painter->setPen(QPen(QColor(Qt::white), 2, Qt::DashLine));
  painter->setBrush(QColor(255, 255, 255, 30));
  painter->drawRect(m_rect);
  painter->setPen(QPen(QColor(Qt::black), 1));
  painter->setBrush(Qt::white);
  qreal r = 6.0;
  QList<QPointF> handles = {m_rect.topLeft(),
                            m_rect.topRight(),
                            m_rect.bottomLeft(),
                            m_rect.bottomRight(),
                            QPointF(m_rect.center().x(), m_rect.top()),
                            QPointF(m_rect.center().x(), m_rect.bottom()),
                            QPointF(m_rect.left(), m_rect.center().y()),
                            QPointF(m_rect.right(), m_rect.center().y())};
  for (auto p : handles)
    painter->drawEllipse(p, r, r);
}

QRectF CropResizer::currentRect() const { return mapRectToScene(m_rect); }

void CropResizer::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
  Handle h = handleAt(event->pos());
  if (h == TopLeft || h == BottomRight)
    setCursor(Qt::SizeFDiagCursor);
  else if (h == TopRight || h == BottomLeft)
    setCursor(Qt::SizeBDiagCursor);
  else if (h == Top || h == Bottom)
    setCursor(Qt::SizeVerCursor);
  else if (h == Left || h == Right)
    setCursor(Qt::SizeHorCursor);
  else
    setCursor(Qt::SizeAllCursor);
  QGraphicsObject::hoverMoveEvent(event);
}

void CropResizer::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  m_dragHandle = handleAt(event->pos());
  m_startPos = event->pos();
  m_startRect = m_rect;
  if (m_dragHandle == None)
    m_dragHandle = Center;
}

void CropResizer::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if (m_dragHandle == Center) {
    moveBy(event->pos().x() - m_startPos.x(),
           event->pos().y() - m_startPos.y());
    return;
  }
  QPointF pos = event->pos();
  qreal l = m_startRect.left(), r = m_startRect.right(),
        t = m_startRect.top(), b = m_startRect.bottom();
  if (m_dragHandle == TopLeft || m_dragHandle == Left ||
      m_dragHandle == BottomLeft)
    l = pos.x();
  if (m_dragHandle == TopRight || m_dragHandle == Right ||
      m_dragHandle == BottomRight)
    r = pos.x();
  if (m_dragHandle == TopLeft || m_dragHandle == Top ||
      m_dragHandle == TopRight)
    t = pos.y();
  if (m_dragHandle == BottomLeft || m_dragHandle == Bottom ||
      m_dragHandle == BottomRight)
    b = pos.y();
  m_rect = QRectF(QPointF(l, t), QPointF(r, b)).normalized();
  prepareGeometryChange();
  update();
}

CropResizer::Handle CropResizer::handleAt(QPointF pos) {
  double r = 20.0;
  if (QVector2D(pos - m_rect.topLeft()).length() < r)
    return TopLeft;
  if (QVector2D(pos - m_rect.topRight()).length() < r)
    return TopRight;
  if (QVector2D(pos - m_rect.bottomLeft()).length() < r)
    return BottomLeft;
  if (QVector2D(pos - m_rect.bottomRight()).length() < r)
    return BottomRight;
  if (QVector2D(pos - QPointF(m_rect.center().x(), m_rect.top())).length() < r)
    return Top;
  if (QVector2D(pos - QPointF(m_rect.center().x(), m_rect.bottom())).length() <
      r)
    return Bottom;
  if (QVector2D(pos - QPointF(m_rect.left(), m_rect.center().y())).length() <
      r)
    return Left;
  if (QVector2D(pos - QPointF(m_rect.right(), m_rect.center().y())).length() <
      r)
    return Right;
  return None;
}
