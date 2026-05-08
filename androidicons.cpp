#include "androidicons.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QtMath>

namespace AndroidIcons {

namespace {

// All paths are designed in a 24x24 viewport (Material Design canonical
// size). The dispatch helper below scales the painter so each draw* call
// can use these natural coordinates directly.
constexpr qreal kViewport = 24.0;

// Cache key combines the glyph name, the colour packed as 0xRRGGBBAA and
// the requested pixel size. Two distinct accent colours produce distinct
// cache entries which is fine - the hash is small (a few KiB at most).
struct CacheKey {
  QString name;
  QRgb color;
  int pxSize;
  bool operator==(const CacheKey &o) const {
    return name == o.name && color == o.color && pxSize == o.pxSize;
  }
};

inline uint qHash(const CacheKey &k, uint seed = 0) noexcept {
  return ::qHash(k.name, seed) ^ uint(k.color) ^ uint(k.pxSize) * 2654435761u;
}

QHash<CacheKey, QPixmap> &cache() {
  static QHash<CacheKey, QPixmap> g;
  return g;
}

// ---------------------------------------------------------------------------
// Glyph painters. Each takes a QPainter already translated/scaled into the
// 24x24 viewport, plus the foreground colour to use. The pen and brush are
// reset at the start of each helper so individual icons can mix stroked +
// filled shapes freely.
// ---------------------------------------------------------------------------

void setupStrokePen(QPainter *p, const QColor &c, qreal width = 2.0) {
  QPen pen(c, width);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  p->setPen(pen);
  p->setBrush(Qt::NoBrush);
}

void setupFillBrush(QPainter *p, const QColor &c) {
  p->setPen(Qt::NoPen);
  p->setBrush(c);
}

// Material "description" / sheet of paper with a folded corner.
void paintNote(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 1.8);
  QPainterPath path;
  path.moveTo(6.5, 3.0);
  path.lineTo(15.0, 3.0);
  path.lineTo(19.5, 7.5);
  path.lineTo(19.5, 21.0);
  path.lineTo(6.5, 21.0);
  path.closeSubpath();
  p->drawPath(path);
  // Folded corner detail
  QPainterPath fold;
  fold.moveTo(15.0, 3.0);
  fold.lineTo(15.0, 7.5);
  fold.lineTo(19.5, 7.5);
  p->drawPath(fold);
  // Three text lines inside
  p->drawLine(QPointF(9.0, 12.0), QPointF(17.0, 12.0));
  p->drawLine(QPointF(9.0, 15.0), QPointF(17.0, 15.0));
  p->drawLine(QPointF(9.0, 18.0), QPointF(14.0, 18.0));
}

// Variant for .blop notes - same silhouette but with a small accent dot
// so users can distinguish file types at a glance.
void paintNoteBlop(QPainter *p, const QColor &c) {
  paintNote(p, c);
  setupFillBrush(p, c);
  p->drawEllipse(QPointF(17.5, 5.5), 1.2, 1.2);
}

// Standard Material folder with raised tab.
void paintFolder(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  QPainterPath path;
  path.moveTo(3.0, 6.5);
  path.lineTo(3.0, 19.0);
  path.quadTo(3.0, 20.5, 4.5, 20.5);
  path.lineTo(19.5, 20.5);
  path.quadTo(21.0, 20.5, 21.0, 19.0);
  path.lineTo(21.0, 9.5);
  path.quadTo(21.0, 8.0, 19.5, 8.0);
  path.lineTo(11.5, 8.0);
  path.lineTo(9.5, 6.0);
  path.lineTo(4.5, 6.0);
  path.quadTo(3.0, 6.0, 3.0, 6.5);
  path.closeSubpath();
  p->drawPath(path);
}

// Three horizontal bars (hamburger).
void paintMenu(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 2.2);
  p->drawLine(QPointF(4.0, 7.0), QPointF(20.0, 7.0));
  p->drawLine(QPointF(4.0, 12.0), QPointF(20.0, 12.0));
  p->drawLine(QPointF(4.0, 17.0), QPointF(20.0, 17.0));
}

