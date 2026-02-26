#ifndef INTROSCREEN_H
#define INTROSCREEN_H

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWidget>


class IntroScreen : public QWidget {
  Q_OBJECT

public:
  explicit IntroScreen(QWidget *parent = nullptr);
  ~IntroScreen();

  void startAnimation();

signals:
  void introFinished();

private slots:
  void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
  void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);

private:
  QMediaPlayer *m_player;
  QAudioOutput *m_audioOutput;
  QVideoWidget *m_videoWidget;
  bool m_finished;
};

#endif // INTROSCREEN_H
