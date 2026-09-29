#pragma once

#include <QStringList>
#include <QWidget>

class QPushButton;
class QVBoxLayout;

/// Slim right dashboard column — shell chrome + weather/modules.
/// Customize controls live here (never overlaid on the cover banner).
class DashRightRail : public QWidget {
  Q_OBJECT
public:
  explicit DashRightRail(QWidget *parent = nullptr);

  void setEditMode(bool on);
  void setUndoAvailable(bool on, int stackDepth = 0);
  void refresh();
  /// Board block ids that are currently visible — rail hides duplicates.
  void setBoardOccupiedIds(const QStringList &ids);
  /// Anchor for Blöcke / Reset menus (edit-mode ⋯).
  QWidget *overflowAnchor() const;

signals:
  void openNotePath(const QString &path);
  void customizeClicked();
  void moreClicked();
  void undoClicked();

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
  QWidget *m_chrome{nullptr};
  QPushButton *m_btnCustomize{nullptr};
  QPushButton *m_btnUndo{nullptr};
  QPushButton *m_btnMore{nullptr};
  QWidget *m_body{nullptr};
  QVBoxLayout *m_bodyLay{nullptr};
  QPushButton *m_btnAdd{nullptr};
};
