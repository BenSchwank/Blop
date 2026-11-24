#pragma once

#include <QByteArray>
#include <QObject>
#include <QVector>
#include <QtGlobal>

class QAudioSource;
class QIODevice;
class QTimer;

// Live microphone capture for the companion. Levels follow real RMS, not a
// canned animation. stopWav() returns 16-bit PCM WAV for Whisper / SAPI.
class BlopAssistantMic : public QObject {
  Q_OBJECT
public:
  explicit BlopAssistantMic(QObject *parent = nullptr);
  ~BlopAssistantMic() override;

  static QByteArray makeWav(const QByteArray &pcm, int sampleRate,
                            int channels = 1, int bits = 16);

  bool available() const;
  bool isActive() const { return m_active; }
  bool start();
  QByteArray stopWav();
  void cancel();

  QVector<qreal> levels() const { return m_levels; }
  int barCount() const { return 7; }

signals:
  void levelsChanged();
  void failed(const QString &reason);

private:
  void pull();
  void ingest(const QByteArray &chunk);
  void resetLevels();

  QAudioSource *m_source{nullptr};
  QIODevice *m_io{nullptr};
  QTimer *m_pump{nullptr};
  QByteArray m_pcm;
  QVector<qreal> m_levels;
  int m_sampleRate{16000};
  int m_channels{1};
  int m_bytesPerSample{2};
  bool m_float{false};
  bool m_active{false};
};
