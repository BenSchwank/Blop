#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QWidget;

/// Right-hand inspector for the selected library note or folder.
class LibraryDetailPanel : public QWidget {
  Q_OBJECT
public:
  explicit LibraryDetailPanel(QWidget *parent = nullptr);

  void setPath(const QString &absolutePath);
  QString path() const { return m_path; }

signals:
  void openRequested(const QString &path);
  void renameRequested(const QString &path);
  void deleteRequested(const QString &path);

private:
  void refresh();
  void applyChrome();
  void addTagFromInput();
  void rebuildTagChips();

  QString m_path;
  QLabel *m_empty{nullptr};
  QWidget *m_body{nullptr};
  QLabel *m_preview{nullptr};
  QLabel *m_name{nullptr};
  QLabel *m_meta{nullptr};
  QLabel *m_when{nullptr};
  QPushButton *m_btnOpen{nullptr};
  QWidget *m_tagHost{nullptr};
  QVBoxLayout *m_tagLay{nullptr};
  QLineEdit *m_tagInput{nullptr};
};
