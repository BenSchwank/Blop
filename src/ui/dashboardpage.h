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

/// Notion-paper widget board — customize lives in the right shell rail
/// (phone keeps a minimal floating Anpassen).
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
  void buildPhoneChrome();
  void updateHeader();
  void layoutPhoneChrome();
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
  QWidget *overflowAnchor() const;
  static bool layoutsEqual(const QVector<DashboardWidgetSpec> &a,
                           const QVector<DashboardWidgetSpec> &b);

  QVBoxLayout *m_root{nullptr};
  QWidget *m_mainCol{nullptr};
  /// Phone-only floating Anpassen (desktop uses DashRightRail chrome).
  QWidget *m_phoneChrome{nullptr};
  QPushButton *m_btnPhoneCustomize{nullptr};
  QPushButton *m_btnPhoneMore{nullptr};
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
