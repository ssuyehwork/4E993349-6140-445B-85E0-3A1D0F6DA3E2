#include "OCRWindow.h"
#include "IconHelper.h"
#include "../core/OCRManager.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

OCRWindow::OCRWindow(QWidget* parent) : FramelessDialog("文字识别", parent) {
    setFixedSize(430, 530);

    initUI();

    connect(&OCRManager::instance(), &OCRManager::recognitionFinished, this, &OCRWindow::onRecognitionFinished);
}

OCRWindow::~OCRWindow() {
}

void OCRWindow::initUI() {
    auto* contentLayout = new QVBoxLayout(m_contentArea);
    contentLayout->setContentsMargins(20, 10, 20, 20);
    contentLayout->setSpacing(12);

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
    if (!mime) return;

    QImage img;
    // 1. 优先尝试直接获取图像数据
    if (mime->hasImage()) {
        img = qvariant_cast<QImage>(mime->imageData());
    } 
    // 2. 尝试从文件路径加载
    else if (mime->hasUrls()) {
        QString path = mime->urls().first().toLocalFile();
        if (!path.isEmpty()) img.load(path);
    } 
    // 3. 兜底检测常见图片格式
    else if (mime->hasFormat("image/png") || mime->hasFormat("image/jpeg") || mime->hasFormat("image/bmp")) {
        if (!img.loadFromData(mime->data("image/png"), "PNG")) {
            if (!img.loadFromData(mime->data("image/jpeg"), "JPG")) {
                img.loadFromData(mime->data("image/bmp"), "BMP");
            }
        }
    }

    if (!img.isNull()) {
        m_ocrResult->setPlainText("正在识别中，请稍候...");
        OCRManager::instance().recognizeAsync(img, 9999);
    } else {
        m_ocrResult->setPlainText("剪贴板中没有发现可用图片！");
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

