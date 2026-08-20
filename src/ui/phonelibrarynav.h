#pragma once

#include <QPoint>
#include <QPointer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/// Floating bottom burger pill + sectioned library sheet for phone-sized UI.
/// Lives on the notes overview only (not inside the editor / MorphTray).
class PhoneLibraryNav : public QWidget {
  Q_OBJECT
public:
  explicit PhoneLibraryNav(QWidget *host);
  ~PhoneLibraryNav() override;

  void setAccountName(const QString &name);
  void setPillVisible(bool on);
  bool isMenuOpen() const;

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
  void keyPressEvent(QKeyEvent *event) override;

private:
  void syncPillGeometry();
  void addRow(const QString &id, const QString &title, bool selected = false,
              bool chevron = false);
  void addSection(const QString &title);
  void addSpacer();
  bool handleSheetPointer(QObject *watched, QEvent *event);

  QWidget *m_host{nullptr};
  QPointer<QWidget> m_sheet;
  QPointer<QWidget> m_card;
  QPointer<QWidget> m_scrim;
  QListWidget *m_list{nullptr};
  QLineEdit *m_search{nullptr};
  QLabel *m_account{nullptr};
  QString m_accountName;
  bool m_swiping{false};
  qreal m_swipeStartY{0};
};
