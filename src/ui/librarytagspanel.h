#pragma once

#include <QColor>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QVBoxLayout;

/// Right-hand library shelf for organizing tags (Blop-native, not a Drawboard clone).
/// Provides create / rename / delete / multi-select filter.
class LibraryTagsPanel : public QWidget {
  Q_OBJECT
public:
  explicit LibraryTagsPanel(QWidget *parent = nullptr);

  void setAccentColor(const QColor &color);
  void reload();

  QStringList selectedTags() const;
  void clearSelection();

signals:
  void filterChanged(const QStringList &selectedTags);
  void catalogChanged();

private slots:
  void onAddClicked();
  void onRenameClicked();
  void onDeleteClicked();
  void onSelectionChanged();

private:
  void refreshTheme();
  void rebuildList(const QStringList &preserveSelection = {});

  QLabel *m_title{nullptr};
  QLabel *m_hint{nullptr};
  QLineEdit *m_input{nullptr};
  QListWidget *m_list{nullptr};
  QPushButton *m_btnAdd{nullptr};
  QPushButton *m_btnRename{nullptr};
  QPushButton *m_btnDelete{nullptr};
  QColor m_accent{QColor(QStringLiteral("#7C5CFC"))};
};
