#include "BlopBridge.h"

#include <QLocalSocket>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <QKeySequence>
#endif

namespace {

#if defined(Q_OS_WIN)
BOOL CALLBACK findBlop(HWND hwnd, LPARAM param) {
    if (!IsWindowVisible(hwnd))
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId())
        return TRUE;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return TRUE;
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    if (!ok)
        return TRUE;
    const QString file = QString::fromWCharArray(path);
    if (file.endsWith(QStringLiteral("Blop.exe"), Qt::CaseInsensitive) &&
        !file.contains(QStringLiteral("Assistent"), Qt::CaseInsensitive)) {
        *reinterpret_cast<HWND *>(param) = hwnd;
        return FALSE;
    }
    return TRUE;
}

WORD virtualKey(int qtKey) {
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return WORD(qtKey);
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return WORD(qtKey);
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return WORD(VK_F1 + (qtKey - Qt::Key_F1));
    return 0;
}

void tap(WORD vk, bool down) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}
#endif

} // namespace

QString BlopBridge::ask(const QString &message, QString *error) {
    QLocalSocket sock;
    sock.connectToServer(QStringLiteral("BlopDesktopSingleInstance_v1"));
    if (!sock.waitForConnected(800)) {
        if (error)
            *error = QStringLiteral("Blop läuft nicht.");
        return {};
    }
    sock.write(message.toUtf8() + '\n');
    sock.flush();
    if (!sock.waitForReadyRead(4000)) {
        if (error)
            *error = QStringLiteral("Blop hat nicht geantwortet.");
        return {};
    }
    return QString::fromUtf8(sock.readAll()).trimmed();
}

bool BlopBridge::sendHotkey(const QString &portable, QString *error) {
#if !defined(Q_OS_WIN)
    Q_UNUSED(portable);
    if (error)
        *error = QStringLiteral("Tasten senden geht hier nur unter Windows.");
    return false;
#else
    const QKeySequence seq(portable, QKeySequence::PortableText);
    if (seq.isEmpty()) {
        if (error)
            *error = QStringLiteral("Für dieses Werkzeug ist kein Kürzel gesetzt.");
        return false;
    }
    const QKeyCombination combo = seq[0];
    const WORD key = virtualKey(combo.key());
    if (key == 0) {
        if (error)
            *error = QStringLiteral("Diese Taste kann ich nicht senden.");
        return false;
    }
    HWND hwnd = nullptr;
    EnumWindows(findBlop, reinterpret_cast<LPARAM>(&hwnd));
    if (!hwnd) {
        if (error)
            *error = QStringLiteral("Das Blop-Fenster ist nicht offen.");
        return false;
    }
    keybd_event(VK_MENU, 0, 0, 0);
    SetForegroundWindow(hwnd);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    if (mods.testFlag(Qt::ControlModifier))
        tap(VK_CONTROL, true);
    if (mods.testFlag(Qt::ShiftModifier))
        tap(VK_SHIFT, true);
    if (mods.testFlag(Qt::AltModifier))
        tap(VK_MENU, true);
    tap(key, true);
    tap(key, false);
    if (mods.testFlag(Qt::AltModifier))
        tap(VK_MENU, false);
    if (mods.testFlag(Qt::ShiftModifier))
        tap(VK_SHIFT, false);
    if (mods.testFlag(Qt::ControlModifier))
        tap(VK_CONTROL, false);
    return true;
#endif
}
