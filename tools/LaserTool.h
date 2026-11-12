#pragma once

#include "AbstractTool.h"

#include <QDateTime>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTabletEvent>
#include <QTimer>
#include <QVector>

/// Ephemeral presentation pointer — red trail that fades after 1.2s.
/// Never added to the note document / undo stack.
class LaserTool : public AbstractTool {
public:
  explicit LaserTool(QObject *parent = nullptr) : AbstractTool(parent) {
    m_fade.setInterval(33);
    connect(&m_fade, &QTimer::timeout, this, [this]() {
      bool any = false;
      const qint64 now = QDateTime::currentMSecsSinceEpoch();
      for (auto it = m_points.begin(); it != m_points.end();) {
        const qreal age = (now - it->t) / kFadeMs;
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
    m_lastCompletedItem = nullptr;
    emit overlayChanged();
  }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    Q_UNUSED(scene);
    m_drawing = true;
    m_lastCompletedItem = nullptr; // never persist
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
    m_lastCompletedItem = nullptr;
    return true;
  }

  bool handleTabletEvent(QTabletEvent *event,
                         const QPointF &scenePos) override {
    if (event->type() == QEvent::TabletPress) {
      m_drawing = true;
      m_lastCompletedItem = nullptr;
      pushPoint(scenePos);
      if (!m_fade.isActive())
        m_fade.start();
      emit overlayChanged();
      return true;
    }
    if (event->type() == QEvent::TabletMove) {
      if (!m_drawing)
        return false;
      pushPoint(scenePos);
      emit overlayChanged();
      return true;
    }
    if (event->type() == QEvent::TabletRelease) {
      m_drawing = false;
      m_lastCompletedItem = nullptr;
      return true;
    }
    return false;
  }

  void drawOverlay(QPainter *painter, const QRectF &rect) override {
    Q_UNUSED(rect);
    if (m_points.size() < 2)
      return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Soft glow under the trail.
    for (int i = 1; i < m_points.size(); ++i) {
      const qreal age = (now - m_points[i].t) / kFadeMs;
      if (age >= 1.0)
        continue;
      QColor glow(0xEF, 0x44, 0x44);
      glow.setAlphaF(0.28 * (1.0 - age));
      QPen glowPen(glow, 14.0 * (1.0 - age * 0.4), Qt::SolidLine, Qt::RoundCap,
                   Qt::RoundJoin);
      painter->setPen(glowPen);
      painter->drawLine(m_points[i - 1].pos, m_points[i].pos);
    }

    for (int i = 1; i < m_points.size(); ++i) {
      const qreal age = (now - m_points[i].t) / kFadeMs;
      if (age >= 1.0)
        continue;
      QColor c(0xEF, 0x44, 0x44);
      c.setAlphaF(0.92 * (1.0 - age));
      QPen pen(c, 4.5 * (1.0 - age * 0.45), Qt::SolidLine, Qt::RoundCap,
               Qt::RoundJoin);
      painter->setPen(pen);
      painter->drawLine(m_points[i - 1].pos, m_points[i].pos);
    }
    if (!m_points.isEmpty()) {
      const auto &last = m_points.last();
      const qreal age = (now - last.t) / kFadeMs;
      if (age < 1.0) {
        QColor glow(0xEF, 0x44, 0x44);
        glow.setAlphaF(0.55 * (1.0 - age));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(last.pos, 9.0, 9.0);
        QColor core(0xEF, 0x44, 0x44);
        core.setAlphaF(0.95 * (1.0 - age));
        painter->setBrush(core);
        painter->drawEllipse(last.pos, 4.0, 4.0);
      }
    }
    painter->restore();
  }

private:
  static constexpr qreal kFadeMs = 1200.0;

  struct Pt {
    QPointF pos;
    qint64 t;
  };
  void pushPoint(const QPointF &p) {
    m_points.push_back({p, QDateTime::currentMSecsSinceEpoch()});
    while (m_points.size() > 96)
      m_points.removeFirst();
  }

  QVector<Pt> m_points;
  QTimer m_fade;
  bool m_drawing{false};
};
