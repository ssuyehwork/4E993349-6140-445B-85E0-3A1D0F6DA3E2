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
#include <QProcess>
#include <QTemporaryFile>
#include <QApplication>
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

    auto* titleLabel = new QLabel("存储文件 (存入数据库)");
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
    m_dropHint = new QPushButton("拖拽文件或文件夹到这里\n数据将完整存入笔记库");
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

    auto* tipLabel = new QLabel("文件夹将自动打包为 ZIP 格式存储");
    tipLabel->setStyleSheet("color: #666; font-size: 10px;");
    tipLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(tipLabel);
}

void FileStorageWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_dropHint->setStyleSheet("color: #f1c40f; font-size: 12px; border: 2px dashed #f1c40f; border-radius: 8px; padding: 20px; background-color: rgba(241, 196, 15, 0.05);");
    }
}

void FileStorageWindow::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    m_dropHint->setStyleSheet("color: #888; font-size: 12px; border: 2px dashed #444; border-radius: 8px; padding: 20px; background: #181818;");
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
    m_dropHint->setStyleSheet("color: #888; font-size: 12px; border: 2px dashed #444; border-radius: 8px; padding: 20px; background: #181818;");
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

void FileStorageWindow::storeArchive(const QStringList& paths) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString zipName = QString("批量存储_%1.zip").arg(timestamp);
    QString tempZip = QDir::tempPath() + "/" + zipName;

    QStringList args;
    args << "-caf" << tempZip;
    for (const QString& path : paths) {
        QFileInfo info(path);
        args << "-C" << info.absolutePath() << info.fileName();
    }

    QProcess proc;
    proc.start("tar", args);
    if (!proc.waitForFinished(60000)) { // 60s timeout for multi files
        m_statusList->addItem("❌ 失败: 压缩超时 - " + zipName);
        return;
    }

    if (proc.exitCode() != 0) {
        m_statusList->addItem("❌ 失败: tar 错误 - " + zipName);
        return;
    }

    QFile zipFile(tempZip);
    if (zipFile.open(QIODevice::ReadOnly)) {
        QByteArray data = zipFile.readAll();
        zipFile.close();
        QFile::remove(tempZip);

        bool ok = DatabaseManager::instance().addNote(
            zipName,
            QString("[已存入实际项目(打包): %1个项目]").arg(paths.size()),
            {"批量存储"},
            "#34495e",
            m_categoryId,
            "file",
            data
        );

        if (ok) {
            m_statusList->addItem("✅ 成功: " + zipName + " (" + QString::number(data.size() / 1024) + " KB)");
        } else {
            m_statusList->addItem("❌ 失败: 数据库写入错误");
        }
    } else {
        m_statusList->addItem("❌ 失败: 无法读取 ZIP");
    }
}

void FileStorageWindow::storeFile(const QString& path) {
    QFileInfo info(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_statusList->addItem("❌ 失败: 无法读取 " + info.fileName());
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    bool ok = DatabaseManager::instance().addNote(
        info.fileName(),
        QString("[已存入实际文件: %1]").arg(info.fileName()),
        {"文件存储"},
        "#2c3e50",
        m_categoryId,
        "file",
        data
    );

    if (ok) {
        m_statusList->addItem("✅ 成功: " + info.fileName() + " (" + QString::number(data.size() / 1024) + " KB)");
    } else {
        m_statusList->addItem("❌ 失败: 数据库写入错误 - " + info.fileName());
    }
}

void FileStorageWindow::storeFolder(const QString& path) {
    QFileInfo info(path);
    QString folderName = info.fileName();
    QString parentDir = info.absolutePath();

    // 创建临时 zip 路径
    QString tempZip = QDir::tempPath() + "/" + folderName + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".zip";

    // 使用 tar 压缩 (Windows 10+ 自带 tar)
    // tar -caf dest.zip -C parent folder
    QStringList args;
    args << "-caf" << tempZip << "-C" << parentDir << folderName;

    QProcess proc;
    proc.start("tar", args);
    if (!proc.waitForFinished(30000)) { // 30s timeout
        m_statusList->addItem("❌ 失败: 压缩超时 - " + folderName);
        return;
    }

    if (proc.exitCode() != 0) {
        m_statusList->addItem("❌ 失败: tar 错误 - " + folderName);
        return;
    }

    QFile zipFile(tempZip);
    if (zipFile.open(QIODevice::ReadOnly)) {
        QByteArray data = zipFile.readAll();
        zipFile.close();
        QFile::remove(tempZip); // 删除临时文件

        bool ok = DatabaseManager::instance().addNote(
            folderName + ".zip",
            QString("[已存入实际文件夹(打包): %1]").arg(folderName),
            {"文件夹存储"},
            "#8e44ad",
            m_categoryId,
            "file",
            data
        );

        if (ok) {
            m_statusList->addItem("✅ 成功(打包): " + folderName + " (" + QString::number(data.size() / 1024) + " KB)");
        } else {
            m_statusList->addItem("❌ 失败: 数据库写入错误 - " + folderName);
        }
    } else {
        m_statusList->addItem("❌ 失败: 无法读取生成的 ZIP - " + folderName);
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
