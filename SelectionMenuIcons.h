#pragma once

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

namespace SelectionMenuIcons {

namespace {
constexpr int kPx = 26;
inline QColor fgPrimary() { return QColor(210, 214, 232); }
inline QColor danger() { return QColor(248, 113, 113); }

inline QIcon makeIcon(void (*draw)(QPainter &, const QColor &)) {
  QPixmap pm(kPx, kPx);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(kPx / 2.0, kPx / 2.0);
  draw(p, fgPrimary());
  return QIcon(pm);
}
} // namespace

inline void drawCut(QPainter &p, const QColor &c) {
  p.scale(0.85, 0.85);
  p.setPen(QPen(c, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  QPainterPath b1;
  b1.moveTo(-4, 6);
  b1.lineTo(2, -8);
  b1.lineTo(6, -6);
  b1.lineTo(0, 8);
  b1.closeSubpath();
  p.drawPath(b1);
  QPainterPath b2;
  b2.moveTo(0, 8);
  b2.lineTo(6, -6);
  b2.lineTo(10, -4);
  b2.lineTo(4, 10);
  b2.closeSubpath();
  p.drawPath(b2);
  p.setBrush(c);
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(1, -1), 2.1, 2.1);
}

inline void drawCopy(QPainter &p, const QColor &c) {
  p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(-9, -7, 11, 14, 2.2, 2.2);
  p.drawRoundedRect(-5, -11, 11, 14, 2.2, 2.2);
}

inline void drawColor(QPainter &p, const QColor &) {
  const QColor a(142, 132, 255);
  const QColor b(96, 210, 255);
  const QColor y(255, 214, 120);
  p.setPen(Qt::NoPen);
  p.setBrush(a);
  p.drawEllipse(QPointF(-5, 2), 5.5, 5.5);
  p.setBrush(b);
  p.drawEllipse(QPointF(4, 2), 5.5, 5.5);
  p.setBrush(y);
  p.drawEllipse(QPointF(-0.5, -4), 5.5, 5.5);
  p.setPen(QPen(fgPrimary(), 1.4));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF(-5, 2), 5.5, 5.5);
  p.drawEllipse(QPointF(4, 2), 5.5, 5.5);
  p.drawEllipse(QPointF(-0.5, -4), 5.5, 5.5);
}

inline void drawCrop(QPainter &p, const QColor &c) {
  p.setPen(QPen(c, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  const qreal s = 9.0;
  p.drawLine(QPointF(-s, -s), QPointF(-s + 5, -s));
  p.drawLine(QPointF(-s, -s), QPointF(-s, -s + 5));
  p.drawLine(QPointF(s, -s), QPointF(s - 5, -s));
  p.drawLine(QPointF(s, -s), QPointF(s, -s + 5));
  p.drawLine(QPointF(-s, s), QPointF(-s + 5, s));
  p.drawLine(QPointF(-s, s), QPointF(-s, s - 5));
  p.drawLine(QPointF(s, s), QPointF(s - 5, s));
  p.drawLine(QPointF(s, s), QPointF(s, s - 5));
}

inline void drawScreenshot(QPainter &p, const QColor &c) {
  p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(-9, -7, 18, 14, 2.5, 2.5);
  p.setBrush(QColor(142, 132, 255, 180));
  p.setPen(Qt::NoPen);
  p.drawEllipse(QPointF(2, -1), 4.5, 4.5);
}

inline void drawTrash(QPainter &p, const QColor &) {
  const QColor d = danger();
  p.setPen(QPen(d, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawLine(QPointF(-7, -8), QPointF(7, -8));
  p.drawLine(QPointF(-5, -8), QPointF(-4, -11));
  p.drawLine(QPointF(5, -8), QPointF(4, -11));
  QPainterPath bin;
  bin.moveTo(-6, -6);
  bin.lineTo(-5, 9);
  bin.quadTo(-5, 11, -3, 11);
  bin.lineTo(3, 11);
  bin.quadTo(5, 11, 5, 9);
  bin.lineTo(6, -6);
  bin.closeSubpath();
  p.drawPath(bin);
  p.setPen(QPen(d.lighter(115), 1.5));
  p.drawLine(QPointF(-2, -3), QPointF(-1, 6));
  p.drawLine(QPointF(2, -3), QPointF(1, 6));
}

inline QIcon fallbackCutIcon() { return makeIcon(drawCut); }
inline QIcon fallbackCopyIcon() { return makeIcon(drawCopy); }

inline QIcon cutIcon() {
  QIcon themed =
      QIcon::fromTheme(QStringLiteral("edit-cut"),
                       QIcon::fromTheme(QStringLiteral("gtk-cut")));
  if (!themed.isNull())
    return themed;
  return fallbackCutIcon();
}

inline QIcon copyIcon() {
  QIcon themed =
      QIcon::fromTheme(QStringLiteral("edit-copy"),
                       QIcon::fromTheme(QStringLiteral("gtk-copy")));
  if (!themed.isNull())
    return themed;
  return fallbackCopyIcon();
}

inline QIcon colorIcon() { return makeIcon(drawColor); }
inline QIcon cropIcon() { return makeIcon(drawCrop); }
inline QIcon screenshotIcon() { return makeIcon(drawScreenshot); }
inline QIcon trashIcon() { return makeIcon(drawTrash); }

inline void drawPageLayout(QPainter &p, const QColor &c) {
  p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(-8, 0, 10, 12, 2, 2);
  p.drawRoundedRect(-4, -4, 10, 12, 2, 2);
  p.drawRoundedRect(0, -8, 10, 12, 2, 2);
}

inline void drawGear(QPainter &p, const QColor &c) {
  const QPointF o(0, 0);
  p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(o, 6.5, 6.5);
  p.drawEllipse(o, 3.2, 3.2);
  constexpr double pi = 3.14159265358979323846;
  for (int i = 0; i < 8; ++i) {
    const double a = (i * pi) / 4.0;
    const double cs = qCos(a);
    const double sn = qSin(a);
    p.drawLine(QPointF(cs * 8.2, sn * 8.2), QPointF(cs * 11.0, sn * 11.0));
  }
}

inline void drawPdfDoc(QPainter &p, const QColor &c) {
  p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(-8, -9, 16, 20, 2, 2);
  p.drawLine(-5, -3, 5, -3);
  p.drawLine(-5, 1, 4, 1);
  p.drawLine(-5, 5, 5, 5);
}

inline void drawImport(QPainter &p, const QColor &c) {
  p.setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(-7, -5, 14, 12, 2, 2);
  p.drawLine(0, -11, 0, -6);
  p.drawLine(-3, -9, 0, -6);
  p.drawLine(3, -9, 0, -6);
}

inline QIcon pageLayoutIcon() { return makeIcon(drawPageLayout); }
inline QIcon gearIcon() { return makeIcon(drawGear); }
inline QIcon pdfDocIcon() { return makeIcon(drawPdfDoc); }
inline QIcon importIcon() { return makeIcon(drawImport); }

} // namespace SelectionMenuIcons