// Three vertical dots.
void paintMoreVert(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  p->drawEllipse(QPointF(12.0, 5.5), 1.6, 1.6);
  p->drawEllipse(QPointF(12.0, 12.0), 1.6, 1.6);
  p->drawEllipse(QPointF(12.0, 18.5), 1.6, 1.6);
}

// Three horizontal dots (used for inline overflow menus).
void paintMoreHorz(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  p->drawEllipse(QPointF(5.5, 12.0), 1.6, 1.6);
  p->drawEllipse(QPointF(12.0, 12.0), 1.6, 1.6);
  p->drawEllipse(QPointF(18.5, 12.0), 1.6, 1.6);
}

void paintHome(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  QPainterPath path;
  path.moveTo(12.0, 3.0);
  path.lineTo(2.5, 11.0);
  path.lineTo(4.5, 11.0);
  path.lineTo(4.5, 20.5);
  path.lineTo(10.0, 20.5);
  path.lineTo(10.0, 14.0);
  path.lineTo(14.0, 14.0);
  path.lineTo(14.0, 20.5);
  path.lineTo(19.5, 20.5);
  path.lineTo(19.5, 11.0);
  path.lineTo(21.5, 11.0);
  path.closeSubpath();
  p->drawPath(path);
}

void paintSettings(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 1.8);
  // Outer cog: 8 rounded teeth
  QPainterPath path;
  const QPointF center(12.0, 12.0);
  const qreal innerR = 4.5;
  const qreal outerR = 7.5;
  for (int i = 0; i < 8; ++i) {
    const qreal a0 = i * M_PI / 4.0 - M_PI / 16.0;
    const qreal a1 = i * M_PI / 4.0 + M_PI / 16.0;
    const QPointF i0(center.x() + innerR * std::cos(a0),
                     center.y() + innerR * std::sin(a0));
    const QPointF o0(center.x() + outerR * std::cos(a0),
                     center.y() + outerR * std::sin(a0));
    const QPointF o1(center.x() + outerR * std::cos(a1),
                     center.y() + outerR * std::sin(a1));
    const QPointF i1(center.x() + innerR * std::cos(a1),
                     center.y() + innerR * std::sin(a1));
    if (i == 0)
      path.moveTo(i0);
    else
      path.lineTo(i0);
    path.lineTo(o0);
    path.lineTo(o1);
    path.lineTo(i1);
    // Arc back along the inner radius to next tooth start.
    const qreal nextStart = (i + 1) * M_PI / 4.0 - M_PI / 16.0;
    const QPointF iNext(center.x() + innerR * std::cos(nextStart),
                        center.y() + innerR * std::sin(nextStart));
    path.lineTo(iNext);
  }
  path.closeSubpath();
  p->drawPath(path);
  // Inner ring
  p->drawEllipse(center, 2.5, 2.5);
}

void paintSearch(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 2.2);
  p->drawEllipse(QPointF(10.5, 10.5), 5.5, 5.5);
  p->drawLine(QPointF(14.5, 14.5), QPointF(20.0, 20.0));
}

void paintAdd(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 2.4);
  p->drawLine(QPointF(12.0, 4.0), QPointF(12.0, 20.0));
  p->drawLine(QPointF(4.0, 12.0), QPointF(20.0, 12.0));
}

void paintClose(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 2.2);
  p->drawLine(QPointF(5.5, 5.5), QPointF(18.5, 18.5));
  p->drawLine(QPointF(18.5, 5.5), QPointF(5.5, 18.5));
}

void paintArrowLeft(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 2.2);
  p->drawLine(QPointF(20.0, 12.0), QPointF(5.0, 12.0));
  p->drawLine(QPointF(5.0, 12.0), QPointF(11.0, 6.0));
  p->drawLine(QPointF(5.0, 12.0), QPointF(11.0, 18.0));
}

