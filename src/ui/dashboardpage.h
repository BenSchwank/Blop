#pragma once

#include "dashboardlayoutstore.h"

#include <QWidget>

class DashCanvas;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class QDialog;

/// Notion-paper widget board — shell + header; layout lives in DashCanvas.
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

private:
  void applyChrome();
  void buildHeader();
  void updateHeader();
  void toggleEditMode();
  void showBlocksMenu();
  void persistAndApply(const QVector<DashboardWidgetSpec> &specs);
  void resetLayout();
  bool usePhone() const;
  void applyDensity();

  QVBoxLayout *m_root{nullptr};
  QWidget *m_header{nullptr};
  QLabel *m_hello{nullptr};
  QLabel *m_date{nullptr};
  QLabel *m_metrics{nullptr};
  QPushButton *m_btnEdit{nullptr};
  QPushButton *m_btnBlocks{nullptr};
  QPushButton *m_btnReset{nullptr};
  QScrollArea *m_scroll{nullptr};
  DashCanvas *m_canvas{nullptr};
  QTimer *m_clockTimer{nullptr};
  bool m_editMode{false};
  QDialog *m_calMaxDlg{nullptr};
};
