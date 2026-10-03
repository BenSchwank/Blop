#include "NotchWindow.h"

#include "AssistantLogo.h"
#include "core/SettingsSync.h"
#include "platform/SpeechInput.h"

#include <atomic>

#include <QApplication>
#include <QCursor>
#include <QEnterEvent>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
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

#if defined(Q_OS_WIN)
HHOOK g_holdHook = nullptr;
std::atomic<unsigned> g_hotVk{0};
std::atomic<unsigned> g_hotMods{0};
std::atomic<unsigned> g_releasedVk{0};

bool holdVkMatches(unsigned vk) {
    const unsigned hotVk = g_hotVk.load();
    const unsigned mods = g_hotMods.load();
    if (hotVk != 0 && vk == hotVk)
        return true;
    if ((mods & MOD_CONTROL) && (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL))
        return true;
    if ((mods & MOD_SHIFT) && (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT))
        return true;
    if ((mods & MOD_ALT) && (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU))
        return true;
    if ((mods & MOD_WIN) && (vk == VK_LWIN || vk == VK_RWIN))
        return true;
    return false;
}

LRESULT CALLBACK holdHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)) {
        const auto *info = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (info && holdVkMatches(info->vkCode))
            g_releasedVk.store(info->vkCode);
    }
    return CallNextHookEx(g_holdHook, code, wParam, lParam);
}
#endif

#if defined(Q_OS_WIN)
UINT virtualKey(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<UINT>(key);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<UINT>(key);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + static_cast<UINT>(key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Tab:
        return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Down:
        return VK_DOWN;
    default:
        return 0;
    }
}

bool winHotkey(const QKeySequence &sequence, UINT *mods, UINT *vk) {
    if (sequence.isEmpty())
        return false;
    const QKeyCombination combo = sequence[0];
    UINT value = MOD_NOREPEAT;
    const Qt::KeyboardModifiers keyboard = combo.keyboardModifiers();
    if (keyboard.testFlag(Qt::ControlModifier))
        value |= MOD_CONTROL;
    if (keyboard.testFlag(Qt::AltModifier))
        value |= MOD_ALT;
    if (keyboard.testFlag(Qt::ShiftModifier))
        value |= MOD_SHIFT;
    if (keyboard.testFlag(Qt::MetaModifier))
        value |= MOD_WIN;
    const UINT key = virtualKey(combo.key());
    if (key == 0)
        return false;
    *mods = value;
    *vk = key;
    return true;
}
#endif
} // namespace

NotchWindow::NotchWindow(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("notch"));
    setWindowTitle(QStringLiteral("Blop Assistent"));
    setWindowIcon(assistantLogoIcon());
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);

    m_speech = new SpeechInput(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(12, 4, 8, 0);
    row->setSpacing(8);
    m_mark = new QLabel(this);
    m_mark->setPixmap(assistantLogoPixmap(22));
    m_mark->setFixedSize(22, 22);
    m_mark->setAlignment(Qt::AlignCenter);
    row->addWidget(m_mark);
    row->addStretch(1);

    m_gear = new QPushButton(QString(QChar(0x2699)), this);
    m_gear->setObjectName(QStringLiteral("gear"));
    m_gear->setFixedSize(22, 22);
    m_gear->setCursor(Qt::PointingHandCursor);
    m_gear->setFocusPolicy(Qt::NoFocus);
    m_gear->setToolTip(QStringLiteral("Einstellungen"));
    m_gear->installEventFilter(this);
    row->addWidget(m_gear);
    layout->addLayout(row);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(QStringLiteral("Befehl eingeben…"));
    m_edit->setClearButtonEnabled(true);
    m_edit->installEventFilter(this);
    layout->addWidget(m_edit);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    setStyleSheet(QStringLiteral(
        "QWidget#notch { background: transparent; border: none; }"
        "QLabel#status { color: #D5D8DE; }"
        "QLineEdit { background: #16181C; color: #F4F6F8; border: 1px solid #3A3F48;"
        " border-radius: 10px; padding: 6px 10px; selection-background-color: #5B9DFF; }"
        "QLineEdit:focus { border: 1px solid #5B9DFF; }"
        "QPushButton#gear { background: transparent; color: #D5D8DE; border: none; font-size: 14px; }"
        "QPushButton#gear:hover { color: #5B9DFF; }"));

    QFont font(QStringLiteral("Segoe UI"));
    font.setPointSize(10);
    setFont(font);

    connect(m_gear, &QPushButton::clicked, this, &NotchWindow::settingsRequested);
    connect(m_edit, &QLineEdit::returnPressed, this, [this]() { runCommand(m_edit->text()); });
    connect(m_speech, &SpeechInput::recognized, this, [this](const QString &text) {
        m_edit->setText(text);
        if (!m_expanded)
            expand();
        runCommand(text);
        scheduleIdle();
    });
    connect(m_speech, &SpeechInput::failed, this, [this](const QString &reason) {
        if (!m_expanded)
            expand();
        m_status->setText(reason);
        applyChrome();
        scheduleIdle();
    });
    connect(m_speech, &SpeechInput::listeningChanged, this, &NotchWindow::setListening);

    m_status->installEventFilter(this);

    m_holdTimer = new QTimer(this);
    m_holdTimer->setInterval(30);
    connect(m_holdTimer, &QTimer::timeout, this, &NotchWindow::pollHold);

    applyChrome();
    qApp->installNativeEventFilter(this);
    registerHotkey();
#if defined(Q_OS_WIN)
    g_holdHook = SetWindowsHookExW(WH_KEYBOARD_LL, holdHookProc, GetModuleHandleW(nullptr), 0);
#endif
}

