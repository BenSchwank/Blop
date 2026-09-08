#include "todostore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {
QString todoPath() {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/blop_todos.json");
}
} // namespace

QVector<TodoItem> TodoStore::load() {
  QFile f(todoPath());
  if (!f.open(QIODevice::ReadOnly))
    return {};
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isArray())
    return {};
  QVector<TodoItem> out;
  for (const QJsonValue &v : doc.array()) {
    const QJsonObject o = v.toObject();
    TodoItem t;
    t.id = o.value(QStringLiteral("id")).toString();
    t.title = o.value(QStringLiteral("title")).toString();
    t.done = o.value(QStringLiteral("done")).toBool(false);
    const QString due = o.value(QStringLiteral("due")).toString();
    if (!due.isEmpty())
      t.due = QDateTime::fromString(due, Qt::ISODate);
    const QString created = o.value(QStringLiteral("created")).toString();
    if (!created.isEmpty())
      t.created = QDateTime::fromString(created, Qt::ISODate);
    t.priority = o.value(QStringLiteral("priority")).toString();
    t.project = o.value(QStringLiteral("project")).toString();
    for (const QJsonValue &tag : o.value(QStringLiteral("tags")).toArray())
      t.tags.append(tag.toString());
    if (t.id.isEmpty())
      t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    out.append(t);
  }
  return out;
}

void TodoStore::save(const QVector<TodoItem> &items) {
  QJsonArray arr;
  for (const TodoItem &t : items) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), t.id);
    o.insert(QStringLiteral("title"), t.title);
    o.insert(QStringLiteral("done"), t.done);
    if (t.due.isValid())
      o.insert(QStringLiteral("due"), t.due.toString(Qt::ISODate));
    if (t.created.isValid())
      o.insert(QStringLiteral("created"), t.created.toString(Qt::ISODate));
    if (!t.priority.isEmpty())
      o.insert(QStringLiteral("priority"), t.priority);
    if (!t.project.isEmpty())
      o.insert(QStringLiteral("project"), t.project);
    if (!t.tags.isEmpty())
      o.insert(QStringLiteral("tags"), QJsonArray::fromStringList(t.tags));
    arr.append(o);
  }
  QFile f(todoPath());
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

TodoItem TodoStore::add(const QString &title, const QDateTime &due,
                        const QString &priority, const QString &project,
                        const QStringList &tags) {
  auto items = load();
  TodoItem t;
  t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  t.title = title.trimmed();
  t.created = QDateTime::currentDateTime();
  t.due = due;
  t.priority = priority;
  t.project = project.trimmed();
  t.tags = tags;
  items.prepend(t);
  save(items);
  return t;
}

void TodoStore::update(const TodoItem &item) {
  auto items = load();
  for (TodoItem &current : items) {
    if (current.id == item.id) {
      current = item;
      break;
    }
  }
  save(items);
}

void TodoStore::setDone(const QString &id, bool done) {
  auto items = load();
  for (TodoItem &t : items) {
    if (t.id == id) {
      t.done = done;
      break;
    }
  }
  save(items);
}

void TodoStore::remove(const QString &id) {
  auto items = load();
  items.erase(std::remove_if(items.begin(), items.end(),
                             [&](const TodoItem &t) { return t.id == id; }),
              items.end());
  save(items);
}

void TodoStore::updateTitle(const QString &id, const QString &title) {
  auto items = load();
  for (TodoItem &t : items) {
    if (t.id == id) {
      t.title = title.trimmed();
      break;
    }
  }
  save(items);
}
