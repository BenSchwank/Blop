#include "notepagerenderer.h"

#include "PageItem.h"
#include "uiscale.h"

#include <QPainter>

namespace NotePageRenderer {

int a4WidthPx() {
#ifdef Q_OS_ANDROID
  return qRound(793 * (UiScale::dp(100) / 100.0));
#else
  return 793;
#endif
}

int a4HeightPx() {
#ifdef Q_OS_ANDROID
  return qRound(1122 * (UiScale::dp(100) / 100.0));
#else
  return 1122;
#endif
}

QImage renderFullPage(const NotePage &page, int pageW, int pageH) {
  if (pageW <= 0)
    pageW = a4WidthPx();
  if (pageH <= 0)
    pageH = a4HeightPx();

  QImage img(pageW, pageH, QImage::Format_ARGB32_Premultiplied);
  const QColor paper =
      page.paperColor.isValid() ? page.paperColor : QColor(Qt::white);
  img.fill(paper);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  if (!page.backgroundImage.isNull()) {
    p.drawImage(QRectF(0, 0, pageW, pageH), page.backgroundImage);
  } else {
    const int L = paper.lightness();
    const QColor lineCol =
        L < 130 ? QColor(255, 255, 255, 50) : QColor(190, 190, 210, 110);
    const auto type = static_cast<PageBackgroundType>(page.backgroundType);
    switch (type) {
    case PageBackgroundType::Blank:
      break;
    case PageBackgroundType::Lined:
    case PageBackgroundType::Legal: {
      p.setPen(QPen(lineCol, 1));
      for (int y = 40; y < pageH; y += 40)
        p.drawLine(0, y, pageW, y);
      if (type == PageBackgroundType::Legal) {
        p.setPen(QPen(L < 130 ? QColor(255, 120, 120) : QColor(220, 80, 80), 2));
        p.drawLine(72, 0, 72, pageH);
      }
      break;
    }
    case PageBackgroundType::Grid: {
      p.setPen(QPen(lineCol, 1));
      for (int x = 40; x < pageW; x += 40)
        p.drawLine(x, 0, x, pageH);
      for (int y = 40; y < pageH; y += 40)
        p.drawLine(0, y, pageW, y);
      break;
    }
    case PageBackgroundType::Dotted: {
      p.setPen(Qt::NoPen);
      p.setBrush(lineCol);
      for (int y = 40; y < pageH; y += 40)
        for (int x = 40; x < pageW; x += 40)
          p.drawEllipse(QPointF(x, y), 1.4, 1.4);
      break;
    }
    }
  }

  for (const auto &s : page.strokes) {
    QColor c = s.color;
    if (s.isHighlighter)
      c.setAlpha(80);
    else if (!s.isEraser)
      c.setAlpha(255);
    QPen pen;
    if (s.isEraser)
      pen = QPen(paper, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    else
      pen = QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(s.path);
  }

  for (const auto &sn : page.stickies) {
    const QRectF r(sn.pos, QSizeF(sn.width, sn.height));
    p.setPen(QPen(QColor(0, 0, 0, 40), 1));
    p.setBrush(sn.color.isValid() ? sn.color : QColor(255, 236, 120));
    p.drawRoundedRect(r, 6, 6);
    p.setPen(QColor(40, 40, 40));
    QFont f = p.font();
    f.setPointSizeF(qMax(8.0, sn.fontPointSize > 0 ? sn.fontPointSize : 14.0));
    p.setFont(f);
    p.drawText(r.adjusted(8, 8, -8, -8), Qt::TextWordWrap | Qt::AlignTop,
               sn.text);
  }

  for (const auto &g : page.graphs) {
    p.setPen(QPen(QColor(90, 90, 110), 1.2));
    p.setBrush(QColor(255, 255, 255, 210));
    p.drawRect(g.rect);
    p.setPen(QColor(120, 120, 140));
    p.drawText(g.rect.adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("Graph"));
  }

  p.end();
  return img;
}

QImage renderPreview(const NotePage &page, const QSize &fitSize,
                     const QRectF &cropNorm) {
  QImage full = renderFullPage(page);
  if (full.isNull() || fitSize.isEmpty())
    return full;

  QImage cropped = full;
  if (cropNorm.isValid() && cropNorm.width() > 0.02 && cropNorm.height() > 0.02) {
    const QRectF n = cropNorm.intersected(QRectF(0, 0, 1, 1));
    const QRect px(
        qRound(n.x() * full.width()), qRound(n.y() * full.height()),
        qMax(1, qRound(n.width() * full.width())),
        qMax(1, qRound(n.height() * full.height())));
    cropped = full.copy(px.intersected(full.rect()));
  }

  return cropped.scaled(fitSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace NotePageRenderer
