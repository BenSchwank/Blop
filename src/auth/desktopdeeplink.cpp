#include "desktopdeeplink.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>
#include <QWindow>
#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
constexpr const char *kServerName = "BlopDesktopSingleInstance_v1";

#ifdef Q_OS_WIN
QString qtRuntimeBinDir() {
  HMODULE mod = GetModuleHandleW(L"Qt6Core.dll");
  if (!mod)
    return {};
  wchar_t buf[MAX_PATH];
  if (!GetModuleFileNameW(mod, buf, MAX_PATH))
    return {};
  return QFileInfo(QString::fromWCharArray(buf)).absolutePath();
}

bool exeHasLocalQtDlls(const QString &exeNative) {
  const QString dir = QFileInfo(exeNative).absolutePath();
  return QFileInfo::exists(QDir(dir).filePath(QStringLiteral("Qt6Core.dll"))) &&
         QFileInfo::exists(QDir(dir).filePath(QStringLiteral("Qt6Gui.dll")));
}

void copyIfMissing(const QString &src, const QString &dst) {
  if (!QFileInfo::exists(src) || QFileInfo::exists(dst))
    return;
  QFile::copy(src, dst);
}

/// Copy the Qt runtime next to Blop.exe so Windows can launch it via blop://
/// without PATH and without a fragile .cmd/.vbs wrapper (those caused console
/// floods / Script Host errors when the OAuth page fired the scheme).
///
/// Also always ensure platforms/ + tls/ plugins exist. Local Qt6*.dll without
/// tls/qschannelbackend.dll makes QSslSocket report "No TLS backend" and every
/// HTTPS claim/poll fails (Google desktop login).
void ensureSidecarQtRuntime(const QString &exeNative) {
  const QString qtBin = qtRuntimeBinDir();
  if (qtBin.isEmpty())
    return;
  const QString dir = QFileInfo(exeNative).absolutePath();
  const QDir qtPlugins(QDir(qtBin).absoluteFilePath(QStringLiteral("../plugins")));

  if (!exeHasLocalQtDlls(exeNative)) {
    const QStringList dlls = {
        QStringLiteral("Qt6Core.dll"),
        QStringLiteral("Qt6Gui.dll"),
        QStringLiteral("Qt6Widgets.dll"),
        QStringLiteral("Qt6Network.dll"),
        QStringLiteral("Qt6Svg.dll"),
        QStringLiteral("Qt6OpenGL.dll"),
        QStringLiteral("Qt6OpenGLWidgets.dll"),
        QStringLiteral("Qt6Multimedia.dll"),
        QStringLiteral("Qt6MultimediaWidgets.dll"),
        QStringLiteral("Qt6Qml.dll"),
        QStringLiteral("Qt6QmlMeta.dll"),
        QStringLiteral("Qt6QmlModels.dll"),
        QStringLiteral("Qt6QmlWorkerScript.dll"),
        QStringLiteral("Qt6Quick.dll"),
        QStringLiteral("Qt6QuickWidgets.dll"),
        QStringLiteral("Qt6QuickControls2.dll"),
        QStringLiteral("Qt6QuickControls2Basic.dll"),
        QStringLiteral("Qt6QuickControls2Impl.dll"),
        QStringLiteral("Qt6QuickTemplates2.dll"),
        QStringLiteral("Qt6NetworkAuth.dll"),
        QStringLiteral("libgcc_s_seh-1.dll"),
        QStringLiteral("libstdc++-6.dll"),
        QStringLiteral("libwinpthread-1.dll"),
    };
    for (const QString &name : dlls) {
      copyIfMissing(QDir(qtBin).filePath(name), QDir(dir).filePath(name));
    }
  }

  const QString platDir = QDir(dir).filePath(QStringLiteral("platforms"));
  QDir().mkpath(platDir);
  copyIfMissing(qtPlugins.filePath(QStringLiteral("platforms/qwindows.dll")),
                QDir(platDir).filePath(QStringLiteral("qwindows.dll")));

  // Qt 6 looks for TLS backends in <app>/tls/ (plugin type "tls").
  const QString tlsDir = QDir(dir).filePath(QStringLiteral("tls"));
  QDir().mkpath(tlsDir);
  for (const QString &name :
       {QStringLiteral("qschannelbackend.dll"),
        QStringLiteral("qopensslbackend.dll"),
        QStringLiteral("qcertonlybackend.dll")}) {
    copyIfMissing(qtPlugins.filePath(QStringLiteral("tls/") + name),
                  QDir(tlsDir).filePath(name));
  }

  // OpenSSL backend needs these next to the exe (schannel does not).
  for (const QString &name :
       {QStringLiteral("libssl-3-x64.dll"), QStringLiteral("libcrypto-3-x64.dll"),
        QStringLiteral("libssl-1_1-x64.dll"),
        QStringLiteral("libcrypto-1_1-x64.dll")}) {
    copyIfMissing(QDir(qtBin).filePath(name), QDir(dir).filePath(name));
  }

  qInfo() << "DesktopDeepLink: ensured Qt sidecar DLLs in" << dir
          << "hasTls="
          << QFileInfo::exists(
                 QDir(tlsDir).filePath(QStringLiteral("qschannelbackend.dll")));
}

void removeLegacyLaunchers(const QString &exeNative) {
  const QString dir = QFileInfo(exeNative).absolutePath();
  QFile::remove(QDir(dir).filePath(QStringLiteral("blop-open.cmd")));
  QFile::remove(QDir(dir).filePath(QStringLiteral("blop-open.vbs")));
}
#endif
} // namespace

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
  ensureSidecarQtRuntime(exe);
  removeLegacyLaunchers(exe);

  {
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\blop"),
                  QSettings::NativeFormat);
    reg.setValue(QStringLiteral("Default"), QStringLiteral("URL:Blop Protocol"));
    reg.setValue(QStringLiteral("."), QStringLiteral("URL:Blop Protocol"));
    reg.setValue(QStringLiteral("URL Protocol"), QString());
    reg.sync();
  }

  // Always point at Blop.exe directly — never a console/.vbs wrapper.
  const QString commandValue =
      QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1"));
  {
    QSettings cmd(
        QStringLiteral(
            "HKEY_CURRENT_USER\\Software\\Classes\\blop\\shell\\open\\command"),
        QSettings::NativeFormat);
    cmd.setValue(QStringLiteral("."), commandValue);
    cmd.setValue(QStringLiteral("Default"), commandValue);
    cmd.sync();
  }
  qInfo() << "DesktopDeepLink: registered blop:// ->" << commandValue
          << "hasLocalDlls=" << exeHasLocalQtDlls(exe);
#elif defined(Q_OS_LINUX)
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

void DesktopDeepLink::bringWindowToFront(QWidget *window) {
#ifndef Q_OS_ANDROID
  if (!window)
    return;
  window->show();
  window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
  window->raise();
  window->activateWindow();
  if (window->windowHandle())
    window->windowHandle()->requestActivate();
#ifdef Q_OS_WIN
  HWND hwnd = reinterpret_cast<HWND>(window->winId());
  if (hwnd) {
    HWND fg = GetForegroundWindow();
    const DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
    const DWORD thisThread = GetCurrentThreadId();
    if (fg && fgThread != thisThread)
      AttachThreadInput(fgThread, thisThread, TRUE);
    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    if (fg && fgThread != thisThread)
      AttachThreadInput(fgThread, thisThread, FALSE);
  }
#endif
#else
  Q_UNUSED(window);
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
