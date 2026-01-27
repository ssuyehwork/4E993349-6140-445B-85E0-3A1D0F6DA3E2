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
    resize(400, 350);

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

    auto* titleLabel = new QLabel("路径提取 (拖拽即复制)");
    titleLabel->setStyleSheet("color: #3a90ff; font-weight: bold; font-size: 13px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto* btnClose = new QPushButton();
    btnClose->setFixedSize(24, 24);
    btnClose->setIcon(IconHelper::getIcon("close", "#888888"));
    btnClose->setStyleSheet("QPushButton { border: none; background: transparent; border-radius: 4px; } QPushButton:hover { background-color: #e74c3c; }");
    connect(btnClose, &QPushButton::clicked, this, &PathAcquisitionWindow::hide);
    titleLayout->addWidget(btnClose);

    contentLayout->addWidget(titleBar);

    // Drop Area / Hint
    m_dropHint = new QLabel("投喂文件或文件夹到这里\n(将自动提取并存入剪贴板)");
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
