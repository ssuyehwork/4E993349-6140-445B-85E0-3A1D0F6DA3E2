#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QBuffer>
#include <QLocalServer>
#include <QLocalSocket>
#include "core/DatabaseManager.h"
#include "core/HotkeyManager.h"
#include "core/ClipboardMonitor.h"
#include "ui/MainWindow.h"
#include "ui/FloatingBall.h"
#include "ui/QuickWindow.h"
#include "ui/SystemTray.h"
#include "ui/Toolbox.h"
#include "ui/ScreenshotTool.h"
#include "core/KeyboardHook.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("RapidNotes");
    a.setOrganizationName("RapidDev");

    // 单实例运行保护
    QString serverName = "RapidNotes_SingleInstance_Server";
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(500)) {
        // 如果已经运行，发送 SHOW 信号并退出当前进程
        socket.write("SHOW");
        socket.waitForBytesWritten(1000);
        return 0;
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    if (!server.listen(serverName)) {
        qWarning() << "无法启动单实例服务器";
    }

    // 加载全局样式表
    QFile styleFile(":/qss/dark_style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        a.setStyleSheet(styleFile.readAll());
    }

    // 1. 初始化数据库
    QString dbPath = QCoreApplication::applicationDirPath() + "/notes.db";
    qDebug() << "[Main] 数据库路径:" << dbPath;

    if (!DatabaseManager::instance().init(dbPath)) {
        QMessageBox::critical(nullptr, "启动失败", 
            "无法初始化数据库！\n请检查是否有写入权限，或缺少 SQLite 驱动。");
        return -1;
    }

    // 2. 初始化主界面
    MainWindow* mainWin = new MainWindow();
    // mainWin->show(); // 默认启动不显示主界面

    // 3. 初始化悬浮球
    FloatingBall* ball = new FloatingBall();
    ball->show();

    // 4. 初始化快速记录窗口与工具箱
    QuickWindow* quickWin = new QuickWindow();
    quickWin->showCentered(); // 默认启动显示极速窗口
    Toolbox* toolbox = new Toolbox();

    auto toggleToolbox = [toolbox](QWidget* parentWin) {
        if (toolbox->isVisible()) {
            toolbox->hide();
        } else {
            if (parentWin) {
                // 如果是快速窗口唤起，停靠在左侧
                if (parentWin->objectName() == "QuickWindow") {
                    toolbox->move(parentWin->x() - toolbox->width() - 10, parentWin->y());
                } else {
                    toolbox->move(parentWin->geometry().center() - toolbox->rect().center());
                }
            }
            toolbox->show();
            toolbox->raise();
            toolbox->activateWindow();
        }
    };

    quickWin->setObjectName("QuickWindow");
    QObject::connect(quickWin, &QuickWindow::toolboxRequested, [=](){ toggleToolbox(quickWin); });
    QObject::connect(mainWin, &MainWindow::toolboxRequested, [=](){ toggleToolbox(mainWin); });

    // 处理主窗口切换信号
    QObject::connect(quickWin, &QuickWindow::toggleMainWindowRequested, [=](){
        if (mainWin->isVisible()) {
            mainWin->hide();
        } else {
            mainWin->showNormal();
            mainWin->activateWindow();
            mainWin->raise();
        }
    });

    // 5. 开启全局键盘钩子 (支持快捷键重映射)
    KeyboardHook::instance().start();

    // 6. 注册全局热键
    // Alt+Space (0x0001 = MOD_ALT, 0x20 = VK_SPACE)
    HotkeyManager::instance().registerHotkey(1, 0x0001, 0x20);
    // Ctrl+Shift+E (0x0002 = MOD_CONTROL, 0x0004 = MOD_SHIFT, 0x45 = 'E')
    HotkeyManager::instance().registerHotkey(2, 0x0002 | 0x0004, 0x45);
    // Ctrl+Alt+A (0x0002 = MOD_CONTROL, 0x0001 = MOD_ALT, 0x41 = 'A')
    HotkeyManager::instance().registerHotkey(3, 0x0002 | 0x0001, 0x41);
    
    QObject::connect(&HotkeyManager::instance(), &HotkeyManager::hotkeyPressed, [&](int id){
        if (id == 1) {
            quickWin->showCentered();
        } else if (id == 2) {
            // 收藏最后一条灵感
            auto notes = DatabaseManager::instance().getAllNotes();
            if (!notes.isEmpty()) {
                int lastId = notes.first()["id"].toInt();
                DatabaseManager::instance().updateNoteState(lastId, "is_favorite", 1);
                qDebug() << "[Main] 已收藏最新灵感 ID:" << lastId;
            }
        } else if (id == 3) {
            // 截屏功能
            auto* tool = new ScreenshotTool();
            tool->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(tool, &ScreenshotTool::screenshotCaptured, [=](const QImage& img){
                QByteArray ba;
                QBuffer buffer(&ba);
                buffer.open(QIODevice::WriteOnly);
                img.save(&buffer, "PNG");

                QString title = "[截屏] " + QDateTime::currentDateTime().toString("MMdd_HHmm");
                DatabaseManager::instance().addNoteAsync(title, "[Image Data]", QStringList() << "截屏", "", -1, "image", ba);
            });
            tool->show();
        }
    });

    // 7. 系统托盘
    QObject::connect(&server, &QLocalServer::newConnection, [&](){
        QLocalSocket* conn = server.nextPendingConnection();
        if (conn->waitForReadyRead(500)) {
            QByteArray data = conn->readAll();
            if (data == "SHOW") {
                quickWin->showCentered();
            }
            conn->disconnectFromServer();
        }
    });

    SystemTray* tray = new SystemTray(&a);
    QObject::connect(tray, &SystemTray::showMainWindow, mainWin, &MainWindow::showNormal);
    QObject::connect(tray, &SystemTray::showQuickWindow, quickWin, &QuickWindow::showCentered);
    QObject::connect(tray, &SystemTray::quitApp, &a, &QApplication::quit);
    tray->show();

    QObject::connect(ball, &FloatingBall::doubleClicked, [&](){
        quickWin->showCentered();
    });
    QObject::connect(ball, &FloatingBall::requestMainWindow, mainWin, &MainWindow::showNormal);
    QObject::connect(ball, &FloatingBall::requestQuickWindow, quickWin, &QuickWindow::showCentered);

    // 8. 监听剪贴板 (智能标题与自动分类)
    QObject::connect(&ClipboardMonitor::instance(), &ClipboardMonitor::newContentDetected, 
        [=](const QString& content, const QString& type, const QByteArray& data,
            const QString& sourceApp, const QString& sourceTitle){
        qDebug() << "[Main] 接收到剪贴板信号:" << type << "来自:" << sourceApp;
        
        QString title;
        if (type == "image") {
            title = "[图片] " + QDateTime::currentDateTime().toString("MMdd_HHmm");
        } else if (type == "file") {
            QStringList files = content.split(";", Qt::SkipEmptyParts);
            if (!files.isEmpty()) {
                QString firstFileName = QFileInfo(files.first()).fileName();
                if (files.size() > 1) title = QString("[多文件] %1 等%2个文件").arg(firstFileName).arg(files.size());
                else title = "[文件] " + firstFileName;
            } else {
                title = "[未知文件]";
            }
        } else {
            // 文本：取第一行
            QString firstLine = content.section('\n', 0, 0).trimmed();
            if (firstLine.isEmpty()) title = "无标题灵感";
            else {
                title = firstLine.left(40);
                if (firstLine.length() > 40) title += "...";
            }
        }

        // 自动归档逻辑
        int catId = -1;
        if (quickWin && quickWin->isAutoCategorizeEnabled()) {
            catId = quickWin->getCurrentCategoryId();
        }

        // 自动生成类型标签
        QStringList tags;
        if (type == "image") tags << "图片";
        else if (type == "file") tags << "文件";
        else {
            QString trimmed = content.trimmed();
            if (trimmed.startsWith("http://") || trimmed.startsWith("https://") || trimmed.startsWith("www.")) {
                tags << "链接";
            } else {
                tags << "文本";
            }
        }
        
        DatabaseManager::instance().addNoteAsync(title, content, tags, "", catId, type, data, sourceApp, sourceTitle);
    });

    return a.exec();
}