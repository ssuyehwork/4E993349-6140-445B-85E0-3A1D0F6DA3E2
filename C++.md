# 代码导出结果 - 20260120_165800

**项目路径**: `C:\Users\fachu\Desktop\Rapidnotes 备份`

**文件总数**: 30

## 文件类型统计

- **cpp**: 26 个文件
- **cmake**: 1 个文件
- **markdown**: 1 个文件
- **xml**: 1 个文件
- **css**: 1 个文件

---

## 文件: `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)

project(RapidNotes VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network Concurrent)

# 源代码列表 - 确保 NoteEditWindow 相关的两个文件都在这里
set(SOURCES
    src/main.cpp
    src/core/DatabaseManager.cpp
    src/core/HotkeyManager.cpp
    src/core/ClipboardMonitor.cpp
    src/core/OCRManager.cpp
    src/models/NoteModel.cpp
    src/ui/FloatingBall.cpp
    src/ui/QuickWindow.cpp
    src/ui/MainWindow.cpp
    src/ui/GraphWidget.cpp
    src/ui/Editor.cpp
    src/ui/NoteDelegate.h
    src/ui/NoteEditWindow.h    # <--- 必须有
    src/ui/NoteEditWindow.cpp  # <--- 必须有
    src/ui/ScreenshotTool.h
    resources/resources.qrc
)

add_executable(RapidNotes ${SOURCES})

target_link_libraries(RapidNotes PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Concurrent
)

if(WIN32)
    target_link_libraries(RapidNotes PRIVATE user32 shell32)
    set_target_properties(RapidNotes PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
```

## 文件: `TUTORIAL.md`

```markdown
# 极速灵感 (RapidNotes) 从零开始编译教程

欢迎使用 RapidNotes！这份教程将带你从零开始，在 Windows 环境下配置环境并生成自己的 `.exe` 文件。

## 第一步：下载并安装必要的工具

### 1. 安装 Qt 6
- **下载地址**：[Qt 官网下载页面](https://www.qt.io/download-open-source)
- **安装步骤**：
  - 运行安装程序并注册/登录 Qt 账号。
  - 在“选择组件”页面，勾选以下项：
    - `Qt 6.x` (建议选择最新稳定版，如 6.5 或 6.6)
    - `MinGW 11.2.0` (或更高版本)
    - `Qt Shader Tools`
    - `Qt SQL` 相关驱动
- **点击下一步直至安装完成**。

### 2. 安装 CMake (可选)
- Qt Creator 自带 CMake，但你也可以从 [CMake 官网](https://cmake.org/download/) 下载独立版。

---

## 第二步：打开并配置项目

1. **启动 Qt Creator**。
2. **打开项目**：
   - 点击 `文件 (File)` -> `打开文件或项目 (Open File or Project)`。
   - 导航到项目文件夹，选择 `CMakeLists.txt`。
3. **配置 Kit (构建套件)**：
   - 在弹出的配置界面，勾选你安装的 `Desktop Qt 6.x.x MinGW 64-bit`。
   - 点击 `Configure Project` 按钮。

---

## 第三步：编译与运行

1. **选择构建模式**：
   - 在界面左下角，点击小电脑图标，确保选择了 `Release` 模式（运行速度最快）。
2. **开始编译**：
   - 点击左下角的 **绿色锤子图标** (构建项目) 或直接按 `Ctrl + B`。
   - 等待下方的进度条变绿。
3. **运行程序**：
   - 点击左下角的 **绿色播放图标** (运行) 或按 `Ctrl + R`。
   - 此时，你的桌面应该会出现悬浮球和主界面！

---

## 第四步：如何找到生成的 .exe 文件

1. 默认情况下，编译出的文件位于项目文件夹旁边的 `build-RapidNotes-xxx-Release` 目录中。
2. 进入该目录下的 `bin` 或根目录，你会发现 `RapidNotes.exe`。
3. **注意**：如果直接双击 `.exe` 提示缺少 DLL，请使用 Qt 提供的 `windeployqt` 工具进行打包。

---

## 常见问题
- **编译报错找不到模块？** 确保在 Qt 安装时勾选了 `Sql`, `Network`, `Concurrent` 模块。
- **热键无效？** 某些电脑上 `Alt+Space` 可能被系统占用，可以在 `main.cpp` 中修改热键 ID。

祝你使用愉快！
```

## 文件: `resources\qss\dark_style.qss`

```css
/* 全局深色主题 */
QWidget {
    background-color: #1E1E1E;
    color: #D4D4D4;
    font-family: "Segoe UI", "Microsoft YaHei";
}

QScrollBar:vertical {
    border: none;
    background: #2D2D2D;
    width: 10px;
}

QScrollBar::handle:vertical {
    background: #3E3E42;
    min-height: 20px;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

QPushButton {
    background-color: #0E639C;
    color: white;
    border: none;
    padding: 6px 20px;
    border-radius: 2px;
}

QPushButton:hover {
    background-color: #1177BB;
}

QPushButton:pressed {
    background-color: #0D598C;
}

QSplitter::handle {
    background-color: #252526;
}

QListView {
    outline: none;
}

QListView::item {
    padding: 10px;
    border-bottom: 1px solid #2D2D2D;
}

QListView::item:selected {
    background-color: #37373D;
}

QTabWidget::pane {
    border: none;
    background: #1E1E1E;
}

QTabBar::tab {
    background: #2D2D2D;
    padding: 8px 20px;
    margin-right: 2px;
}

QTabBar::tab:selected {
    background: #1E1E1E;
    border-top: 2px solid #007ACC;
}

QLineEdit {
    background-color: #2D2D2D;
    border: 1px solid #3E3E42;
    padding: 5px;
    color: white;
}
```

## 文件: `resources\resources.qrc`

```xml
<RCC>
    <qresource prefix="/">
        <file>qss/dark_style.qss</file>
    </qresource>
</RCC>
```

## 文件: `src\core\ClipboardMonitor.cpp`

```cpp
#include "ClipboardMonitor.h"
#include <QMimeData>
#include <QDebug>

ClipboardMonitor& ClipboardMonitor::instance() {
    static ClipboardMonitor inst;
    return inst;
}

ClipboardMonitor::ClipboardMonitor(QObject* parent) : QObject(parent) {
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardMonitor::onClipboardChanged);
    qDebug() << "[ClipboardMonitor] 初始化完成，开始监听...";
}

void ClipboardMonitor::onClipboardChanged() {
    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    
    if (!mimeData) {
        qDebug() << "[ClipboardMonitor] 剪贴板数据为空指针";
        return;
    }

    if (mimeData->hasText()) {
        QString text = mimeData->text();
        if (text.isEmpty()) {
            qDebug() << "[ClipboardMonitor] 剪贴板文本为空";
            return;
        }

        // 计算 SHA256 去重
        QString currentHash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex();
        
        // 日志追踪去重逻辑
        if (currentHash == m_lastHash) {
            qDebug() << "[ClipboardMonitor] 检测到重复内容，已忽略。Hash:" << currentHash.left(8);
            return;
        }

        qDebug() << "[ClipboardMonitor] 捕获新内容! Hash:" << currentHash.left(8) << " 内容预览:" << text.left(20);
        
        m_lastHash = currentHash;
        emit newContentDetected(text);
    } else {
        qDebug() << "[ClipboardMonitor] 剪贴板内容不是文本格式";
    }
}
```

## 文件: `src\core\ClipboardMonitor.h`

```cpp
#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include <QObject>
#include <QClipboard>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QStringList>

class ClipboardMonitor : public QObject {
    Q_OBJECT
public:
    static ClipboardMonitor& instance();

signals:
    void newContentDetected(const QString& content);

private slots:
    void onClipboardChanged();

private:
    ClipboardMonitor(QObject* parent = nullptr);
    QString m_lastHash;
};

#endif // CLIPBOARDMONITOR_H
```

## 文件: `src\core\DatabaseManager.cpp`

```cpp
#include "DatabaseManager.h"
#include <QDebug>
#include <QSqlRecord>
#include <QtConcurrent>
#include <QCoreApplication>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::init(const QString& dbPath) {
    QMutexLocker locker(&m_mutex);
    m_dbPath = dbPath;
    
    if (m_db.isOpen()) m_db.close();

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    if (!createTables()) return false;

    QSqlQuery query(m_db);
    if (query.exec("SELECT COUNT(*) FROM notes")) {
        if (query.next() && query.value(0).toInt() == 0) {
            QSqlQuery insertQuery(m_db);
            insertQuery.prepare("INSERT INTO notes (title, content, tags) VALUES (:title, :content, :tags)");
            insertQuery.bindValue(":title", "欢迎使用极速灵感");
            insertQuery.bindValue(":content", "这是一条自动生成的欢迎笔记。");
            insertQuery.bindValue(":tags", "入门");
            insertQuery.exec();
        }
    }

    return true;
}

bool DatabaseManager::createTables() {
    QSqlQuery query(m_db);
    
    QString createNotesTable = R"(
        CREATE TABLE IF NOT EXISTS notes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT,
            content TEXT,
            tags TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            is_pinned INTEGER DEFAULT 0,
            is_locked INTEGER DEFAULT 0,
            is_favorite INTEGER DEFAULT 0,
            is_deleted INTEGER DEFAULT 0
        )
    )";
    
    if (!query.exec(createNotesTable)) return false;

    QString createFtsTable = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(
            title, content, content='notes', content_rowid='id'
        )
    )";
    
    query.exec(createFtsTable);

    query.exec("CREATE TRIGGER IF NOT EXISTS notes_ai AFTER INSERT ON notes BEGIN "
               "INSERT INTO notes_fts(rowid, title, content) VALUES (new.id, new.title, new.content); END;");
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_ad AFTER DELETE ON notes BEGIN "
               "INSERT INTO notes_fts(notes_fts, rowid, title, content) VALUES('delete', old.id, old.title, old.content); END;");
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_au AFTER UPDATE ON notes BEGIN "
               "INSERT INTO notes_fts(notes_fts, rowid, title, content) VALUES('delete', old.id, old.title, old.content); "
               "INSERT INTO notes_fts(rowid, title, content) VALUES (new.id, new.title, new.content); END;");

    return true;
}

