#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include "core/DatabaseManager.h"
#include "core/HotkeyManager.h"
#include "core/ClipboardMonitor.h"
#include "ui/MainWindow.h"
#include "ui/FloatingBall.h"
#include "ui/QuickWindow.h"
#include "ui/SystemTray.h"

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
    MainWindow mainWin;
    mainWin.show();

    // 3. 初始化悬浮球
    FloatingBall* ball = new FloatingBall();
    ball->show();

    // 4. 初始化快速记录窗口
    QuickWindow* quickWin = new QuickWindow();

    // 5. 注册全局热键
    // Alt+Space (0x0001 = MOD_ALT, 0x20 = VK_SPACE)
    HotkeyManager::instance().registerHotkey(1, 0x0001, 0x20);
    // Ctrl+Shift+E (0x0002 = MOD_CONTROL, 0x0004 = MOD_SHIFT, 0x45 = 'E')
    HotkeyManager::instance().registerHotkey(2, 0x0002 | 0x0004, 0x45);

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
        }
    });

    // 6. 系统托盘
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
    QObject::connect(tray, &SystemTray::showMainWindow, &mainWin, &MainWindow::showNormal);
    QObject::connect(tray, &SystemTray::showQuickWindow, quickWin, &QuickWindow::showCentered);
    QObject::connect(tray, &SystemTray::quitApp, &a, &QApplication::quit);
    tray->show();

    QObject::connect(ball, &FloatingBall::doubleClicked, [&](){
        quickWin->showCentered();
    });
    QObject::connect(ball, &FloatingBall::requestMainWindow, &mainWin, &MainWindow::showNormal);
    QObject::connect(ball, &FloatingBall::requestQuickWindow, quickWin, &QuickWindow::showCentered);

    // 7. 监听剪贴板 (智能标题与自动分类)
    QObject::connect(&ClipboardMonitor::instance(), &ClipboardMonitor::newContentDetected,
        [&](const QString& content, const QString& type, const QByteArray& data){
        qDebug() << "[Main] 接收到剪贴板信号:" << type;

        QString title;
        if (type == "image") {
            title = "[图片] " + QDateTime::currentDateTime().toString("MMdd_HHmm");
        } else if (type == "file") {
            QStringList files = content.split(";", Qt::SkipEmptyParts);
            if (!files.isEmpty()) {
                QString firstFileName = QFileInfo(files.first()).fileName();
                if (files.size() > 1) title = QString("[多文件] %1 等%2个文件").arg(firstFileName).arg(files.size());
                else title = "[文件] " + firstFileName;
            } else title = "[未知文件]";
        } else {
            // 文本：取第一行
            QString firstLine = content.section('\n', 0, 0).trimmed();
            if (firstLine.isEmpty()) title = "无标题灵感";
            else {
                title = firstLine.left(40);
                if (firstLine.length() > 40) title += "...";
            }
        }

        DatabaseManager::instance().addNoteAsync(title, content, {"剪贴板"}, "", -1, type, data);
    });

    return a.exec();
}