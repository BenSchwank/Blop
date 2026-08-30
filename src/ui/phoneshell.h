#pragma once

#include <QWidget>

class QToolButton;

/// Phone overview chrome: 4-destination bottom nav (Dashboard, Notes, Study,
/// More). More opens the existing library sheet; it is not a persistent tab.
class PhoneShell : public QWidget {
  Q_OBJECT
public:
  enum class Dest { Dashboard, Notes, Study };

  explicit PhoneShell(QWidget *host);

  void setDestination(Dest dest);
  Dest destination() const { return m_dest; }

  /// Visible on Dashboard / Notes overview / Study; hidden in editor and login.
  void setShellVisible(bool on);

  void syncGeometry();

signals:
  void destinationChosen(Dest dest);
  void moreRequested();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void rebuildStyles();
  QToolButton *makeItem(const QString &id, const QString &label,
                        const QString &iconName);

  QWidget *m_host{nullptr};
  QWidget *m_bar{nullptr};
  QToolButton *m_btnDash{nullptr};
  QToolButton *m_btnNotes{nullptr};
  QToolButton *m_btnStudy{nullptr};
  QToolButton *m_btnMore{nullptr};
  Dest m_dest{Dest::Notes};
};
