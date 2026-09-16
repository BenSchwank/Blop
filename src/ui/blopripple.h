#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QWidget>

class BlopRipple : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal ripScale READ ripScale WRITE setRipScale)
  Q_PROPERTY(qreal ripOpacity READ ripOpacity WRITE setRipOpacity)

public:
  static void spawn(QWidget *host, const QPoint &globalPos,
                    const QColor &accent = QColor(QStringLiteral("#7C5CFC")));

  /// Press feedback hook. Intentionally a no-op: geometry shrink broke
  /// QAbstractButton hit-testing on mouse release (no clicked/toggled).
  /// Prefer stylesheet `:pressed` / `:checked` for visual feedback.
  static void animatePress(QWidget *target, qreal pressedScale = 0.94);

  /// v3.18.2: idempotent pressed-hook for fixed-size buttons.
  static void attachPressFeedback(QAbstractButton *btn, qreal pressedScale = 0.94);

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
