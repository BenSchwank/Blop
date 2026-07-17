#pragma once

#include <QColor>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QToolButton;

/// Compact tag organizer — Drawboard-style section for the left Super sidebar.
/// Click tags to filter the library; long-press/context for rename/delete;
/// type + Enter to add. Assigning tags to notes happens via note ⋯ menu.
class LibraryTagsPanel : public QWidget {
  Q_OBJECT
public:
  explicit LibraryTagsPanel(QWidget *parent = nullptr);

  void setAccentColor(const QColor &color);
  void reload();

  QStringList selectedTags() const;
  void clearSelection();

  /// Compact sidebar chrome (no huge right shelf).
  void setSidebarMode(bool on);

signals:
  void filterChanged(const QStringList &selectedTags);
  void catalogChanged();

private slots:
  void onAddClicked();
  void onRenameClicked();
  void onDeleteClicked();
  void onSelectionChanged();
  void onToggleCollapsed();

private:
  void refreshTheme();
  void rebuildList(const QStringList &preserveSelection = {});
  void applyCollapsed();

  QWidget *m_header{nullptr};
  QToolButton *m_btnToggle{nullptr};
  QLabel *m_title{nullptr};
  QWidget *m_body{nullptr};
  QLineEdit *m_input{nullptr};
  QListWidget *m_list{nullptr};
  QPushButton *m_btnAdd{nullptr};
  QPushButton *m_btnManage{nullptr};
  QColor m_accent{QColor(QStringLiteral("#7C5CFC"))};
  bool m_sidebarMode{false};
  bool m_collapsed{false};
};
