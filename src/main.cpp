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
#include "ui/DatabaseLockDialog.h"
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

    // 1. 数据库解密逻辑
    QString appDir = QCoreApplication::applicationDirPath();
    QString dbPath = appDir + "/notes.db";
    QString encPath = appDir + "/notes.db.enc";
    QString masterPassword;

    // 崩溃恢复/安全清理：如果加密文件存在，说明明文文件不应长期驻留
    if (QFile::exists(encPath) && QFile::exists(dbPath)) {
        qDebug() << "[Main] 检测到残留明文数据库，正在执行安全清理...";
        FileCryptoHelper::secureDelete(dbPath);
    }

    if (QFile::exists(encPath)) {
        // 已加密，要求登录
        DatabaseLockDialog dlg(DatabaseLockDialog::Login);
        if (dlg.exec() != QDialog::Accepted) return 0;
        masterPassword = dlg.password();

        if (!FileCryptoHelper::decryptFile(encPath, dbPath, masterPassword)) {
            QMessageBox::critical(nullptr, "错误", "主密码错误或数据库损坏，无法解密。");
            return -1;
        }
    } else {
        // 首次运行或尚未加密
        bool shouldEncrypt = true;
        if (!QFile::exists(dbPath)) {
            // 全新安装，询问是否启用加密
            if (QMessageBox::question(nullptr, "安全提醒", "是否为数据库启用主密码加密？\n启用后，没有密码将无法读取任何数据。") == QMessageBox::No) {
                shouldEncrypt = false;
            }
        }

        if (shouldEncrypt) {
            DatabaseLockDialog dlg(DatabaseLockDialog::SetPassword);
            if (dlg.exec() == QDialog::Accepted) {
                masterPassword = dlg.password();
                if (QFile::exists(dbPath)) {
                    if (FileCryptoHelper::encryptFile(dbPath, encPath, masterPassword)) {
                        FileCryptoHelper::secureDelete(dbPath);
                        FileCryptoHelper::decryptFile(encPath, dbPath, masterPassword);
                    }
                }
            } else if (!QFile::exists(dbPath)) {
                // 全新安装拒绝设置密码，则不启动 (强制安全) 或 继续明文?
                // 遵从用户“防止有人提取”的强烈意愿，建议强制设置
                QMessageBox::warning(nullptr, "提示", "未设置主密码，数据库将以明文形式存储。");
            }
        }
    }

    qDebug() << "[Main] 数据库路径:" << dbPath;
    if (!DatabaseManager::instance().init(dbPath)) {
        QMessageBox::critical(nullptr, "启动失败", 
            "无法初始化数据库！\n请检查是否有写入权限，或缺少 SQLite 驱动。");
        FileCryptoHelper::secureDelete(dbPath);
        return -1;
    }

    // 注册退出时的重加密逻辑
    QObject::connect(&a, &QApplication::aboutToQuit, [&]() {
        qDebug() << "[Main] 正在退出，执行数据库回密...";
        DatabaseManager::instance().close();
        if (!masterPassword.isEmpty()) {
            if (FileCryptoHelper::encryptFile(dbPath, encPath, masterPassword)) {
                FileCryptoHelper::secureDelete(dbPath);
                qDebug() << "[Main] 数据库已安全锁定";
            }
        } else {
            // 如果从未设置过主密码，直接删除明文临时文件（逻辑保护）
            // FileCryptoHelper::secureDelete(dbPath);
        }
    });

    // 2. 初始化主界面
    MainWindow* mainWin = new MainWindow();
    // mainWin->show(); // 默认启动不显示主界面

    // 3. 初始化悬浮球
    FloatingBall* ball = new FloatingBall();
    ball->show();

    // 4. 初始化快速记录窗口与工具箱及其功能子窗口
    QuickWindow* quickWin = new QuickWindow();
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