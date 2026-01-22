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
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        // 使用 GetKeyState 确保修饰键状态与当前消息同步，防止输入法干扰
        bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000);
        bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000);

        // 1. Ctrl + Shift + Backspace -> Ctrl+A then Backspace (全选删除)
        // 迁移至 Backspace 键触发，彻底避开中文输入法对 Space 的依赖
        if (pKey->vkCode == VK_BACK && ctrlDown && shiftDown) {
            if (isKeyDown) {
                qDebug() << "[Hook] Triggered: Ctrl + Shift + Backspace";
                // 模拟执行：释放 Shift -> 发送 A (Ctrl 已按下) -> 释放 Ctrl -> 发送 Backspace
                keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
                keybd_event('A', 0, 0, 0);
                keybd_event('A', 0, KEYEVENTF_KEYUP, 0);
                keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                keybd_event(VK_BACK, 0, 0, 0);
                keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
            }
            return 1;
        }

        // 2. Shift + Space -> Shift + Enter (换行)
        // 增加严密的逻辑判断：必须同时拦截 KeyDown 和 KeyUp，且在无 Ctrl 干扰下生效
        if (pKey->vkCode == VK_SPACE && shiftDown && !ctrlDown) {
            if (isKeyDown) {
                qDebug() << "[Hook] Triggered: Shift + Space";
                keybd_event(VK_RETURN, 0, 0, 0);
            } else if (isKeyUp) {
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            }
            return 1;
        }

        // 3. CapsLock -> Enter
        if (pKey->vkCode == VK_CAPITAL) {
            if (isKeyDown) {
                qDebug() << "[Hook] Triggered: CapsLock -> Enter";
                keybd_event(VK_RETURN, 0, 0, 0);
            } else if (isKeyUp) {
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            }
            return 1;
        }

        // 4. 反引号 (`) -> Backspace
        if (pKey->vkCode == VK_OEM_3) {
            if (isKeyDown) {
                qDebug() << "[Hook] Triggered: Backtick -> Backspace";
                keybd_event(VK_BACK, 0, 0, 0);
            } else if (isKeyUp) {
                keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
            }
            return 1;
        }

        // 5. 工具箱数字拦截 (仅在使能时且按下时触发)
        if (isKeyDown && KeyboardHook::instance().m_digitInterceptEnabled) {
            if (pKey->vkCode >= 0x30 && pKey->vkCode <= 0x39) {
                int digit = pKey->vkCode - 0x30;
                emit KeyboardHook::instance().digitPressed(digit);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
#endif
