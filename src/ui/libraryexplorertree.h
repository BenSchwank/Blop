#pragma once

#include <QColor>
#include <QModelIndex>
#include <QWidget>

class QFileSystemModel;
class QLabel;
class QSortFilterProxyModel;
class QToolButton;
class QTreeView;

/// Compact note/folder tree for the library sidebar (Obsidian-like).
class LibraryExplorerTree : public QWidget {
  Q_OBJECT
public:
  explicit LibraryExplorerTree(QWidget *parent = nullptr);

  void setRootPath(const QString &absolutePath);
  void setAccentColor(const QColor &color);
  /// Case-insensitive name filter. Empty shows every visible row.
  void setFilterText(const QString &text);
  QString rootPath() const;
  QString currentPath() const;

signals:
  void pathActivated(const QString &path);
  void pathSelected(const QString &path);
  void newFolderRequested();
  /// Request creating a note/folder under the given directory path.
  void newNoteInFolderRequested(const QString &folderPath);
  void newFolderInFolderRequested(const QString &folderPath);
  void contextMenuRequested(const QString &path, const QPoint &globalPos);
  /// Move sources into destDir (absolute paths).
  void movePathsRequested(const QStringList &sources, const QString &destDir);

private:
  void applyChrome();
  QString pathFor(const QModelIndex &viewIndex) const;

  QTreeView *m_tree{nullptr};
  QFileSystemModel *m_model{nullptr};
  QSortFilterProxyModel *m_proxy{nullptr};
  QLabel *m_sectionLabel{nullptr};
  QToolButton *m_btnNewFolder{nullptr};
  QToolButton *m_btnNewNote{nullptr};
  QColor m_accent;
};
