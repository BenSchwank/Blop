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

/// Legacy in-app cloud explorer chrome (kept for linkage). Login/browse is
/// opened in the system browser via showOver — cookies / passwords apply.
class CloudWebExplorer : public QWidget {
  Q_OBJECT
public:
  explicit CloudWebExplorer(QWidget *parent = nullptr);

  /// Resolve URL (prompt for Nextcloud/custom if needed) and open it in the
  /// system browser. Always returns nullptr (no overlay).
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
