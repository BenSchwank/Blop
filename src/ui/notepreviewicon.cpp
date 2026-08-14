#include "notepreviewicon.h"

#include <QCache>
#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace NotePreviewIcon {
namespace {

struct CacheKey {
  QString path;
  qint64 mtime{0};
  qint64 size{0};
  int px{0};
  bool operator==(const CacheKey &o) const {
    return path == o.path && mtime == o.mtime && size == o.size && px == o.px;
  }
};

inline size_t qHash(const CacheKey &k, size_t seed = 0) noexcept {
  return ::qHash(k.path, seed) ^ size_t(k.mtime) ^ size_t(k.size) ^
         size_t(uint(k.px) * 2654435761u);
}

QCache<CacheKey, QPixmap> &pixmapCache() {
  static QCache<CacheKey, QPixmap> c(8 * 1024);
  return c;
}

QColor readablePaper(const QColor &paper) {
  if (!paper.isValid())
    return QColor(252, 250, 245);
  if (paper.alpha() < 20)
    return QColor(252, 250, 245);
  return paper;
}

void paintPattern(QPainter *p, const QRectF &r, int t, const QColor &paper) {
  const bool dark = paper.lightness() < 130;
  const QColor line =
      dark ? QColor(255, 255, 255, 78) : QColor(78, 86, 128, 140);
  const qreal w = r.width();
  const qreal h = r.height();
  if (w < 4 || h < 4)
    return;

  switch (t) {
  case 0: // Blank
    break;
  case 1: // Lined
  case 4: { // Legal
    p->setPen(QPen(line, qMax(1.0, w / 42.0), Qt::SolidLine, Qt::FlatCap));
    const int n = 7;
    for (int i = 1; i <= n; ++i) {
      const qreal y = r.top() + h * (qreal(i) / qreal(n + 1));
      p->drawLine(QPointF(r.left() + w * 0.10, y),
                  QPointF(r.right() - w * 0.08, y));
    }
    if (t == 4) {
      p->setPen(QPen(QColor(210, 68, 68), qMax(1.3, w / 26.0)));
      const qreal x = r.left() + w * 0.24;
      p->drawLine(QPointF(x, r.top() + h * 0.05),
                  QPointF(x, r.bottom() - h * 0.05));
    }
    break;
  }
  case 2: { // Grid
    p->setPen(QPen(line, qMax(0.9, w / 52.0)));
    const int n = 6;
    for (int i = 1; i < n; ++i) {
      const qreal x = r.left() + w * (qreal(i) / qreal(n));
      p->drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
      const qreal y = r.top() + h * (qreal(i) / qreal(n));
      p->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
    }
    break;
  }
  case 3: { // Dotted
    p->setPen(Qt::NoPen);
    p->setBrush(line);
    const int n = 5;
    const qreal dx = w / (n + 1);
    const qreal dy = h / (n + 1);
    const qreal rad = qMax(0.85, w / 34.0);
    for (int i = 1; i <= n; ++i) {
      for (int j = 1; j <= n; ++j)
        p->drawEllipse(QPointF(r.left() + dx * i, r.top() + dy * j), rad, rad);
    }
    break;
  }
  default:
    break;
  }
}

void paintA4(QPainter *p, const QRect &full, const Spec &spec) {
  const qreal s = full.width();
  const QRectF sheet(full.left() + s * 0.20, full.top() + s * 0.08, s * 0.58,
                     s * 0.80);
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0, 0, 0, 55));
  p->drawRoundedRect(sheet.translated(s * 0.035, s * 0.04), s * 0.045,
                     s * 0.045);

  const QColor paper = readablePaper(spec.paper);
  p->setBrush(paper);
  p->setPen(QPen(QColor(0, 0, 0, 45), qMax(1.0, s / 64.0)));
  p->drawRoundedRect(sheet, s * 0.045, s * 0.045);

  QPainterPath clip;
  clip.addRoundedRect(sheet.adjusted(1, 1, -1, -1), s * 0.04, s * 0.04);
  p->save();
  p->setClipPath(clip);
  paintPattern(p, sheet.adjusted(s * 0.02, s * 0.04, -s * 0.02, -s * 0.04),
               spec.backgroundType, paper);
  p->restore();
}