NotchWindow::~NotchWindow() {
    if (qApp)
        qApp->removeNativeEventFilter(this);
#if defined(Q_OS_WIN)
    if (g_holdHook) {
        UnhookWindowsHookEx(g_holdHook);
        g_holdHook = nullptr;
    }
    if (m_hotkey)
        UnregisterHotKey(nullptr, kHotkeyId);
#endif
}

bool NotchWindow::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) {
#if defined(Q_OS_WIN)
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;
    const auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == kHotkeyId) {
        QMetaObject::invokeMethod(this, [this]() { beginHold(); }, Qt::QueuedConnection);
        return true;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

void NotchWindow::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    if (!surfaceOpen()) {
        painter.setBrush(m_listening ? QColor(0x5B, 0x9D, 0xFF) : QColor(0x11, 0x12, 0x14));
        const qreal radius = height() / 2.0;
        painter.setClipRect(rect());
        QPainterPath path;
        path.addRoundedRect(QRectF(0, -radius, width(), height() + radius), radius, radius);
        painter.drawPath(path);
        return;
    }
    painter.setBrush(QColor(0x24, 0x26, 0x2B));
    painter.drawRoundedRect(QRectF(rect()), 14, 14);
}

void NotchWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
#if defined(Q_OS_WIN)
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    style = (style & ~WS_EX_TOOLWINDOW) | WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
}

void NotchWindow::reveal() {
    show();
    expand();
}

