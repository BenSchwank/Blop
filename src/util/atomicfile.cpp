#include "atomicfile.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QSaveFile>

namespace AtomicFile {

bool write(const QString &path, const QByteArray &data, const QString &what) {
  if (path.isEmpty())
    return false;
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() ||
      !f.commit()) {
    qWarning() << "AtomicFile: write failed" << path << f.errorString();
    SaveStatus::instance().reportFailure(
        what.isEmpty() ? QFileInfo(path).fileName() : what, path);
    return false;
  }
  return true;
}

void rotateBackup(const QString &path) {
  QFileInfo fi(path);
  if (!fi.exists() || fi.size() <= 0)
    return;
  const QString bak = path + QStringLiteral(".bak");
  QFile::remove(bak);
  if (!QFile::copy(path, bak))
    qWarning() << "AtomicFile: backup failed" << path;
}

} // namespace AtomicFile

SaveStatus &SaveStatus::instance() {
  static SaveStatus s;
  static const bool moved = [] {
    if (QCoreApplication::instance())
      s.moveToThread(QCoreApplication::instance()->thread());
    return true;
  }();
  Q_UNUSED(moved);
  return s;
}

void SaveStatus::reportRecovered(const QString &path) {
  QMetaObject::invokeMethod(
      this, [this, path]() { emit recovered(path); }, Qt::QueuedConnection);
}

void SaveStatus::reportFailure(const QString &what, const QString &path) {
  QMetaObject::invokeMethod(
      this, [this, what, path]() { emit failed(what, path); },
      Qt::QueuedConnection);
}
