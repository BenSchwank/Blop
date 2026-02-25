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

// A particle burst on initial splash impact
struct Particle {
  QPointF pos;
  QPointF vel;
  qreal life; // 1.0 → 0.0
  qreal size;
};

// A single ink drip that falls from the bottom of the logo after it settles
struct InkDrip {
  QPointF
      anchor;   // start point on blob edge (screen coords, relative to center)
  qreal fallY;  // how far it has fallen (0 = at anchor)
  qreal speed;  // pixels per tick
  qreal width;  // drip width
  qreal length; // drip tail length (grows as it falls)
  qreal splash; // 0‥1: splash ring at landing
  bool landed;
  qreal landY; // Y at landing
};

class IntroScreen : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal dropY READ dropY WRITE setDropY)
  Q_PROPERTY(qreal dropScaleX READ dropScaleX WRITE setDropScaleX)
  Q_PROPERTY(qreal dropScaleY READ dropScaleY WRITE setDropScaleY)
  Q_PROPERTY(qreal splash READ splash WRITE setSplash)
  Q_PROPERTY(qreal nibReveal READ nibReveal WRITE setNibReveal)
  Q_PROPERTY(qreal textFade READ textFade WRITE setTextFade)
  Q_PROPERTY(qreal dripsStart READ dripsStart WRITE setDripsStart)

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
  qreal dripsStart() const { return m_dripsStart; }

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
  void setDripsStart(qreal v) {
    bool wasZero = (m_dripsStart == 0.0);
    m_dripsStart = v;
    if (wasZero && v > 0)
      spawnDrips();
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
  void spawnDrips();
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
  qreal m_dripsStart;

  QList<Particle> m_particles;
  QList<InkDrip> m_drips;
  int m_particleTimer;

  QSequentialAnimationGroup *m_seq;
  bool m_finished;

  int m_cx, m_cy; // screen center
};

#endif // INTROSCREEN_H
