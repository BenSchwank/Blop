#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

class QWidget;

/// One row in a remote cloud folder (API browse).
struct CloudRemoteItem {
  QString id;   ///< Drive/OneDrive file id, or Nextcloud relative path
  QString name;
  QString mimeType; ///< e.g. application/pdf, application/vnd.google-apps.document
  QDateTime modified;
  QDateTime created;
  qint64 sizeBytes{-1}; ///< -1 = unknown (folders / Google Docs)
  bool isFolder{false};
};

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

  /// List files in a cloud folder. Empty folderId = provider root (all files).
  QVector<CloudRemoteItem> listNotes(const QString &type,
                                     const QString &folderId = {});
  /// Download one remote file into localPath (parent dirs created).
  bool fetchNote(const QString &type, const QString &remoteId,
                 const QString &localPath);

  /// Create a folder under parentId (empty = provider root).
  bool createFolder(const QString &type, const QString &parentId,
                    const QString &name);
  /// Delete any remote file/folder by id (Nextcloud: relative path).
  bool deleteRemote(const QString &type, const QString &remoteId);
  /// Rename any remote file/folder by id.
  bool renameRemote(const QString &type, const QString &remoteId,
                    const QString &newName);

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
  void beginDropbox();
  void beginNextcloud(QWidget *parent);
  void beginIcloud();
  void pollNextcloud();
  void finishNextcloud(const QString &server, const QString &user,
                       const QString &appPassword);

  bool googlePut(const QString &localPath);
  bool googleRemove(const QString &fileName);
  bool googleRename(const QString &oldName, const QString &newName);
  QVector<CloudRemoteItem> googleList(const QString &folderId);
  bool googleFetch(const QString &remoteId, const QString &localPath);
  bool nextcloudPut(const QString &localPath);
  bool nextcloudRemove(const QString &fileName);
  bool nextcloudRename(const QString &oldName, const QString &newName);
  QVector<CloudRemoteItem> nextcloudList(const QString &folderPath);
  bool nextcloudFetch(const QString &remoteId, const QString &localPath);
  bool oneDrivePut(const QString &localPath);
  bool oneDriveRemove(const QString &fileName);
  bool oneDriveRename(const QString &oldName, const QString &newName);
  QVector<CloudRemoteItem> oneDriveList(const QString &folderId);
  bool oneDriveFetch(const QString &remoteId, const QString &localPath);
  bool dropboxPut(const QString &localPath);
  bool dropboxRemove(const QString &fileName);
  bool dropboxRename(const QString &oldName, const QString &newName);
  QVector<CloudRemoteItem> dropboxList(const QString &folderPath);
  bool dropboxFetch(const QString &remoteId, const QString &localPath);
  QVector<CloudRemoteItem> icloudList(const QString &folderPath);
  bool icloudFetch(const QString &remoteId, const QString &localPath);
  bool icloudCreateFolder(const QString &parentId, const QString &name);
  bool icloudDelete(const QString &remoteId);
  bool icloudRename(const QString &remoteId, const QString &newName);

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
