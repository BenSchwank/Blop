#pragma once

#include "dashboardlayoutstore.h"

#include <QDateTime>
#include <QPoint>
#include <QVector>
#include <QWidget>
#include <functional>

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class QFrame;
class DashSnapOverlay;

/// Notion-style dashboard: soft blocks on a 12-column grid with edit mode.
class DashboardPage : public QWidget {
  Q_OBJECT
public:
  explicit DashboardPage(QWidget *parent = nullptr);

  void refresh();
  void setEditMode(bool on);
  bool editMode() const { return m_editMode; }
  void showCalendarMaximized();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void openNotePath(const QString &path);
  void newNoteRequested();
  void snapToNotesRequested();
  void studyRequested();
  void searchLibrary(const QString &text);
  void customizeToggled(bool editing);

private:
  void applyChromeStyles();
  bool usePhoneDashboard() const;
  void applyDashboardDensity();
  void rebuildWidgets();
  void rebuildEditBar();
  void ensurePersistentHeader();
  void updatePersistentHeader();
  void purgeFloatingHostWidgets();
  void toggleEditMode();
  void persistSpecs();
  void updateSpec(const QString &id,
                  const std::function<void(DashboardWidgetSpec &)> &mutator);
  enum class PushDir { Auto, East, West, South, North };
  void resolveOverlaps(QVector<DashboardWidgetSpec> &specs,
                       const QString &primaryId, PushDir dir) const;
  void commitSpecs(QVector<DashboardWidgetSpec> specs);
  int snapGridLineX(int hostX) const;
  int snapGridLineY(int hostY) const;
  void showBlocksMenu(QPushButton *anchor);
  void snapGridFromPos(const QPoint &hostPos, int colSpan, int rowSpan,
                       int &outRow, int &outCol) const;
  QRect snapHighlightRect(int row, int col, int colSpan, int rowSpan) const;
  void gridColumnEdges(QVector<int> &out) const;
  void gridRowEdges(QVector<int> &out) const;
  int gridRowUnit() const;
  int cellHeightForSpan(int rowSpan) const;
  static int minRowSpanForBlock(const QString &id);
  void applyBlockCellSize(QWidget *block, int rowSpan) const;
  int snapRowFromY(int hostY) const;
  int snapColFromX(int hostX, int colSpan) const;
  int rowTopForSnap(int row) const;
  int maxSnapRow() const;
  void attachResizeHandles(QFrame *frame);
  void layoutResizeHandles(QFrame *frame) const;
  void setScrollLocked(bool locked);
  enum class DashGesture { None, Move, ResizeN, ResizeE, ResizeS, ResizeW };
  void applyResizePreview(const QPoint &hostPos);
  void applyMovePreview(const QPoint &hostPos);
  void beginGesture(QFrame *frame, const QString &blockId, DashGesture gesture);
  void handleGestureRelease();
  void finishGesture(const std::function<void()> &commit);
  void updateSnapOverlay(int row, int col, int colSpan, int rowSpan);
  void clearSnapOverlay();
  QWidget *wrapBlock(const QString &id, QWidget *content, int minHeight = 0,
                     bool showTitle = true);
  QWidget *buildEditChrome(const QString &id);
  QWidget *buildEmptyStatePanel();
  QWidget *buildTodosBlock();
  QWidget *buildClockBlock();
  QWidget *buildCalendarBlock(bool maximizedChrome);
  QWidget *buildRecentBlock();
  QWidget *buildShortcutsBlock();
  QWidget *buildActionsBlock();
  QWidget *buildContentFor(const QString &id, bool maximizedChrome = false);

  void openCreateEventDialog(const QDateTime &presetStart = QDateTime());

  QVBoxLayout *m_rootLay{nullptr};
  QWidget *m_persistentHeader{nullptr};
  QLabel *m_lblHello{nullptr};
  QLabel *m_lblDate{nullptr};
  QLabel *m_lblClock{nullptr};
  QLabel *m_lblMetrics{nullptr};
  QPushButton *m_btnEdit{nullptr};
  QPushButton *m_btnBlocks{nullptr};
  bool m_headerBuilt{false};
  QTimer *m_clockTimer{nullptr};
  QWidget *m_host{nullptr};
  QScrollArea *m_scroll{nullptr};
  QGridLayout *m_gridLay{nullptr};
  QWidget *m_editBar{nullptr};
  QHBoxLayout *m_editBarLay{nullptr};
  DashSnapOverlay *m_snapOverlay{nullptr};
  QFrame *m_dragFrame{nullptr};
  bool m_editMode{false};
  DashGesture m_gesture{DashGesture::None};
  bool m_dragging{false};
  QString m_dragBlockId;
  int m_dragStartRow{0};
  int m_dragStartCol{0};
  int m_dragColSpan{6};
  int m_dragRowSpan{1};
  int m_dragStartColSpan{6};
  int m_dragStartRowSpan{1};
  int m_previewRow{0};
  int m_previewCol{0};
  int m_previewColSpan{6};
  int m_previewRowSpan{1};
  QPoint m_dragOffset;
  QPoint m_pressHostPos;
  QPoint m_dragOriginHost;
  bool m_havePressHostPos{false};
  bool m_floatActive{false};
  bool m_appFilterInstalled{false};
  int m_frozenScrollY{0};
  QMetaObject::Connection m_scrollFreezeConn;
  class QDialog *m_calMaxDlg{nullptr};
};
