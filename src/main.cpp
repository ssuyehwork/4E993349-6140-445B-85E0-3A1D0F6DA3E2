#include <QApplication>
#include <QFile>
#include <QToolTip>
#include <QCursor>
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
#include "core/OCRManager.h"
#include "ui/MainWindow.h"
#include "ui/FloatingBall.h"
#include "ui/QuickWindow.h"
#include "ui/SystemTray.h"
#include "ui/Toolbox.h"
#include "ui/TimePasteWindow.h"
#include "ui/PasswordGeneratorWindow.h"
#include "ui/OCRWindow.h"
#include "ui/PathAcquisitionWindow.h"
#include "ui/FileStorageWindow.h"
#include "ui/TagManagerWindow.h"
#include "ui/FileSearchWindow.h"
#include "ui/ColorPickerWindow.h"
#include "ui/FireworksOverlay.h"
#include "ui/ScreenshotTool.h"
#include "ui/SettingsWindow.h"
#include "core/KeyboardHook.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("RapidNotes");
    a.setOrganizationName("RapidDev");
    a.setQuitOnLastWindowClosed(false);

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

    // 3. 初始化特效层与悬浮球
    FireworksOverlay::instance(); // 预初始化特效层
    FloatingBall* ball = new FloatingBall();
    ball->show();

    // 设置全局应用图标
    a.setWindowIcon(FloatingBall::generateBallIcon());

    // 4. 初始化快速记录窗口与工具箱及其功能子窗口
    QuickWindow* quickWin = new QuickWindow();
    quickWin->showAuto(); // 默认启动显示极速窗口
    
    Toolbox* toolbox = new Toolbox();
    TimePasteWindow* timePasteWin = new TimePasteWindow();
    PasswordGeneratorWindow* passwordGenWin = new PasswordGeneratorWindow();
    OCRWindow* ocrWin = new OCRWindow();
    PathAcquisitionWindow* pathAcqWin = new PathAcquisitionWindow();
    TagManagerWindow* tagMgrWin = new TagManagerWindow();
    FileStorageWindow* fileStorageWin = new FileStorageWindow();
    FileSearchWindow* fileSearchWin = new FileSearchWindow();
    ColorPickerWindow* colorPickerWin = new ColorPickerWindow();

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
    QObject::connect(mainWin, &MainWindow::fileStorageRequested, [=](){
        fileStorageWin->setCurrentCategory(mainWin->getCurrentCategoryId());
        toggleWindow(fileStorageWin, mainWin);
    });

    // 连接工具箱按钮信号
    QObject::connect(toolbox, &Toolbox::showTimePasteRequested, [=](){ toggleWindow(timePasteWin); });
    QObject::connect(toolbox, &Toolbox::showPasswordGeneratorRequested, [=](){ toggleWindow(passwordGenWin); });
    QObject::connect(toolbox, &Toolbox::showOCRRequested, [=](){ toggleWindow(ocrWin); });
    QObject::connect(toolbox, &Toolbox::showPathAcquisitionRequested, [=](){ toggleWindow(pathAcqWin); });
    QObject::connect(toolbox, &Toolbox::showTagManagerRequested, [=](){
        tagMgrWin->refreshData();
        toggleWindow(tagMgrWin);
    });
    QObject::connect(toolbox, &Toolbox::showFileStorageRequested, [=](){
        int catId = -1;
        if (quickWin->isVisible()) catId = quickWin->getCurrentCategoryId();
        else if (mainWin->isVisible()) catId = mainWin->getCurrentCategoryId();
        fileStorageWin->setCurrentCategory(catId);
        toggleWindow(fileStorageWin);
    });
    QObject::connect(toolbox, &Toolbox::showFileSearchRequested, [=](){ toggleWindow(fileSearchWin); });
    QObject::connect(toolbox, &Toolbox::showColorPickerRequested, [=](){ toggleWindow(colorPickerWin); });

    // 统一显示主窗口的逻辑，处理启动锁定状态
    auto showMainWindow = [=]() {
        if (quickWin->isLocked()) {
            quickWin->showAuto();
            return;
        }
        if (mainWin->isVisible()) {
            mainWin->hide();
        } else {
            mainWin->showNormal();
            mainWin->activateWindow();
            mainWin->raise();
        }
    };

    // 处理主窗口切换信号
    QObject::connect(quickWin, &QuickWindow::toggleMainWindowRequested, showMainWindow);

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
                quickWin->showAuto();
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

    // 监听 OCR 完成信号并更新笔记内容 (排除工具箱特有的 ID 9999 及 OCRWindow 的 ID 范围 1000000+)
    QObject::connect(&OCRManager::instance(), &OCRManager::recognitionFinished, [](const QString& text, int noteId){
        if (noteId > 0 && noteId < 1000000 && noteId != 9999) {
            DatabaseManager::instance().updateNoteState(noteId, "content", text);
        }
    }, Qt::QueuedConnection);

    // 7. 系统托盘
    QObject::connect(&server, &QLocalServer::newConnection, [&](){
        QLocalSocket* conn = server.nextPendingConnection();
        if (conn->waitForReadyRead(500)) {
            QByteArray data = conn->readAll();
            if (data == "SHOW") {
                quickWin->showAuto();
            }
            conn->disconnectFromServer();
        }
    });

    SystemTray* tray = new SystemTray(&a);
    QObject::connect(tray, &SystemTray::showMainWindow, showMainWindow);
    QObject::connect(tray, &SystemTray::showQuickWindow, quickWin, &QuickWindow::showAuto);
    QObject::connect(tray, &SystemTray::showSettings, [=](){
        SettingsWindow* settingsWin = new SettingsWindow();
        settingsWin->setAttribute(Qt::WA_DeleteOnClose);
        settingsWin->show();
    });
    QObject::connect(tray, &SystemTray::quitApp, &a, &QApplication::quit);
    tray->show();

    QObject::connect(ball, &FloatingBall::doubleClicked, [&](){
        quickWin->showAuto();
    });
    QObject::connect(ball, &FloatingBall::requestMainWindow, showMainWindow);
    QObject::connect(ball, &FloatingBall::requestQuickWindow, quickWin, &QuickWindow::showAuto);
    QObject::connect(ball, &FloatingBall::requestNewIdea, [=](){
        NoteEditWindow* win = new NoteEditWindow();
        QObject::connect(win, &NoteEditWindow::noteSaved, quickWin, &QuickWindow::refreshData);
        win->show();
    });

    // 8. 监听剪贴板 (智能标题与自动分类)
    QObject::connect(&ClipboardMonitor::instance(), &ClipboardMonitor::clipboardChanged, [=](){
        // 触发烟花爆炸特效
        FireworksOverlay::instance()->explode(QCursor::pos());
    });

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