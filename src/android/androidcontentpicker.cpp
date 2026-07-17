#include "androidcontentpicker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>
#endif

namespace {
#ifdef Q_OS_ANDROID
extern "C" JNIEXPORT void JNICALL
Java_com_benschwank_blop_BlopContentPickerBridge_nativeNotifyPicked(
    JNIEnv *env, jclass /*clazz*/, jstring path) {
  QString s;
  if (path) {
    const char *raw = env->GetStringUTFChars(path, nullptr);
    s = QString::fromUtf8(raw ? raw : "");
    env->ReleaseStringUTFChars(path, raw);
  }
  // Always hop to Qt thread — JNI may fire on Android UI thread.
  QMetaObject::invokeMethod(
      qApp,
      [s]() { AndroidContentPicker::instance().handlePickedPath(s); },
      Qt::QueuedConnection);
}
#endif
} // namespace

AndroidContentPicker &AndroidContentPicker::instance() {
  static AndroidContentPicker inst;
  return inst;
}

AndroidContentPicker::AndroidContentPicker(QObject *parent)
    : QObject(parent) {
#ifdef Q_OS_ANDROID
  static bool s_jniRegistered = false;
  if (!s_jniRegistered) {
    QJniEnvironment env;
    if (env.registerNativeMethods(
            "com/benschwank/blop/BlopContentPickerBridge",
            {{"nativeNotifyPicked", "(Ljava/lang/String;)V",
              reinterpret_cast<void *>(
                  &Java_com_benschwank_blop_BlopContentPickerBridge_nativeNotifyPicked)}})) {
      s_jniRegistered = true;
      qInfo() << "AndroidContentPicker: JNI registered";
    } else {
      qWarning() << "AndroidContentPicker: JNI registration failed";
    }
  }
  QJniObject pending = QJniObject::callStaticObjectMethod(
      "com/benschwank/blop/BlopContentPickerBridge", "consumePendingPath",
      "()Ljava/lang/String;");
  if (pending.isValid()) {
    const QString s = pending.toString();
    if (!s.isEmpty())
      handlePickedPath(s);
  }
#endif
}

void AndroidContentPicker::handlePickedPath(const QString &path) {
  Callback cb = std::move(m_pending);
  m_pending = {};
  if (m_saveMode && !path.isEmpty())
    m_pendingSaveCache = path;
  m_saveMode = false;
  if (cb)
    cb(path);
}

void AndroidContentPicker::pickOpen(const QStringList &mimeFilters,
                                    Callback cb) {
#ifndef Q_OS_ANDROID
  Q_UNUSED(mimeFilters);
  if (cb)
    cb(QString());
  return;
#else
  if (m_pending) {
    // Cancel previous outstanding pick.
    Callback old = std::move(m_pending);
    m_pending = {};
    if (old)
      old(QString());
  }
  m_pending = std::move(cb);
  m_saveMode = false;
  const QString joined = mimeFilters.join(QLatin1Char(';'));
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([joined]() {
    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopActivity", "openDocument",
        "(Ljava/lang/String;)V",
        QJniObject::fromString(joined).object<jstring>());
  });
#endif
}

void AndroidContentPicker::pickSave(const QString &mimeType,
                                    const QString &suggestedName,
                                    Callback cb) {
#ifndef Q_OS_ANDROID
  Q_UNUSED(mimeType);
  Q_UNUSED(suggestedName);
  if (cb)
    cb(QString());
  return;
#else
  if (m_pending) {
    Callback old = std::move(m_pending);
    m_pending = {};
    if (old)
      old(QString());
  }
  m_pending = std::move(cb);
  m_saveMode = true;
  // Pre-create a cache file path; Java CREATE_DOCUMENT will give us a URI,
  // copy empty placeholder, then we write into cache and finishSave copies out.
  const QString cacheDir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      QStringLiteral("/blop_picker");
  QDir().mkpath(cacheDir);
  const QString base =
      suggestedName.isEmpty()
          ? QStringLiteral("blop_export_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
          : QFileInfo(suggestedName).fileName();
  m_pendingSaveCache = cacheDir + QLatin1Char('/') + base;
  QFile f(m_pendingSaveCache);
  if (f.open(QIODevice::WriteOnly))
    f.close();

  const QString cachePath = m_pendingSaveCache;
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
      [mimeType, suggestedName, cachePath]() {
        QJniObject::callStaticMethod<void>(
            "com/benschwank/blop/BlopActivity", "createDocument",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(mimeType).object<jstring>(),
            QJniObject::fromString(suggestedName).object<jstring>(),
            QJniObject::fromString(cachePath).object<jstring>());
      });
#endif
}

void AndroidContentPicker::finishSave(const QString &cachePath) {
#ifndef Q_OS_ANDROID
  Q_UNUSED(cachePath);
#else
  const QString path =
      cachePath.isEmpty() ? m_pendingSaveCache : cachePath;
  if (path.isEmpty())
    return;
  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([path]() {
    QJniObject::callStaticMethod<void>(
        "com/benschwank/blop/BlopActivity", "publishCacheToLastUri",
        "(Ljava/lang/String;)V",
        QJniObject::fromString(path).object<jstring>());
  });
#endif
}