// 【修复核心】防止死锁的 addNote
bool DatabaseManager::addNote(const QString& title, const QString& content, const QStringList& tags) {
    QVariantMap newNoteMap;
    bool success = false;

    {   // === 锁的作用域开始 ===
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QSqlQuery query(m_db);
        query.prepare("INSERT INTO notes (title, content, tags) VALUES (:title, :content, :tags)");
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", tags.join(","));
        
        if (query.exec()) {
            success = true;
            // 获取刚插入的数据
            QVariant lastId = query.lastInsertId();
            QSqlQuery fetch(m_db);
            fetch.prepare("SELECT * FROM notes WHERE id = :id");
            fetch.bindValue(":id", lastId);
            if (fetch.exec() && fetch.next()) {
                QSqlRecord rec = fetch.record();
                for (int i = 0; i < rec.count(); ++i) {
                    newNoteMap[rec.fieldName(i)] = fetch.value(i);
                }
            }
        } else {
            qCritical() << "添加笔记失败:" << query.lastError().text();
        }
    }   // === 锁的作用域结束，在此处自动解锁 ===

    // 锁已经解开了，现在发信号是绝对安全的
    if (success && !newNoteMap.isEmpty()) {
        emit noteAdded(newNoteMap);
    }
    
    return success;
}

// 【修复核心】防止死锁的 updateNote
bool DatabaseManager::updateNote(int id, const QString& title, const QString& content, const QStringList& tags) {
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QSqlQuery query(m_db);
        query.prepare("UPDATE notes SET title=:title, content=:content, tags=:tags, updated_at=CURRENT_TIMESTAMP WHERE id=:id");
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", tags.join(","));
        query.bindValue(":id", id);
        
        success = query.exec();
    } // 自动解锁

    if (success) emit noteUpdated();
    return success;
}

// 【修复核心】防止死锁的 updateNoteState
bool DatabaseManager::updateNoteState(int id, const QString& column, const QVariant& value) {
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;
        
        QStringList allowedColumns = {"is_pinned", "is_locked", "is_favorite", "is_deleted"};
        if (!allowedColumns.contains(column)) return false;

        QSqlQuery query(m_db);
        QString sql = QString("UPDATE notes SET %1 = :val WHERE id = :id").arg(column);
        query.prepare(sql);
        query.bindValue(":val", value);
        query.bindValue(":id", id);
        
        success = query.exec();
    } // 自动解锁

    if (success) emit noteUpdated();
    return success;
}

// 【修复核心】防止死锁的 deleteNote
bool DatabaseManager::deleteNote(int id) {
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QSqlQuery query(m_db);
        query.prepare("DELETE FROM notes WHERE id=:id");
        query.bindValue(":id", id);
        success = query.exec();
    } // 自动解锁

    if (success) emit noteUpdated();
    return success;
}

void DatabaseManager::addNoteAsync(const QString& title, const QString& content, const QStringList& tags) {
    QMetaObject::invokeMethod(this, [this, title, content, tags]() {
        addNote(title, content, tags);
    }, Qt::QueuedConnection);
}

QList<QVariantMap> DatabaseManager::searchNotes(const QString& keyword) {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    if (!m_db.isOpen()) return results;

    QSqlQuery query(m_db);
    query.prepare("SELECT notes.* FROM notes JOIN notes_fts ON notes.id = notes_fts.rowid WHERE notes_fts MATCH :keyword ORDER BY rank");
    query.bindValue(":keyword", keyword);
    if (!query.exec()) {
        query.prepare("SELECT * FROM notes WHERE title LIKE :keyword OR content LIKE :keyword");
        query.bindValue(":keyword", "%" + keyword + "%");
        query.exec();
    }
    while (query.next()) {
        QVariantMap map;
        QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i) {
            map[rec.fieldName(i)] = query.value(i);
        }
        results.append(map);
    }
    return results;
}

QList<QVariantMap> DatabaseManager::getAllNotes() {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    if (!m_db.isOpen()) return results;

    QSqlQuery query(m_db);
    if (query.exec("SELECT * FROM notes WHERE is_deleted = 0 ORDER BY is_pinned DESC, updated_at DESC")) {
        while (query.next()) {
            QVariantMap map;
            QSqlRecord rec = query.record();
            for (int i = 0; i < rec.count(); ++i) {
                map[rec.fieldName(i)] = query.value(i);
            }
            results.append(map);
        }
    }
    return results;
}

