#include "desktopdeeplink.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
namespace {
constexpr const char *kServerName = "BlopDesktopSingleInstance_v1";
}

DesktopDeepLink &DesktopDeepLink::instance() {
  static DesktopDeepLink inst;
  return inst;
}

DesktopDeepLink::DesktopDeepLink(QObject *parent) : QObject(parent) {}

void DesktopDeepLink::registerProtocolHandler() {
#ifndef Q_OS_ANDROID
  const QString exe =
      QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
  if (exe.isEmpty())
    return;
#ifdef Q_OS_WIN
  // Portable + installed builds: HKCU so we don't need admin.
  // Write both via QSettings and a flat command key — some Windows builds
  // ignore nested QSettings paths for URL Protocol handlers.
  {
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\blop"),
                  QSettings::NativeFormat);
    reg.setValue(QStringLiteral("Default"), QStringLiteral("URL:Blop Protocol"));
    reg.setValue(QStringLiteral("."), QStringLiteral("URL:Blop Protocol"));
    reg.setValue(QStringLiteral("URL Protocol"), QString());
    reg.sync();
  }
  {
    QSettings cmd(
        QStringLiteral(
            "HKEY_CURRENT_USER\\Software\\Classes\\blop\\shell\\open\\command"),
        QSettings::NativeFormat);
    // "%1" is replaced by Windows with the blop:// URL.
    cmd.setValue(QStringLiteral("."),
                 QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1")));
    cmd.setValue(QStringLiteral("Default"),
                 QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1")));
    cmd.sync();
  }
  qInfo() << "DesktopDeepLink: registered blop:// ->" << exe;
#elif defined(Q_OS_LINUX)
  // Best-effort user .desktop entry for xdg-open.
  const QString apps =
      QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
  if (apps.isEmpty())
    return;
  QDir().mkpath(apps);
  const QString desktopPath = apps + QStringLiteral("/blop-url.desktop");
  QFile f(desktopPath);
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    const QByteArray body =
        QByteArrayLiteral("[Desktop Entry]\n") +
        QByteArrayLiteral("Type=Application\n") +
        QByteArrayLiteral("Name=Blop URL Handler\n") +
        QByteArrayLiteral("Exec=") +
        QStringLiteral("\"%1\" %u\n").arg(exe).toUtf8() +
        QByteArrayLiteral("MimeType=x-scheme-handler/blop;\n") +
        QByteArrayLiteral("NoDisplay=true\n") +
        QByteArrayLiteral("Terminal=false\n");
    f.write(body);
    f.close();
    qInfo() << "DesktopDeepLink: wrote" << desktopPath;
  }
#else
  Q_UNUSED(exe);
#endif
#endif
}

QString DesktopDeepLink::deepLinkFromArguments(const QStringList &args) {
  for (const QString &a : args) {
    if (a.startsWith(QStringLiteral("blop:"), Qt::CaseInsensitive))
      return a;
  }
  return {};
}

bool DesktopDeepLink::handOffToRunningInstance(const QString &message) {
  QLocalSocket sock;
  sock.connectToServer(QString::fromLatin1(kServerName));
  if (!sock.waitForConnected(400))
    return false;
  const QByteArray payload =
      (message.isEmpty() ? QByteArrayLiteral("ACTIVATE") : message.toUtf8()) +
      '\n';
  sock.write(payload);
  sock.flush();
  sock.waitForBytesWritten(800);
  sock.disconnectFromServer();
  return true;
}

void DesktopDeepLink::startServer() {
  if (m_server)
    return;
  QLocalServer::removeServer(QString::fromLatin1(kServerName));
  m_server = new QLocalServer(this);
  connect(m_server, &QLocalServer::newConnection, this, [this]() {
    while (m_server->hasPendingConnections()) {
      QLocalSocket *client = m_server->nextPendingConnection();
      if (!client)
        continue;
      connect(client, &QLocalSocket::readyRead, this, [this, client]() {
        const QByteArray raw = client->readAll().trimmed();
        client->disconnectFromServer();
        client->deleteLater();
        if (raw.isEmpty())
          return;
        emit messageReceived(QString::fromUtf8(raw));
      });
    }
  });
  if (!m_server->listen(QString::fromLatin1(kServerName))) {
    qWarning() << "DesktopDeepLink: listen failed" << m_server->errorString();
  }
}
