#pragma once

#include <QColor>
#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QHBoxLayout;
class QPushButton;

/// Smart-view + sort chip strip above the library grid.
class LibraryOrgBar : public QWidget {
  Q_OBJECT
public:
  enum class SmartView { All, Favorites, Recent, Untagged };
  enum class SortMode { Name, Modified };

  explicit LibraryOrgBar(QWidget *parent = nullptr);

  SmartView smartView() const { return m_view; }
  SortMode sortMode() const { return m_sort; }

  void setAccentColor(const QColor &color);

signals:
  void smartViewChanged(LibraryOrgBar::SmartView view);
  void sortModeChanged(LibraryOrgBar::SortMode mode);

private:
  void rebuildStyles();
  void onViewClicked(int id);
  void onSortClicked();

  QButtonGroup *m_viewGroup{nullptr};
  QPushButton *m_btnSort{nullptr};
  SmartView m_view{SmartView::All};
  SortMode m_sort{SortMode::Name};
  QColor m_accent{QColor(QStringLiteral("#7C5CFC"))};
};