QStringList DatabaseManager::getAllTags() {
    QMutexLocker locker(&m_mutex);
    QStringList allTags;
    if (!m_db.isOpen()) return allTags;

    QSqlQuery query(m_db);
    if (query.exec("SELECT tags FROM notes WHERE tags != '' AND is_deleted = 0")) {
        while (query.next()) {
            QString tagsStr = query.value(0).toString();
            QStringList parts = tagsStr.split(",", Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                QString trimmed = part.trimmed();
                if (!allTags.contains(trimmed)) {
                    allTags.append(trimmed);
                }
            }
        }
    }
    allTags.sort();
    return allTags;
}
```

## 文件: `src\core\DatabaseManager.h`

```cpp
#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariantList>
#include <QMutex>
#include <QStringList>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool init(const QString& dbPath = "rapid_notes.db");
    
    // 核心 CRUD 操作
    bool addNote(const QString& title, const QString& content, const QStringList& tags);
    bool updateNote(int id, const QString& title, const QString& content, const QStringList& tags);
    bool deleteNote(int id);
    bool updateNoteState(int id, const QString& column, const QVariant& value);

    // 搜索与查询
    QList<QVariantMap> searchNotes(const QString& keyword);
    QList<QVariantMap> getAllNotes();
    QStringList getAllTags(); 

    // 异步操作
    void addNoteAsync(const QString& title, const QString& content, const QStringList& tags);

signals:
    // 【修改】现在信号携带具体数据，实现增量更新
    void noteAdded(const QVariantMap& note);
    void noteUpdated(); // 用于普通刷新

private:
    DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createTables();
    
    QSqlDatabase m_db;
    QString m_dbPath; 
    QMutex m_mutex;
};

#endif // DATABASEMANAGER_H
```

## 文件: `src\core\HotkeyManager.cpp`

```cpp
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
    qWarning() << "注册热键失败:" << id;
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
```

## 文件: `src\core\HotkeyManager.h`

```cpp
#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QObject>
#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    static HotkeyManager& instance();
    
    bool registerHotkey(int id, uint modifiers, uint vk);
    void unregisterHotkey(int id);

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void hotkeyPressed(int id);

private:
    HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager();
};

#endif // HOTKEYMANAGER_H
```

## 文件: `src\core\OCRManager.cpp`

```cpp
#include "OCRManager.h"

OCRManager& OCRManager::instance() {
    static OCRManager inst;
    return inst;
}
```

## 文件: `src\core\OCRManager.h`

```cpp
#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QObject>
#include <QImage>
#include <QtConcurrent>

class OCRManager : public QObject {
    Q_OBJECT
public:
    static OCRManager& instance();

    void recognizeAsync(const QImage& image) {
        (void)QtConcurrent::run([this, image]() {
            // 这里通常调用 Windows.Media.Ocr 或 Tesseract
            // 示例逻辑：
            QString result = "识别出的文字示例"; 
            emit recognitionFinished(result);
        });
    }

signals:
    void recognitionFinished(const QString& text);

private:
    OCRManager(QObject* parent = nullptr) : QObject(parent) {}
};

#endif // OCRMANAGER_H
```

## 文件: `src\core\Utils.h`

```cpp
#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QDateTime>
#include <QRandomGenerator>

class Utils {
public:
    static QString generatePassword(int length = 16) {
        const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+");
        QString password;
        for(int i=0; i<length; ++i) {
            int index = QRandomGenerator::global()->bounded(possibleCharacters.length());
            password.append(possibleCharacters.at(index));
        }
        return password;
    }

    static QString getTimestamp() {
        return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    }
};

#endif // UTILS_H
```

## 文件: `src\main.cpp`

```cpp
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QDebug> // 必须包含
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

    // 5. 注册全局热键 (Alt+Space)
    HotkeyManager::instance().registerHotkey(1, 0x0001, 0x20);
    
    QObject::connect(&HotkeyManager::instance(), &HotkeyManager::hotkeyPressed, [&](int id){
        if (id == 1) {
            quickWin->showCentered();
        }
    });

    QObject::connect(ball, &FloatingBall::doubleClicked, [&](){
        quickWin->showCentered();
    });

    // 6. 监听剪贴板 (带详细调试日志)
    QObject::connect(&ClipboardMonitor::instance(), &ClipboardMonitor::newContentDetected, [&](const QString& content){
        qDebug() << "[Main] 接收到剪贴板信号，准备写入数据库...";
        
        QString title = content.left(20).simplified(); 
        if (content.length() > 20) title += "...";
        
        // 调用异步写入
        DatabaseManager::instance().addNoteAsync(title, content, {"剪贴板"});
    });

    return a.exec();
}
```

## 文件: `src\models\NoteModel.cpp`

```cpp
#include "NoteModel.h"
#include <QDateTime>

NoteModel::NoteModel(QObject* parent) : QAbstractListModel(parent) {}

int NoteModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_notes.count();
}

QVariant NoteModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_notes.count()) return QVariant();

    const QVariantMap& note = m_notes.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return note.value("title");
        case ContentRole:
            return note.value("content");
        case IdRole:
            return note.value("id");
        case TagsRole:
            return note.value("tags");
        case TimeRole:
            return note.value("updated_at");
        case PinnedRole:
            return note.value("is_pinned");
        case LockedRole:
            return note.value("is_locked");
        case FavoriteRole:
            return note.value("is_favorite");
        default:
            return QVariant();
    }
}

void NoteModel::setNotes(const QList<QVariantMap>& notes) {
    beginResetModel();
    m_notes = notes;
    endResetModel();
}

// 【新增】函数的具体实现
void NoteModel::prependNote(const QVariantMap& note) {
    // 通知视图：我要在第0行插入1条数据
    beginInsertRows(QModelIndex(), 0, 0);
    m_notes.prepend(note);
    endInsertRows();
}
```

## 文件: `src\models\NoteModel.h`

```cpp
#ifndef NOTEMODEL_H
#define NOTEMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>
#include <QList>

class NoteModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum NoteRoles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ContentRole,
        TagsRole,
        TimeRole,
        PinnedRole,
        LockedRole,
        FavoriteRole
    };

    explicit NoteModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    
    // 全量重置
    void setNotes(const QList<QVariantMap>& notes);
    
    // 【新增】增量插入 (这就是报错缺失的函数！)
    void prependNote(const QVariantMap& note);

private:
    QList<QVariantMap> m_notes;
};

#endif // NOTEMODEL_H
```

## 文件: `src\ui\Editor.cpp`

```cpp
#include "Editor.h"
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    HighlightingRule rule;

    // 标题 (#)
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor("#569CD6"));
    headerFormat.setFontWeight(QFont::Bold);
    headerFormat.setFontPointSize(14); // 稍微大一点
    rule.pattern = QRegularExpression("^#+.*");
    rule.format = headerFormat;
    m_highlightingRules.append(rule);

    // 加粗 (**)
    QTextCharFormat boldFormat;
    boldFormat.setFontWeight(QFont::Bold);
    boldFormat.setForeground(QColor("#CE9178"));
    rule.pattern = QRegularExpression("\\*\\*.*\\*\\*");
    rule.format = boldFormat;
    m_highlightingRules.append(rule);

    // 双向链接 ([[]])
    QTextCharFormat linkFormat;
    linkFormat.setForeground(QColor("#4EC9B0"));
    linkFormat.setFontUnderline(true);
    rule.pattern = QRegularExpression("\\[\\[.*\\]\\]");
    rule.format = linkFormat;
    m_highlightingRules.append(rule);
    
    // 列表 (-)
    QTextCharFormat listFormat;
    listFormat.setForeground(QColor("#C586C0"));
    rule.pattern = QRegularExpression("^\\s*-\\s");
    rule.format = listFormat;
    m_highlightingRules.append(rule);
}

