#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QLocalServer;
class QWidget;

/// Desktop single-instance + blop:// protocol helpers.
class DesktopDeepLink : public QObject {
  Q_OBJECT
public:
  static DesktopDeepLink &instance();

  /// Register blop:// for the current user (HKCU on Windows; best-effort elsewhere).
  static void registerProtocolHandler();

  /// If another Blop is running, forward @p message and return true (caller should exit).
  bool handOffToRunningInstance(const QString &message);

  /// Start listening for hand-off messages from secondary processes.
  void startServer();

  /// Parse argv for a blop: URL (empty if none).
  static QString deepLinkFromArguments(const QStringList &args);

  /// Raise + activate the running window after browser OAuth (Windows
  /// foreground-lock workaround).
  static void bringWindowToFront(QWidget *window);

signals:
  void messageReceived(const QString &message);

private:
  explicit DesktopDeepLink(QObject *parent = nullptr);
  QLocalServer *m_server{nullptr};
};