void paintInfinite(QPainter *p, const QRect &full, const Spec &spec) {
  const qreal s = full.width();
  const QRectF back(full.left() + s * 0.22, full.top() + s * 0.14, s * 0.62,
                    s * 0.62);
  const QRectF front(full.left() + s * 0.12, full.top() + s * 0.22, s * 0.64,
                     s * 0.64);
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0, 0, 0, 40));
  p->drawRoundedRect(back.translated(s * 0.02, s * 0.03), s * 0.06, s * 0.06);

  const QColor paper = readablePaper(spec.paper);
  p->setBrush(paper.darker(108));
  p->setPen(QPen(QColor(0, 0, 0, 35), qMax(1.0, s / 72.0)));
  p->drawRoundedRect(back, s * 0.06, s * 0.06);

  p->setBrush(paper);
  p->drawRoundedRect(front, s * 0.055, s * 0.055);

  QPainterPath clip;
  clip.addRoundedRect(front.adjusted(1, 1, -1, -1), s * 0.05, s * 0.05);
  p->save();
  p->setClipPath(clip);
  // Pattern to the edges so it reads as unbounded canvas, not a page.
  paintPattern(p, front, spec.backgroundType, paper);
  p->restore();
}

void paintFolder(QPainter *p, const QRect &full) {
  const qreal s = full.width();
  const QColor bodyDark(210, 158, 58);
  const QColor tab(242, 206, 112);

  p->setPen(Qt::NoPen);
  p->setBrush(QColor(0, 0, 0, 50));
  p->drawRoundedRect(QRectF(full.left() + s * 0.12, full.top() + s * 0.34,
                            s * 0.78, s * 0.50),
                     s * 0.06, s * 0.06);

  // Paper peeking out.
  p->setBrush(QColor(248, 246, 240));
  p->drawRoundedRect(QRectF(full.left() + s * 0.22, full.top() + s * 0.22,
                            s * 0.52, s * 0.22),
                     s * 0.03, s * 0.03);

  QPainterPath tabPath;
  tabPath.moveTo(full.left() + s * 0.14, full.top() + s * 0.36);
  tabPath.lineTo(full.left() + s * 0.14, full.top() + s * 0.24);
  tabPath.cubicTo(full.left() + s * 0.14, full.top() + s * 0.20,
                  full.left() + s * 0.17, full.top() + s * 0.18,
                  full.left() + s * 0.20, full.top() + s * 0.18);
  tabPath.lineTo(full.left() + s * 0.46, full.top() + s * 0.18);
  tabPath.cubicTo(full.left() + s * 0.50, full.top() + s * 0.18,
                  full.left() + s * 0.52, full.top() + s * 0.21,
                  full.left() + s * 0.54, full.top() + s * 0.26);
  tabPath.lineTo(full.left() + s * 0.56, full.top() + s * 0.36);
  tabPath.closeSubpath();
  p->setBrush(tab);
  p->drawPath(tabPath);

  QLinearGradient g(0, full.top() + s * 0.34, 0, full.top() + s * 0.84);
  g.setColorAt(0, tab);
  g.setColorAt(1, bodyDark);
  p->setBrush(g);
  p->drawRoundedRect(QRectF(full.left() + s * 0.12, full.top() + s * 0.34,
                            s * 0.76, s * 0.48),
                     s * 0.07, s * 0.07);

  p->setBrush(QColor(255, 255, 255, 55));
  p->drawRoundedRect(QRectF(full.left() + s * 0.18, full.top() + s * 0.40,
                            s * 0.64, s * 0.10),
                     s * 0.04, s * 0.04);
}

int clampBg(int v) {
  if (v < 0 || v > 4)
    return 2;
  return v;
}

int fromBlopPageStyle(int style) {
  // CanvasView::PageStyle: Blank=0 Lined=1 Squared=2 Dotted=3
  switch (style) {
  case 0:
    return 0;
  case 1:
    return 1;
  case 3:
    return 3;
  case 2:
  default:
    return 2;
  }
}