void MarkdownHighlighter::highlightBlock(const QString& text) {
    for (const HighlightingRule& rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

Editor::Editor(QWidget* parent) : QPlainTextEdit(parent) {
    m_highlighter = new MarkdownHighlighter(document());
    setStyleSheet("background: #1E1E1E; color: #D4D4D4; font-family: 'Consolas', 'Courier New'; font-size: 13pt; border: none; padding: 10px;");
}

void Editor::insertFromMimeData(const QMimeData* source) {
    if (source->hasImage()) {
        // 简单处理：提示图片已捕获
        appendPlainText("\n[图片已粘贴 - 待实现存储逻辑]\n");
        return;
    }
    if (source->hasUrls()) {
        for (const QUrl& url : source->urls()) {
            if (url.isLocalFile()) {
                 appendPlainText(QString("\n[文件引用: %1]\n").arg(url.toLocalFile()));
            } else {
                 appendPlainText(QString("\n[链接: %1]\n").arg(url.toString()));
            }
        }
        return;
    }
    QPlainTextEdit::insertFromMimeData(source);
}
```

## 文件: `src\ui\Editor.h`

```cpp
#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MarkdownHighlighter(QTextDocument* parent = nullptr);
protected:
    void highlightBlock(const QString& text) override;
private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightingRule> m_highlightingRules;
};

class Editor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit Editor(QWidget* parent = nullptr);
protected:
    void insertFromMimeData(const QMimeData* source) override;
private:
    MarkdownHighlighter* m_highlighter;
};

#endif // EDITOR_H
```

## 文件: `src\ui\FloatingBall.cpp`

```cpp
#include "FloatingBall.h"
#include "../core/DatabaseManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainterPath>
#include <QtMath>
#include <QRandomGenerator>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>

// 修复点：移除了 Qt::Tool，防止在 MinGW 环境下与透明背景冲突导致崩溃
FloatingBall::FloatingBall(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    setFixedSize(60, 60);
    
    // 初始化位置在屏幕右侧
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->geometry();
        move(screenGeom.width() - 80, screenGeom.height() / 2);
    }

    m_inertiaTimer = new QTimer(this);
    connect(m_inertiaTimer, &QTimer::timeout, this, &FloatingBall::startInertiaAnimation);
    
    m_particleTimer = new QTimer(this);
    connect(m_particleTimer, &QTimer::timeout, this, [this](){
        updateParticleEffect();
        update();
    });
}

void FloatingBall::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制阴影/发光
    painter.setBrush(QColor(40, 40, 40, 200));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect().adjusted(2, 2, -2, -2));

    // 绘制粒子
    for (const auto& p : m_particles) {
        QColor c = p.color;
        c.setAlphaF(p.life);
        painter.setBrush(c);
        painter.drawEllipse(p.pos, 2, 2);
    }

    // 绘制主体
    QLinearGradient gradient(0, 0, width(), height());
    gradient.setColorAt(0, QColor("#4FACFE"));
    gradient.setColorAt(1, QColor("#00F2FE"));
    painter.setBrush(gradient);
    painter.drawEllipse(rect().adjusted(5, 5, -5, -5));
    
    // 绘制图标感
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(rect().center(), 10, 10);
}

void FloatingBall::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_inertiaTimer->stop();
    }
}

void FloatingBall::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        QPoint oldPos = pos();
        move(event->globalPosition().toPoint() - m_dragPosition);
        m_velocity = pos() - oldPos;
    }
}

void FloatingBall::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        if (m_velocity.manhattanLength() > 5) {
            m_inertiaTimer->start(16);
        } else {
            checkEdgeAdsorption();
        }
    }
}

void FloatingBall::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    emit doubleClicked();
}

void FloatingBall::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    
    // 弹出效果
    if (pos().x() < 0) move(0, pos().y());
    if (pos().x() > screen->geometry().width() - width()) move(screen->geometry().width() - width(), pos().y());
}

void FloatingBall::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    checkEdgeAdsorption();
}

void FloatingBall::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.addAction("关闭程序", [](){ qApp->quit(); });
    menu.exec(event->globalPos());
}

void FloatingBall::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText() || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FloatingBall::dropEvent(QDropEvent* event) {
    QString content = event->mimeData()->text();
    if (event->mimeData()->hasUrls()) {
        content = event->mimeData()->urls().first().toLocalFile();
    }
    DatabaseManager::instance().addNoteAsync("拖拽记录", content, {"投喂"});
    burstParticles();
    event->acceptProposedAction();
}

void FloatingBall::switchSkin(const QString& name) {
    Q_UNUSED(name);
    update();
}

void FloatingBall::burstParticles() {
    for(int i=0; i<20; ++i) {
        Particle p;
        p.pos = rect().center();
        double angle = QRandomGenerator::global()->generateDouble() * 2 * 3.14159265358979323846;
        double speed = QRandomGenerator::global()->generateDouble() * 5 + 2;
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.life = 1.0;
        p.color = QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 255);
        m_particles.append(p);
    }
    m_particleTimer->start(16);
}

void FloatingBall::startInertiaAnimation() {
    move(pos() + m_velocity);
    m_velocity *= 0.9; // 阻尼
    
    if (m_velocity.manhattanLength() < 1) {
        m_inertiaTimer->stop();
        checkEdgeAdsorption();
    }
}

void FloatingBall::checkEdgeAdsorption() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeom = screen->geometry();
    int x = pos().x();
    if (x < screenGeom.width() / 2) {
        // 吸附到左侧
        QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
        anim->setDuration(300);
        anim->setEndValue(QPoint(-width() / 2 + 10, pos().y()));
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        // 吸附到右侧
        QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
        anim->setDuration(300);
        anim->setEndValue(QPoint(screenGeom.width() - width() / 2 - 10, pos().y()));
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void FloatingBall::updateParticleEffect() {
    for (int i = 0; i < m_particles.size(); ++i) {
        m_particles[i].pos += m_particles[i].velocity;
        m_particles[i].life -= 0.05;
        if (m_particles[i].life <= 0) {
            m_particles.removeAt(i);
            --i;
        }
    }
    if (m_particles.isEmpty()) m_particleTimer->stop();
}
```

## 文件: `src\ui\FloatingBall.h`

```cpp
#ifndef FLOATINGBALL_H
#define FLOATINGBALL_H

#include <QWidget>
#include <QPoint>
#include <QPropertyAnimation>
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

class FloatingBall : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QPoint pos READ pos WRITE move)

public:
    explicit FloatingBall(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void switchSkin(const QString& name);
    void burstParticles();
    void startInertiaAnimation();
    void checkEdgeAdsorption();
    void updateParticleEffect();

    QPoint m_dragPosition;
    bool m_isDragging = false;
    QPoint m_velocity;
    QTimer* m_inertiaTimer;
    QTimer* m_particleTimer;
    
    struct Particle {
        QPointF pos;
        QPointF velocity;
        double life;
        QColor color;
    };
    QList<Particle> m_particles;

signals:
    void doubleClicked();
};

#endif // FLOATINGBALL_H
```

## 文件: `src\ui\GraphWidget.cpp`

```cpp
#include "GraphWidget.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <QRandomGenerator>

class EdgeItem : public QGraphicsItem {
public:
    EdgeItem(NodeItem* sourceNode, NodeItem* destNode);
    void adjust();
protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    NodeItem *source, *dest;
    QPointF sourcePoint, destPoint;
};

// --- EdgeItem Implementation ---
EdgeItem::EdgeItem(NodeItem* sourceNode, NodeItem* destNode) : source(sourceNode), dest(destNode) {
    source->addEdge(this);
    dest->addEdge(this);
    adjust();
    setZValue(-1); // 线在节点下层
}
void EdgeItem::adjust() {
    prepareGeometryChange();
    sourcePoint = source->pos();
    destPoint = dest->pos();
}
QRectF EdgeItem::boundingRect() const {
    return QRectF(sourcePoint, QSizeF(destPoint.x() - sourcePoint.x(), destPoint.y() - sourcePoint.y())).normalized();
}
void EdgeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(QPen(QColor(100, 100, 100, 100), 1));
    painter->drawLine(sourcePoint, destPoint);
}

