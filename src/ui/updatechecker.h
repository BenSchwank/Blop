#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QWidget;

/// In-app updater (desktop): asks the GitHub release feed for a newer version,
/// downloads the Windows installer and only runs it after its SHA-256 matches
/// the digest published with the release asset. Android updates go through
/// the Play Store, so check() is a no-op there.
class UpdateChecker : public QObject {
  Q_OBJECT
public:
  /// `dialogParent` hosts the in-window dialogs; `nam` is shared with the host.
  UpdateChecker(QWidget *dialogParent, QNetworkAccessManager *nam,
                const QString &currentVersion, QObject *parent = nullptr);

  void check();

private:
  void promptAndDownload(const QString &tagName, const QString &downloadUrl,
                         const QString &releasePageUrl,
                         const QString &expectedSha256);

  QPointer<QWidget> m_parent;
  QNetworkAccessManager *m_nam{nullptr};
  QString m_currentVersion;
};