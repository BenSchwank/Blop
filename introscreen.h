#ifndef INTROSCREEN_H
#define INTROSCREEN_H

#include <QList>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QVariantAnimation>
#include <QWidget>


class IntroScreen : public QWidget {
  Q_OBJECT
  // Wir animieren numerisch durch die Bilder (0.0 bis 4.0)
  Q_PROPERTY(qreal frameProgress READ frameProgress WRITE setFrameProgress)

public:
  explicit IntroScreen(QWidget *parent = nullptr);
  ~IntroScreen();

  void startAnimation();

  qreal frameProgress() const { return m_frameProgress; }
  void setFrameProgress(qreal progress) {
    m_frameProgress = progress;
    update();
  }

signals:
  void introFinished();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void finishIntro();

  QList<QPixmap> m_images;
  qreal m_frameProgress;

  QSequentialAnimationGroup *m_mainSequence;
  bool m_finished;
};

#endif // INTROSCREEN_H
