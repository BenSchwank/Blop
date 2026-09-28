#pragma once

#include <QStringList>
#include <QWidget>

class QPushButton;
class QVBoxLayout;

/// Slim right dashboard column — individual removable widget segments, no
/// heavy outer chrome / collapse chevron.
class DashRightRail : public QWidget {
  Q_OBJECT
public:
  explicit DashRightRail(QWidget *parent = nullptr);

  void setEditMode(bool on);
  void refresh();
  /// Board block ids that are currently visible — rail hides duplicates.
  void setBoardOccupiedIds(const QStringList &ids);

signals:
  void openNotePath(const QString &path);

private:
  void applyChrome();
  void rebuildModules();
  void loadModulePrefs();
  void saveModulePrefs() const;
  void hideModule(const QString &id);
  void showModule(const QString &id);
  void showAddMenu();
  void promptWeatherPlace();
  bool isSuppressedByBoard(const QString &id) const;

  QWidget *makeSegment(const QString &id, const QString &title,
                       QWidget *content);
  QWidget *buildWeatherContent();
  QWidget *buildRecentContent();
  QWidget *buildTodosContent();

  bool m_editMode{false};
  QStringList m_moduleOrder;
  QStringList m_hiddenModules;
  QStringList m_boardOccupied;
  QWidget *m_body{nullptr};
  QVBoxLayout *m_bodyLay{nullptr};
  QPushButton *m_btnAdd{nullptr};
};
