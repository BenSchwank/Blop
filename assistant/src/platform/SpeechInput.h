#pragma once

#include <QObject>
#include <QString>

class QProcess;

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

    bool m_listening = false;
    bool m_reported = false;
    QString m_stopFile;
    QProcess *m_proc = nullptr;
};
