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