// --- GraphWidget Implementation ---
GraphWidget::GraphWidget(QWidget* parent) : QGraphicsView(parent) {
    QGraphicsScene* scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    scene->setSceneRect(-400, -300, 800, 600);
    setScene(scene);
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(QGraphicsView::ScrollHandDrag);
}

void GraphWidget::itemMoved() {
    if (!m_timerId) m_timerId = startTimer(1000 / 25);
}

void GraphWidget::loadNotes(const QList<QVariantMap>& notes) {
    scene()->clear();
    
    QList<NodeItem*> nodes;
    QMap<QString, QList<NodeItem*>> tagMap; // 用于按标签建立连接

    // 1. 创建节点
    for (const auto& note : notes) {
        QString title = note["title"].toString();
        int id = note["id"].toInt();
        QString tagsStr = note["tags"].toString();
        
        NodeItem* node = new NodeItem(this, title, id);
        // 随机初始位置，避免重叠
        node->setPos(-200 + QRandomGenerator::global()->bounded(400), 
                     -200 + QRandomGenerator::global()->bounded(400));
        scene()->addItem(node);
        nodes.append(node);

        // 记录标签归属
        QStringList tags = tagsStr.split(",", Qt::SkipEmptyParts);
        for(const QString& t : tags) {
            tagMap[t.trimmed()].append(node);
        }
    }

    // 2. 建立连接 (如果两个笔记有相同的标签，连线)
    for (auto it = tagMap.begin(); it != tagMap.end(); ++it) {
        QList<NodeItem*> group = it.value();
        for (int i = 0; i < group.size(); ++i) {
            for (int j = i + 1; j < group.size(); ++j) {
                scene()->addItem(new EdgeItem(group[i], group[j]));
            }
        }
    }
    
    // 启动物理引擎
    itemMoved();
}

void GraphWidget::timerEvent(QTimerEvent* event) {
    Q_UNUSED(event);
    QList<NodeItem*> nodes;
    for (QGraphicsItem* item : scene()->items()) {
        if (NodeItem* node = qgraphicsitem_cast<NodeItem*>(item)) nodes << node;
    }
    for (NodeItem* node : nodes) node->calculateForces();
    bool itemsMoved = false;
    for (NodeItem* node : nodes) {
        if (node->advancePosition()) itemsMoved = true;
    }
    if (!itemsMoved) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

void GraphWidget::wheelEvent(QWheelEvent* event) {
    scale(pow(2.0, event->angleDelta().y() / 240.0), pow(2.0, event->angleDelta().y() / 240.0));
}

void GraphWidget::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, QColor(30, 30, 30));
}

// --- NodeItem Implementation ---
NodeItem::NodeItem(GraphWidget* graphWidget, const QString& title, int id) 
    : m_graph(graphWidget), m_title(title), m_id(id) {
    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setToolTip(QString("%1 (ID: %2)").arg(title).arg(id));
}
void NodeItem::addEdge(EdgeItem* edge) { m_edgeList << edge; }

void NodeItem::calculateForces() {
    if (!scene() || scene()->mouseGrabberItem() == this) {
        m_newPos = pos();
        return;
    }
    // 简化物理引擎：斥力 (防止节点堆积)
    qreal xvel = 0, yvel = 0;
    for (QGraphicsItem* item : scene()->items()) {
        NodeItem* node = qgraphicsitem_cast<NodeItem*>(item);
        if (!node || node == this) continue;
        QLineF line(mapToScene(0, 0), node->mapToScene(0, 0));
        qreal dx = line.dx();
        qreal dy = line.dy();
        double l = 2.0 * (dx * dx + dy * dy);
        if (l > 0) {
            xvel += (dx * 200.0) / l;
            yvel += (dy * 200.0) / l;
        }
    }
    // 简化物理引擎：拉力 (让连接的节点靠近)
    double weight = (m_edgeList.size() + 1) * 10;
    for (EdgeItem* edge : m_edgeList) {
        QPointF vec;
        if (edge->mapToScene(0,0) == pos()) // 这里需要判断边的哪一头是自己
             // 简化处理：由于EdgeItem并没有简单的方法暴露对方坐标，这里仅作受力示意
             // 实际应该在EdgeItem里存source/dest指针
             continue; 
    }
    
    // 向中心聚集的重力，防止飞出屏幕
    QPointF centerVec = -pos(); 
    xvel += centerVec.x() / 1000.0;
    yvel += centerVec.y() / 1000.0;

    if (qAbs(xvel) < 0.1 && qAbs(yvel) < 0.1) xvel = yvel = 0;

    m_newPos = pos() + QPointF(xvel, yvel);
}

bool NodeItem::advancePosition() {
    if (m_newPos == pos()) return false;
    setPos(m_newPos);
    return true;
}

QRectF NodeItem::boundingRect() const { return QRectF(-20, -20, 40, 40); }

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setBrush(QColor("#4FACFE"));
    painter->setPen(QPen(Qt::white, 1));
    painter->drawEllipse(-10, -10, 20, 20);
    
    // 绘制标题
    painter->setPen(Qt::white);
    painter->drawText(QRectF(-50, 12, 100, 20), Qt::AlignCenter, m_title);
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        for (EdgeItem* edge : m_edgeList) edge->adjust();
        m_graph->itemMoved();
    }
    return QGraphicsItem::itemChange(change, value);
}
void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) { update(); QGraphicsItem::mousePressEvent(event); }
void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) { update(); QGraphicsItem::mouseReleaseEvent(event); }
```

## 文件: `src\ui\GraphWidget.h`

```cpp
#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QTimer>
#include <QVariantMap>

class NodeItem;
class EdgeItem;

class GraphWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphWidget(QWidget* parent = nullptr);

    void itemMoved();
    // 新增：加载笔记生成图谱
    void loadNotes(const QList<QVariantMap>& notes);

protected:
    void timerEvent(QTimerEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    int m_timerId = 0;
};

class NodeItem : public QGraphicsItem {
public:
    NodeItem(GraphWidget* graphWidget, const QString& title, int id);
    void addEdge(EdgeItem* edge);
    
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    void calculateForces();
    bool advancePosition();

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    int noteId() const { return m_id; }
    QString title() const { return m_title; }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QList<EdgeItem*> m_edgeList;
    QPointF m_newPos;
    GraphWidget* m_graph;
    QString m_title;
    int m_id;
};

#endif // GRAPHWIDGET_H
```

## 文件: `src\ui\MainWindow.cpp`

```cpp
#include "MainWindow.h"
#include "../core/DatabaseManager.h"
#include "NoteDelegate.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTabWidget>
#include <QLabel>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCursor>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("极速灵感 (RapidNotes) - 开发版");
    resize(1200, 800);
    initUI();
    refreshData();

    // 【关键修改】区分两种信号
    // 1. 增量更新：添加新笔记时不刷新全表
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, this, &MainWindow::onNoteAdded);
    
    // 2. 全量刷新：修改、删除时才刷新全表
    connect(&DatabaseManager::instance(), &DatabaseManager::noteUpdated, this, &MainWindow::refreshData);
}