void NotchWindow::reloadVoiceHotkey() {
    registerHotkey();
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

bool NotchWindow::surfaceOpen() const {
    return m_hovered || m_expanded;
}

void NotchWindow::applyChrome() {
    const bool open = surfaceOpen();
    const bool pill = !open;
    const int pillWidth = m_listening ? 210 : 168;
    const int pillHeight = m_listening ? 36 : 34;
    const int markSide = open ? 22 : 18;
    m_edit->setVisible(open);
    m_gear->setVisible(open);
    if (m_mark) {
        m_mark->setVisible(true);
        m_mark->setPixmap(assistantLogoPixmap(markSide));
        m_mark->setFixedSize(markSide, markSide);
    }
    const bool showStatus = open && !m_status->text().isEmpty();
    m_status->setVisible(showStatus);
    setProperty("resting", pill);
    setProperty("listening", m_listening);
    style()->unpolish(this);
    style()->polish(this);
    auto *row = qobject_cast<QHBoxLayout *>(layout()->itemAt(0)->layout());
    if (pill) {
        layout()->setContentsMargins(0, 0, 0, 0);
        if (row) {
            row->setStretch(1, 0);
            const int side = qMax(0, (pillWidth - markSide) / 2);
            const int top = qMax(0, (pillHeight - markSide) / 2);
            row->setContentsMargins(side, top, 0, 0);
        }
        setFixedSize(pillWidth, pillHeight);
    } else {
        layout()->setContentsMargins(14, 2, 12, 12);
        if (row) {
            row->setStretch(1, 1);
            row->setContentsMargins(12, 4, 8, 0);
        }
        m_status->setFixedWidth(392);
        int height = 36 + m_edit->sizeHint().height();
        if (showStatus)
            height += 8 + qBound(18, m_status->heightForWidth(392), 220);
        setFixedSize(440, height + 8);
    }
    pinToTop();
}

void NotchWindow::pinToTop() {
    QScreen *screen = QGuiApplication::screenAt(QPoint(x() + width() / 2, 1));
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect full = screen->geometry();
    const int top = screen->availableGeometry().top();
    move(full.center().x() - width() / 2, top);
}

void NotchWindow::runCommand(const QString &text) {
    bool understood = true;
    for (const Command &command : m_engine.parseAll(text)) {
        if (command.kind == CommandKind::Unknown)
            understood = false;
    }
    if (!understood) {
        m_status->setText(QStringLiteral("Ich frage OpenRouter…"));
        if (!m_expanded)
            m_expanded = true;
        applyChrome();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    const ActionResult result = m_runner.runText(text);
    m_status->setText(result.message);
    if (!m_expanded)
        m_expanded = true;
    applyChrome();
    m_edit->selectAll();
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
        UnregisterHotKey(nullptr, kHotkeyId);
    m_hotkey = false;
    UINT mods = 0;
    UINT vk = 0;
    const QKeySequence sequence(SettingsSync::voiceHotkey(), QKeySequence::PortableText);
    m_hotMods = 0;
    m_hotVk = 0;
    g_hotMods.store(0);
    g_hotVk.store(0);
    if (!winHotkey(sequence, &mods, &vk))
        return;
    m_hotMods = mods;
    m_hotVk = vk;
    g_hotMods.store(mods);
    g_hotVk.store(vk);
    m_hotkey = RegisterHotKey(nullptr, kHotkeyId, mods, vk);
#endif
}

void NotchWindow::setListening(bool on) {
    m_listening = on;
    applyChrome();
}

void NotchWindow::scheduleIdle() {
    QTimer::singleShot(1400, this, [this]() {
        if (m_listening)
            return;
        if (rect().contains(mapFromGlobal(QCursor::pos())))
            return;
        m_edit->clearFocus();
        m_hovered = false;
        m_expanded = false;
        applyChrome();
    });
}

void NotchWindow::beginHold() {
    show();
    m_sawHold = false;
#if defined(Q_OS_WIN)
    g_releasedVk.store(0);
#endif
    if (!m_speech->listening())
        m_speech->start();
    if (m_holdTimer && !m_holdTimer->isActive())
        m_holdTimer->start();
}

void NotchWindow::finishHold() {
    if (m_holdTimer)
        m_holdTimer->stop();
    if (m_speech->listening())
        m_speech->stop();
}

void NotchWindow::releaseHoldKey(quint32 vk) {
    if (!m_speech || !m_speech->listening())
        return;
#if defined(Q_OS_WIN)
    const bool mainUp = m_hotVk != 0 && vk == m_hotVk;
    bool modUp = false;
    if (m_hotMods & MOD_CONTROL)
        modUp = vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL;
    if (m_hotMods & MOD_SHIFT)
        modUp = modUp || vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT;
    if (m_hotMods & MOD_ALT)
        modUp = modUp || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU;
    if (m_hotMods & MOD_WIN)
        modUp = modUp || vk == VK_LWIN || vk == VK_RWIN;
    if (mainUp || modUp)
        finishHold();
#else
    Q_UNUSED(vk);
#endif
}

void NotchWindow::pollHold() {
#if defined(Q_OS_WIN)
    const unsigned released = g_releasedVk.exchange(0);
    if (released != 0)
        releaseHoldKey(static_cast<quint32>(released));
    if (!m_speech->listening())
        return;
    const auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    const bool keyDown = m_hotVk != 0 && down(static_cast<int>(m_hotVk));
    bool modsDown = true;
    if (m_hotMods & MOD_CONTROL)
        modsDown = modsDown && (down(VK_CONTROL) || down(VK_LCONTROL) || down(VK_RCONTROL));
    if (m_hotMods & MOD_SHIFT)
        modsDown = modsDown && (down(VK_SHIFT) || down(VK_LSHIFT) || down(VK_RSHIFT));
    if (m_hotMods & MOD_ALT)
        modsDown = modsDown && (down(VK_MENU) || down(VK_LMENU) || down(VK_RMENU));
    if (m_hotMods & MOD_WIN)
        modsDown = modsDown && (down(VK_LWIN) || down(VK_RWIN));
    if (keyDown && modsDown) {
        m_sawHold = true;
        return;
    }
    if (!m_sawHold)
        return;
#else
    return;
#endif
    finishHold();
}

void NotchWindow::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    m_hovered = true;
    applyChrome();
}

void NotchWindow::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    QTimer::singleShot(0, this, [this]() {
        if (rect().contains(mapFromGlobal(QCursor::pos())))
            return;
        m_hovered = false;
        if (!m_listening) {
            m_edit->clearFocus();
            m_expanded = false;
        }
        applyChrome();
    });
}

void NotchWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && m_expanded) {
        collapse();
        return;
    }
    QWidget::keyPressEvent(event);
}

void NotchWindow::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
}

bool NotchWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Enter) {
        m_hovered = true;
        applyChrome();
    }
    if (event->type() == QEvent::Leave || event->type() == QEvent::FocusOut) {
        QTimer::singleShot(0, this, [this]() {
            if (rect().contains(mapFromGlobal(QCursor::pos())))
                return;
            m_hovered = false;
            if (!m_listening) {
                m_edit->clearFocus();
                m_expanded = false;
            }
            applyChrome();
        });
    }
    Q_UNUSED(watched);
    return QWidget::eventFilter(watched, event);
}
