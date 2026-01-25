#include "HotkeyManager.h"
#include <QCoreApplication>
#include <QDebug>

HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager inst;
    return inst;
}

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    qApp->installNativeEventFilter(this);
}

HotkeyManager::~HotkeyManager() {
    // 退出时取消所有注册
}

bool HotkeyManager::registerHotkey(int id, uint modifiers, uint vk) {
#ifdef Q_OS_WIN
    if (RegisterHotKey(nullptr, id, modifiers, vk)) {
        return true;
    }
    
    QString keyDesc = QString("ID=%1").arg(id);
    if (id == 1) keyDesc = "Alt+Space (快速窗口)";
    else if (id == 2) keyDesc = "Ctrl+Shift+E (全局收藏)";
    else if (id == 3) keyDesc = "Ctrl+Alt+A (全局截屏)";

    qWarning().noquote() << QString("[HotkeyManager] 注册热键失败: %1 (错误代码: %2). 该快捷键可能已被系统或其他软件占用。")
                            .arg(keyDesc).arg(GetLastError());
#endif
    return false;
}

void HotkeyManager::unregisterHotkey(int id) {
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, id);
#endif
}

bool HotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            emit hotkeyPressed(static_cast<int>(msg->wParam));
            return true;
        }
    }
#endif
    return false;
}