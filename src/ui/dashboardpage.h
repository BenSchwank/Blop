#pragma once

#include "dashboardlayoutstore.h"

#include <QVector>
#include <QWidget>

class BlopModal;
class DashCanvas;
class DashPageScroll;
class DashRightRail;
class QPushButton;
class QShortcut;
class QTimer;
class QVBoxLayout;
class QWidget;

/// Notion-paper widget board — shell + quiet overflow; layout lives in DashCanvas.
class DashboardPage : public QWidget {
  Q_OBJECT
public:
  explicit DashboardPage(QWidget *parent = nullptr);

  void refresh();
  void setEditMode(bool on);
  bool editMode() const { return m_editMode; }
  void showCalendarMaximized();

signals:
  void openNotePath(const QString &path);
  void newNoteRequested();
  void snapToNotesRequested();
  void studyRequested();
  void searchLibrary(const QString &text);
  void customizeToggled(bool editing);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void applyChrome();
  void buildHeader();
  void updateHeader();
  void layoutFloatingHeader();
  void toggleEditMode();
  void showOverflowMenu();
  void showBlocksMenu();
  void persistAndApply(const QVector<DashboardWidgetSpec> &specs);
  void resetLayout();
  bool usePhone() const;
  void applyDensity();
  void undoLayout();
  void updateUndoButton();
  void syncRailFromBoard();
  static bool layoutsEqual(const QVector<DashboardWidgetSpec> &a,
                           const QVector<DashboardWidgetSpec> &b);

  QVBoxLayout *m_root{nullptr};
  QWidget *m_mainCol{nullptr};
  QWidget *m_header{nullptr};
  QPushButton *m_btnCustomize{nullptr};
  QPushButton *m_btnUndo{nullptr};
  QPushButton *m_btnMore{nullptr};
  QWidget *m_bodyRow{nullptr};
  DashPageScroll *m_scroll{nullptr};
  DashCanvas *m_canvas{nullptr};
  DashRightRail *m_rightRail{nullptr};
  QTimer *m_clockTimer{nullptr};
  QShortcut *m_undoShortcut{nullptr};
  bool m_editMode{false};
  bool m_undoRestoring{false};
  BlopModal *m_calModal{nullptr};
  QVector<DashboardWidgetSpec> m_committed;
  QVector<QVector<DashboardWidgetSpec>> m_undoStack;
};
