#include "blopassistantmic.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>
#include <QtMath>

namespace {

void put32(QByteArray *out, quint32 v) {
  char b[4] = {char(v & 0xff), char((v >> 8) & 0xff), char((v >> 16) & 0xff),
               char((v >> 24) & 0xff)};
  out->append(b, 4);
}

void put16(QByteArray *out, quint16 v) {
  char b[2] = {char(v & 0xff), char((v >> 8) & 0xff)};
  out->append(b, 2);
}

} // namespace

BlopAssistantMic::BlopAssistantMic(QObject *parent) : QObject(parent) {
  resetLevels();
}

BlopAssistantMic::~BlopAssistantMic() { cancel(); }

QByteArray BlopAssistantMic::makeWav(const QByteArray &pcm, int sampleRate,
                                    int channels, int bits) {
  QByteArray wav;
  wav.reserve(44 + pcm.size());
  const quint32 dataSize = quint32(pcm.size());
  const quint16 ch = quint16(qMax(1, channels));
  const quint16 bps = quint16(bits);
  const quint32 rate = quint32(qMax(1, sampleRate));
  const quint16 blockAlign = quint16(ch * (bps / 8));
  const quint32 byteRate = rate * blockAlign;
  wav.append("RIFF", 4);
  put32(&wav, 36 + dataSize);
  wav.append("WAVE", 4);
  wav.append("fmt ", 4);
  put32(&wav, 16);
  put16(&wav, 1);
  put16(&wav, ch);
  put32(&wav, rate);
  put32(&wav, byteRate);
  put16(&wav, blockAlign);
  put16(&wav, bps);
  wav.append("data", 4);
  put32(&wav, dataSize);
  wav.append(pcm);
  return wav;
}

bool BlopAssistantMic::available() const {
  return !QMediaDevices::defaultAudioInput().isNull();
}

void BlopAssistantMic::resetLevels() {
  m_levels = QVector<qreal>(7, 0.07);
}

bool BlopAssistantMic::start() {
  cancel();
  const QAudioDevice device = QMediaDevices::defaultAudioInput();
  if (device.isNull()) {
    emit failed(QStringLiteral("Kein Mikrofon gefunden."));
    return false;
  }

  QAudioFormat fmt;
  fmt.setSampleRate(16000);
  fmt.setChannelCount(1);
  fmt.setSampleFormat(QAudioFormat::Int16);
  if (!device.isFormatSupported(fmt))
    fmt = device.preferredFormat();
  if (!fmt.isValid()) {
    emit failed(QStringLiteral("Mikrofon-Format wird nicht unterstützt."));
    return false;
  }

  m_sampleRate = fmt.sampleRate() > 0 ? fmt.sampleRate() : 16000;
  m_channels = qMax(1, fmt.channelCount());
  m_float = fmt.sampleFormat() == QAudioFormat::Float;
  m_bytesPerSample = m_float ? 4 : 2;
  m_pcm.clear();
  m_pcm.reserve(m_sampleRate * m_channels * m_bytesPerSample * 8);
  resetLevels();

  m_source = new QAudioSource(device, fmt, this);
  m_io = m_source->start();
  if (!m_io) {
    delete m_source;
    m_source = nullptr;
    emit failed(QStringLiteral("Mikrofon ließ sich nicht starten."));
    return false;
  }
  connect(m_io, &QIODevice::readyRead, this, &BlopAssistantMic::pull);
  if (!m_pump) {
    m_pump = new QTimer(this);
    m_pump->setInterval(20);
    connect(m_pump, &QTimer::timeout, this, &BlopAssistantMic::pull);
  }
  m_pump->start();
  m_active = true;
  return true;
}

void BlopAssistantMic::pull() {
  if (!m_io || !m_active)
    return;
  const QByteArray chunk = m_io->readAll();
  if (!chunk.isEmpty())
    ingest(chunk);
}