bool peekBnote(const QString &path, Spec *out) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  const QByteArray head = f.read(4096);
  const int cover = head.indexOf("\"cover\"");
  if (cover >= 0) {
    const int brace = head.indexOf('{', cover);
    const int end = brace >= 0 ? head.indexOf('}', brace) : -1;
    if (brace >= 0 && end > brace) {
      const QJsonDocument doc =
          QJsonDocument::fromJson(head.mid(brace, end - brace + 1));
      if (doc.isObject()) {
        const QJsonObject o = doc.object();
        out->kind = Kind::A4;
        out->backgroundType = clampBg(o.value(QStringLiteral("bg")).toInt(2));
        const QString paper = o.value(QStringLiteral("paper")).toString();
        if (!paper.isEmpty())
          out->paper = QColor(paper);
        return true;
      }
    }
  }
  // Older files: first "bg":N is page 0 (after an empty strokes array on new notes).
  const int bgKey = head.indexOf("\"bg\"");
  if (bgKey >= 0) {
    const int colon = head.indexOf(':', bgKey);
    if (colon > 0) {
      bool ok = false;
      const int v = head.mid(colon + 1, 8).trimmed().toInt(&ok);
      if (ok) {
        out->kind = Kind::A4;
        out->backgroundType = clampBg(v);
        return true;
      }
    }
  }
  out->kind = Kind::A4;
  out->backgroundType = 2;
  return true;
}

bool peekBlop(const QString &path, Spec *out) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  QDataStream in(&f);
  quint32 magic = 0;
  in >> magic;
  bool infinite = true;
  qint32 style = 2; // PageStyle::Squared — keep peek independent of CanvasView
  if (magic == 0xB10B0005) {
    qint32 grid = 40;
    QColor paper;
    in >> infinite >> style >> grid >> paper;
    out->kind = infinite ? Kind::Infinite : Kind::A4;
    out->backgroundType = fromBlopPageStyle(style);
    if (paper.isValid())
      out->paper = paper;
    return in.status() == QDataStream::Ok;
  }
  if (magic == 0xB10B0002 || magic == 0xB10B0003 || magic == 0xB10B0004) {
    in >> infinite;
    out->kind = infinite ? Kind::Infinite : Kind::A4;
    out->backgroundType = 2;
    return true;
  }
  if (magic == 0xB10B0001) {
    out->kind = Kind::Infinite;
    out->backgroundType = 2;
    return true;
  }
  return false;
}

} // namespace

Spec specForPath(const QString &path, bool isDirectory) {
  Spec s;
  if (isDirectory) {
    s.kind = Kind::Folder;
    return s;
  }
  if (path.endsWith(QLatin1String(".bnote"), Qt::CaseInsensitive)) {
    s.kind = Kind::A4;
    peekBnote(path, &s);
    return s;
  }
  if (path.endsWith(QLatin1String(".blop"), Qt::CaseInsensitive)) {
    s.kind = Kind::Infinite;
    peekBlop(path, &s);
    return s;
  }
  s.kind = Kind::Folder;
  return s;
}

QPixmap pixmap(const Spec &spec, int px) {
  px = qMax(16, px);
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  const QRect full(0, 0, px, px);
  switch (spec.kind) {
  case Kind::Folder:
    paintFolder(&p, full);
    break;
  case Kind::Infinite:
    paintInfinite(&p, full, spec);
    break;
  case Kind::A4:
  default:
    paintA4(&p, full, spec);
    break;
  }
  return pm;
}

QPixmap pixmapForPath(const QString &path, bool isDirectory, int px) {
  px = qMax(16, px);
  CacheKey key;
  key.path = path;
  key.px = px;
  if (!isDirectory && !path.isEmpty()) {
    const QFileInfo fi(path);
    key.mtime = fi.lastModified().toMSecsSinceEpoch();
    key.size = fi.size();
  }
  if (QPixmap *hit = pixmapCache().object(key))
    return *hit;
  const Spec spec = specForPath(path, isDirectory);
  auto *stored = new QPixmap(pixmap(spec, px));
  pixmapCache().insert(key, stored, qMax(1, (px * px * 4) / 1024));
  return *stored;
}

} // namespace NotePreviewIcon
