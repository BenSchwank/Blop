#pragma once

#include <QDateTime>
#include <QString>
#include <QWidget>

class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QWidget;

/// Right-hand inspector for the selected library note, folder, or cloud item.
class LibraryDetailPanel : public QWidget {
  Q_OBJECT
public:
  struct CloudInfo {
    QString remoteId;
    QString name;
    QString mimeType;
    QString providerLabel;
    QDateTime modified;
    QDateTime created;
    qint64 sizeBytes{-1};
    bool isFolder{false};
  };

  explicit LibraryDetailPanel(QWidget *parent = nullptr);

  void setPath(const QString &absolutePath);
  QString path() const { return m_path; }

  void setCloudItem(const CloudInfo &info);
  void clearSelection();
  bool hasCloudItem() const { return !m_cloud.remoteId.isEmpty(); }
  CloudInfo cloudItem() const { return m_cloud; }

  void setCollapsed(bool on);
  bool isCollapsed() const { return m_collapsed; }

signals:
  void openRequested(const QString &path);
  void renameRequested(const QString &path);
  void deleteRequested(const QString &path);
  void cloudOpenRequested();
  void cloudRenameRequested();
  void cloudDeleteRequested();
  void collapsedChanged(bool collapsed);
  void favoriteChanged(const QString &path, bool favorite);

private:
  void refresh();
  void applyChrome();
  void addTagFromInput();
  void rebuildTagChips();
  void applyCollapsed();
  void updateCollapseIcon();
  void updateFavoriteButton();
  void setCloudUiVisible(bool cloud);

  QString m_path;
  CloudInfo m_cloud;
  bool m_collapsed{false};
  int m_expandedWidth{280};

  QWidget *m_chrome{nullptr};
  QLabel *m_title{nullptr};
  QToolButton *m_btnCollapse{nullptr};
  QWidget *m_emptyHost{nullptr};
  QLabel *m_emptyIcon{nullptr};
  QLabel *m_emptyTitle{nullptr};
  QLabel *m_emptyHint{nullptr};
  QWidget *m_body{nullptr};
  QWidget *m_contentHost{nullptr};
  QLabel *m_preview{nullptr};
  QLabel *m_name{nullptr};
  QLabel *m_meta{nullptr};
  QLabel *m_when{nullptr};
  QLabel *m_provider{nullptr};
  QPushButton *m_btnFavorite{nullptr};
  QPushButton *m_btnOpen{nullptr};
  QPushButton *m_btnRename{nullptr};
  QPushButton *m_btnDelete{nullptr};
  QWidget *m_tagSection{nullptr};
  QWidget *m_tagHost{nullptr};
  QGridLayout *m_tagLay{nullptr};
  QLineEdit *m_tagInput{nullptr};
  QPushButton *m_btnAddTag{nullptr};
};
