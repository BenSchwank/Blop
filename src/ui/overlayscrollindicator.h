#ifndef OVERLAYSCROLLINDICATOR_H
#define OVERLAYSCROLLINDICATOR_H

#include <QPointer>
#include <QWidget>

class QAbstractScrollArea;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;

// Thin, auto-hiding scroll indicator that replaces the native QScrollBar on
// QAbstractScrollArea subclasses (QGraphicsView, QScrollArea, QListView,
// QTreeView, ...). Visual: 4 dp rounded pill, accent-neutral grey, alpha 200
// at peak. Lifecycle: 80 ms fade-in -> 600 ms hold -> 200 ms fade-out.
//
// Usage:
//   OverlayScrollIndicator::install(myScrollArea);
// Installs both axes and forces ScrollBarAlwaysOff on the host so the native
// chrome disappears. Indicators are parented to the viewport so they pan with
// it and never intercept mouse / touch events.
class OverlayScrollIndicator : public QWidget {
  Q_OBJECT
public:
  enum Orientation { Vertical, Horizontal };

  // Install both axes on `area` and hide the native scrollbars. Safe to call
  // multiple times; only attaches once per area.
  static void install(QAbstractScrollArea *area);

  explicit OverlayScrollIndicator(QAbstractScrollArea *area, Orientation o);

protected:
  void paintEvent(QPaintEvent *) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onScrolled();
  void onRangeChanged();

private:
  void positionSelf();
  void showIndicator();
  void scheduleHide();

  QPointer<QAbstractScrollArea> m_area;
  Orientation m_orientation;
  QGraphicsOpacityEffect *m_opacity{nullptr};
  QPropertyAnimation *m_fadeAnim{nullptr};
  QTimer *m_hideTimer{nullptr};
};

#endif // OVERLAYSCROLLINDICATOR_H
