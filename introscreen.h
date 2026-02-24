#ifndef INTROSCREEN_H
#define INTROSCREEN_H

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QVariantAnimation>
#include <QWidget>

class IntroScreen : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal dropY READ dropY WRITE setDropY)
  Q_PROPERTY(qreal dropStretch READ dropStretch WRITE setDropStretch)
  Q_PROPERTY(qreal splashProgress READ splashProgress WRITE setSplashProgress)
  Q_PROPERTY(qreal logoOpacity READ logoOpacity WRITE setLogoOpacity)
  Q_PROPERTY(qreal textOpacity READ textOpacity WRITE setTextOpacity)

public:
  explicit IntroScreen(QWidget *parent = nullptr);
  ~IntroScreen();

  void startAnimation();

  qreal dropY() const { return m_dropY; }
  void setDropY(qreal v) {
    m_dropY = v;
    update();
  }

  qreal dropStretch() const { return m_dropStretch; }
  void setDropStretch(qreal v) {
    m_dropStretch = v;
    update();
  }

  qreal splashProgress() const { return m_splashProgress; }
  void setSplashProgress(qreal v) {
    m_splashProgress = v;
    update();
  }

  qreal logoOpacity() const { return m_logoOpacity; }
  void setLogoOpacity(qreal v) {
    m_logoOpacity = v;
    update();
  }

  qreal textOpacity() const { return m_textOpacity; }
  void setTextOpacity(qreal v) {
    m_textOpacity = v;
    update();
  }

signals:
  void introFinished();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void finishIntro();

  qreal m_dropY;
  qreal m_dropStretch;
  qreal m_splashProgress;
  qreal m_logoOpacity;
  qreal m_textOpacity;

  QSequentialAnimationGroup *m_mainSequence;
  bool m_finished;
};

#endif // INTROSCREEN_H
