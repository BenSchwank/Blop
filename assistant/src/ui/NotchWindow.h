#pragma once

#include "core/ActionRunner.h"
#include "core/CommandEngine.h"

#include <QAbstractNativeEventFilter>
#include <QWidget>

class QEnterEvent;
class QKeyEvent;
class QShowEvent;
class QLabel;
class QLineEdit;
class QMouseEvent;
class QPushButton;
class SpeechInput;

class NotchWindow : public QWidget, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit NotchWindow(QWidget *parent = nullptr);
    ~NotchWindow() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;
    void reveal();
    void reloadVoiceHotkey();

signals:
    void settingsRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void expand();
    void collapse();
    void applyChrome();
    void pinToTop();
    void runCommand(const QString &text);
    void bringToFront();
    void registerHotkey();
    void setListening(bool on);
    void toggleSpeech();
    void scheduleIdle();
    bool surfaceOpen() const;

    CommandEngine m_engine;
    ActionRunner m_runner;
    SpeechInput *m_speech = nullptr;
    QLabel *m_mark = nullptr;
    QLabel *m_status = nullptr;
    QLineEdit *m_edit = nullptr;
    QPushButton *m_gear = nullptr;

    bool m_expanded = false;
    bool m_hovered = false;
    bool m_hotkey = false;
    bool m_listening = false;
};