void paintExport(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 1.8);
  // Box with arrow exiting upward
  p->drawLine(QPointF(5.0, 11.0), QPointF(5.0, 20.0));
  p->drawLine(QPointF(5.0, 20.0), QPointF(19.0, 20.0));
  p->drawLine(QPointF(19.0, 20.0), QPointF(19.0, 11.0));
  // Up arrow
  p->drawLine(QPointF(12.0, 4.0), QPointF(12.0, 15.0));
  p->drawLine(QPointF(12.0, 4.0), QPointF(8.0, 8.0));
  p->drawLine(QPointF(12.0, 4.0), QPointF(16.0, 8.0));
}

void paintShare(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  p->drawEllipse(QPointF(6.0, 12.0), 2.2, 2.2);
  p->drawEllipse(QPointF(18.0, 6.0), 2.2, 2.2);
  p->drawEllipse(QPointF(18.0, 18.0), 2.2, 2.2);
  setupStrokePen(p, c, 1.6);
  p->drawLine(QPointF(8.0, 11.0), QPointF(16.0, 7.0));
  p->drawLine(QPointF(8.0, 13.0), QPointF(16.0, 17.0));
}

void paintDevice(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 1.8);
  // Phone outline
  QRectF body(8.0, 3.0, 8.0, 18.0);
  p->drawRoundedRect(body, 1.5, 1.5);
  setupFillBrush(p, c);
  p->drawEllipse(QPointF(12.0, 18.5), 0.8, 0.8);
}

void paintCloud(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  QPainterPath path;
  path.moveTo(7.0, 18.0);
  path.quadTo(3.0, 18.0, 3.0, 14.0);
  path.quadTo(3.0, 10.5, 6.5, 10.0);
  path.quadTo(7.5, 6.0, 12.0, 6.0);
  path.quadTo(16.5, 6.0, 17.5, 10.5);
  path.quadTo(21.0, 11.0, 21.0, 14.5);
  path.quadTo(21.0, 18.0, 17.0, 18.0);
  path.closeSubpath();
  p->drawPath(path);
}

void paintDrive(QPainter *p, const QColor &c) {
  // Stylised triangle (Google Drive-ish)
  setupFillBrush(p, c);
  QPolygonF poly;
  poly << QPointF(8.0, 4.0) << QPointF(16.0, 4.0) << QPointF(20.0, 11.0)
       << QPointF(12.0, 11.0);
  p->drawPolygon(poly);
  poly.clear();
  poly << QPointF(4.0, 11.0) << QPointF(12.0, 11.0) << QPointF(8.0, 18.0)
       << QPointF(0.0, 18.0);
  p->drawPolygon(poly);
}

void paintDropbox(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  QPolygonF top1;
  top1 << QPointF(5.0, 5.0) << QPointF(11.0, 9.0) << QPointF(5.0, 13.0)
       << QPointF(-1.0, 9.0);
  QPolygonF top2;
  top2 << QPointF(13.0, 5.0) << QPointF(19.0, 9.0) << QPointF(13.0, 13.0)
       << QPointF(7.0, 9.0);
  p->drawPolygon(top1.translated(2.0, 1.0));
  p->drawPolygon(top2.translated(2.0, 1.0));
  QPolygonF bottom;
  bottom << QPointF(7.0, 16.0) << QPointF(12.0, 19.0) << QPointF(17.0, 16.0)
         << QPointF(12.0, 13.0);
  p->drawPolygon(bottom);
}

void paintOneDrive(QPainter *p, const QColor &c) {
  setupFillBrush(p, c);
  QPainterPath path;
  path.moveTo(5.0, 18.0);
  path.quadTo(2.0, 18.0, 2.0, 15.0);
  path.quadTo(2.0, 12.0, 5.0, 11.5);
  path.quadTo(6.0, 7.5, 11.0, 7.5);
  path.quadTo(15.0, 7.5, 16.5, 11.5);
  path.quadTo(22.0, 11.5, 22.0, 15.5);
  path.quadTo(22.0, 18.0, 18.0, 18.0);
  path.closeSubpath();
  p->drawPath(path);
}

void paintPages(QPainter *p, const QColor &c) {
  setupStrokePen(p, c, 1.8);
  QRectF back(7.0, 3.0, 13.0, 17.0);
  QRectF front(4.0, 5.0, 13.0, 17.0);
  p->drawRoundedRect(back, 1.5, 1.5);
  setupFillBrush(p, c);
  p->drawRoundedRect(front, 1.5, 1.5);
}

