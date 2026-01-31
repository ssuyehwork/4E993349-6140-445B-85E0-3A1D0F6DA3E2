#include "OCRWindow.h"
#include "IconHelper.h"
#include "../core/OCRManager.h"
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QFileDialog>
#include <QDropEvent>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>

OCRWindow::OCRWindow(QWidget* parent) : FramelessDialog("文字识别", parent) {
    setFixedSize(800, 500); // 增加宽度以适应三栏
    setAcceptDrops(true);

    initUI();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(300); // 300ms 刷新节流
    connect(m_updateTimer, &QTimer::timeout, this, &OCRWindow::updateRightDisplay);

    onClearResults();

    connect(&OCRManager::instance(), &OCRManager::recognitionFinished, this, &OCRWindow::onRecognitionFinished);
}

OCRWindow::~OCRWindow() {
}

void OCRWindow::initUI() {
    auto* mainHLayout = new QHBoxLayout(m_contentArea);
    mainHLayout->setContentsMargins(10, 10, 10, 10);
    mainHLayout->setSpacing(10);

    // --- 左侧：操作面板 ---
    auto* leftPanel = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto* btnBrowse = new QPushButton(" 本地浏览");
    btnBrowse->setIcon(IconHelper::getIcon("folder", "#ffffff"));
    btnBrowse->setFixedHeight(36);
    btnBrowse->setStyleSheet("QPushButton { background: #4a90e2; color: white; border-radius: 4px; padding: 0 10px; text-align: left; } QPushButton:hover { background: #357abd; }");
    connect(btnBrowse, &QPushButton::clicked, this, &OCRWindow::onBrowseAndRecognize);
    leftLayout->addWidget(btnBrowse);

    auto* btnPaste = new QPushButton(" 粘贴识别");
    btnPaste->setIcon(IconHelper::getIcon("copy", "#ffffff"));
    btnPaste->setFixedHeight(36);
    btnPaste->setStyleSheet("QPushButton { background: #2ecc71; color: white; border-radius: 4px; padding: 0 10px; text-align: left; } QPushButton:hover { background: #27ae60; }");
    connect(btnPaste, &QPushButton::clicked, this, &OCRWindow::onPasteAndRecognize);
    leftLayout->addWidget(btnPaste);

    leftLayout->addSpacing(10);

    auto* btnClear = new QPushButton(" 清空列表");
    btnClear->setIcon(IconHelper::getIcon("trash", "#ddbbbb"));
    btnClear->setFixedHeight(36);
    btnClear->setStyleSheet("QPushButton { background: #333; color: #ccc; border: 1px solid #444; border-radius: 4px; padding: 0 10px; text-align: left; } QPushButton:hover { background: #444; color: #fff; }");
    connect(btnClear, &QPushButton::clicked, this, &OCRWindow::onClearResults);
    leftLayout->addWidget(btnClear);

    leftLayout->addStretch();

    auto* btnCopy = new QPushButton(" 复制文字");
    btnCopy->setIcon(IconHelper::getIcon("copy", "#ffffff"));
    btnCopy->setFixedHeight(36);
    btnCopy->setStyleSheet("QPushButton { background: #3d3d3d; color: #eee; border: 1px solid #4a4a4a; border-radius: 4px; padding: 0 10px; text-align: left; } QPushButton:hover { background: #4d4d4d; }");
    connect(btnCopy, &QPushButton::clicked, this, &OCRWindow::onCopyResult);
    leftLayout->addWidget(btnCopy);

    leftPanel->setFixedWidth(120);
    mainHLayout->addWidget(leftPanel);

    // --- 中间：项目列表 ---
    auto* middlePanel = new QWidget();
    auto* middleLayout = new QVBoxLayout(middlePanel);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(5);

    auto* listLabel = new QLabel("上传的项目");
    listLabel->setStyleSheet("color: #888; font-size: 11px; font-weight: bold;");
    middleLayout->addWidget(listLabel);

    m_itemList = new QListWidget();
    m_itemList->setStyleSheet(
        "QListWidget { background: #222; border: 1px solid #333; border-radius: 4px; color: #ddd; }"
        "QListWidget::item { height: 32px; padding-left: 5px; }"
        "QListWidget::item:selected { background: #357abd; color: white; border-radius: 2px; }"
    );
    connect(m_itemList, &QListWidget::itemSelectionChanged, this, &OCRWindow::onItemSelectionChanged);
    middleLayout->addWidget(m_itemList);

    middlePanel->setFixedWidth(180);
    mainHLayout->addWidget(middlePanel);

    // --- 右侧：结果展示 ---
    auto* rightPanel = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(5);

    auto* resultLabel = new QLabel("识别结果");
    resultLabel->setStyleSheet("color: #888; font-size: 11px; font-weight: bold;");
    rightLayout->addWidget(resultLabel);

    m_ocrResult = new QTextEdit();
    m_ocrResult->setReadOnly(true);
    m_ocrResult->setPlaceholderText("选择左侧项目查看识别结果...");
    m_ocrResult->setStyleSheet("QTextEdit { background: #1a1a1a; border: 1px solid #333; border-radius: 6px; color: #eee; font-size: 13px; padding: 10px; line-height: 1.4; }");
    rightLayout->addWidget(m_ocrResult);

    mainHLayout->addWidget(rightPanel);
}

