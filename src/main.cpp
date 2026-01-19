#include <QApplication>
#include <QFile>
#include "core/DatabaseManager.h"
#include "core/HotkeyManager.h"
#include "core/ClipboardMonitor.h"
#include "ui/MainWindow.h"
#include "ui/FloatingBall.h"
#include "ui/QuickWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("RapidNotes");
    a.setOrganizationName("RapidDev");

    // 加载全局样式表
    QFile styleFile(":/qss/dark_style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        a.setStyleSheet(styleFile.readAll());
    }

    // 1. 初始化数据库
    if (!DatabaseManager::instance().init()) {
        return -1;
    }

    // 2. 初始化主界面
    MainWindow mainWin;
    mainWin.show();

    // 3. 初始化悬浮球
    FloatingBall* ball = new FloatingBall();
    ball->show();

    // 4. 初始化快速记录窗口
    QuickWindow* quickWin = new QuickWindow();

    // 5. 注册全局热键 (Alt+Space)
    // 注意: MOD_ALT = 0x0001, VK_SPACE = 0x20
    HotkeyManager::instance().registerHotkey(1, 0x0001, 0x20);

    QObject::connect(&HotkeyManager::instance(), &HotkeyManager::hotkeyPressed, [&](int id){
        if (id == 1) {
            quickWin->showCentered();
        }
    });

    QObject::connect(ball, &FloatingBall::doubleClicked, [&](){
        quickWin->showCentered();
    });

    // 6. 监听剪贴板
    QObject::connect(&ClipboardMonitor::instance(), &ClipboardMonitor::newContentDetected, [&](const QString& content){
        qDebug() << "检测到剪贴板新内容:" << content;
        // 可以在这里弹出托盘通知或自动保存
    });

    return a.exec();
}
