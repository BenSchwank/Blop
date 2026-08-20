#pragma once

#include "cloudstoragestore.h"

#include <QUrl>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

#ifdef BLOP_HAS_WEBENGINE
class QWebEngineView;
#endif

/// In-app cloud file explorer: sign in at the provider and browse like a
/// web app (Google Drive / Nextcloud / OneDrive / Dropbox / custom URL).
class CloudWebExplorer : public QWidget {
  Q_OBJECT
public:
  explicit CloudWebExplorer(QWidget *parent = nullptr);

  /// Cover `host`'s window with the explorer for this cloud entry.
  /// On Android this hands off to MainWindow's in-app WebView (same surface
  /// as Study) instead of opening Chrome.
  static CloudWebExplorer *showOver(QWidget *host, CloudStorageEntry entry);

  void openEntry(const CloudStorageEntry &entry);

signals:
  void closed();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void rebuildChrome();
  void goHome();
  void persistSession();
  QUrl homeUrl() const;

  CloudStorageEntry m_entry;
  QLabel *m_title{nullptr};
  QLineEdit *m_urlBar{nullptr};
  QPushButton *m_btnBack{nullptr};
  QPushButton *m_btnForward{nullptr};
  QPushButton *m_btnReload{nullptr};
  QPushButton *m_btnClose{nullptr};
#ifdef BLOP_HAS_WEBENGINE
  QWebEngineView *m_view{nullptr};
#endif
};
