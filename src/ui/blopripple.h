#pragma once

#include <QColor>
#include <QWidget>

class BlopRipple : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal ripScale READ ripScale WRITE setRipScale)
  Q_PROPERTY(qreal ripOpacity READ ripOpacity WRITE setRipOpacity)

public:
  static void spawn(QWidget *host, const QPoint &globalPos,
                    const QColor &accent = QColor(QStringLiteral("#7C5CFC")));

  /// v3.17.0: short scale-down + spring-back animation on a widget.
  /// Mirrors the haptic feel of `runTilePressScaleAnim` in
  /// androidtiledelegate.cpp without requiring a top-level Tool window.
  /// Safe to call on any platform; uses QGraphicsOpacityEffect-free
  /// `QPropertyAnimation` on a windowOpacity proxy + geometry shrink.
  /// `pressedScale` is the minimum scale at the peak of the press (~0.94 is
  /// a good default). The widget is restored on completion.
  static void animatePress(QWidget *target, qreal pressedScale = 0.94);

  qreal ripScale() const { return m_ripScale; }
  void setRipScale(qreal s);
  qreal ripOpacity() const { return m_ripOpacity; }
  void setRipOpacity(qreal o);

protected:
  void paintEvent(QPaintEvent *e) override;

private:
  explicit BlopRipple(const QColor &accent, QWidget *parent = nullptr);
  QColor m_accent;
  qreal m_ripScale{0.0};
  qreal m_ripOpacity{1.0};
};
