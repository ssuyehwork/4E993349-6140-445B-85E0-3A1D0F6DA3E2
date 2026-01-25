#include "OCRWindow.h"
#include "IconHelper.h"
#include "../core/OCRManager.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMouseEvent>

OCRWindow::OCRWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle("文字识别工具");
    setFixedSize(400, 500);

    initUI();

    connect(&OCRManager::instance(), &OCRManager::recognitionFinished, this, &OCRWindow::onRecognitionFinished);
}

OCRWindow::~OCRWindow() {
}

void OCRWindow::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* container = new QWidget();
    container->setObjectName("OCRContainer");
    container->setStyleSheet("#OCRContainer { background-color: #1E1E1E; border-radius: 12px; border: 1px solid #333; }");
    mainLayout->addWidget(container);

    auto* contentLayout = new QVBoxLayout(container);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(12);

    // Title Bar (within content layout for simplicity or use same pattern as others)
    auto* titleHeader = new QHBoxLayout();
    auto* titleLabel = new QLabel("📝 图片文字识别 (OCR)");
    titleLabel->setStyleSheet("font-weight: bold; color: #4a90e2; font-size: 14px;");
    titleHeader->addWidget(titleLabel);
    titleHeader->addStretch();
    
    auto* closeBtn = new QPushButton();
    closeBtn->setIcon(IconHelper::getIcon("close", "#888888"));
    closeBtn->setFixedSize(30, 30);
    closeBtn->setIconSize(QSize(20, 20));
    closeBtn->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background: #c42b1c; border-radius: 5px; }");
    connect(closeBtn, &QPushButton::clicked, this, &OCRWindow::hide);
    titleHeader->addWidget(closeBtn);
    contentLayout->addLayout(titleHeader);

    auto* btnPaste = new QPushButton(" 从剪贴板识别图片");
    btnPaste->setIcon(IconHelper::getIcon("copy", "#ffffff"));
    btnPaste->setIconSize(QSize(20, 20));
    btnPaste->setFixedHeight(40);
    btnPaste->setStyleSheet("QPushButton { background: #4a90e2; color: white; font-weight: bold; border-radius: 6px; } QPushButton:hover { background: #357abd; }");
    connect(btnPaste, &QPushButton::clicked, this, &OCRWindow::onPasteAndRecognize);
    contentLayout->addWidget(btnPaste);

    m_ocrResult = new QTextEdit();
    m_ocrResult->setPlaceholderText("识别结果将显示在这里...");
    m_ocrResult->setStyleSheet("QTextEdit { background: #1a1a1a; border: 1px solid #444; border-radius: 6px; color: #eee; font-size: 14px; padding: 10px; }");
    contentLayout->addWidget(m_ocrResult);

    auto* btnCopy = new QPushButton(" 复制结果");
    btnCopy->setIcon(IconHelper::getIcon("copy", "#ffffff"));
    btnCopy->setIconSize(QSize(18, 18));
    btnCopy->setFixedHeight(32);
    btnCopy->setStyleSheet("QPushButton { background: #333; border: 1px solid #444; border-radius: 4px; color: #ddd; } QPushButton:hover { background: #3e3e42; }");
    connect(btnCopy, &QPushButton::clicked, this, &OCRWindow::onCopyResult);
    contentLayout->addWidget(btnCopy);

    contentLayout->addStretch();
}

void OCRWindow::onPasteAndRecognize() {
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (mime && mime->hasImage()) {
        QImage img = qvariant_cast<QImage>(mime->imageData());
        if (!img.isNull()) {
            m_ocrResult->setPlainText("正在识别中，请稍候...");
            OCRManager::instance().recognizeAsync(img, 9999);
        } else {
            m_ocrResult->setPlainText("获取图片数据失败！");
        }
    } else {
        m_ocrResult->setPlainText("剪贴板中没有图片！");
    }
}

void OCRWindow::onRecognitionFinished(const QString& text, int contextId) {
    if (contextId == 9999 && m_ocrResult) {
        m_ocrResult->setPlainText(text);
    }
}

void OCRWindow::onCopyResult() {
    QApplication::clipboard()->setText(m_ocrResult->toPlainText());
}

void OCRWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->pos().y() <= 50) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void OCRWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void OCRWindow::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_dragPos = QPoint();
}
