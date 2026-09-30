#include "NotchWindow.h"

#include "AccountDialog.h"
#include "platform/SpeechInput.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
#endif

namespace {
constexpr int kHotkeyId = 0xB107;
}

NotchWindow::NotchWindow(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("notch"));
    setWindowTitle(QStringLiteral("Blop Assistent"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    m_speech = new SpeechInput(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 6, 10, 8);
    layout->setSpacing(6);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    m_brand = new QLabel(QStringLiteral("Blop"), this);
    m_brand->setObjectName(QStringLiteral("brand"));
    row->addWidget(m_brand);
    row->addStretch(1);

    m_mic = new QPushButton(QStringLiteral("●"), this);
    m_mic->setObjectName(QStringLiteral("mic"));
    m_mic->setFixedSize(28, 28);
    m_mic->setCursor(Qt::PointingHandCursor);
    m_mic->setFocusPolicy(Qt::NoFocus);
    m_mic->setToolTip(QStringLiteral("Halten und sprechen"));
    m_mic->installEventFilter(this);
    row->addWidget(m_mic);
    auto *account = new QPushButton(QStringLiteral("Konto"), this);
    account->setObjectName(QStringLiteral("account"));
    account->setCursor(Qt::PointingHandCursor);
    account->setFocusPolicy(Qt::NoFocus);
    row->addWidget(account);
    layout->addLayout(row);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(QStringLiteral("Befehl eingeben…"));
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    setStyleSheet(QStringLiteral(
        "QWidget#notch { background: #24262B; border: 1px solid #3A3F48; border-radius: 18px; }"
        "QLabel#brand { color: #5B9DFF; font-weight: 700; }"
        "QLabel#status { color: #D5D8DE; }"
        "QLineEdit { background: #16181C; color: #F4F6F8; border: 1px solid #3A3F48;"
        " border-radius: 10px; padding: 6px 10px; selection-background-color: #5B9DFF; }"
        "QLineEdit:focus { border: 1px solid #5B9DFF; }"
        "QPushButton#mic { background: #343840; color: #F4F6F8; border: none; border-radius: 14px; }"
        "QPushButton#mic[listening=\"true\"] { background: #5B9DFF; color: #0E1116; }"
        "QPushButton#account { background: transparent; color: #D5D8DE; border: none; padding: 0 4px; }"));

    QFont font(QStringLiteral("Segoe UI"));
    font.setPointSize(10);
    setFont(font);

    connect(account, &QPushButton::clicked, this, [this]() {
        if (!m_expanded)
            expand();
        AccountDialog dialog(this);
        dialog.exec();
    });
    connect(m_edit, &QLineEdit::returnPressed, this, [this]() { runCommand(m_edit->text()); });
    connect(m_speech, &SpeechInput::recognized, this, [this](const QString &text) {
        m_edit->setText(text);
        if (!m_expanded)
            expand();
        runCommand(text);
    });
    connect(m_speech, &SpeechInput::failed, this, [this](const QString &reason) {
        if (!m_expanded)
            expand();
        m_status->setText(reason);
        applyChrome();
    });
    connect(m_speech, &SpeechInput::listeningChanged, this, &NotchWindow::setListening);

    applyChrome();
    placeDefault();
    restorePosition();
    qApp->installNativeEventFilter(this);
    registerHotkey();
}

NotchWindow::~NotchWindow() {
    if (qApp)
        qApp->removeNativeEventFilter(this);
#if defined(Q_OS_WIN)
    if (m_hotkey)
        UnregisterHotKey(nullptr, kHotkeyId);
#endif
    savePosition();
}

bool NotchWindow::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) {
#if defined(Q_OS_WIN)
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;
    const auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == kHotkeyId) {
        QMetaObject::invokeMethod(this, [this]() { expand(); }, Qt::QueuedConnection);
        return true;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

void NotchWindow::expand() {
    m_expanded = true;
    applyChrome();
    bringToFront();
    m_edit->setFocus();
}

void NotchWindow::collapse() {
    m_expanded = false;
    m_edit->clearFocus();
    applyChrome();
}

void NotchWindow::applyChrome() {
    const int top = y();
    const int mid = x() + width() / 2;
    m_edit->setVisible(m_expanded);
    const bool showStatus = m_expanded && !m_status->text().isEmpty();
    m_status->setVisible(showStatus);
    if (!m_expanded) {
        setFixedSize(156, 40);
    } else {
        m_status->setFixedWidth(416);
        int height = 46 + m_edit->sizeHint().height();
        if (showStatus)
            height += 8 + qBound(18, m_status->heightForWidth(416), 240);
        setFixedSize(440, height + 12);
    }
    if (m_expanded || top != 0)
        move(mid - width() / 2, top);
}

void NotchWindow::runCommand(const QString &text) {
    const ActionResult result = m_runner.run(m_engine.parse(text));
    m_status->setText(result.message);
    if (!m_expanded)
        m_expanded = true;
    applyChrome();
    m_edit->selectAll();
}

void NotchWindow::placeDefault() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect area = screen->availableGeometry();
    move(area.center().x() - width() / 2, area.top() + 8);
}

