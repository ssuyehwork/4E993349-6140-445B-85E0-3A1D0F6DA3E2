#include "KeyboardHook.h"
#include <QDebug>

#ifdef Q_OS_WIN
HHOOK g_hHook = nullptr;
#endif

KeyboardHook& KeyboardHook::instance() {
    static KeyboardHook inst;
    return inst;
}

KeyboardHook::KeyboardHook() {}

KeyboardHook::~KeyboardHook() {
    stop();
}

void KeyboardHook::start() {
#ifdef Q_OS_WIN
    if (g_hHook) return;
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, HookProc, GetModuleHandle(NULL), 0);
    if (g_hHook) {
        m_active = true;
        qDebug() << "Keyboard hook started";
    }
#endif
}

void KeyboardHook::stop() {
#ifdef Q_OS_WIN
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = nullptr;
        m_active = false;
        qDebug() << "Keyboard hook stopped";
    }
#endif
}

#ifdef Q_OS_WIN
LRESULT CALLBACK KeyboardHook::HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        // 检查是否是主键盘数字键 0-9 (VK_0 to VK_9)
        if (pKey->vkCode >= 0x30 && pKey->vkCode <= 0x39) {
            int digit = pKey->vkCode - 0x30;
            emit KeyboardHook::instance().digitPressed(digit);
            return 1; // 拦截按键
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
#endif
