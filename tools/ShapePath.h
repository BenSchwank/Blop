#pragma once

/// Pure geometry for ShapeTool drag-bounds → QPainterPath (no scene / Graph deps).
/// Headless tests link this header alone; ShapeTool::buildShapePath forwards here.

#include "ToolSettings.h"

#include <QLineF>
#include <QPainterPath>
#include <QRectF>
#include <QtMath>

#include <cmath>

inline QPainterPath blopBuildShapePath(const QRectF &dragRect, const ToolConfig &cfg) {
  const QRectF rn = dragRect.normalized();
  if (rn.width() < 1.0 && rn.height() < 1.0)
    return {};

  // Directed tools must use the raw drag corners (QRectF(start,end)), not the
  // normalized TL→BR — otherwise up/left drags flip arrow/line direction and
  // feel like they "snap" to a fixed diagonal.
  const QPointF dragA = dragRect.topLeft();
  const QPointF dragB = dragRect.bottomRight();

  switch (cfg.shapeToolKind) {
  case ShapeToolKind::Rectangle: {
    QPainterPath p;
    p.addRect(rn);
    return p;
  }
  case ShapeToolKind::Circle: {
    // True circle inscribed in the drag bounds (never a square outline).
    const QPointF c = rn.center();
    const qreal rad = qMin(rn.width(), rn.height()) * 0.5;
    if (rad < 0.5)
      return {};
    QPainterPath p;
    p.addEllipse(QRectF(c.x() - rad, c.y() - rad, 2.0 * rad, 2.0 * rad));
    return p;
  }
  case ShapeToolKind::Ellipse: {
    QPainterPath p;
    p.addEllipse(rn);
    return p;
  }
  case ShapeToolKind::Line: {
    QPainterPath p;
    p.moveTo(dragA);
    p.lineTo(dragB);
    return p;
  }
  case ShapeToolKind::Arrow: {
    QPainterPath p;
    const QPointF a = dragA;
    const QPointF b = dragB;
    p.moveTo(a);
    p.lineTo(b);
    QLineF stem(a, b);
    if (stem.length() < 1.0)
      return p;
    const qreal head = qBound(8.0, stem.length() * 0.22, 28.0);
    QLineF u = stem.unitVector();
    const QPointF dir(u.dx(), u.dy());
    const QPointF n(-dir.y(), dir.x());
    p.moveTo(b);
    p.lineTo(b - dir * head + n * head * 0.45);
    p.moveTo(b);
    p.lineTo(b - dir * head - n * head * 0.45);
    return p;
  }
  case ShapeToolKind::Axes2D: {
    QPainterPath p;
    const qreal mx = (rn.left() + rn.right()) * 0.5;
    const qreal my = (rn.top() + rn.bottom()) * 0.5;
    p.moveTo(rn.left(), my);
    p.lineTo(rn.right(), my);
    p.moveTo(mx, rn.top());
    p.lineTo(mx, rn.bottom());
    const int nt = qBound(2, cfg.shapeAxisTicks, 12);
    const qreal dx = rn.width() / qMax(1, nt - 1);
    const qreal dy = rn.height() / qMax(1, nt - 1);
    const qreal tickH = qMax(3.0, qMin(rn.width(), rn.height()) * 0.04);
    for (int i = 0; i < nt; ++i) {
      const qreal x = rn.left() + i * dx;
      p.moveTo(x, my - tickH * 0.5);
      p.lineTo(x, my + tickH * 0.5);
    }
    for (int i = 0; i < nt; ++i) {
      const qreal y = rn.top() + i * dy;
      p.moveTo(mx - tickH * 0.5, y);
      p.lineTo(mx + tickH * 0.5, y);
    }
    return p;
  }
  case ShapeToolKind::SineGraph: {
    QPainterPath p;
    const qreal cy = rn.center().y();
    const qreal a = qBound(0.01, cfg.shapeMathA, 2.0);
    const qreal b = qBound(0.05, cfg.shapeMathB, 12.0);
    const qreal c = qBound(-12.57, cfg.shapeMathC, 12.57);
    const qreal d = qBound(-1.5, cfg.shapeMathD, 1.5);
    const qreal w = rn.width();
    if (cfg.shapeSineFixedParams) {
      constexpr qreal kLenRef = 100.0;
      constexpr qreal kAmpRef = 45.0;
      const qreal omega = b * (2.0 * M_PI) / kLenRef;
      const int N = qBound(96, static_cast<int>(w / 3.0) + 1, 640);
      for (int i = 0; i <= N; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(N);
        const qreal x = rn.left() + t * w;
        const qreal ang = omega * (x - rn.left()) + c;
        const qreal y = cy - (a * kAmpRef * std::sin(ang) + d * kAmpRef);
        if (i == 0)
          p.moveTo(x, y);
        else
          p.lineTo(x, y);
      }
    } else {
      const int N = 96;
      const qreal halfH = rn.height() * 0.5;
      for (int i = 0; i <= N; ++i) {
        const qreal t = static_cast<qreal>(i) / static_cast<qreal>(N);
        const qreal x = rn.left() + t * w;
        const qreal ang = b * (2.0 * M_PI) * t + c;
        const qreal y = cy - halfH * (a * std::sin(ang) + d);
        if (i == 0)
          p.moveTo(x, y);
        else
          p.lineTo(x, y);
      }
    }
    return p;
  }
  case ShapeToolKind::CoordinateGraph: {
    QPainterPath p;
    p.addRect(rn);
    const qreal x0 =
        rn.left() +
        (0.0 - cfg.graphXMin) /
            qMax(1e-6, (cfg.graphXMax - cfg.graphXMin)) * rn.width();
    const qreal y0 =
        rn.bottom() -
        (0.0 - cfg.graphYMin) /
            qMax(1e-6, (cfg.graphYMax - cfg.graphYMin)) * rn.height();
    p.moveTo(rn.left(), y0);
    p.lineTo(rn.right(), y0);
    p.moveTo(x0, rn.top());
    p.lineTo(x0, rn.bottom());
    // Graph starts empty; curves are added interactively on the graph item.
    return p;
  }
  default:
    return {};
  }
}