// Keep "<name>_pill" entries pointing at a clean glyph (no capsule
// background) - the host QToolButton already provides the pill-shaped
// background through its stylesheet, so a second one would double up.
void paintPillMenu(QPainter *p, const QColor &c) { paintMenu(p, c); }
void paintPillSettings(QPainter *p, const QColor &c) { paintSettings(p, c); }
void paintPillPages(QPainter *p, const QColor &c) { paintPages(p, c); }
void paintPillMore(QPainter *p, const QColor &c) { paintMoreHorz(p, c); }

// Dispatch table - keep this in alphabetical order so it stays scannable.
using PaintFn = void (*)(QPainter *, const QColor &);
PaintFn dispatch(const QString &name) {
  if (name == QLatin1String("add"))
    return paintAdd;
  if (name == QLatin1String("arrow_left"))
    return paintArrowLeft;
  if (name == QLatin1String("close"))
    return paintClose;
  if (name == QLatin1String("cloud"))
    return paintCloud;
  if (name == QLatin1String("device"))
    return paintDevice;
  if (name == QLatin1String("drive"))
    return paintDrive;
  if (name == QLatin1String("dropbox"))
    return paintDropbox;
  if (name == QLatin1String("export"))
    return paintExport;
  if (name == QLatin1String("folder"))
    return paintFolder;
  if (name == QLatin1String("home"))
    return paintHome;
  if (name == QLatin1String("menu"))
    return paintMenu;
  if (name == QLatin1String("menu_pill"))
    return paintPillMenu;
  if (name == QLatin1String("more_horz"))
    return paintMoreHorz;
  if (name == QLatin1String("more_pill"))
    return paintPillMore;
  if (name == QLatin1String("more_vert"))
    return paintMoreVert;
  if (name == QLatin1String("note_blop"))
    return paintNoteBlop;
  if (name == QLatin1String("note_bnote"))
    return paintNote;
  if (name == QLatin1String("onedrive"))
    return paintOneDrive;
  if (name == QLatin1String("pages"))
    return paintPages;
  if (name == QLatin1String("pages_pill"))
    return paintPillPages;
  if (name == QLatin1String("search"))
    return paintSearch;
  if (name == QLatin1String("settings"))
    return paintSettings;
  if (name == QLatin1String("settings_pill"))
    return paintPillSettings;
  if (name == QLatin1String("share"))
    return paintShare;
  return nullptr;
}

QPixmap render(const QString &name, const QColor &color, int pxSize) {
  pxSize = qMax(8, pxSize);
  QPixmap pm(pxSize, pxSize);
  pm.fill(Qt::transparent);
  PaintFn fn = dispatch(name);
  if (!fn)
    return pm; // Caller falls back to the Windows path; keeps callers safe.
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  // Map 24x24 viewport to the requested pixel size.
  const qreal s = pxSize / kViewport;
  p.scale(s, s);
  fn(&p, color.isValid() ? color : QColor(Qt::white));
  p.end();
  return pm;
}

} // namespace

QPixmap pixmap(const QString &name, const QColor &color, int pxSize) {
  pxSize = qMax(8, pxSize);
  CacheKey key{name, color.rgba(), pxSize};
  auto &c = cache();
  auto it = c.constFind(key);
  if (it != c.constEnd())
    return it.value();
  QPixmap pm = render(name, color, pxSize);
  c.insert(key, pm);
  return pm;
}

QIcon icon(const QString &name, const QColor &color, int dpSize) {
  // We bake the icon at 2x the requested dp value so ToolButtons that are
  // rendered larger than dpSize still get a sharp glyph. The cache key
  // already includes the px size, so this only generates one pixmap per
  // (name, colour) pair.
  const int px = qMax(16, dpSize * 2);
  QPixmap pm = pixmap(name, color, px);
  QIcon ic(pm);
  return ic;
}

} // namespace AndroidIcons
