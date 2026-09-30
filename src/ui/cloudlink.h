#pragma once

#include <QObject>
#include <QString>

class QWidget;

/// Connects note storage to a provider API (Google Drive, Nextcloud,
/// OneDrive) or to the iCloud for Windows folder. Tokens stay in
/// QSettings, not in note files.
class CloudLinkHub : public QObject {
  Q_OBJECT
public:
  static CloudLinkHub &instance();

  /// Starts the provider login. `parent` is used for the Nextcloud URL dialog.
  void connectProvider(const QString &type, QWidget *parent);

  QString lastError() const { return m_lastError; }

  /// Primary cloud can upload without a local sync folder.
  bool primaryUsesApi() const;
  bool providerReady(const QString &type) const;

  bool putNote(const QString &localPath);
  bool removeNote(const QString &fileName);
  bool renameNote(const QString &oldName, const QString &newName);

signals:
  void connectFinished(const QString &type, bool ok, const QString &detail);

private:
  explicit CloudLinkHub(QObject *parent = nullptr);

  void fail(const QString &type, const QString &detail);
  void succeed(const QString &type, const QString &detail);
  void markApi(const QString &type);

  void beginGoogle();
  void beginOneDrive();
  void beginNextcloud(QWidget *parent);
  void beginIcloud();
  void pollNextcloud();
  void finishNextcloud(const QString &server, const QString &user,
                       const QString &appPassword);

  bool googlePut(const QString &localPath);
  bool googleRemove(const QString &fileName);
  bool googleRename(const QString &oldName, const QString &newName);
  bool nextcloudPut(const QString &localPath);
  bool nextcloudRemove(const QString &fileName);
  bool nextcloudRename(const QString &oldName, const QString &newName);
  bool oneDrivePut(const QString &localPath);
  bool oneDriveRemove(const QString &fileName);
  bool oneDriveRename(const QString &oldName, const QString &newName);

  QString m_lastError;
  bool m_busy{false};

  class QTcpServer *m_loopback{nullptr};
  class QNetworkAccessManager *m_nam{nullptr};
  class QTimer *m_ncTimer{nullptr};
  QString m_verifier;
  QString m_state;
  QString m_redirect;
  QString m_ncEndpoint;
  QString m_ncToken;
  QString m_ncServer;
  int m_ncPolls{0};
};
