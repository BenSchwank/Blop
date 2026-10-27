#include "updatechecker.h"

#include "blop_dialogs.h"
#include "versioncompare.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

UpdateChecker::UpdateChecker(QWidget *dialogParent, QNetworkAccessManager *nam,
                             const QString &currentVersion, QObject *parent)
    : QObject(parent), m_parent(dialogParent), m_nam(nam),
      m_currentVersion(currentVersion) {}

void UpdateChecker::check() {
#ifdef Q_OS_ANDROID
  return;
#endif
  if (!m_nam)
    return;

  // Latest stable versioned release (e.g. v3.5.3.9), NOT nightly.
  QNetworkRequest request(QUrl(QStringLiteral(
      "https://api.github.com/repos/BenSchwank/Blop-releases/releases/latest")));
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    "Blop-App/" + m_currentVersion.toUtf8());
  request.setRawHeader("Accept", "application/vnd.github+json");

  QNetworkReply *reply = m_nam->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
      return; // silent on network errors

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject())
      return;
    const QJsonObject root = doc.object();

    QString tagName = root.value(QStringLiteral("tag_name")).toString().trimmed();
    if (tagName.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
      tagName.remove(0, 1);

    // Tolerates git-describe suffixes ("3.29.47-3-gabc").
    if (!BlopVersion::isNewer(tagName, m_currentVersion))
      return;

    QSettings settings(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
    if (settings.value(QStringLiteral("dismissedUpdateVersion")).toString() ==
        tagName)
      return; // user already chose "Später" for this version

    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    const QString releasePageUrl =
        root.value(QStringLiteral("html_url")).toString();
    QString downloadUrl;
    QString expectedSha256; // GitHub asset "digest" = "sha256:<hex>"

#ifdef Q_OS_ANDROID
    for (const QJsonValue &a : assets) {
      if (a.toObject().value(QStringLiteral("name")).toString() ==
          QLatin1String("Blop-signed.apk")) {
        downloadUrl = a.toObject()
                          .value(QStringLiteral("browser_download_url"))
                          .toString();
        break;
      }
    }
#else
    for (const QJsonValue &a : assets) {
      if (a.toObject().value(QStringLiteral("name")).toString() ==
          QLatin1String("Blop_Windows_Installer.exe")) {
        downloadUrl = a.toObject()
                          .value(QStringLiteral("browser_download_url"))
                          .toString();
        const QString digest =
            a.toObject().value(QStringLiteral("digest")).toString();
        if (digest.startsWith(QLatin1String("sha256:"), Qt::CaseInsensitive))
          expectedSha256 = digest.mid(7).trimmed().toLower();
        break;
      }
    }
    // Never auto-run an installer we cannot verify: without a published
    // SHA-256 fall back to the release page (manual download).
    if (!downloadUrl.isEmpty() && expectedSha256.size() != 64)
      downloadUrl.clear();
#endif
    if (downloadUrl.isEmpty())
      downloadUrl = releasePageUrl;

    promptAndDownload(tagName, downloadUrl, releasePageUrl, expectedSha256);
  });
}

void UpdateChecker::promptAndDownload(const QString &tagName,
                                      const QString &downloadUrl,
                                      const QString &releasePageUrl,
                                      const QString &expectedSha256) {
  QString localVersion = m_currentVersion;
  if (localVersion.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
    localVersion.remove(0, 1);

  const bool downloadNow = BlopDialogs::confirm(
      m_parent, QStringLiteral("Update verfügbar"),
      QStringLiteral("Blop v%1 ist verfügbar!\nDeine Version: v%2\n\nJetzt "
                     "herunterladen?")
          .arg(tagName, localVersion),
      QStringLiteral("Jetzt herunterladen"), QStringLiteral("Später"));
  if (!downloadNow) {
    // Remember the choice; do not nag again for this version.
    QSettings(QStringLiteral("Blop"), QStringLiteral("BlopApp"))
        .setValue(QStringLiteral("dismissedUpdateVersion"), tagName);
    return;
  }

#ifdef Q_OS_ANDROID
  QDesktopServices::openUrl(QUrl(downloadUrl));
  Q_UNUSED(releasePageUrl);
  Q_UNUSED(expectedSha256);
#else
  if (downloadUrl == releasePageUrl) {
    QDesktopServices::openUrl(QUrl(downloadUrl));
    return;
  }

  QWidget *host = m_parent.data();
  auto *progress = new QProgressDialog(QStringLiteral("Update wird heruntergeladen..."),
                                       QStringLiteral("Abbrechen"), 0, 100, host);
  progress->setWindowTitle(QStringLiteral("Blop Update"));
  progress->setWindowModality(Qt::WindowModal);
  progress->setMinimumDuration(0);
  progress->setValue(0);

  QNetworkRequest dlReq{QUrl(downloadUrl)};
  dlReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *dlReply = m_nam->get(dlReq);

  const QString tempPath =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
      QStringLiteral("/Blop_Update.exe");
  auto *file = new QFile(tempPath);
  if (!file->open(QIODevice::WriteOnly)) {
    BlopDialogs::notify(host, QStringLiteral("Fehler"),
                        QStringLiteral("Lokale Datei konnte nicht erstellt werden."));
    delete file;
    delete progress;
    dlReply->abort();
    dlReply->deleteLater();
    return;
  }

  connect(dlReply, &QNetworkReply::readyRead, this,
          [dlReply, file]() { file->write(dlReply->readAll()); });
  connect(dlReply, &QNetworkReply::downloadProgress, this,
          [progress](qint64 received, qint64 total) {
            if (total > 0) {
              progress->setMaximum(static_cast<int>(total));
              progress->setValue(static_cast<int>(received));
            }
          });
  connect(progress, &QProgressDialog::canceled, this,
          [dlReply, file, tempPath]() {
            dlReply->abort();
            file->close();
            QFile::remove(tempPath);
            file->deleteLater();
            dlReply->deleteLater();
          });
  connect(dlReply, &QNetworkReply::finished, this,
          [this, dlReply, file, tempPath, progress, expectedSha256, host]() {
            progress->deleteLater();
            file->close();

            if (dlReply->error() == QNetworkReply::NoError) {
              // Verify integrity before executing anything.
              QString actual;
              QFile dl(tempPath);
              if (dl.open(QIODevice::ReadOnly)) {
                QCryptographicHash h(QCryptographicHash::Sha256);
                h.addData(&dl);
                actual = QString::fromLatin1(h.result().toHex()).toLower();
                dl.close();
              }
              if (actual.isEmpty() || actual != expectedSha256) {
                QFile::remove(tempPath);
                BlopDialogs::notify(
                    host, QStringLiteral("Update abgebrochen"),
                    QStringLiteral("Die Prüfsumme des Updates stimmt nicht "
                                   "überein. Die Datei wurde verworfen und "
                                   "nicht ausgeführt."));
              } else {
                QProcess::startDetached(tempPath, QStringList());
                QApplication::quit();
              }
            } else {
              QFile::remove(tempPath);
              if (dlReply->error() != QNetworkReply::OperationCanceledError) {
                BlopDialogs::notify(
                    host, QStringLiteral("Fehler"),
                    QStringLiteral("Fehler beim Download des Updates:\n%1")
                        .arg(dlReply->errorString()));
              }
            }
            file->deleteLater();
            dlReply->deleteLater();
          });
#endif
}