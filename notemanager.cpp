#include "notemanager.h"
#include "util/Async.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QSaveFile>


NoteManager::NoteManager(QObject *parent) : QObject(parent) {}

void NoteManager::saveNoteAsync(const Note &note, const QString &path,
                                std::function<void(bool)> onDone) {
  fireAndForget([note, path, onDone]() {
    bool ok = saveNote(note, path);
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
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  auto doc = toJson(note);
  file.write(doc.toJson(QJsonDocument::Compact));
  return file.commit();
}

bool NoteManager::loadNote(const QString &path, Note &out) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  auto doc = QJsonDocument::fromJson(f.readAll());
  return fromJson(doc, out);
}

QJsonDocument NoteManager::toJson(const Note &note) {
  QJsonObject root;
  root["id"] = note.id;
  root["title"] = note.title;
  QJsonArray pagesArr;
  for (const auto &p : note.pages) {
    QJsonArray strokesArr;
    for (const auto &s : p.strokes) {
      QJsonObject so;
      so["w"] = s.width;
      so["c"] = s.color.name();
      so["e"] = s.isEraser;
      QJsonArray pts;
      for (const auto &pt : s.points) {
        QJsonArray a;
        a.append(pt.x());
        a.append(pt.y());
        pts.append(a);
      }
      so["pts"] = pts;
      strokesArr.append(so);
    }
    QJsonObject pageObj;
    pageObj["strokes"] = strokesArr;
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
  out.pages.clear();
  auto pagesArr = root.value("pages").toArray();
  out.pages.resize(pagesArr.size());
  for (int i = 0; i < pagesArr.size(); ++i) {
    auto pageObj = pagesArr[i].toObject();
    auto strokesArr = pageObj.value("strokes").toArray();
    for (const auto &sv : strokesArr) {
      auto so = sv.toObject();
      Stroke s;
      s.width = so.value("w").toDouble(2.0);
      s.color = QColor(so.value("c").toString("#000000"));
      s.isEraser = so.value("e").toBool(false);
      auto pts = so.value("pts").toArray();
      for (const auto &pv : pts) {
        auto a = pv.toArray();
        if (a.size() == 2)
          s.points.push_back(QPointF(a[0].toDouble(), a[1].toDouble()));
      }
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
  }
  return true;
}
