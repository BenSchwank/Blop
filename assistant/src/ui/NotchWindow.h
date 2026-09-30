#pragma once

#include "core/ActionRunner.h"
#include "core/CommandEngine.h"

#include <QAbstractNativeEventFilter>
#include <QPoint>
#include <QWidget>

class QKeyEvent;
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

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void expand();
    void collapse();
    void applyChrome();
    void runCommand(const QString &text);
    void placeDefault();
    void restorePosition();
    void savePosition() const;
    void bringToFront();
    void registerHotkey();
    void setListening(bool on);

    CommandEngine m_engine;
    ActionRunner m_runner;
    SpeechInput *m_speech = nullptr;
    QLabel *m_brand = nullptr;
    QLabel *m_status = nullptr;
    QLineEdit *m_edit = nullptr;
    QPushButton *m_mic = nullptr;

    bool m_expanded = false;
    bool m_pressed = false;
    bool m_dragging = false;
    bool m_hotkey = false;
    QPoint m_pressGlobal;
    QPoint m_dragOffset;
};
