#include "notemanager.h"
#include "tools/math/MathExpressionParser.h"
#include "util/Async.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QSaveFile>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include "atomicfile.h"


NoteManager::NoteManager(QObject *parent) : QObject(parent) {}

namespace {
QMutex g_saveMutex;
QHash<QString, quint64> g_issuedSeq;
QHash<QString, quint64> g_writtenSeq;
QHash<QString, qint64> g_lastBackupMs;
constexpr qint64 kBackupIntervalMs = 60 * 1000;
} // namespace

quint64 NoteManager::nextSeq(const QString &path) {
  const QString key = QFileInfo(path).absoluteFilePath();
  QMutexLocker l(&g_saveMutex);
  return ++g_issuedSeq[key];
}

void NoteManager::saveNoteAsync(const Note &note, const QString &path,
                                std::function<void(bool)> onDone) {
  // Sequence is taken on the caller thread so a slower, older snapshot can
  // never overwrite a newer one that was queued later.
  const quint64 seq = nextSeq(path);
  fireAndForget([note, path, onDone, seq]() {
    bool ok = writeNote(note, path, seq);
    if (onDone)
      QMetaObject::invokeMethod(
          qApp, [onDone, ok]() { onDone(ok); }, Qt::QueuedConnection);
  });
}

void NoteManager::loadNoteAsync(const QString &path,
                                std::function<void(bool, Note)> onDone) {
  fireAndForget([path, onDone]() {
    Note n;
    bool ok = loadNote(path, n);
    if (onDone)
      QMetaObject::invokeMethod(
          qApp, [onDone, ok, n]() { onDone(ok, n); }, Qt::QueuedConnection);
  });
}

bool NoteManager::saveNote(const Note &note, const QString &path) {
  return writeNote(note, path, nextSeq(path));
}

bool NoteManager::writeNote(const Note &note, const QString &path,
                            quint64 seq) {
  // Android SAF `content://` tree URIs (and other remote schemes) are not
  // local files. mkpath() on them logs "Cannot create file, parent doesn't
  // exist or not a directory" and QSaveFile can abort the process.
  const int schemeEnd = path.indexOf(QLatin1String("://"));
  if (schemeEnd > 0 &&
      path.left(schemeEnd).compare(QLatin1String("file"), Qt::CaseInsensitive) !=
          0) {
    qWarning() << "NoteManager: refusing to save to non-filesystem path"
               << path;
    return false;
  }

  // Ensure the parent directory exists before attempting to write; otherwise
  // saving to a freshly created sub-folder or cloud mirror path can silently fail.
  QDir().mkpath(QFileInfo(path).absolutePath());

  const QString key = QFileInfo(path).absoluteFilePath();
  const QByteArray bytes = toJson(note).toJson(QJsonDocument::Compact);

  QMutexLocker lock(&g_saveMutex);
  if (g_writtenSeq.value(key, 0) > seq)
    return true; // a newer snapshot was already written
  // Rolling backup of the previous version (at most once a minute per note).
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (nowMs - g_lastBackupMs.value(key, 0) > kBackupIntervalMs) {
    AtomicFile::rotateBackup(path);
    g_lastBackupMs.insert(key, nowMs);
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(bytes) != bytes.size() || !file.commit()) {
    qWarning() << "NoteManager: save failed" << path << file.errorString();
    SaveStatus::instance().reportFailure(
        note.title.isEmpty() ? QFileInfo(path).completeBaseName() : note.title,
        path);
    return false;
  }
  g_writtenSeq.insert(key, seq);
  return true;
}

bool NoteManager::loadNoteFile(const QString &path, Note &out) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  auto doc = QJsonDocument::fromJson(f.readAll());
  return fromJson(doc, out);
}

