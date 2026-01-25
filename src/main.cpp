#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QBuffer>
#include <QTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include "core/DatabaseManager.h"
#include "core/FileCryptoHelper.h"
#include "core/HotkeyManager.h"
#include "core/ClipboardMonitor.h"
#include "core/OCRManager.h"
#include "ui/MainWindow.h"
#include "ui/FloatingBall.h"
#include "ui/QuickWindow.h"
#include "ui/SystemTray.h"
#include "ui/Toolbox.h"
#include "ui/TimePasteWindow.h"
#include "ui/PasswordGeneratorWindow.h"
#include "ui/OCRWindow.h"
#include "ui/ScreenshotTool.h"
#include "core/KeyboardHook.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("RapidNotes");
    a.setOrganizationName("RapidDev");
    a.setQuitOnLastWindowClosed(false); // 关键修复：防止登录对话框关闭后导致应用退出

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

    // 1. 数据库解密与隐藏逻辑
    QString appDir = QCoreApplication::applicationDirPath();
    QString oldDbPath = appDir + "/notes.db";
    QString encPath = appDir + "/notes.db.enc";
    // 关键改变：解密库永远不放在程序目录下，而是系统临时目录的隐藏文件
    QString dbPath = QDir::tempPath() + "/.rn_vault_cache.db";
    QString masterPassword;

    // 崩溃恢复：如果加密库存在，强制清理程序目录下的任何旧明文库
    if (QFile::exists(encPath) && QFile::exists(oldDbPath)) {
        FileCryptoHelper::secureDelete(oldDbPath);
    }
    // 清理临时目录残留
    if (QFile::exists(dbPath)) {
        FileCryptoHelper::secureDelete(dbPath);
    }

    bool dbInitiallyLocked = QFile::exists(encPath);
    if (dbInitiallyLocked) {
        // 已锁定，主进程先不初始化 DB，等待 UI 触发解密 (密码在 QuickWindow.cpp 中硬编码)
        qDebug() << "[Main] 数据库已加密，进入待解锁状态...";
    } else {
        // 初始化明文库
        if (!DatabaseManager::instance().init(oldDbPath)) {
            QMessageBox::critical(nullptr, "启动失败", "无法初始化数据库。");
            return -1;
        }
    }

    // 注册退出时的重加密逻辑 (使用 QuickWindow.cpp 中硬编码的密码)
    QObject::connect(&a, &QApplication::aboutToQuit, [=]() {
        qDebug() << "[Main] 正在退出，执行数据库回密...";
        DatabaseManager::instance().close();

        QString appDir = QCoreApplication::applicationDirPath();
        QString dbPath = QDir::tempPath() + "/.rn_vault_cache.db";
        // 如果临时库不存在（可能是未加密模式），使用默认库
        if (!QFile::exists(dbPath)) dbPath = appDir + "/notes.db";

        QString encPath = appDir + "/notes.db.enc";
        QString masterPwd = QuickWindow::masterPassword();

        if (QFile::exists(dbPath)) {
            if (FileCryptoHelper::encryptFile(dbPath, encPath, masterPwd)) {
                FileCryptoHelper::secureDelete(dbPath);
                qDebug() << "[Main] 数据库已安全锁定";
            }
        }
    });

    // 2. 初始化窗口
    MainWindow* mainWin = new MainWindow();
    QuickWindow* quickWin = new QuickWindow();

    // 3. 极速窗口负责处理核心解锁逻辑 (密码在 QuickWindow.cpp 中硬编码)
    QObject::connect(mainWin->m_dbLockWidget, &DatabaseLockWidget::unlocked, [=](const QString& pwd){
        // 主窗口仅作为触发器，将输入传给极速窗口统一处理
        emit quickWin->m_dbLockWidget->unlocked(pwd);
    });

    QObject::connect(mainWin->m_dbLockWidget, &DatabaseLockWidget::quitRequested, &a, &QApplication::quit);
    QObject::connect(quickWin->m_dbLockWidget, &DatabaseLockWidget::quitRequested, &a, &QApplication::quit);

    // 监听数据库初始化成功信号，同步主窗口
    QObject::connect(&DatabaseManager::instance(), &DatabaseManager::categoriesChanged, [=](){
        if (mainWin->m_dbLockWidget->isVisible() && DatabaseManager::instance().isOpen()) {
            mainWin->setDatabaseLocked(false);
            mainWin->refreshData();
        }
    });

    if (dbInitiallyLocked) {
        mainWin->setDatabaseLocked(true);
        quickWin->setDatabaseLocked(true);
    } else {
        // 如果未加密，直接尝试初始化默认路径
        DatabaseManager::instance().init(appDir + "/notes.db");
    }

    // 4. 初始化悬浮球
    FloatingBall* ball = new FloatingBall();
    ball->show();

    quickWin->showCentered(); // 默认启动显示极速窗口
    
    // 强制激活，确保在对话框结束后能拿到焦点
    QTimer::singleShot(100, quickWin, &QuickWindow::raise);
    QTimer::singleShot(200, quickWin, &QuickWindow::activateWindow);

    Toolbox* toolbox = new Toolbox();
    TimePasteWindow* timePasteWin = new TimePasteWindow();
    PasswordGeneratorWindow* passwordGenWin = new PasswordGeneratorWindow();
    OCRWindow* ocrWin = new OCRWindow();

    auto toggleWindow = [](QWidget* win, QWidget* parentWin = nullptr) {
        if (win->isVisible()) {
            win->hide();
        } else {
            if (parentWin) {
                if (parentWin->objectName() == "QuickWindow") {
                    win->move(parentWin->x() - win->width() - 10, parentWin->y());
                } else {
                    win->move(parentWin->geometry().center() - win->rect().center());
                }
            }
            win->show();
            win->raise();
            win->activateWindow();
        }
    };

    quickWin->setObjectName("QuickWindow");
    QObject::connect(quickWin, &QuickWindow::toolboxRequested, [=](){ toggleWindow(toolbox, quickWin); });
    QObject::connect(mainWin, &MainWindow::toolboxRequested, [=](){ toggleWindow(toolbox, mainWin); });

    // 连接工具箱按钮信号
    QObject::connect(toolbox, &Toolbox::showTimePasteRequested, [=](){ toggleWindow(timePasteWin); });
    QObject::connect(toolbox, &Toolbox::showPasswordGeneratorRequested, [=](){ toggleWindow(passwordGenWin); });
    QObject::connect(toolbox, &Toolbox::showOCRRequested, [=](){ toggleWindow(ocrWin); });

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
            if (quickWin->isVisible() && quickWin->isActiveWindow()) {
                quickWin->hide();
            } else {
                quickWin->showCentered();
            }
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
                // 1. 保存到剪贴板
                QApplication::clipboard()->setImage(img);

                // 2. 保存到数据库
                QByteArray ba;
                QBuffer buffer(&ba);
                buffer.open(QIODevice::WriteOnly);
                img.save(&buffer, "PNG");
                
                QString title = "[截屏] " + QDateTime::currentDateTime().toString("MMdd_HHmm");
                int noteId = DatabaseManager::instance().addNote(title, "[正在进行文字识别...]", QStringList() << "截屏", "", -1, "image", ba);
                
                // 3. 触发 OCR
                OCRManager::instance().recognizeAsync(img, noteId);
            });
            tool->show();
        }
    });

    // 监听 OCR 完成信号并更新笔记内容 (排除工具箱特有的 ID 9999)
    QObject::connect(&OCRManager::instance(), &OCRManager::recognitionFinished, [](const QString& text, int noteId){
        if (noteId > 0 && noteId != 9999) {
            DatabaseManager::instance().updateNoteState(noteId, "content", text);
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