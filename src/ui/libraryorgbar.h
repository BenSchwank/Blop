#pragma once

#include <QColor>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;
class QPushButton;

/// Smart-view chip strip for the library. Sort lives in the top action bar
/// (Drawboard-style) via takeSortButton()/sortButton().
class LibraryOrgBar : public QWidget {
  Q_OBJECT
public:
  enum class SmartView { All, Favorites, Recent, Untagged };
  enum class SortMode { Name, Modified };

  explicit LibraryOrgBar(QWidget *parent = nullptr);

  SmartView smartView() const { return m_view; }
  SortMode sortMode() const { return m_sort; }
  void setSmartView(SmartView view);

  void setAccentColor(const QColor &color);

  /// Sort control for embedding in the library action bar (higher up).
  QPushButton *sortButton() const { return m_btnSort; }
  void placeSortInActionBar(QLayout *actionRow);

signals:
  void smartViewChanged(LibraryOrgBar::SmartView view);
  void sortModeChanged(LibraryOrgBar::SortMode mode);

public slots:
  void cycleSortMode();

private:
  void rebuildStyles();
  void onViewClicked(int id);
  void refreshSortLabel();

  QButtonGroup *m_viewGroup{nullptr};
  QPushButton *m_btnSort{nullptr};
  SmartView m_view{SmartView::All};
  SortMode m_sort{SortMode::Name};
  QColor m_accent{QColor(QStringLiteral("#7C5CFC"))};
};
