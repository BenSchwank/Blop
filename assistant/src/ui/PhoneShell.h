#pragma once

#include "core/ActionRunner.h"
#include "core/CommandEngine.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class SpeechInput;

class PhoneShell : public QWidget {
    Q_OBJECT
public:
    explicit PhoneShell(QWidget *parent = nullptr);

private:
    void runCommand(const QString &text);
    void setListening(bool on);

    CommandEngine m_engine;
    ActionRunner m_runner;
    SpeechInput *m_speech = nullptr;
    QLabel *m_status = nullptr;
    QLineEdit *m_edit = nullptr;
    QPushButton *m_mic = nullptr;
};
