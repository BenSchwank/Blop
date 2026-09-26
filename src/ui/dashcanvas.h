#pragma once

#include "dashboardlayoutstore.h"

#include <QHash>
#include <QPoint>
#include <QVector>
#include <QWidget>

class DashWidget;
class QScrollArea;
class DashSnapGhost;

/// Discrete 12-col board host. Move + size-class resize with collision reflow.
class DashCanvas : public QWidget {
  Q_OBJECT
public:
  explicit DashCanvas(QWidget *parent = nullptr);

  void setPhoneMode(bool phone);
  void setEditMode(bool on);
  bool editMode() const { return m_editMode; }

  void setSpecs(const QVector<DashboardWidgetSpec> &specs);
  QVector<DashboardWidgetSpec> specs() const { return m_specs; }

  void syncWidgets();
  void applyPositions();
  void refreshAll();

  DashWidget *widgetFor(const QString &id) const;
  /// Change size class and push neighbors so nothing overlaps / leaves the grid.
  void resizeBlock(const QString &id, DashSizeClass sizeClass);

signals:
  void specsChanged(const QVector<DashboardWidgetSpec> &specs);
  void openNotePath(const QString &path);
  void newNoteRequested();
  void snapToNotesRequested();
  void studyRequested();
  void searchLibrary(const QString &text);
  void maximizeCalendarRequested();

protected:
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  enum class Gesture { None, Move, Resize };

  int rowUnit() const;
  int hGap() const;
  int vGap() const;
  QMargins boardMargins() const;
  QRect cellRect(int row, int col, int rowSpan, int colSpan) const;
  void layoutPhoneStack();
  void layoutDesktopGrid();
  int contentBottom() const;

  bool overlaps(const DashboardWidgetSpec &a,
                const DashboardWidgetSpec &b) const;
  bool canPlace(const DashboardWidgetSpec &candidate,
                const QString &ignoreId) const;
  DashboardWidgetSpec *findSpec(const QString &id);
  void clampSpec(DashboardWidgetSpec &s) const;
  void reflowKeeping(const QString &anchorId);
  DashSizeClass nearestSizeClass(int colSpan, int rowSpan) const;

  void beginMove(DashWidget *w, const QPoint &globalPos);
  void beginResize(DashWidget *w, const QPoint &globalPos);
  void updateGesture(const QPoint &globalPos);
  void endGesture();
  void clearGhost();
  void setScrollLocked(bool locked);
  QScrollArea *scrollArea() const;
  void wireWidget(DashWidget *w);

  QVector<DashboardWidgetSpec> m_specs;
  QHash<QString, DashWidget *> m_widgets;
  bool m_phone{false};
  bool m_editMode{false};

  Gesture m_gesture{Gesture::None};
  DashWidget *m_dragWidget{nullptr};
  QString m_dragId;
  QPoint m_pressGlobal;
  QPoint m_originTopLeft;
  QSize m_originSize;
  DashSizeClass m_startSize{DashSizeClass::M};
  int m_previewRow{0};
  int m_previewCol{0};
  DashSizeClass m_previewSize{DashSizeClass::M};
  bool m_dragging{false};
  DashSnapGhost *m_ghost{nullptr};
  int m_frozenScrollY{0};
  Qt::ScrollBarPolicy m_savedVScrollPolicy{Qt::ScrollBarAsNeeded};
  QMetaObject::Connection m_scrollFreezeConn;
  QMetaObject::Connection m_scrollRangeConn;
};