void BlopAssistantMic::ingest(const QByteArray &chunk) {
  m_pcm.append(chunk);
  const int stride = qMax(1, m_channels) * m_bytesPerSample;
  const int frames = chunk.size() / stride;
  if (frames < 8)
    return;

  QVector<qreal> energy(7, 0.0);
  QVector<int> counts(7, 0);
  auto add = [&](int frame, qreal amp) {
    const int band = qBound(0, frame * 7 / qMax(1, frames), 6);
    energy[band] += amp * amp;
    counts[band] += 1;
  };

  if (m_float) {
    const float *s = reinterpret_cast<const float *>(chunk.constData());
    const int n = chunk.size() / int(sizeof(float));
    for (int i = 0; i + m_channels <= n; i += m_channels) {
      qreal mix = 0.0;
      for (int c = 0; c < m_channels; ++c)
        mix += qAbs(qreal(s[i + c]));
      mix /= m_channels;
      add(i / m_channels, mix);
    }
  } else {
    const qint16 *s = reinterpret_cast<const qint16 *>(chunk.constData());
    const int n = chunk.size() / int(sizeof(qint16));
    for (int i = 0; i + m_channels <= n; i += m_channels) {
      qreal mix = 0.0;
      for (int c = 0; c < m_channels; ++c)
        mix += qAbs(qreal(s[i + c]) / 32768.0);
      mix /= m_channels;
      add(i / m_channels, mix);
    }
  }

  for (int i = 0; i < 7; ++i) {
    const qreal rms =
        counts[i] > 0 ? qSqrt(energy[i] / qreal(counts[i])) : 0.0;
    // Speech RMS is small; gain so talking actually moves the island bars.
    const qreal vu = qBound(0.06, rms * 9.5, 1.0);
    m_levels[i] = qMax(vu, m_levels[i] * 0.62);
  }
  emit levelsChanged();
}

QByteArray BlopAssistantMic::stopWav() {
  pull();
  m_active = false;
  if (m_pump)
    m_pump->stop();
  if (m_source)
    m_source->stop();
  delete m_source;
  m_source = nullptr;
  m_io = nullptr;

  QByteArray pcm = m_pcm;
  m_pcm.clear();
  if (pcm.isEmpty())
    return {};

  if (m_float) {
    const float *s = reinterpret_cast<const float *>(pcm.constData());
    const int n = pcm.size() / int(sizeof(float));
    QByteArray i16;
    i16.resize((n / m_channels) * 2);
    qint16 *d = reinterpret_cast<qint16 *>(i16.data());
    int o = 0;
    for (int i = 0; i + m_channels <= n; i += m_channels) {
      qreal mix = 0.0;
      for (int c = 0; c < m_channels; ++c)
        mix += qreal(s[i + c]);
      mix /= m_channels;
      mix = qBound(-1.0, mix, 1.0);
      d[o++] = qint16(mix * 32767.0);
    }
    i16.resize(o * 2);
    return makeWav(i16, m_sampleRate, 1, 16);
  }

  if (m_channels > 1) {
    const qint16 *s = reinterpret_cast<const qint16 *>(pcm.constData());
    const int n = pcm.size() / int(sizeof(qint16));
    QByteArray mono;
    mono.resize((n / m_channels) * 2);
    qint16 *d = reinterpret_cast<qint16 *>(mono.data());
    int o = 0;
    for (int i = 0; i + m_channels <= n; i += m_channels) {
      int acc = 0;
      for (int c = 0; c < m_channels; ++c)
        acc += s[i + c];
      d[o++] = qint16(acc / m_channels);
    }
    mono.resize(o * 2);
    return makeWav(mono, m_sampleRate, 1, 16);
  }
  return makeWav(pcm, m_sampleRate, 1, 16);
}

void BlopAssistantMic::cancel() {
  m_active = false;
  if (m_pump)
    m_pump->stop();
  if (m_source)
    m_source->stop();
  delete m_source;
  m_source = nullptr;
  m_io = nullptr;
  m_pcm.clear();
  resetLevels();
}
