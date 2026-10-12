#pragma once

#include "cloudlink.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

/// BlopNotizen browser hosted inside the notes overview (not a window overlay).
class CloudNotesBrowser : public QWidget {
  Q_OBJECT
public:
  explicit CloudNotesBrowser(QWidget *parent = nullptr);

  /// Show inside `host` (typically OverviewContainer) — keeps the sidebar visible.
  static CloudNotesBrowser *showIn(QWidget *host, const QString &providerType,
                                   const QString &displayName,
                                   bool listMode = false);

  void openProvider(const QString &providerType, const QString &displayName);
  void setListMode(bool on);

signals:
  void noteOpenRequested(const QString &localPath);
  void closed();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void rebuildChrome();
  void refreshList();
  void openCurrentItem();
  void applyViewMode();
  void syncViewButtons();
  QIcon fileIconFor(const QString &name) const;

  QString m_type;
  bool m_listMode{false};
  QLabel *m_title{nullptr};
  QLabel *m_status{nullptr};
  QListWidget *m_list{nullptr};
  QPushButton *m_btnGrid{nullptr};
  QPushButton *m_btnList{nullptr};
  QPushButton *m_btnRefresh{nullptr};
  QPushButton *m_btnClose{nullptr};
};
