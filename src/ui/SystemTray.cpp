#include "SystemTray.h"
#include "IconHelper.h"
#include <QApplication>
#include <QIcon>
#include <QStyle>

SystemTray::SystemTray(QObject* parent) : QObject(parent) {
    m_trayIcon = new QSystemTrayIcon(this);
    
    // 使用标准图标作为占位，或者自定义图标
    m_trayIcon->setIcon(QApplication::windowIcon());
    m_trayIcon->setToolTip("极速灵感 (RapidNotes)");

    m_menu = new QMenu();
    m_menu->setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; }");
    m_menu->addAction(IconHelper::getIcon("monitor", "#aaaaaa"), "显示主界面", this, &SystemTray::showMainWindow);
    m_menu->addAction(IconHelper::getIcon("zap", "#aaaaaa"), "显示快速记录", this, &SystemTray::showQuickWindow);
    m_menu->addSeparator();
    m_menu->addAction(IconHelper::getIcon("power", "#aaaaaa"), "退出", this, &SystemTray::quitApp);

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