void OCRWindow::onPasteAndRecognize() {
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime) return;

    QList<QPair<QImage, QString>> imageData;

    if (mime->hasImage()) {
        QImage img = qvariant_cast<QImage>(mime->imageData());
        if (!img.isNull()) imageData << qMakePair(img, "来自剪贴板的图片");
    }

    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QImage img(path);
                if (!img.isNull()) imageData << qMakePair(img, QFileInfo(path).fileName());
            }
        }
    }

    if (!imageData.isEmpty()) {
        int firstNewIdx = 0;
        QList<QImage> imgs;
        {
            QMutexLocker locker(&m_itemsMutex);
            firstNewIdx = m_items.size();
            for (auto& p : imageData) {
                OCRItem item;
                item.image = p.first;
                item.name = p.second;
                item.id = ++m_lastUsedId;
                m_items.append(item);
                imgs << p.first;

                auto* listItem = new QListWidgetItem(item.name, m_itemList);
                listItem->setData(Qt::UserRole, item.id);
                listItem->setIcon(IconHelper::getIcon("image", "#888"));
            }
        }
        processImages(imgs);
        
        // 自动选中第一个新加入的项目
        m_itemList->setCurrentRow(m_itemList->count() - imageData.size());
    }
}

void OCRWindow::onBrowseAndRecognize() {
    QStringList files = QFileDialog::getOpenFileNames(this, "选择识别图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (files.isEmpty()) return;

    QList<QImage> imgs;
    {
        QMutexLocker locker(&m_itemsMutex);
        for (const QString& file : files) {
            QImage img(file);
            if (!img.isNull()) {
                OCRItem item;
                item.image = img;
                item.name = QFileInfo(file).fileName();
                item.id = ++m_lastUsedId;
                m_items.append(item);
                imgs << img;

                auto* listItem = new QListWidgetItem(item.name, m_itemList);
                listItem->setData(Qt::UserRole, item.id);
                listItem->setIcon(IconHelper::getIcon("image", "#888"));
            }
        }
    }

    if (!imgs.isEmpty()) {
        processImages(imgs);
        m_itemList->setCurrentRow(m_itemList->count() - imgs.size());
    }
}

void OCRWindow::onClearResults() {
    QMutexLocker locker(&m_itemsMutex);
    m_itemList->clear();
    m_items.clear();
    m_ocrResult->clear();
    // 使用较大的起始 ID 避免与数据库笔记 ID (通常较小) 冲突
    m_lastUsedId = 1000000;
    
    auto* summaryItem = new QListWidgetItem("--- 全部结果汇总 ---", m_itemList);
    summaryItem->setData(Qt::UserRole, 0); // 0 代表汇总
    summaryItem->setIcon(IconHelper::getIcon("file_managed", "#1abc9c"));
    m_itemList->setCurrentItem(summaryItem);
}

void OCRWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasImage() || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void OCRWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    QList<QImage> imgsToProcess;
    {
        QMutexLocker locker(&m_itemsMutex);
        if (mime->hasImage()) {
            QImage img = qvariant_cast<QImage>(mime->imageData());
            if (!img.isNull()) {
                OCRItem item;
                item.image = img;
                item.name = "拖入的图片";
                item.id = ++m_lastUsedId;
                m_items.append(item);
                imgsToProcess << img;

                auto* listItem = new QListWidgetItem(item.name, m_itemList);
                listItem->setData(Qt::UserRole, item.id);
                listItem->setIcon(IconHelper::getIcon("image", "#888"));
            }
        }

        if (mime->hasUrls()) {
            for (const QUrl& url : mime->urls()) {
                QString path = url.toLocalFile();
                if (!path.isEmpty()) {
                    QImage img(path);
                    if (!img.isNull()) {
                        OCRItem item;
                        item.image = img;
                        item.name = QFileInfo(path).fileName();
                        item.id = ++m_lastUsedId;
                        m_items.append(item);
                        imgsToProcess << img;

                        auto* listItem = new QListWidgetItem(item.name, m_itemList);
                        listItem->setData(Qt::UserRole, item.id);
                        listItem->setIcon(IconHelper::getIcon("image", "#888"));
                    }
                }
            }
        }
    }

    if (!imgsToProcess.isEmpty()) {
        processImages(imgsToProcess);
        m_itemList->setCurrentRow(m_itemList->count() - imgsToProcess.size());
        event->acceptProposedAction();
    }
}

