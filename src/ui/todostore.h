#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

struct TodoItem {
  QString id;
  QString title;
  bool done{false};
  QDateTime due; // invalid = none
  QDateTime created;
  QString priority;
  QString project;
  QStringList tags;
};

class TodoStore {
public:
  static QVector<TodoItem> load();
  static void save(const QVector<TodoItem> &items);
  static TodoItem add(const QString &title, const QDateTime &due = QDateTime(),
                      const QString &priority = QString(),
                      const QString &project = QString(),
                      const QStringList &tags = {});
  static void update(const TodoItem &item);
  static void setDone(const QString &id, bool done);
  static void remove(const QString &id);
  static void updateTitle(const QString &id, const QString &title);
};
