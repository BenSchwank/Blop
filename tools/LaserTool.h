#pragma once

#include "AbstractTool.h"

#include <QDateTime>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVector>

/// Ephemeral presentation pointer — red trail that fades after ~1.5s.
/// Never added to the note document / undo stack.
class LaserTool : public AbstractTool {
public:
  explicit LaserTool(QObject *parent = nullptr) : AbstractTool(parent) {
    m_fade.setInterval(33);
    connect(&m_fade, &QTimer::timeout, this, [this]() {
      bool any = false;
      const qint64 now = QDateTime::currentMSecsSinceEpoch();
      for (auto it = m_points.begin(); it != m_points.end();) {
        const qreal age = (now - it->t) / 1500.0;
        if (age >= 1.0)
          it = m_points.erase(it);
        else {
          any = true;
          ++it;
        }
      }
      emit overlayChanged();
      if (!any)
        m_fade.stop();
    });
  }

  ToolMode mode() const override { return ToolMode::Laser; }
  QString name() const override { return QStringLiteral("Laser"); }
  QString iconName() const override { return QStringLiteral("laser"); }

  void onDeactivated() override {
    m_points.clear();
    m_fade.stop();
    m_drawing = false;
    emit overlayChanged();
  }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    Q_UNUSED(scene);
    m_drawing = true;
    pushPoint(event->scenePos());
    if (!m_fade.isActive())
      m_fade.start();
    emit overlayChanged();
    return true;
  }

  bool handleMouseMove(QGraphicsSceneMouseEvent *event,
                       QGraphicsScene *scene) override {
    Q_UNUSED(scene);
    if (!m_drawing)
      return false;
    pushPoint(event->scenePos());
    emit overlayChanged();
    return true;
  }

  bool handleMouseRelease(QGraphicsSceneMouseEvent *event,
                          QGraphicsScene *scene) override {
    Q_UNUSED(event);
    Q_UNUSED(scene);
    m_drawing = false;
    return true;
  }

  void drawOverlay(QPainter *painter, const QRectF &rect) override {
    Q_UNUSED(rect);
    if (m_points.size() < 2)
      return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = 1; i < m_points.size(); ++i) {
      const qreal age = (now - m_points[i].t) / 1500.0;
      if (age >= 1.0)
        continue;
      QColor c(0xEF, 0x44, 0x44);
      c.setAlphaF(0.85 * (1.0 - age));
      QPen pen(c, 4.0 * (1.0 - age * 0.5), Qt::SolidLine, Qt::RoundCap,
               Qt::RoundJoin);
      painter->setPen(pen);
      painter->drawLine(m_points[i - 1].pos, m_points[i].pos);
    }
    if (!m_points.isEmpty()) {
      const auto &last = m_points.last();
      const qreal age = (now - last.t) / 1500.0;
      if (age < 1.0) {
        QColor glow(0xEF, 0x44, 0x44);
        glow.setAlphaF(0.9 * (1.0 - age));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(last.pos, 5.0, 5.0);
      }
    }
    painter->restore();
  }

private:
  struct Pt {
    QPointF pos;
    qint64 t;
  };
  void pushPoint(const QPointF &p) {
    m_points.push_back({p, QDateTime::currentMSecsSinceEpoch()});
    while (m_points.size() > 64)
      m_points.removeFirst();
  }

  QVector<Pt> m_points;
  QTimer m_fade;
  bool m_drawing{false};
};
