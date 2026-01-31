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
#include <QThread>

OCRWindow::OCRWindow(QWidget* parent) : FramelessDialog("文字识别", parent) {
    setFixedSize(800, 500);
    setAcceptDrops(true);

    initUI();
    onClearResults();
    
    // 初始化处理定时器
    m_processTimer = new QTimer(this);
    m_processTimer->setSingleShot(true);
    connect(m_processTimer, &QTimer::timeout, this, &OCRWindow::processNextImage);

    // 初始化结果展示节流定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(300);
    connect(m_updateTimer, &QTimer::timeout, this, [this](){
        updateRightDisplay();
    });
    
    qDebug() << "[OCR] OCRWindow 初始化完成，使用顺序处理模式";
    
    connect(&OCRManager::instance(), &OCRManager::recognitionFinished, 
            this, &OCRWindow::onRecognitionFinished, Qt::QueuedConnection);
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

    
    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setTextVisible(true);
    m_progressBar->setAlignment(Qt::AlignCenter);
    m_progressBar->setStyleSheet("QProgressBar { border: none; background: #333; border-radius: 4px; height: 6px; text-align: center; color: transparent; } QProgressBar::chunk { background-color: #4a90e2; border-radius: 4px; }");
    m_progressBar->hide();
    rightLayout->addWidget(m_progressBar);

    m_ocrResult = new QTextEdit();
    m_ocrResult->setReadOnly(true);
    m_ocrResult->setPlaceholderText("选择左侧项目查看识别结果...");
    m_ocrResult->setStyleSheet("QTextEdit { background: #1a1a1a; border: 1px solid #333; border-radius: 6px; color: #eee; font-size: 13px; padding: 10px; line-height: 1.4; }");
    rightLayout->addWidget(m_ocrResult);

    mainHLayout->addWidget(rightPanel);
}

void OCRWindow::onPasteAndRecognize() {
    qDebug() << "[OCR] 粘贴识别: 开始";
    
    // 自动清空之前的数据
    onClearResults();
    
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();

    QList<QPair<QImage, QString>> imageData;

    if (!mimeData) return;

    if (mimeData->hasImage()) {
        QImage img = qvariant_cast<QImage>(mimeData->imageData());
        if (!img.isNull()) {
            imageData.append({img, "粘贴的图片"});
        }
    } else if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QImage img(path);
                if (!img.isNull()) {
                    imageData.append({img, QFileInfo(path).fileName()});
                }
            }
        }
    }

    if (!imageData.isEmpty()) {
        // 限制最多 10 张图片
        if (imageData.size() > 10) {
            qDebug() << "[OCR] 粘贴识别: 图片数量超过限制，仅处理前 10 张";
            imageData = imageData.mid(0, 10);
        }
        
        qDebug() << "[OCR] 粘贴识别: 开始处理" << imageData.size() << "张图片";
        QList<QImage> imgs;
        for (auto& p : imageData) {
            OCRItem item;
            item.image = p.first;
            item.name = p.second;
            item.id = ++m_lastUsedId;
            item.sessionVersion = m_sessionVersion;
            m_items.append(item);
            imgs << p.first;
            qDebug() << "[OCR] 添加任务 ID:" << item.id << "名称:" << item.name;

            auto* listItem = new QListWidgetItem(item.name, m_itemList);
            listItem->setData(Qt::UserRole, item.id);
            listItem->setIcon(IconHelper::getIcon("image", "#888"));
        }
        processImages(imgs);
        
        // 自动选中第一个新加入的项目
        m_itemList->setCurrentRow(m_itemList->count() - imageData.size());
    }
}

