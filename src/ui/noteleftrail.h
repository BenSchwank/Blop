#pragma once

// Drawboard-style left menu strip — pages / bookmarks / history / utilities.

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QWidget>

class QFrame;
class QToolButton;
class QVBoxLayout;

class NoteLeftRail : public QWidget {
  Q_OBJECT
public:
  explicit NoteLeftRail(QWidget *parent = nullptr);

  void setAccentColor(const QColor &color);
  void setIcon(const QString &id, const QIcon &icon);
  /// When page thumbnails are visible beside this rail, hide the shared edge.
  void setThumbsAdjacent(bool on);
  /// Hide page/bookmark/history/all-pages controls (infinite canvas).
  void setPageFeaturesVisible(bool on);

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
  void addGroupSeparator();
  void refreshStyles();

  QVBoxLayout *m_lay{nullptr};
  QHash<QString, QToolButton *> m_btns;
  QList<QFrame *> m_separators;
  QColor m_accent{QColor(QStringLiteral("#5B9DFF"))};
  bool m_pagesExpanded{true};
  bool m_thumbsAdjacent{false};
};