void MainWindow::initUI() {
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background-color: #333; }");

    // 左侧
    QWidget* leftPanel = new QWidget();
    leftPanel->setStyleSheet("background-color: #252526;");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0,0,0,0);
    
    QLabel* tagLabel = new QLabel(" 🏷️ 分类视图");
    tagLabel->setStyleSheet("padding: 15px; font-weight: bold; color: #BBB; font-size: 14px;");
    leftLayout->addWidget(tagLabel);

    m_sideBar = new QTreeView();
    m_sideModel = new QStandardItemModel(this);
    m_sideBar->setModel(m_sideModel);
    m_sideBar->setHeaderHidden(true);
    m_sideBar->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sideBar->setStyleSheet("QTreeView { background: transparent; border: none; color: #CCCCCC; } QTreeView::item { padding: 5px; } QTreeView::item:selected { background: #37373D; }");
    connect(m_sideBar, &QTreeView::clicked, this, &MainWindow::onTagSelected);
    leftLayout->addWidget(m_sideBar);

    // 中间
    m_noteList = new QListView();
    m_noteModel = new NoteModel(this);
    m_noteList->setModel(m_noteModel);
    m_noteList->setItemDelegate(new NoteDelegate(m_noteList));
    m_noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_noteList, &QListView::customContextMenuRequested, this, &MainWindow::showContextMenu);
    m_noteList->setSpacing(2);
    m_noteList->setStyleSheet("QListView { background: #1E1E1E; border: none; border-right: 1px solid #333; outline: none; }");
    m_noteList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_noteList, &QListView::clicked, this, &MainWindow::onNoteSelected);
    
    // 右侧
    auto* rightTab = new QTabWidget();
    rightTab->setStyleSheet("QTabBar::tab { background: #2D2D2D; color: #CCC; padding: 10px 20px; border: none; } QTabBar::tab:selected { background: #1E1E1E; color: #FFF; border-top: 2px solid #007ACC; } QTabWidget::pane { border: none; background: #1E1E1E; }");
    
    m_editorArea = new QWidget();
    auto* editorLayout = new QVBoxLayout(m_editorArea);
    editorLayout->setContentsMargins(0,0,0,0);
    m_editor = new Editor();
    m_editor->setPlaceholderText("在这里记录你的灵感...\n(支持 Markdown 语法: #标题 **加粗** [[链接]])");
    editorLayout->addWidget(m_editor);
    m_graphWidget = new GraphWidget();
    rightTab->addTab(m_editorArea, "📝 编辑器");
    rightTab->addTab(m_graphWidget, "🕸️ 知识图谱");

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_noteList);
    splitter->addWidget(rightTab);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 6);

    mainLayout->addWidget(splitter);
}

// 【新增】增量更新逻辑
void MainWindow::onNoteAdded(const QVariantMap& note) {
    // 1. 只在 Model 头部插入一条数据 (瞬间完成)
    m_noteModel->prependNote(note);
    
    // 2. 列表滚动到顶部
    m_noteList->scrollToTop();
    
    // 3. (可选) 如果你想图谱也增量更新，可以在 GraphWidget 加 addSingleNode 接口
    // 这里暂时不做，因为图谱不一定开着
}

void MainWindow::refreshData() {
    auto allNotes = DatabaseManager::instance().getAllNotes();
    m_noteModel->setNotes(allNotes);
    m_graphWidget->loadNotes(allNotes);

    m_sideModel->clear();
    QStandardItem* rootItem = m_sideModel->invisibleRootItem();
    QStandardItem* allItem = new QStandardItem("📂 全部笔记");
    allItem->setData("ALL", Qt::UserRole);
    rootItem->appendRow(allItem);

    QStringList tags = DatabaseManager::instance().getAllTags();
    for (const QString& tag : tags) {
        QStandardItem* item = new QStandardItem("# " + tag);
        item->setData(tag, Qt::UserRole);
        rootItem->appendRow(item);
    }
}

void MainWindow::onNoteSelected(const QModelIndex& index) {
    if (!index.isValid()) return;
    QString content = index.data(NoteModel::ContentRole).toString();
    QString title = index.data(NoteModel::TitleRole).toString();
    m_editor->setPlainText(QString("# %1\n\n%2").arg(title, content));
}

void MainWindow::onTagSelected(const QModelIndex& index) {
    QString tag = index.data(Qt::UserRole).toString();
    if (tag == "ALL") {
        m_noteModel->setNotes(DatabaseManager::instance().getAllNotes());
    } else {
        m_noteModel->setNotes(DatabaseManager::instance().searchNotes(tag));
    }
}

void MainWindow::showContextMenu(const QPoint& pos) {
    QModelIndex index = m_noteList->indexAt(pos);
    if (!index.isValid()) return;

    int id = index.data(NoteModel::IdRole).toInt();
    bool isPinned = index.data(NoteModel::PinnedRole).toBool();
    bool isLocked = index.data(NoteModel::LockedRole).toBool();
    int favorite = index.data(NoteModel::FavoriteRole).toInt();

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item { padding: 8px 25px; } QMenu::item:selected { background: #3E3E42; }");

    QAction* actEdit = menu.addAction("📝 编辑");
    connect(actEdit, &QAction::triggered, [this, id](){
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });

    menu.addSeparator();

    QMenu* starMenu = menu.addMenu("⭐ 设置星级");
    starMenu->setStyleSheet("QMenu { background: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item:selected { background: #3E3E42; }");
    for(int i=0; i<=5; ++i) {
        QString label = (i == 0) ? "无星级" : QString("%1 星").arg(i);
        QAction* act = starMenu->addAction(label);
        if (favorite == i) act->setCheckable(true);
        if (favorite == i) act->setChecked(true);
        connect(act, &QAction::triggered, [id, i](){
            DatabaseManager::instance().updateNoteState(id, "is_favorite", i);
        });
    }

    QAction* actLock = menu.addAction(isLocked ? "🔓 解锁" : "🔒 锁定");
    connect(actLock, &QAction::triggered, [id, isLocked](){
        DatabaseManager::instance().updateNoteState(id, "is_locked", !isLocked);
    });

    menu.addSeparator();

    QAction* actPin = menu.addAction(isPinned ? "🚫 取消置顶" : "📌 置顶");
    connect(actPin, &QAction::triggered, [id, isPinned](){
        DatabaseManager::instance().updateNoteState(id, "is_pinned", !isPinned);
    });

    menu.addSeparator();

    QAction* actDel = menu.addAction("🗑️ 移至回收站");
    connect(actDel, &QAction::triggered, [this, id](){
        DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
    });

    menu.exec(QCursor::pos());
}
```

## 文件: `src\ui\MainWindow.h`

```cpp
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QSplitter>
#include <QStandardItemModel>
#include "../models/NoteModel.h"
#include "Editor.h"
#include "GraphWidget.h"
#include "NoteEditWindow.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNoteSelected(const QModelIndex& index);
    void onTagSelected(const QModelIndex& index);
    void showContextMenu(const QPoint& pos);
    
    // 【新增】处理单条笔记添加，不刷新全表
    void onNoteAdded(const QVariantMap& note);
    
    void refreshData();

private:
    void initUI();
    
    QTreeView* m_sideBar;
    QStandardItemModel* m_sideModel;
    
    QListView* m_noteList;
    NoteModel* m_noteModel;
    
    Editor* m_editor;
    GraphWidget* m_graphWidget;
    
    QWidget* m_editorArea;
};

#endif // MAINWINDOW_H
```

## 文件: `src\ui\NoteDelegate.h`

```cpp
#ifndef NOTEDELEGATE_H
#define NOTEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>
#include "../models/NoteModel.h"

class NoteDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit NoteDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    // 定义卡片高度
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        return QSize(option.rect.width(), 110); // 每个卡片高度 110px
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // 1. 获取数据
        QString title = index.data(NoteModel::TitleRole).toString();
        QString content = index.data(NoteModel::ContentRole).toString();
        QString timeStr = index.data(NoteModel::TimeRole).toDateTime().toString("yyyy-MM-dd HH:mm");
        bool isPinned = index.data(NoteModel::PinnedRole).toBool();
        
        // 2. 处理选中状态和背景
        QRect rect = option.rect.adjusted(5, 5, -5, -5); // 留出间距
        QColor bgColor = (option.state & QStyle::State_Selected) ? QColor("#37373D") : QColor("#2D2D2D");
        
        // 绘制圆角卡片背景
        QPainterPath path;
        path.addRoundedRect(rect, 6, 6);
        painter->fillPath(path, bgColor);

        // 如果选中，画个边框
        if (option.state & QStyle::State_Selected) {
            painter->setPen(QPen(QColor("#007ACC"), 2));
            painter->drawPath(path);
        }

        // 3. 绘制标题 (加粗，白色)
        painter->setPen(QColor("#E0E0E0"));
        painter->setFont(QFont("Microsoft YaHei", 11, QFont::Bold));
        QRect titleRect = rect.adjusted(10, 10, -40, -60);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, title);

        // 4. 绘制置顶图标 (如果有)
        if (isPinned) {
            painter->setPen(QColor("#FFD700")); // 金色
            painter->setFont(QFont("Segoe UI Emoji", 10)); // 使用 Emoji 字体或其他图标字体
            painter->drawText(rect.right() - 30, rect.top() + 25, "📌");
        }

        // 5. 绘制内容预览 (灰色，最多2行)
        painter->setPen(QColor("#AAAAAA"));
        painter->setFont(QFont("Microsoft YaHei", 9));
        QRect contentRect = rect.adjusted(10, 35, -10, -30);
        // 去除换行符，只显示纯文本预览
        QString cleanContent = content.simplified(); 
        painter->drawText(contentRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, 
                          painter->fontMetrics().elidedText(cleanContent, Qt::ElideRight, contentRect.width() * 2));

        // 6. 绘制底部信息栏 (时间 + 标签)
        painter->setPen(QColor("#666666"));
        painter->setFont(QFont("Consolas", 8));
        QRect bottomRect = rect.adjusted(10, 80, -10, -5);
        painter->drawText(bottomRect, Qt::AlignLeft | Qt::AlignVCenter, "🕒 " + timeStr);

        // 绘制一个假的标签气泡演示
        QString tags = index.data(NoteModel::TagsRole).toString();
        if (!tags.isEmpty()) {
            QRect tagRect(rect.right() - 100, rect.bottom() - 25, 90, 20);
            painter->setBrush(QColor("#1E1E1E"));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(tagRect, 4, 4);
            
            painter->setPen(QColor("#888888"));
            painter->drawText(tagRect, Qt::AlignCenter, tags.left(10));
        }

        painter->restore();
    }
};

#endif // NOTEDELEGATE_H
```

## 文件: `src\ui\NoteEditWindow.cpp`

```cpp
#include "NoteEditWindow.h"
#include "../core/DatabaseManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QMessageBox>
#include <QPainter>
#include <QGraphicsDropShadowEffect>

NoteEditWindow::NoteEditWindow(int noteId, QWidget* parent) 
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint), m_noteId(noteId) 
{
    setAttribute(Qt::WA_TranslucentBackground); 
    resize(900, 600); 
    initUI();
    
    if (m_noteId > 0) {
        loadNoteData(m_noteId);
    }
}

void NoteEditWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    painter.setBrush(QColor("#252526")); 
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10);
}

void NoteEditWindow::initUI() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 左侧面板
    QWidget* leftPanel = new QWidget();
    leftPanel->setStyleSheet("background-color: #1E1E1E; border-top-left-radius: 10px; border-bottom-left-radius: 10px; border-right: 1px solid #333;");
    leftPanel->setFixedWidth(280);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(20, 20, 20, 20);
    setupLeftPanel(leftLayout);

    // 右侧面板
    QWidget* rightPanel = new QWidget();
    rightPanel->setStyleSheet("background-color: #252526; border-top-right-radius: 10px; border-bottom-right-radius: 10px;");
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(20, 10, 20, 20); 
    setupRightPanel(rightLayout);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 5);
    setGraphicsEffect(shadow);

    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(rightPanel);
    
    // 关闭按钮
    QPushButton* closeBtn = new QPushButton("×", this);
    closeBtn->setGeometry(width() - 40, 10, 30, 30);
    closeBtn->setStyleSheet("QPushButton { color: #888; background: transparent; font-size: 20px; border: none; } QPushButton:hover { color: white; }");
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
}

void NoteEditWindow::setupLeftPanel(QVBoxLayout* layout) {
    QString labelStyle = "color: #888; font-size: 12px; margin-bottom: 5px; margin-top: 10px;";
    QString inputStyle = "QLineEdit, QComboBox { background: #2D2D2D; border: 1px solid #3E3E42; border-radius: 4px; padding: 8px; color: #EEE; font-size: 13px; } QLineEdit:focus { border: 1px solid #409EFF; }";

    QLabel* winTitle = new QLabel("📝 记录灵感");
    winTitle->setStyleSheet("color: #EEE; font-size: 16px; font-weight: bold; margin-bottom: 20px;");
    layout->addWidget(winTitle);

    QLabel* lblCat = new QLabel("分区");
    lblCat->setStyleSheet(labelStyle);
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItems({"未分类", "工作", "学习", "生活"});
    m_categoryCombo->setStyleSheet(inputStyle);
    layout->addWidget(lblCat);
    layout->addWidget(m_categoryCombo);

    QLabel* lblTitle = new QLabel("标题");
    lblTitle->setStyleSheet(labelStyle);
    m_titleEdit = new QLineEdit();
    m_titleEdit->setPlaceholderText("请输入灵感标题...");
    m_titleEdit->setStyleSheet(inputStyle);
    layout->addWidget(lblTitle);
    layout->addWidget(m_titleEdit);

    QLabel* lblTags = new QLabel("标签");
    lblTags->setStyleSheet(labelStyle);
    m_tagEdit = new QLineEdit();
    m_tagEdit->setPlaceholderText("使用逗号分隔");
    m_tagEdit->setStyleSheet(inputStyle);
    layout->addWidget(lblTags);
    layout->addWidget(m_tagEdit);

    QLabel* lblColor = new QLabel("标记颜色");
    lblColor->setStyleSheet(labelStyle);
    layout->addWidget(lblColor);

    QWidget* colorGrid = new QWidget();
    QGridLayout* grid = new QGridLayout(colorGrid);
    grid->setContentsMargins(0, 10, 0, 10);
    
    m_colorGroup = new QButtonGroup(this);
    QStringList colors = {"#FF9800", "#444444", "#2196F3", "#4CAF50", "#F44336", "#9C27B0"};
    for(int i=0; i<colors.size(); ++i) {
        QPushButton* btn = createColorBtn(colors[i], i);
        grid->addWidget(btn, i/3, i%3);
        m_colorGroup->addButton(btn, i);
    }
    if(m_colorGroup->button(0)) m_colorGroup->button(0)->setChecked(true);
    
    layout->addWidget(colorGrid);

    layout->addStretch(); 

    QPushButton* saveBtn = new QPushButton("💾  保存 (Ctrl+S)");
    saveBtn->setShortcut(QKeySequence("Ctrl+S"));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setFixedHeight(45);
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #409EFF; color: white; border-radius: 4px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #66B1FF; }"
    );
    connect(saveBtn, &QPushButton::clicked, [this](){
        QString title = m_titleEdit->text();
        if(title.isEmpty()) title = "未命名灵感";
        QString content = m_contentEdit->toPlainText();
        QString tags = m_tagEdit->text();
        
        if (m_noteId == 0) {
            DatabaseManager::instance().addNoteAsync(title, content, tags.split(","));
        } else {
            DatabaseManager::instance().updateNote(m_noteId, title, content, tags.split(","));
        }
        emit noteSaved();
        close();
    });
    layout->addWidget(saveBtn);
}