void NotchWindow::restorePosition() {
    const QSettings settings;
    const QPoint saved = settings.value(QStringLiteral("notch/pos")).toPoint();
    if (saved.isNull() && !settings.contains(QStringLiteral("notch/pos")))
        return;
    const QRect probe(saved, size());
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->availableGeometry().intersects(probe)) {
            move(saved);
            return;
        }
    }
}

void NotchWindow::savePosition() const {
    QSettings settings;
    settings.setValue(QStringLiteral("notch/pos"), pos());
}

void NotchWindow::bringToFront() {
#if defined(Q_OS_WIN)
    HWND hwnd = reinterpret_cast<HWND>(winId());
    HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
    const DWORD thisThread = GetCurrentThreadId();
    if (foregroundThread != 0 && foregroundThread != thisThread)
        AttachThreadInput(foregroundThread, thisThread, TRUE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    if (foregroundThread != 0 && foregroundThread != thisThread)
        AttachThreadInput(foregroundThread, thisThread, FALSE);
#endif
    raise();
    activateWindow();
}

void NotchWindow::registerHotkey() {
#if defined(Q_OS_WIN)
    if (m_hotkey)
        return;
    m_hotkey = RegisterHotKey(nullptr, kHotkeyId, MOD_CONTROL | MOD_NOREPEAT, VK_SPACE);
#endif
}

void NotchWindow::setListening(bool on) {
    m_mic->setProperty("listening", on);
    m_mic->style()->unpolish(m_mic);
    m_mic->style()->polish(m_mic);
    m_mic->update();
}

void NotchWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && m_expanded) {
        collapse();
        return;
    }
    QWidget::keyPressEvent(event);
}

void NotchWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressed = true;
    m_dragging = false;
    m_pressGlobal = event->globalPosition().toPoint();
    m_dragOffset = m_pressGlobal - frameGeometry().topLeft();
    grabMouse();
}

void NotchWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!m_pressed) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint global = event->globalPosition().toPoint();
    if ((global - m_pressGlobal).manhattanLength() > 4)
        m_dragging = true;
    if (m_dragging)
        move(global - m_dragOffset);
}

void NotchWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (mouseGrabber() == this)
        releaseMouse();
    const bool dragged =
        m_dragging && (event->globalPosition().toPoint() - m_pressGlobal).manhattanLength() > 5;
    m_pressed = false;
    m_dragging = false;
    if (dragged) {
        savePosition();
        return;
    }
    if (m_expanded)
        collapse();
    else
        expand();
}

bool NotchWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_mic) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_mic->grabMouse();
            m_speech->start();
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            if (QWidget::mouseGrabber() == m_mic)
                m_mic->releaseMouse();
            m_speech->stop();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
