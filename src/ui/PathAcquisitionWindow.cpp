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

PathAcquisitionWindow::PathAcquisitionWindow(QWidget* parent) : FramelessDialog("路径提取", parent) {
    setAcceptDrops(true);
    resize(430, 380);

    initUI();
}

PathAcquisitionWindow::~PathAcquisitionWindow() {
}

void PathAcquisitionWindow::initUI() {
    auto* contentLayout = new QVBoxLayout(m_contentArea);
    contentLayout->setContentsMargins(20, 10, 20, 20);
    contentLayout->setSpacing(10);

    // Drop Area / Hint
    m_dropHint = new QLabel("投喂文件或文件夹到这里\n(自动提取并复制到剪贴板)");
    m_dropHint->setAlignment(Qt::AlignCenter);
    m_dropHint->setStyleSheet("color: #666; font-size: 12px; border: 2px dashed #444; border-radius: 8px; padding: 10px; background: #181818;");
    m_dropHint->setFixedHeight(60);
    contentLayout->addWidget(m_dropHint);

    // List
    m_pathList = new QListWidget();
    m_pathList->setStyleSheet("QListWidget { background-color: #252526; border: 1px solid #333; border-radius: 6px; color: #AAA; padding: 5px; font-size: 11px; } "
                              "QListWidget::item { padding: 4px; border-bottom: 1px solid #2d2d2d; } "
                              "QListWidget::item:selected { background-color: #3E3E42; color: #FFF; }");
    contentLayout->addWidget(m_pathList);

    auto* tipLabel = new QLabel("提示：每次拖拽都会覆盖旧内容并自动复制");
    tipLabel->setStyleSheet("color: #555; font-size: 11px;");
    tipLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(tipLabel);
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
        m_pathList->clear();
        
        QStringList paths;
        QList<QUrl> urlList = mimeData->urls();
        for (const QUrl& url : urlList) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                m_pathList->addItem(path);
                paths << path;
            }
        }
        
        if (!paths.isEmpty()) {
            QApplication::clipboard()->setText(paths.join("\n"));
            QToolTip::showText(QCursor::pos(), "✅ 已提取并复制 " + QString::number(paths.size()) + " 条路径", this);
        }
        
        m_pathList->scrollToBottom();
    }
    m_dropHint->setStyleSheet("color: #666; font-size: 12px; border: 2px dashed #444; border-radius: 8px; padding: 10px; background: #181818;");
}