QPushButton* NoteEditWindow::createColorBtn(const QString& color, int id) {
    QPushButton* btn = new QPushButton();
    btn->setCheckable(true);
    btn->setFixedSize(30, 30);
    btn->setStyleSheet(QString(
        "QPushButton { background-color: %1; border-radius: 15px; border: 2px solid transparent; }"
        "QPushButton:checked { border: 2px solid white; }"
    ).arg(color));
    return btn;
}

void NoteEditWindow::setupRightPanel(QVBoxLayout* layout) {
    QHBoxLayout* toolBar = new QHBoxLayout();
    QStringList tools = {"↩", "↪", "☰", "🔢", "Todo", "🗑️"};
    for(const QString& t : tools) {
        QPushButton* btn = new QPushButton(t);
        btn->setFixedSize(35, 35);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("QPushButton { background: transparent; color: #888; border: 1px solid #333; border-radius: 4px; font-size: 14px; } QPushButton:hover { background: #333; color: white; }");
        toolBar->addWidget(btn);
    }
    toolBar->addStretch();
    layout->addLayout(toolBar);

    layout->addSpacing(10);
    m_contentEdit = new Editor(); 
    m_contentEdit->setPlaceholderText("在这里记录详细内容...");
    m_contentEdit->setStyleSheet("QPlainTextEdit { background: transparent; border: none; color: #D4D4D4; font-size: 14px; line-height: 1.5; }");
    layout->addWidget(m_contentEdit);
}

void NoteEditWindow::loadNoteData(int id) {
    auto notes = DatabaseManager::instance().getAllNotes();
    for(const auto& note : notes) {
        if (note["id"].toInt() == id) {
            m_titleEdit->setText(note["title"].toString());
            m_contentEdit->setPlainText(note["content"].toString());
            m_tagEdit->setText(note["tags"].toString());
            break;
        }
    }
}
```

## 文件: `src\ui\NoteEditWindow.h`

```cpp
#ifndef NOTEEDITWINDOW_H
#define NOTEEDITWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QVBoxLayout> // 【修复点】必须包含这个，否则编译器不认识 QVBoxLayout
#include "Editor.h" 

class NoteEditWindow : public QWidget {
    Q_OBJECT
public:
    // mode: 0=新建, >0=编辑(传入笔记ID)
    explicit NoteEditWindow(int noteId = 0, QWidget* parent = nullptr);

signals:
    void noteSaved(); // 保存成功后通知主界面刷新

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void initUI();
    void loadNoteData(int id);
    // 这里使用了 QVBoxLayout 指针，所以上面必须 include 它
    void setupLeftPanel(QVBoxLayout* layout);
    void setupRightPanel(QVBoxLayout* layout);
    QPushButton* createColorBtn(const QString& color, int id);

    int m_noteId;
    
    // UI 控件引用
    QComboBox* m_categoryCombo;
    QLineEdit* m_titleEdit;
    QLineEdit* m_tagEdit;
    QButtonGroup* m_colorGroup;
    QCheckBox* m_defaultColorCheck;
    Editor* m_contentEdit;
};

#endif // NOTEEDITWINDOW_H
```

## 文件: `src\ui\QuickWindow.cpp`

```cpp
#include "QuickWindow.h"
#include "../core/DatabaseManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>

QuickWindow::QuickWindow(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    initUI();

    // 修复：由于信号增加了参数，这里使用 lambda 忽略参数即可
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, [this](const QVariantMap&){
        refreshData();
    });
}

void QuickWindow::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    auto* container = new QWidget();
    container->setObjectName("container");
    container->setStyleSheet(
        "QWidget#container { background: #1E1E1E; border-radius: 10px; border: 1px solid #333; }"
        "QLineEdit { background: transparent; border: none; color: white; font-size: 18px; padding: 10px; border-bottom: 1px solid #333; }"
        "QListView { background: transparent; border: none; color: #BBB; outline: none; }"
        "QListView::item { padding: 8px; }"
        "QListView::item:selected { background: #37373D; border-radius: 4px; }"
    );
    
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 5);
    container->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(5, 5, 5, 5);
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索笔记或输入新内容按回车保存...");
    
    m_listView = new QListView();
    m_model = new NoteModel(this);
    m_listView->setModel(m_model);
    m_listView->setFixedHeight(300);

    // 搜索逻辑
    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text){
        if (text.isEmpty()) {
            m_model->setNotes(DatabaseManager::instance().getAllNotes());
        } else {
            m_model->setNotes(DatabaseManager::instance().searchNotes(text));
        }
    });

    // 回车保存逻辑
    connect(m_searchEdit, &QLineEdit::returnPressed, [this](){
        QString text = m_searchEdit->text();
        if (!text.isEmpty()) {
            DatabaseManager::instance().addNoteAsync("快速记录", text, {"Quick"});
            m_searchEdit->clear();
            hide();
        }
    });

    layout->addWidget(m_searchEdit);
    layout->addWidget(m_listView);
    
    mainLayout->addWidget(container);
    setFixedSize(600, 400);
    
    refreshData();
}

void QuickWindow::refreshData() {
    if (m_searchEdit->text().isEmpty()) {
        m_model->setNotes(DatabaseManager::instance().getAllNotes());
    }
}

void QuickWindow::showCentered() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->geometry();
        move(screenGeom.center() - rect().center());
    }
    show();
    activateWindow();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

bool QuickWindow::event(QEvent* event) {
    if (event->type() == QEvent::WindowDeactivate) {
        hide();
    }
    return QWidget::event(event);
}
```

## 文件: `src\ui\QuickWindow.h`

```cpp
#ifndef QUICKWINDOW_H
#define QUICKWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QListView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../models/NoteModel.h"

class QuickWindow : public QWidget {
    Q_OBJECT
public:
    explicit QuickWindow(QWidget* parent = nullptr);
    void showCentered();

public slots:
    void refreshData();

protected:
    bool event(QEvent* event) override;

private:
    void initUI();
    
    QLineEdit* m_searchEdit;
    QListView* m_listView;
    NoteModel* m_model;
};

#endif // QUICKWINDOW_H
```

## 文件: `src\ui\ScreenshotTool.h`

```cpp
#ifndef SCREENSHOTTOOL_H
#define SCREENSHOTTOOL_H

#include <QWidget>
#include <QRubberBand>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>

class ScreenshotTool : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotTool(QWidget* parent = nullptr) : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowState(Qt::WindowFullScreen);
        setCursor(Qt::CrossCursor);
        m_screenPixmap = QGuiApplication::primaryScreen()->grabWindow(0);
    }

signals:
    void screenshotCaptured(const QImage& image);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_screenPixmap);
        painter.fillRect(rect(), QColor(0, 0, 0, 100));
    }

    void mousePressEvent(QMouseEvent* event) override {
        m_origin = event->pos();
        if (!m_rubberBand) m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        m_rubberBand->setGeometry(QRect(m_origin, QSize()));
        m_rubberBand->show();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        m_rubberBand->setGeometry(QRect(m_origin, event->pos()).normalized());
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        m_rubberBand->hide();
        QRect rect = QRect(m_origin, event->pos()).normalized();
        if (rect.width() > 5 && rect.height() > 5) {
            QImage img = m_screenPixmap.copy(rect).toImage();
            emit screenshotCaptured(img);
        }
        close();
    }

private:
    QPixmap m_screenPixmap;
    QRubberBand* m_rubberBand = nullptr;
    QPoint m_origin;
};

#endif // SCREENSHOTTOOL_H
```

