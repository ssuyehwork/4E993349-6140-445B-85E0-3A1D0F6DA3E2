#include "SystemTray.h"
#include "IconHelper.h"
#include "FloatingBall.h"
#include <QApplication>
#include <QIcon>
#include <QStyle>

SystemTray::SystemTray(QObject* parent) : QObject(parent) {
    m_trayIcon = new QSystemTrayIcon(this);
    
    // 复刻 Python 版：使用渲染的悬浮球作为托盘图标
    m_trayIcon->setIcon(FloatingBall::generateBallIcon());
    m_trayIcon->setToolTip("快速笔记 (RapidNotes)");

    m_menu = new QMenu();
    m_menu->setStyleSheet(
        "QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; } "
        "QMenu::item { padding: 6px 24px; } "
        "QMenu::item:selected { background-color: #4a90e2; color: white; }"
    );
    
    m_menu->addAction(IconHelper::getIcon("monitor", "#aaaaaa"), "显示主界面", this, &SystemTray::showMainWindow);
    m_menu->addAction(IconHelper::getIcon("zap", "#aaaaaa"), "显示快速笔记", this, &SystemTray::showQuickWindow);
    m_menu->addSeparator();
    m_menu->addAction(IconHelper::getIcon("power", "#aaaaaa"), "退出程序", this, &SystemTray::quitApp);

    m_trayIcon->setContextMenu(m_menu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger) {
            emit showQuickWindow();
        }
    });
}

void SystemTray::show() {
    m_trayIcon->show();
}