void OCRWindow::onBrowseAndRecognize() {
    QStringList files = QFileDialog::getOpenFileNames(this, "选择识别图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (files.isEmpty()) return;

    // 自动清空之前的数据
    onClearResults();

    qDebug() << "[OCR] 浏览识别: 选择了" << files.size() << "个文件";
    
    // 限制最多 10 张图片
    if (files.size() > 10) {
        qDebug() << "[OCR] 浏览识别: 文件数量超过限制，仅处理前 10 个";
        files = files.mid(0, 10);
    }
    
    QList<QImage> imgs;
    for (const QString& file : std::as_const(files)) {
        QImage img(file);
        if (!img.isNull()) {
            OCRItem item;
            item.image = img;
            item.name = QFileInfo(file).fileName();
            item.id = ++m_lastUsedId;
            item.sessionVersion = m_sessionVersion;
            m_items.append(item);
            imgs << img;
            qDebug() << "[OCR] 添加任务 ID:" << item.id << "文件:" << file;

            auto* listItem = new QListWidgetItem(item.name, m_itemList);
            listItem->setData(Qt::UserRole, item.id);
            listItem->setIcon(IconHelper::getIcon("image", "#888"));
        }
    }

    if (!imgs.isEmpty()) {
        processImages(imgs);
        m_itemList->setCurrentRow(m_itemList->count() - imgs.size());
    }
}

void OCRWindow::onClearResults() {
    qDebug() << "[OCR] 清空结果";
    
    m_sessionVersion++;
    m_isProcessing = false;
    m_processingQueue.clear();
    
    m_itemList->blockSignals(true);
    m_itemList->clear();
    m_itemList->blockSignals(false);

    m_items.clear();
    m_ocrResult->clear();
    
    // 重置进度条
    if (m_progressBar) {
        m_progressBar->hide();
        m_progressBar->setValue(0);
    }
    
    // 不重置 m_lastUsedId，防止异步回调 ID 冲突
    
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
    qDebug() << "[OCR] 拖入识别: 开始";
    
    // 自动清空之前的数据
    onClearResults();
    
    const QMimeData* mime = event->mimeData();
    QList<QImage> imgsToProcess;

    // 限制最多 10 张图片
    int imageCount = 0;
    const int MAX_IMAGES = 10;

    if (mime->hasImage()) {
        QImage img = qvariant_cast<QImage>(mime->imageData());
        if (!img.isNull() && imageCount < MAX_IMAGES) {
            OCRItem item;
            item.image = img;
            item.name = "拖入的图片";
            item.id = ++m_lastUsedId;
            item.sessionVersion = m_sessionVersion;
            m_items.append(item);
            imgsToProcess << img;
            imageCount++;

            auto* listItem = new QListWidgetItem(item.name, m_itemList);
            listItem->setData(Qt::UserRole, item.id);
            listItem->setIcon(IconHelper::getIcon("image", "#888"));
        }
    }

    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (imageCount >= MAX_IMAGES) {
                qDebug() << "[OCR] 拖入识别: 已达到最大图片数量限制 (10 张)";
                break;
            }
            
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QImage img(path);
                if (!img.isNull()) {
                    OCRItem item;
                    item.image = img;
                    item.name = QFileInfo(path).fileName();
                    item.id = ++m_lastUsedId;
                    item.sessionVersion = m_sessionVersion;
                    m_items.append(item);
                    imgsToProcess << img;
                    imageCount++;

                    auto* listItem = new QListWidgetItem(item.name, m_itemList);
                    listItem->setData(Qt::UserRole, item.id);
                    listItem->setIcon(IconHelper::getIcon("image", "#888"));
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
    if (images.isEmpty()) return;
    
    qDebug() << "[OCR] processImages: 添加" << images.size() << "张图片到任务列表";

    // 将图片 ID 加入处理队列
    int startIdx = m_items.size() - images.size();
    for (int i = 0; i < images.size(); ++i) {
        m_processingQueue.enqueue(m_items[startIdx + i].id);
    }
    
    // 初始化并显示进度条
    if (m_progressBar) {
        m_progressBar->setMaximum(m_items.size());
        m_progressBar->setValue(0);
        m_progressBar->show();
    }
    
    // 如果当前没在处理，则启动串行处理流程
    if (!m_isProcessing) {
        m_isProcessing = true;
        processNextImage();
    }
}

void OCRWindow::processNextImage() {
    // 队列为空，结束处理
    if (m_processingQueue.isEmpty()) {
        qDebug() << "[OCR] 所有图片处理完毕";
        m_isProcessing = false;
        if (m_updateTimer) m_updateTimer->start();
        return;
    }

    int taskId = m_processingQueue.dequeue();
    
    // 查找任务项并验证会话版本
    OCRItem* targetItem = nullptr;
    for (auto& item : m_items) {
        if (item.id == taskId && item.sessionVersion == m_sessionVersion) {
            targetItem = &item;
            break;
        }
    }

    if (!targetItem) {
        // 如果任务失效（比如已被清空），直接跳到下一个
        QTimer::singleShot(0, this, &OCRWindow::processNextImage);
        return;
    }

    qDebug() << "[OCR] 启动异步任务 ID:" << taskId;
    OCRManager::instance().recognizeAsync(targetItem->image, taskId);
}


void OCRWindow::onItemSelectionChanged() {
    updateRightDisplay();
}

void OCRWindow::onRecognitionFinished(const QString& text, int contextId) {
    // 异步回调核心防护：如果当前已退出处理状态或 ID 无效，立即返回
    // 同时过滤掉属于数据库笔记的任务 ID
    if (contextId < TASK_ID_START) return;
    if (!m_isProcessing && m_processingQueue.isEmpty()) return;

    qDebug() << "[OCR] 收到回调 ID:" << contextId << "线程:" << QThread::currentThread();
    
    for (auto& item : m_items) {
        if (item.id == contextId) {
            // 严格校验会话版本，防止串号或过期数据写入
            if (item.sessionVersion == m_sessionVersion) {
                item.result = text.trimmed();
                item.isFinished = true;
                qDebug() << "[OCR] 任务已更新:" << item.name;
            }
            break;
        }
    }
    
    // 无论当前任务是否有效（可能在等待期间被清空），只要处于处理流程中，就必须驱动队列继续
    if (m_isProcessing) {
        // 统计总体完成进度 (仅针对当前 session)
        int finished = 0;
        int currentSessionTotal = 0;
        for (const auto& item : std::as_const(m_items)) {
            if (item.sessionVersion == m_sessionVersion) {
                currentSessionTotal++;
                if (item.isFinished) finished++;
            }
        }

        if (m_progressBar) {
            m_progressBar->setMaximum(currentSessionTotal);
            m_progressBar->setValue(finished);
            if (finished >= currentSessionTotal && currentSessionTotal > 0) {
                QTimer::singleShot(1500, m_progressBar, &QProgressBar::hide);
            }
        }

        // 触发 UI 节流刷新
        if (m_updateTimer) m_updateTimer->start();

        // 驱动下一个任务 (延迟 10ms 释放当前 CPU 轮转)
        QTimer::singleShot(10, this, &OCRWindow::processNextImage);
    }
}

void OCRWindow::updateRightDisplay() {
    if (!m_itemList || !m_ocrResult) return;
    
    auto* current = m_itemList->currentItem();
    if (!current) return;

    int id = current->data(Qt::UserRole).toInt();

    if (id == 0) {
        // 展示全部：使用 QStringList 优化大文本拼接性能
        QStringList allTextList;
        allTextList.reserve(m_items.size() * 4);

        for (const auto& item : std::as_const(m_items)) {
            if (!allTextList.isEmpty()) allTextList << "\n\n";
            allTextList << QString("【%1】\n").arg(item.name);
            allTextList << (item.isFinished ? item.result : "正在识别中...");
            allTextList << "\n-----------------------------------";
        }
        m_ocrResult->setPlainText(allTextList.join(""));
    } else {
        // 展示单个
        for (const auto& item : std::as_const(m_items)) {
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

