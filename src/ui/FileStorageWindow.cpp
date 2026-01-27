#include "FileStorageWindow.h"
#include "IconHelper.h"
#include "../core/DatabaseManager.h"
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <utility>
#include <QApplication>
#include <QCoreApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QFileDialog>
#include <QMenu>
#include <QToolTip>
#include <QDateTime>
#include <QDebug>

FileStorageWindow::FileStorageWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setAcceptDrops(true);
    resize(420, 400);

    initUI();
}

void FileStorageWindow::initUI() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(15, 15, 15, 15);

    auto* container = new QFrame();
    container->setObjectName("FileStoreContainer");
    container->setStyleSheet("#FileStoreContainer { background-color: #1E1E1E; border-radius: 12px; border: 1px solid #444; }");
    rootLayout->addWidget(container);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 150));
    container->setGraphicsEffect(shadow);

    auto* contentLayout = new QVBoxLayout(container);
    contentLayout->setContentsMargins(10, 5, 10, 15);
    contentLayout->setSpacing(10);

    // Title Bar
    auto* titleBar = new QFrame();
    titleBar->setFixedHeight(40);
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 5, 0);

    auto* iconLabel = new QLabel();
    iconLabel->setPixmap(IconHelper::getIcon("save", "#f1c40f").pixmap(20, 20));
    titleLayout->addWidget(iconLabel);

    auto* titleLabel = new QLabel("存储文件 (文件系统托管)");
    titleLabel->setStyleSheet("color: #f1c40f; font-weight: bold; font-size: 13px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto* btnClose = new QPushButton();
    btnClose->setFixedSize(24, 24);
    btnClose->setIcon(IconHelper::getIcon("close", "#888888"));
    btnClose->setStyleSheet("QPushButton { border: none; background: transparent; border-radius: 4px; } QPushButton:hover { background-color: #e74c3c; }");
    connect(btnClose, &QPushButton::clicked, this, &FileStorageWindow::hide);
    titleLayout->addWidget(btnClose);

    contentLayout->addWidget(titleBar);

    // Drop Area
    m_dropHint = new QPushButton("拖拽文件或文件夹到这里\n数据将完整拷贝至存储库");
    m_dropHint->setObjectName("DropArea");
    m_dropHint->setStyleSheet("QPushButton#DropArea { color: #888; font-size: 12px; border: 2px dashed #444; border-radius: 8px; padding: 20px; background: #181818; outline: none; } "
                               "QPushButton#DropArea:hover { border-color: #f1c40f; color: #f1c40f; background-color: rgba(241, 196, 15, 0.05); }");
    m_dropHint->setFixedHeight(100);
    connect(m_dropHint, &QPushButton::clicked, this, &FileStorageWindow::onSelectItems);
    contentLayout->addWidget(m_dropHint);

    // Status List
    m_statusList = new QListWidget();
    m_statusList->setStyleSheet("QListWidget { background-color: #252526; border: 1px solid #333; border-radius: 6px; color: #BBB; padding: 5px; font-size: 11px; } "
                                "QListWidget::item { padding: 4px; border-bottom: 1px solid #2d2d2d; }");
    contentLayout->addWidget(m_statusList);

    auto* tipLabel = new QLabel("文件将直接复制到 attachments 文件夹");
    tipLabel->setStyleSheet("color: #666; font-size: 10px;");
    tipLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(tipLabel);
}

// ==========================================
// 1. 辅助工具函数
// ==========================================

QString FileStorageWindow::getStorageRoot() {
    QString path = QCoreApplication::applicationDirPath() + "/attachments";
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return path;
}

QString FileStorageWindow::getUniqueFilePath(const QString& dirPath, const QString& fileName) {
    QDir dir(dirPath);
    QString baseName = QFileInfo(fileName).completeBaseName();
    QString suffix = QFileInfo(fileName).suffix();
    if (!suffix.isEmpty()) suffix = "." + suffix;

    QString finalName = fileName;
    int counter = 1;

    while (dir.exists(finalName)) {
        finalName = QString("%1_%2%3").arg(baseName).arg(counter).arg(suffix);
        counter++;
    }
    return dir.filePath(finalName);
}

bool FileStorageWindow::copyRecursively(const QString& srcStr, const QString& dstStr) {
    QDir srcDir(srcStr);
    if (!srcDir.exists()) return false;

    QDir dstDir(dstStr);
    if (!dstDir.exists()) {
        dstDir.mkpath(".");
    }

    // 1. 复制所有文件
    for (const QString& file : srcDir.entryList(QDir::Files)) {
        QString srcFile = srcDir.filePath(file);
        QString dstFile = dstDir.filePath(file);
        if (!QFile::copy(srcFile, dstFile)) {
            return false;
        }
    }

    // 2. 递归复制子文件夹
    for (const QString& dir : srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString srcSub = srcDir.filePath(dir);
        QString dstSub = dstDir.filePath(dir);
        if (!copyRecursively(srcSub, dstSub)) {
            return false;
        }
    }
    return true;
}

// ==========================================
// 2. 核心存储逻辑
// ==========================================

void FileStorageWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_dropHint->setStyleSheet("QPushButton#DropArea { color: #f1c40f; font-size: 12px; border: 2px dashed #f1c40f; border-radius: 8px; padding: 20px; background-color: rgba(241, 196, 15, 0.05); }");
    }
}

void FileStorageWindow::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    m_dropHint->setStyleSheet("QPushButton#DropArea { color: #888; font-size: 12px; border: 2px dashed #444; border-radius: 8px; padding: 20px; background: #181818; outline: none; }");
}

void FileStorageWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList paths;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) paths << url.toLocalFile();
        }
        
        if (!paths.isEmpty()) {
            processStorage(paths);
        }
    }
    m_dropHint->setStyleSheet("QPushButton#DropArea { color: #888; font-size: 12px; border: 2px dashed #444; border-radius: 8px; padding: 20px; background: #181818; outline: none; }");
}

void FileStorageWindow::processStorage(const QStringList& paths) {
    m_statusList->clear();
    if (paths.isEmpty()) return;

    if (paths.size() == 1) {
        QFileInfo info(paths.first());
        if (info.isDir()) {
            storeFolder(paths.first());
        } else {
            storeFile(paths.first());
        }
    } else {
        storeArchive(paths);
    }
}

void FileStorageWindow::storeFile(const QString& path) {
    QFileInfo info(path);
    QString storageDir = getStorageRoot();
    QString destPath = getUniqueFilePath(storageDir, info.fileName());
    
    if (QFile::copy(path, destPath)) {
        QFileInfo destInfo(destPath);
        QString relativePath = "attachments/" + destInfo.fileName();

        bool ok = DatabaseManager::instance().addNote(
            info.fileName(),
            relativePath,
            {"文件链接"},
            "#2c3e50",
            m_categoryId,
            "local_file",
            QByteArray(),
            "FileStorage",
            info.absoluteFilePath()
        );

        if (ok) {
            m_statusList->addItem("✅ 已归档: " + info.fileName());
        } else {
            m_statusList->addItem("❌ 数据库错误: " + info.fileName());
            QFile::remove(destPath);
        }
    } else {
        m_statusList->addItem("❌ 复制失败: 权限不足或文件被占用");
    }
}

void FileStorageWindow::storeFolder(const QString& path) {
    QFileInfo info(path);
    QString storageDir = getStorageRoot();
    QString destDir = getUniqueFilePath(storageDir, info.fileName());
    
    m_statusList->addItem("📂 正在导入文件夹: " + info.fileName() + "...");
    QApplication::processEvents();

    if (copyRecursively(path, destDir)) {
        QDir d(destDir);
        QString relativePath = "attachments/" + d.dirName();

        bool ok = DatabaseManager::instance().addNote(
            info.fileName(),
            relativePath,
            {"文件夹链接"},
            "#8e44ad",
            m_categoryId,
            "local_folder",
            QByteArray(),
            "FileStorage",
            info.absoluteFilePath()
        );

        if (ok) {
            m_statusList->addItem("✅ 文件夹归档成功");
        } else {
            m_statusList->addItem("❌ 数据库错误");
            QDir(destDir).removeRecursively();
        }
    } else {
        m_statusList->addItem("❌ 文件夹复制失败");
    }
}

void FileStorageWindow::storeArchive(const QStringList& paths) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString folderName = "批量导入_" + timestamp;
    
    QString storageRoot = getStorageRoot();
    QString destDir = storageRoot + "/" + folderName;
    
    if (!QDir().mkpath(destDir)) {
        m_statusList->addItem("❌ 无法创建存储目录");
        return;
    }

    int successCount = 0;
    m_statusList->addItem("📦 正在处理 " + QString::number(paths.size()) + " 个项目...");
    QApplication::processEvents();

    for (const QString& srcPath : std::as_const(paths)) {
        QFileInfo srcInfo(srcPath);
        QString destPath = destDir + "/" + srcInfo.fileName();

        bool copyOk = false;
        if (srcInfo.isDir()) {
            copyOk = copyRecursively(srcPath, destPath);
        } else {
            copyOk = QFile::copy(srcPath, destPath);
        }

        if (copyOk) successCount++;
    }

    if (successCount > 0) {
        QString relativePath = "attachments/" + folderName;
        
        // 构建描述性标题：[数量个项目] 文件1, 文件2...
        QStringList names;
        for (const QString& p : paths) {
            names << QFileInfo(p).fileName();
        }
        QString descriptiveTitle = QString("[%1个项目] %2").arg(paths.size()).arg(names.join(", "));
        if (descriptiveTitle.length() > 120) {
            descriptiveTitle = descriptiveTitle.left(117) + "...";
        }

        bool ok = DatabaseManager::instance().addNote(
            descriptiveTitle,
            relativePath,
            {"批量导入"},
            "#34495e",
            m_categoryId,
            "local_batch",
            QByteArray(),
            "FileStorage",
            ""
        );

        if (ok) {
            m_statusList->addItem(QString("✅ 成功归档 %1/%2 个项目").arg(successCount).arg(paths.size()));
        } else {
            m_statusList->addItem("❌ 数据库写入失败");
        }
    } else {
        m_statusList->addItem("❌ 所有项目导入失败");
        QDir(destDir).removeRecursively();
    }
}

void FileStorageWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->pos().y() <= 60) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void FileStorageWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void FileStorageWindow::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_dragPos = QPoint();
}

void FileStorageWindow::onSelectItems() {
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                       "QMenu::item { padding: 6px 20px; border-radius: 3px; } "
                       "QMenu::item:selected { background-color: #f1c40f; color: black; }");

    menu.addAction("选择并存入文件...", [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this, "选择文件", "", "所有文件 (*.*)");
        if (!files.isEmpty()) {
            processStorage(files);
        }
    });

    menu.addAction("选择并存入文件夹...", [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", "");
        if (!dir.isEmpty()) {
            processStorage({dir});
        }
    });

    menu.exec(QCursor::pos());
}
