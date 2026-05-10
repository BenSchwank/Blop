#ifndef INTROSCREEN_H
#define INTROSCREEN_H

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWidget>
#include <QPropertyAnimation>
#include <QEvent>
#include <QResizeEvent>


class IntroScreen : public QWidget {
  Q_OBJECT

public:
  explicit IntroScreen(QWidget *parent = nullptr);
  ~IntroScreen();

  void startAnimation();

signals:
  void introFinished();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
  void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
  void onAnimationFinished();

private:
  QMediaPlayer *m_player;
  QAudioOutput *m_audioOutput;
  QVideoWidget *m_videoWidget;
  bool m_finished;
};

#endif // INTROSCREEN_H