bool NoteManager::loadNote(const QString &path, Note &out) {
  if (loadNoteFile(path, out))
    return true;
  // The file exists but cannot be parsed (truncated/corrupt): try the backup.
  const QFileInfo fi(path);
  if (fi.exists() && fi.size() > 0) {
    const QString bak = path + QStringLiteral(".bak");
    if (QFileInfo::exists(bak) && loadNoteFile(bak, out)) {
      qWarning() << "NoteManager: restored from backup" << bak;
      // Keep the corrupt file for forensics and put the good copy in place,
      // so the next rolling backup does not overwrite .bak with garbage.
      const QString corrupt = path + QStringLiteral(".corrupt");
      QFile::remove(corrupt);
      QFile::rename(path, corrupt);
      QFile::copy(bak, path);
      SaveStatus::instance().reportRecovered(path);
      return true;
    }
  }
  return false;
}

QJsonDocument NoteManager::toJson(const Note &note) {
  QJsonObject root;
  // Schema 2: adds per-page "images". Older files simply lack the key.
  root["schema"] = 2;
  root["id"] = note.id;
  root["title"] = note.title;
  if (!note.pages.isEmpty()) {
    QJsonObject cover;
    cover["bg"] = note.pages.first().backgroundType;
    if (note.pages.first().paperColor.isValid())
      cover["paper"] = note.pages.first().paperColor.name(QColor::HexRgb);
    root["cover"] = cover;
  }
  {
    QJsonArray tagsArr;
    for (const QString &t : note.tags)
      tagsArr.append(t);
    root["tags"] = tagsArr;
  }
  QJsonArray pagesArr;
  for (const auto &p : note.pages) {
    QJsonArray strokesArr;
    for (const auto &s : p.strokes) {
      QJsonObject so;
      so["w"] = s.width;
      so["c"] = s.color.name(QColor::HexArgb);
      so["e"] = s.isEraser;
      so["h"] = s.isHighlighter;
      if (s.filled) {
        // Filled remnant of a pixel-erased stroke: serialise path elements
        // as [type, x, y] triples (0=move, 1=line, 2=curve, 3=curve data).
        so["f"] = true;
        so["fr"] = static_cast<int>(s.path.fillRule());
        QJsonArray els;
        for (int i = 0; i < s.path.elementCount(); ++i) {
          const QPainterPath::Element el = s.path.elementAt(i);
          QJsonArray a;
          a.append(static_cast<int>(el.type));
          a.append(el.x);
          a.append(el.y);
          els.append(a);
        }
        so["fp"] = els;
        strokesArr.append(so);
        continue;
      }
      QJsonArray pts;
      const bool hasPressure = s.pressures.size() == s.points.size();
      for (int i = 0; i < s.points.size(); ++i) {
        QJsonArray a;
        a.append(s.points[i].x());
        a.append(s.points[i].y());
        if (hasPressure)
          a.append(s.pressures[i]);
        pts.append(a);
      }
      so["pts"] = pts;
      strokesArr.append(so);
    }
    QJsonObject pageObj;
    pageObj["strokes"] = strokesArr;
    QJsonArray graphsArr;
    for (const auto& g : p.graphs) {
      QJsonObject go;
      go["x"] = g.rect.x();
      go["y"] = g.rect.y();
      go["w"] = g.rect.width();
      go["h"] = g.rect.height();
      go["sel"] = g.selectedFunction;
      go["xmin"] = g.xMin;
      go["xmax"] = g.xMax;
      go["ymin"] = g.yMin;
      go["ymax"] = g.yMax;
      go["xtm"] = g.xTickMode;
      go["ytm"] = g.yTickMode;
      go["xts"] = g.xTickStep;
      go["yts"] = g.yTickStep;
      go["xtc"] = g.xTickCount;
      go["ytc"] = g.yTickCount;
      QJsonArray fnArr;
      for (const auto& fn : g.functions) {
        QJsonObject fo;
        fo["expr"] = fn.expression;
        fo["color"] = fn.color.name(QColor::HexRgb);
        fo["visible"] = fn.visible;
        fo["der"] = fn.showDerivative;
        fo["roots"] = fn.showRoots;
        fo["ext"] = fn.showExtrema;
        fo["tan"] = fn.showTangent;
        fo["tanx"] = fn.tangentX;
        fo["isDerCurve"] = fn.isDerivativeCurve;
        fo["srcExpr"] = fn.sourceExpression;
        fo["rootColor"] = fn.rootMarkerColor.name(QColor::HexRgb);
        fo["extColor"] = fn.extremaMarkerColor.name(QColor::HexRgb);
        fnArr.append(fo);
      }
      go["fns"] = fnArr;
      graphsArr.append(go);
    }
    pageObj["graphs"] = graphsArr;
    QJsonArray stickiesArr;
    for (const auto &sn : p.stickies) {
      QJsonObject so;
      so["x"] = sn.pos.x();
      so["y"] = sn.pos.y();
      so["w"] = sn.width;
      so["h"] = sn.height;
      so["t"] = sn.text;
      so["c"] = sn.color.name(QColor::HexArgb);
      so["fs"] = sn.fontPointSize;
      stickiesArr.append(so);
    }
    pageObj["stickies"] = stickiesArr;
    QJsonArray textsArr;
    for (const auto &t : p.texts) {
      QJsonObject to;
      to["x"] = t.pos.x();
      to["y"] = t.pos.y();
      to["w"] = t.width;
      to["text"] = t.text;
      if (!t.html.isEmpty())
        to["html"] = t.html;
      if (t.freeMove)
        to["free"] = true;
      to["c"] = t.color.name(QColor::HexArgb);
      to["font"] = t.fontFamily;
      to["size"] = t.fontPointSize;
      textsArr.append(to);
    }
    pageObj["texts"] = textsArr;
    if (!p.images.isEmpty()) {
      QJsonArray imagesArr;
      for (const auto &im : p.images) {
        if (im.image.isNull())
          continue;
        QByteArray bytes;
        QBuffer buf(&bytes);
        if (!buf.open(QIODevice::WriteOnly) || !im.image.save(&buf, "PNG"))
          continue;
        QJsonObject io;
        io["x"] = im.pos.x();
        io["y"] = im.pos.y();
        io["w"] = im.width;
        io["h"] = im.height;
        io["op"] = im.opacity;
        io["rot"] = im.rotation;
        io["img"] = QString::fromLatin1(bytes.toBase64());
        imagesArr.append(io);
      }
      pageObj["images"] = imagesArr;
    }
    pageObj["bg"] = p.backgroundType;
    if (!p.title.isEmpty())
      pageObj["title"] = p.title;
    pageObj["rot"] = p.rotationDegrees;
    pageObj["bm"] = p.bookmarked;
    if (p.paperColor.isValid())
      pageObj["paper"] = p.paperColor.name(QColor::HexRgb);
    if (!p.backgroundImage.isNull()) {
      QByteArray bytes;
      QBuffer buf(&bytes);
      if (buf.open(QIODevice::WriteOnly) &&
          p.backgroundImage.save(&buf, "PNG"))
        pageObj["bgImg"] = QString::fromLatin1(bytes.toBase64());
    }
    pagesArr.append(pageObj);
  }
  root["pages"] = pagesArr;
  return QJsonDocument(root);
}

