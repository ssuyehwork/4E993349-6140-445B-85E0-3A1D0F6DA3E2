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

PathAcquisitionWindow::PathAcquisitionWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setAcceptDrops(true);
    resize(450, 500);

    initUI();
}

PathAcquisitionWindow::~PathAcquisitionWindow() {
}

void PathAcquisitionWindow::initUI() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(15, 15, 15, 15);

    auto* container = new QFrame();
    container->setObjectName("PathAcqContainer");
    container->setStyleSheet("#PathAcqContainer { background-color: #1E1E1E; border-radius: 12px; border: 1px solid #333; }");
    rootLayout->addWidget(container);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 120));
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
    iconLabel->setPixmap(IconHelper::getIcon("branch", "#3a90ff").pixmap(20, 20));
    titleLayout->addWidget(iconLabel);

    auto* titleLabel = new QLabel("路径提取 (拖拽投喂)");
    titleLabel->setStyleSheet("color: #3a90ff; font-weight: bold; font-size: 14px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto* btnClose = new QPushButton();
    btnClose->setFixedSize(28, 28);
    btnClose->setIcon(IconHelper::getIcon("close", "#888888"));
    btnClose->setStyleSheet("QPushButton { border: none; background: transparent; border-radius: 4px; } QPushButton:hover { background-color: #e74c3c; }");
    connect(btnClose, &QPushButton::clicked, this, &PathAcquisitionWindow::hide);
    titleLayout->addWidget(btnClose);

    contentLayout->addWidget(titleBar);

    // Drop Area / Hint
    m_dropHint = new QLabel("请将文件或文件夹拖拽至此处");
    m_dropHint->setAlignment(Qt::AlignCenter);
    m_dropHint->setStyleSheet("color: #666; font-size: 13px; border: 2px dashed #444; border-radius: 8px; padding: 20px;");
    m_dropHint->setFixedHeight(80);
    contentLayout->addWidget(m_dropHint);

    // List
    m_pathList = new QListWidget();
    m_pathList->setStyleSheet("QListWidget { background-color: #252526; border: 1px solid #333; border-radius: 6px; color: #CCC; padding: 5px; font-size: 12px; } "
                              "QListWidget::item { padding: 5px; border-bottom: 1px solid #333; } "
                              "QListWidget::item:selected { background-color: #3E3E42; color: #FFF; }");
    contentLayout->addWidget(m_pathList);

    // Buttons
    auto* btnLayout = new QHBoxLayout();

    auto* btnClear = new QPushButton("清空列表");
    btnClear->setStyleSheet("QPushButton { background-color: #333; color: #CCC; border: none; border-radius: 4px; padding: 8px 15px; } "
                            "QPushButton:hover { background-color: #444; }");
    connect(btnClear, &QPushButton::clicked, this, &PathAcquisitionWindow::clearList);
    btnLayout->addWidget(btnClear);

    btnLayout->addStretch();

    auto* btnCopy = new QPushButton("复制所有路径");
    btnCopy->setStyleSheet("QPushButton { background-color: #3a90ff; color: #FFF; border: none; border-radius: 4px; padding: 8px 20px; font-weight: bold; } "
                           "QPushButton:hover { background-color: #5a9fff; }");
    connect(btnCopy, &QPushButton::clicked, this, &PathAcquisitionWindow::copyToClipboard);
    btnLayout->addWidget(btnCopy);

    contentLayout->addLayout(btnLayout);
}

void PathAcquisitionWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_dropHint->setStyleSheet("color: #3a90ff; font-size: 13px; border: 2px dashed #3a90ff; border-radius: 8px; padding: 20px; background-color: rgba(58, 144, 255, 0.05);");
    }
}

void PathAcquisitionWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for (const QUrl& url : urlList) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                m_pathList->addItem(path);
            }
        }
        m_pathList->scrollToBottom();
    }
    m_dropHint->setStyleSheet("color: #666; font-size: 13px; border: 2px dashed #444; border-radius: 8px; padding: 20px;");
}

void PathAcquisitionWindow::copyToClipboard() {
    if (m_pathList->count() == 0) return;

    QStringList paths;
    for (int i = 0; i < m_pathList->count(); ++i) {
        paths << m_pathList->item(i)->text();
    }

    QApplication::clipboard()->setText(paths.join("\n"));
    QToolTip::showText(QCursor::pos(), "✅ 已复制 " + QString::number(paths.size()) + " 条路径到剪贴板");
}

void PathAcquisitionWindow::clearList() {
    m_pathList->clear();
}

void PathAcquisitionWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->pos().y() <= 60) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PathAcquisitionWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void PathAcquisitionWindow::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_dragPos = QPoint();
}
