#ifndef INTROSCREEN_H
#define INTROSCREEN_H

#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPointF>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QWidget>


struct Particle {
  QPointF pos;
  QPointF vel;
  qreal life; // 1.0 → 0.0
  qreal size;
};

class IntroScreen : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal dropY READ dropY WRITE setDropY)
  Q_PROPERTY(qreal dropScaleX READ dropScaleX WRITE setDropScaleX)
  Q_PROPERTY(qreal dropScaleY READ dropScaleY WRITE setDropScaleY)
  Q_PROPERTY(qreal splash READ splash WRITE setSplash)
  Q_PROPERTY(qreal nibReveal READ nibReveal WRITE setNibReveal)
  Q_PROPERTY(qreal textFade READ textFade WRITE setTextFade)

public:
  explicit IntroScreen(QWidget *parent = nullptr);
  ~IntroScreen() {}

  void startAnimation();

  qreal dropY() const { return m_dropY; }
  qreal dropScaleX() const { return m_dropScaleX; }
  qreal dropScaleY() const { return m_dropScaleY; }
  qreal splash() const { return m_splash; }
  qreal nibReveal() const { return m_nibReveal; }
  qreal textFade() const { return m_textFade; }

  void setDropY(qreal v) {
    m_dropY = v;
    update();
  }
  void setDropScaleX(qreal v) {
    m_dropScaleX = v;
    update();
  }
  void setDropScaleY(qreal v) {
    m_dropScaleY = v;
    update();
  }
  void setSplash(qreal v) {
    bool wasZero = (m_splash == 0.0);
    m_splash = v;
    if (wasZero && v > 0)
      spawnParticles();
    update();
  }
  void setNibReveal(qreal v) {
    m_nibReveal = v;
    update();
  }
  void setTextFade(qreal v) {
    m_textFade = v;
    update();
  }

signals:
  void introFinished();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void timerEvent(QTimerEvent *event) override;

private:
  void finishIntro();
  void spawnParticles();
  void drawGlowShape(QPainter &p, const QPainterPath &path, const QColor &col,
                     int layers);
  QPainterPath buildBlobPath(qreal scale) const;
  QPainterPath buildDropPath(qreal scaleX, qreal scaleY) const;

  qreal m_dropY;
  qreal m_dropScaleX;
  qreal m_dropScaleY;
  qreal m_splash;
  qreal m_nibReveal;
  qreal m_textFade;

  QList<Particle> m_particles;
  int m_particleTimer;

  QSequentialAnimationGroup *m_seq;
  bool m_finished;

  int m_cx, m_cy; // screen center
};

#endif // INTROSCREEN_H
