#pragma once

// Drawboard-style left menu strip — pages / bookmarks / history / utilities.

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QWidget>

class QToolButton;
class QVBoxLayout;

class NoteLeftRail : public QWidget {
  Q_OBJECT
public:
  explicit NoteLeftRail(QWidget *parent = nullptr);

  void setAccentColor(const QColor &color);
  void setIcon(const QString &id, const QIcon &icon);

  bool pagesExpanded() const { return m_pagesExpanded; }
  void setPagesExpanded(bool on);

  int preferredWidth() const;

signals:
  void pagesToggled(bool expanded);
  void searchClicked();
  void moreClicked();
  void selectClicked();
  void exportClicked();
  void settingsClicked();
  void bookmarksClicked();
  void historyClicked();
  void allPagesClicked();
  void themeToggleClicked();
  void propertiesClicked();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QToolButton *makeBtn(const QString &id, const QString &tip);
  void refreshStyles();

  QVBoxLayout *m_lay{nullptr};
  QHash<QString, QToolButton *> m_btns;
  QColor m_accent{QColor(QStringLiteral("#5B9DFF"))};
  bool m_pagesExpanded{true};
};
