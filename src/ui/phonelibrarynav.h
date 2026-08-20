#pragma once

#include <QPointer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/// Vercel-style floating bottom pill + full-screen library menu for phone UI.
/// Lives on the notes overview only (not inside the editor / MorphTray).
class PhoneLibraryNav : public QWidget {
  Q_OBJECT
public:
  explicit PhoneLibraryNav(QWidget *host);

  void setAccountName(const QString &name);
  void setPillVisible(bool on);

public slots:
  void openMenu();
  void closeMenu();
  void rebuildMenu();

signals:
  void menuAction(const QString &id);
  void searchChanged(const QString &query);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void syncPillGeometry();
  void addRow(const QString &id, const QString &title, bool selected = false,
              bool chevron = false);

  QWidget *m_host{nullptr};
  QPointer<QWidget> m_sheet;
  QListWidget *m_list{nullptr};
  QLineEdit *m_search{nullptr};
  QLabel *m_account{nullptr};
  QString m_accountName;
};
