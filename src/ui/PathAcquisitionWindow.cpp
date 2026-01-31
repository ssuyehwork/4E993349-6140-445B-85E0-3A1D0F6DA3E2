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
#include <QCheckBox>

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

    // 拖拽提示区
    m_dropHint = new QLabel("📁 投喂文件/文件夹\n(自动提取路径)");
    m_dropHint->setAlignment(Qt::AlignCenter);
    m_dropHint->setStyleSheet(
        "QLabel { color: #888; font-size: 13px; border: 2px dashed #444; border-radius: 8px; background: #181818; }"
        "QLabel:hover { border-color: #555; background: #202020; }"
    );
    // m_dropHint->setFixedHeight(120); // 让它自适应或者固定高度
    leftLayout->addWidget(m_dropHint, 1); // 占据更多空间

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

    auto* listLabel = new QLabel("提取结果 (自动复制)");
    listLabel->setStyleSheet("color: #888; font-size: 12px; font-weight: bold;");
    rightLayout->addWidget(listLabel);

    m_pathList = new QListWidget();
    m_pathList->setStyleSheet("QListWidget { background-color: #252526; border: 1px solid #333; border-radius: 6px; color: #AAA; padding: 5px; font-size: 12px; } "
                              "QListWidget::item { padding: 4px; border-bottom: 1px solid #2d2d2d; } "
                              "QListWidget::item:selected { background-color: #3E3E42; color: #FFF; }");
    rightLayout->addWidget(m_pathList);

    mainLayout->addWidget(rightPanel);
}

void PathAcquisitionWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_dropHint->setStyleSheet("color: #3a90ff; font-size: 12px; border: 2px dashed #3a90ff; border-radius: 8px; padding: 10px; background-color: rgba(58, 144, 255, 0.05);");
    }
}

void PathAcquisitionWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        m_currentUrls = mimeData->urls(); // 缓存 URL
        processStoredUrls(); // 处理并生成结果
    }
    m_dropHint->setStyleSheet("QLabel { color: #888; font-size: 13px; border: 2px dashed #444; border-radius: 8px; background: #181818; } QLabel:hover { border-color: #555; background: #202020; }");
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
        QApplication::clipboard()->setText(paths.join("\n"));
        QToolTip::showText(QCursor::pos(), "✅ 已提取并复制 " + QString::number(paths.size()) + " 条路径", this);
    } else if (!m_currentUrls.isEmpty()) {
        // 如果处理了 URL 但没有产出（例如空文件夹），也提示一下
         QToolTip::showText(QCursor::pos(), "⚠️ 没有找到文件", this);
    }
    
    m_pathList->scrollToBottom();
}

