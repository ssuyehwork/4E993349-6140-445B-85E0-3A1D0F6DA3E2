#include "FileSearchWindow.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QSet>
#include <QtConcurrent>
#include <QMouseEvent>
#include <QPainter>

// ----------------------------------------------------------------------------
// ResizeHandle 实现
// ----------------------------------------------------------------------------
ResizeHandle::ResizeHandle(QWidget* target, QWidget* parent)
    : QWidget(parent), m_target(target)
{
    setFixedSize(20, 20);
    setCursor(Qt::SizeFDiagCursor);
}

void ResizeHandle::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_startPos = event->globalPosition().toPoint();
        m_startSize = m_target->size();
        event->accept();
    }
}

void ResizeHandle::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->globalPosition().toPoint() - m_startPos;
        int newW = qMax(m_startSize.width() + delta.x(), 400);
        int newH = qMax(m_startSize.height() + delta.y(), 300);
        m_target->resize(newW, newH);
        event->accept();
    }
}

// ----------------------------------------------------------------------------
// FileSearchWindow 实现
// ----------------------------------------------------------------------------
FileSearchWindow::FileSearchWindow(QWidget* parent)
    : FramelessDialog("查找文件", parent)
{
    resize(850, 600);
    initUI();
    m_resizeHandle = new ResizeHandle(this, this);
    m_resizeHandle->raise();
}

void FileSearchWindow::initUI() {
    // 强制微软雅黑字体
    QFont yahei("Microsoft YaHei", 10);
    this->setFont(yahei);

    // 应用 Python 风格样式表
    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 14px;
            color: #E0E0E0;
            outline: none;
        }
        QListWidget {
            background-color: #252526;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 4px;
        }
        QListWidget::item {
            height: 30px;
            padding-left: 8px;
            border-radius: 4px;
            color: #CCCCCC;
        }
        QListWidget::item:selected {
            background-color: #37373D;
            border-left: 3px solid #007ACC;
            color: #FFFFFF;
        }
        QListWidget::item:hover {
            background-color: #2A2D2E;
        }
        QLineEdit {
            background-color: #333333;
            border: 1px solid #444444;
            color: #FFFFFF;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #264F78;
        }
        QLineEdit:focus {
            border: 1px solid #007ACC;
            background-color: #2D2D2D;
        }
        QPushButton#ActionBtn {
            background-color: #007ACC;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 18px;
            font-weight: bold;
        }
        QPushButton#ActionBtn:hover {
            background-color: #0062A3;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #555555;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");

    auto* layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(24, 15, 24, 24);
    layout->setSpacing(16);

    // 第一行：文件夹路径选择
    auto* pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText("在此粘贴文件夹路径 (Ctrl+V)，然后按回车...");
    m_pathEdit->setClearButtonEnabled(true);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &FileSearchWindow::startSearch);

    auto* browseBtn = new QPushButton("浏览");
    browseBtn->setObjectName("ActionBtn");
    browseBtn->setAutoDefault(false);
    browseBtn->setDefault(false);
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, this, &FileSearchWindow::browseFolder);

    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseBtn);
    layout->addLayout(pathLayout);

    // 第二行：过滤与搜索
    auto* filterLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("🔍 输入文件名过滤...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FileSearchWindow::filterResults);

    m_extEdit = new QLineEdit();
    m_extEdit->setPlaceholderText("后缀 (如 py)");
    m_extEdit->setFixedWidth(120);
    connect(m_extEdit, &QLineEdit::textChanged, this, &FileSearchWindow::filterResults);

    filterLayout->addWidget(m_searchEdit);
    filterLayout->addWidget(m_extEdit);
    layout->addLayout(filterLayout);

    m_infoLabel = new QLabel("等待操作...");
    m_infoLabel->setStyleSheet("color: #888888; font-size: 12px;");
    layout->addWidget(m_infoLabel);

    // 文件列表
    m_fileList = new QListWidget();
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &FileSearchWindow::locateFile);
    layout->addWidget(m_fileList);

    m_searchBtn = new QPushButton(); // 隐藏但在后台使用，或者移除
    m_searchBtn->setVisible(false);
    m_searchBtn->setAutoDefault(false);
    connect(&m_watcher, &QFutureWatcher<QStringList>::finished, this, &FileSearchWindow::onSearchFinished);
}

void FileSearchWindow::browseFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", m_pathEdit->text());
    if (!dir.isEmpty()) {
        m_pathEdit->setText(dir);
    }
}

void FileSearchWindow::startSearch() {
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty() || !QDir(path).exists()) {
        m_infoLabel->setText("❌ 路径不存在");
        m_pathEdit->setStyleSheet("border: 1px solid #FF3333;");
        return;
    }
    m_pathEdit->setStyleSheet("");

    m_infoLabel->setText("🚀 正在扫描: " + path);
    m_allFiles.clear();
    m_fileList->clear();

    // 在后台线程执行遍历，增加忽略逻辑并支持高效剪枝
    QFuture<QStringList> future = QtConcurrent::run([path]() {
        QStringList result;
        QSet<QString> ignored = {".git", ".idea", "__pycache__", "node_modules", "$RECYCLE.BIN", "System Volume Information"};

        std::function<void(const QString&)> scanDir = [&](const QString& currentPath) {
            QDir dir(currentPath);
            // 获取所有文件
            QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
            for (const auto& fi : files) {
                result << fi.absoluteFilePath();
            }

            // 获取所有子目录并过滤
            QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& di : subDirs) {
                if (!ignored.contains(di.fileName())) {
                    scanDir(di.absoluteFilePath());
                }
            }
        };

        scanDir(path);
        return result;
    });
    m_watcher.setFuture(future);
}

void FileSearchWindow::onSearchFinished() {
    m_allFiles = m_watcher.result();
    m_infoLabel->setText(QString("✅ 扫描结束，共 %1 个文件").arg(m_allFiles.size()));
    filterResults();
}

void FileSearchWindow::filterResults() {
    m_fileList->clear();
    QString keyword = m_searchEdit->text().toLower();
    QString ext = m_extEdit->text().toLower().trimmed();
    if (ext.startsWith(".")) ext = ext.mid(1);

    int limit = 500;
    int count = 0;

    for (const QString& path : m_allFiles) {
        QFileInfo info(path);
        QString fileName = info.fileName();

        bool matchKeyword = keyword.isEmpty() || fileName.toLower().contains(keyword);
        bool matchExt = ext.isEmpty() || info.suffix().toLower() == ext;

        if (matchKeyword && matchExt) {
            auto* item = new QListWidgetItem(fileName);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            m_fileList->addItem(item);
            count++;

            if (count >= limit) {
                auto* warn = new QListWidgetItem("--- 结果过多，仅显示前 500 条 ---");
                warn->setForeground(QColor("#FFAA00"));
                warn->setTextAlignment(Qt::AlignCenter);
                warn->setFlags(Qt::NoItemFlags);
                m_fileList->addItem(warn);
                break;
            }
        }
    }
}

void FileSearchWindow::resizeEvent(QResizeEvent* event) {
    FramelessDialog::resizeEvent(event);
    if (m_resizeHandle) {
        m_resizeHandle->move(width() - 20, height() - 20);
    }
}

void FileSearchWindow::locateFile(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    QFileInfo info(path);
    if (info.exists()) {
        // 定位并选中文件
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer.exe", args);
    }
}
