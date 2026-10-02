#pragma once

#include <QObject>
#include <QString>

class QProcess;
class QTimer;

class SpeechInput : public QObject {
    Q_OBJECT
public:
    explicit SpeechInput(QObject *parent = nullptr);
    ~SpeechInput() override;

    bool available() const;
    bool listening() const;

public slots:
    void start();
    void stop();

signals:
    void recognized(const QString &text);
    void failed(const QString &reason);
    void listeningChanged(bool listening);

private:
    void report(const QString &text, bool ok);
    void pollHeard();
    void warm();

    bool m_listening = false;
    bool m_reported = false;
    QString m_ear;
    QString m_bootError;
    QProcess *m_proc = nullptr;
    QTimer *m_poll = nullptr;
};
