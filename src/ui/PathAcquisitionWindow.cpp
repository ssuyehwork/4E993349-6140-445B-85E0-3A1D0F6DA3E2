#include "PathAcquisitionWindow.h"
#include "IconHelper.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QToolTip>
#include <QDirIterator>
#include <QDir>
#include <QFileInfo>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QDesktopServices>
#include <QFileDialog>

PathAcquisitionWindow::PathAcquisitionWindow(QWidget* parent) : FramelessDialog("路径提取", parent) {
    setAcceptDrops(true);
    resize(700, 500); // 增加尺寸以适应左右布局

    initUI();
}

PathAcquisitionWindow::~PathAcquisitionWindow() {
}

void PathAcquisitionWindow::initUI() {
    auto* mainLayout = new QHBoxLayout(m_contentArea);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // --- 左侧面板 (操作区) ---
    auto* leftPanel = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(15);

    // 拖拽提示区 (使用内部布局和自定义 Label 确保在任意高度下都能完美居中)
    m_dropHint = new QToolButton();
    m_dropHint->setObjectName("DropZone");
    m_dropHint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* dropLayout = new QVBoxLayout(m_dropHint);
    dropLayout->setContentsMargins(20, 20, 20, 20);
    dropLayout->setSpacing(15);
    dropLayout->setAlignment(Qt::AlignCenter);

    m_dropIconLabel = new QLabel();
    m_dropIconLabel->setAlignment(Qt::AlignCenter);
    m_dropIconLabel->setFixedSize(48, 48);
    m_dropIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_dropTextLabel = new QLabel("投喂文件/文件夹\n(或点击进行浏览)");
    m_dropTextLabel->setAlignment(Qt::AlignCenter);
    m_dropTextLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    dropLayout->addWidget(m_dropIconLabel);
    dropLayout->addWidget(m_dropTextLabel);

    updateDropHintStyle(false); // 初始化样式

    connect(m_dropHint, &QToolButton::clicked, this, &PathAcquisitionWindow::onBrowse);
    leftLayout->addWidget(m_dropHint, 1);

    // 选项
    m_recursiveCheck = new QCheckBox("递归遍历文件夹\n(包含所有子目录)");
    m_recursiveCheck->setStyleSheet("QCheckBox { color: #ccc; font-size: 12px; spacing: 5px; } QCheckBox::indicator { width: 16px; height: 16px; }");
    // 连接信号实现自动刷新
    connect(m_recursiveCheck, &QCheckBox::toggled, this, [this](bool){
        if (!m_currentUrls.isEmpty()) {
            processStoredUrls();
        }
    });
    leftLayout->addWidget(m_recursiveCheck);

    auto* tipLabel = new QLabel("提示：切换选项会自动刷新\n无需重新拖入");
    tipLabel->setStyleSheet("color: #666; font-size: 11px;");
    tipLabel->setAlignment(Qt::AlignLeft);
    tipLabel->setWordWrap(true);
    leftLayout->addWidget(tipLabel);

    leftLayout->addStretch(); // 底部弹簧

    leftPanel->setFixedWidth(200);
    mainLayout->addWidget(leftPanel);

    // --- 右侧面板 (列表区) ---
    auto* rightPanel = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    auto* listLabel = new QLabel("提取结果");
    listLabel->setStyleSheet("color: #888; font-size: 12px; font-weight: bold;");
    rightLayout->addWidget(listLabel);

    m_pathList = new QListWidget(this);
    m_pathList->setStyleSheet("QListWidget { background-color: #252526; border: 1px solid #333; border-radius: 6px; color: #AAA; padding: 5px; font-size: 12px; outline: none; } "
                              "QListWidget::item { padding: 4px; border-bottom: 1px solid #2d2d2d; } "
                              "QListWidget::item:selected { background-color: #3E3E42; color: #FFF; }");
    m_pathList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_pathList->setCursor(Qt::PointingHandCursor);
    connect(m_pathList, &QListWidget::customContextMenuRequested, this, &PathAcquisitionWindow::onShowContextMenu);
    connect(m_pathList, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem* item) {
        QString path = item->text();
        QString nativePath = QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer.exe", { "/select," + nativePath });
    });
    rightLayout->addWidget(m_pathList);

    mainLayout->addWidget(rightPanel);
}

void PathAcquisitionWindow::updateDropHintStyle(bool dragging) {
    if (dragging) {
        m_dropHint->setStyleSheet(
            "QToolButton#DropZone { border: 2px dashed #3a90ff; border-radius: 8px; background-color: rgba(58, 144, 255, 0.05); }"
        );
        m_dropIconLabel->setPixmap(IconHelper::getIcon("folder", "#3a90ff", 48).pixmap(48, 48));
        m_dropTextLabel->setStyleSheet("color: #3a90ff; font-size: 13px; background: transparent; border: none; font-weight: bold;");
    } else {
        m_dropHint->setStyleSheet(
            "QToolButton#DropZone { border: 2px dashed #444; border-radius: 8px; background: #181818; }"
            "QToolButton#DropZone:hover { border-color: #555; background: #202020; }"
        );
        m_dropIconLabel->setPixmap(IconHelper::getIcon("folder", "#888888", 48).pixmap(48, 48));
        m_dropTextLabel->setStyleSheet("color: #888; font-size: 13px; background: transparent; border: none;");
    }
}

void PathAcquisitionWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        updateDropHintStyle(true);
    }
}

void PathAcquisitionWindow::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    updateDropHintStyle(false);
}

void PathAcquisitionWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        m_currentUrls = mimeData->urls();
        processStoredUrls();
    }
    updateDropHintStyle(false);
}

void PathAcquisitionWindow::hideEvent(QHideEvent* event) {
    m_currentUrls.clear();
    m_pathList->clear();
    FramelessDialog::hideEvent(event);
}

void PathAcquisitionWindow::processStoredUrls() {
    m_pathList->clear();
    
    QStringList paths;
    for (const QUrl& url : std::as_const(m_currentUrls)) {
        QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            QFileInfo fileInfo(path);
            
            // 如果选中递归且是目录，则遍历
            if (m_recursiveCheck->isChecked() && fileInfo.isDir()) {
                QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString subPath = it.next();
                    // 统一路径分隔符
                    subPath.replace("\\", "/");
                    m_pathList->addItem(subPath);
                    paths << subPath;
                }
            } else {
                // 默认行为：只添加该路径本身
                // 统一路径分隔符
                path.replace("\\", "/");
                m_pathList->addItem(path);
                paths << path;
            }
        }
    }
    
    if (!paths.isEmpty()) {
        QToolTip::showText(QCursor::pos(), "已提取 " + QString::number(paths.size()) + " 条路径\n右键可复制或定位", this);
    } else if (!m_currentUrls.isEmpty()) {
        // 如果处理了 URL 但没有产出（例如空文件夹），也提示一下
         QToolTip::showText(QCursor::pos(), "没有找到文件", this);
    }
    
    m_pathList->scrollToBottom();
}

void PathAcquisitionWindow::onShowContextMenu(const QPoint& pos) {
    auto* item = m_pathList->itemAt(pos);
    if (!item) return;

    QString path = item->text();
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                       "QMenu::item { padding: 6px 10px 6px 10px; border-radius: 3px; } "
                       "QMenu::item:selected { background-color: #4a90e2; color: white; }");

    menu.addAction(IconHelper::getIcon("copy", "#1abc9c", 18), "复制路径", [path]() {
        QApplication::clipboard()->setText(path);
    });

    menu.addAction(IconHelper::getIcon("file", "#3498db", 18), "复制文件", [path]() {
        QMimeData* mimeData = new QMimeData;
        QList<QUrl> urls;
        urls << QUrl::fromLocalFile(path);
        mimeData->setUrls(urls);
        QApplication::clipboard()->setMimeData(mimeData);
    });

    menu.addSeparator();

    menu.addAction(IconHelper::getIcon("search", "#e67e22", 18), "定位文件", [path]() {
        QString nativePath = QDir::toNativeSeparators(path);
        // explorer.exe /select,"path"
        QProcess::startDetached("explorer.exe", { "/select," + nativePath });
    });

    menu.addAction(IconHelper::getIcon("folder", "#f1c40f", 18), "定位文件夹", [path]() {
        QFileInfo fi(path);
        QString dirPath = fi.isDir() ? path : fi.absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    });

    menu.exec(m_pathList->mapToGlobal(pos));
}

void PathAcquisitionWindow::onBrowse() {
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                       "QMenu::item { padding: 6px 10px 6px 10px; border-radius: 3px; } "
                       "QMenu::item:selected { background-color: #4a90e2; color: white; }");

    menu.addAction(IconHelper::getIcon("file", "#3498db", 18), "选择文件", [this]() {
        QStringList filePaths = QFileDialog::getOpenFileNames(this, "选择文件", "", "所有文件 (*.*)");
        if (!filePaths.isEmpty()) {
            m_currentUrls.clear();
            for (const QString& path : filePaths) {
                m_currentUrls << QUrl::fromLocalFile(path);
            }
            processStoredUrls();
        }
    });

    menu.addAction(IconHelper::getIcon("folder", "#f1c40f", 18), "选择文件夹", [this]() {
        QString dirPath = QFileDialog::getExistingDirectory(this, "选择文件夹", "");
        if (!dirPath.isEmpty()) {
            m_currentUrls.clear();
            m_currentUrls << QUrl::fromLocalFile(dirPath);
            processStoredUrls();
        }
    });

    menu.exec(QCursor::pos());
}
