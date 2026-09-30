#pragma once

#include <QStringList>
#include <QWidget>

class QPushButton;
class QTimer;
class QVBoxLayout;

/// Slim right dashboard column — Notion-style modules + shell chrome.
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
  void newNoteRequested();
  void snapToNotesRequested();
  void studyRequested();
  void openCalendarRequested();
  void searchLibrary(const QString &text);
  void contentChanged();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void applyChrome();
  void rebuildModules();
  void loadModulePrefs();
  void saveModulePrefs() const;
  void hideModule(const QString &id);
  void showModule(const QString &id);
  void showRailMenu();
  void promptWeatherPlace();
  void promptQuickSearch();
  void tickFocus();
  bool isSuppressedByBoard(const QString &id) const;
  bool isModuleVisible(const QString &id) const;
  static QStringList knownModules();
  static QString moduleTitle(const QString &id);

  QWidget *makeSegment(const QString &id, const QString &title,
                       QWidget *content);
  QWidget *buildWeatherContent();
  QWidget *buildClockContent();
  QWidget *buildNextUpContent();
  QWidget *buildFocusContent();
  QWidget *buildCaptureContent();
  QWidget *buildShortcutsContent();
  QWidget *buildSearchContent();
  QWidget *buildFavoritesContent();
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
  QTimer *m_clockTimer{nullptr};
  QTimer *m_focusTick{nullptr};
  bool m_focusRunning{false};
  int m_focusSecsLeft{25 * 60};
};