bool NoteManager::fromJson(const QJsonDocument &doc, Note &out) {
  if (doc.isNull() || !doc.isObject())
    return false;
  auto root = doc.object();
  out.id = root.value("id").toString();
  out.title = root.value("title").toString();
  out.tags.clear();
  for (const auto &tv : root.value("tags").toArray())
    out.tags.append(tv.toString());
  out.pages.clear();
  auto pagesArr = root.value("pages").toArray();
  out.pages.resize(pagesArr.size());
  for (int i = 0; i < pagesArr.size(); ++i) {
    auto pageObj = pagesArr[i].toObject();
    out.pages[i].backgroundType = pageObj.value("bg").toInt(2);
    out.pages[i].title = pageObj.value("title").toString(
        QStringLiteral("Seite %1").arg(i + 1));
    out.pages[i].rotationDegrees = pageObj.value("rot").toInt(0);
    out.pages[i].bookmarked = pageObj.value("bm").toBool(false);
    {
      const QString pc = pageObj.value("paper").toString();
      if (!pc.isEmpty()) {
        QColor c(pc);
        if (c.isValid())
          out.pages[i].paperColor = c;
      }
    }
    if (pageObj.contains("bgImg")) {
      QByteArray raw =
          QByteArray::fromBase64(pageObj.value("bgImg").toString().toLatin1());
      if (!raw.isEmpty())
        out.pages[i].backgroundImage.loadFromData(raw, "PNG");
    }
    auto strokesArr = pageObj.value("strokes").toArray();
    for (const auto &sv : strokesArr) {
      auto so = sv.toObject();
      Stroke s;
      s.width = so.value("w").toDouble(2.0);
      s.color = QColor(so.value("c").toString("#000000"));
      s.isEraser = so.value("e").toBool(false);
      s.isHighlighter = so.value("h").toBool(false);
      if (so.value("f").toBool(false)) {
        s.filled = true;
        QPainterPath fp;
        fp.setFillRule(so.value("fr").toInt(0) == 1 ? Qt::WindingFill
                                                    : Qt::OddEvenFill);
        const auto els = so.value("fp").toArray();
        for (int k = 0; k < els.size(); ++k) {
          const auto a = els[k].toArray();
          if (a.size() < 3)
            continue;
          const QPointF pt(a[1].toDouble(), a[2].toDouble());
          switch (a[0].toInt()) {
          case QPainterPath::MoveToElement:
            fp.moveTo(pt);
            break;
          case QPainterPath::LineToElement:
            fp.lineTo(pt);
            break;
          case QPainterPath::CurveToElement: {
            // Followed by two CurveToData elements (control 2, end point).
            if (k + 2 < els.size()) {
              const auto b = els[k + 1].toArray();
              const auto c = els[k + 2].toArray();
              if (b.size() >= 3 && c.size() >= 3)
                fp.cubicTo(pt, QPointF(b[1].toDouble(), b[2].toDouble()),
                           QPointF(c[1].toDouble(), c[2].toDouble()));
              k += 2;
            }
            break;
          }
          default:
            break;
          }
        }
        s.path = fp;
        out.pages[i].strokes.push_back(std::move(s));
        continue;
      }
      auto pts = so.value("pts").toArray();
      bool anyPressure = false;
      for (const auto &pv : pts) {
        auto a = pv.toArray();
        if (a.size() >= 2) {
          s.points.push_back(QPointF(a[0].toDouble(), a[1].toDouble()));
          const qreal pr = a.size() >= 3 ? a[2].toDouble(1.0) : 1.0;
          s.pressures.push_back(pr);
          if (pr < 1.0)
            anyPressure = true;
        }
      }
      if (!anyPressure)
        s.pressures.clear();
      // Rebuild path
      QPainterPath path;
      if (!s.points.isEmpty()) {
        path.moveTo(s.points[0]);
        for (int k = 1; k < s.points.size(); ++k)
          path.lineTo(s.points[k]);
      }
      s.path = path;
      out.pages[i].strokes.push_back(std::move(s));
    }
    const auto graphsArr = pageObj.value("graphs").toArray();
    for (const auto& gv : graphsArr) {
      const auto go = gv.toObject();
      GraphObject g;
      const double gx = go.value("x").toDouble(0.0);
      const double gy = go.value("y").toDouble(0.0);
      const double gw = go.value("w").toDouble(280.0);
      const double gh = go.value("h").toDouble(180.0);
      g.rect = QRectF(gx, gy, gw, gh);
      g.selectedFunction = go.value("sel").toInt(0);
      g.xMin = go.value("xmin").toDouble(-10.0);
      g.xMax = go.value("xmax").toDouble(10.0);
      g.yMin = go.value("ymin").toDouble(-10.0);
      g.yMax = go.value("ymax").toDouble(10.0);
      g.xTickMode = go.value("xtm").toInt(0);
      g.yTickMode = go.value("ytm").toInt(0);
      g.xTickStep = go.value("xts").toDouble(1.0);
      g.yTickStep = go.value("yts").toDouble(1.0);
      g.xTickCount = go.value("xtc").toInt(8);
      g.yTickCount = go.value("ytc").toInt(8);
      g.functions.clear();
      const auto fnArr = go.value("fns").toArray();
      for (const auto& fv : fnArr) {
        const auto fo = fv.toObject();
        GraphFunction fn;
        fn.expression = fo.value("expr").toString("");
        fn.color = QColor(fo.value("color").toString("#5e5ce6"));
        fn.visible = fo.value("visible").toBool(true);
        fn.showDerivative = fo.value("der").toBool(false);
        fn.showRoots = fo.value("roots").toBool(false);
        fn.showExtrema = fo.value("ext").toBool(false);
        fn.showTangent = fo.value("tan").toBool(false);
        fn.tangentX = fo.value("tanx").toDouble(0.0);
        fn.isDerivativeCurve = fo.value("isDerCurve").toBool(false);
        fn.sourceExpression = fo.value("srcExpr").toString();
        if (fn.isDerivativeCurve) {
          if (fn.sourceExpression.isEmpty())
            fn.sourceExpression = fn.expression;
          const QString sym =
              MathExpressionParser::symbolicDerivativeString(fn.sourceExpression);
          if (!sym.isEmpty())
            fn.expression = sym;
          else if (!fn.sourceExpression.isEmpty() &&
                   (fn.expression == fn.sourceExpression ||
                    fn.expression.startsWith(QStringLiteral("d/dx("))))
            fn.expression = QStringLiteral("d/dx(%1)").arg(fn.sourceExpression);
        }
        fn.rootMarkerColor = QColor(fo.value("rootColor").toString("#e1585a"));
        fn.extremaMarkerColor = QColor(fo.value("extColor").toString("#46aa66"));
        g.functions.push_back(fn);
      }
      if (g.functions.isEmpty()) {
        g.selectedFunction = -1;
      } else if (g.selectedFunction < 0) {
        g.selectedFunction = 0;
      } else if (g.selectedFunction >= g.functions.size()) {
        g.selectedFunction = g.functions.size() - 1;
      }
      out.pages[i].graphs.push_back(std::move(g));
    }
    const auto stickiesArr = pageObj.value("stickies").toArray();
    for (const auto &sv : stickiesArr) {
      const auto so = sv.toObject();
      StickyNoteObject sn;
      sn.pos = QPointF(so.value("x").toDouble(0.0), so.value("y").toDouble(0.0));
      sn.width = so.value("w").toDouble(168.0);
      sn.height = so.value("h").toDouble(148.0);
      sn.text = so.value("t").toString();
      {
        const QString cn = so.value("c").toString();
        QColor c(cn);
        if (c.isValid())
          sn.color = c;
      }
      sn.fontPointSize = so.value("fs").toInt(14);
      out.pages[i].stickies.push_back(std::move(sn));
    }
    const auto textsArr = pageObj.value("texts").toArray();
    for (const auto &tv : textsArr) {
      const auto to = tv.toObject();
      TextObject t;
      t.pos = QPointF(to.value("x").toDouble(0.0), to.value("y").toDouble(0.0));
      t.width = to.value("w").toDouble(300.0);
      t.text = to.value("text").toString();
      t.html = to.value("html").toString();
      t.freeMove = to.value("free").toBool(false);
      {
        const QString cn = to.value("c").toString();
        QColor c(cn);
        if (c.isValid())
          t.color = c;
      }
      t.fontFamily = to.value("font").toString();
      t.fontPointSize = to.value("size").toInt(14);
      out.pages[i].texts.push_back(std::move(t));
    }
    const auto imagesArr = pageObj.value("images").toArray();
    for (const auto &iv : imagesArr) {
      const auto io = iv.toObject();
      const QByteArray raw =
          QByteArray::fromBase64(io.value("img").toString().toLatin1());
      ImageObject im;
      if (raw.isEmpty() || !im.image.loadFromData(raw, "PNG"))
        continue;
      im.pos = QPointF(io.value("x").toDouble(0.0), io.value("y").toDouble(0.0));
      im.width = io.value("w").toDouble(0.0);
      im.height = io.value("h").toDouble(0.0);
      im.opacity = io.value("op").toDouble(1.0);
      im.rotation = io.value("rot").toDouble(0.0);
      out.pages[i].images.push_back(std::move(im));
    }
  }
  return true;
}