void OCRWindow::processImages(const QList<QImage>& images) {
    QMutexLocker locker(&m_itemsMutex);
    // 基于刚才添加的 item 顺序进行识别
    int startIdx = m_items.size() - images.size();
    for (int i = 0; i < images.size(); ++i) {
        OCRManager::instance().recognizeAsync(images[i], m_items[startIdx + i].id);
    }
}

void OCRWindow::onRecognitionFinished(const QString& text, int contextId) {
    {
        QMutexLocker locker(&m_itemsMutex);
        for (auto& item : m_items) {
            if (item.id == contextId) {
                item.result = text.trimmed();
                item.isFinished = true;
                break;
            }
        }
    }
    // 异步完成时使用节流刷新，防止批量完成时 UI 线程被 setPlainText 淹没
    m_updateTimer->start();
}

void OCRWindow::onItemSelectionChanged() {
    updateRightDisplay();
}

void OCRWindow::updateRightDisplay() {
    auto* current = m_itemList->currentItem();
    if (!current) return;

    int id = current->data(Qt::UserRole).toInt();

    QMutexLocker locker(&m_itemsMutex);
    if (id == 0) {
        // 展示全部
        QString allText;
        for (const auto& item : m_items) {
            if (!allText.isEmpty()) allText += "\n\n";
            allText += QString("【%1】\n").arg(item.name);
            allText += item.isFinished ? item.result : "正在识别管理中...";
            allText += "\n-----------------------------------";
        }
        m_ocrResult->setPlainText(allText);
    } else {
        // 展示单个
        for (const auto& item : m_items) {
            if (item.id == id) {
                m_ocrResult->setPlainText(item.isFinished ? item.result : "正在识别中，请稍候...");
                break;
            }
        }
    }
}

void OCRWindow::onCopyResult() {
    QApplication::clipboard()->setText(m_ocrResult->toPlainText());
}

