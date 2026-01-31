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
#include <QMessageBox>
#include <utility>

OCRWindow::OCRWindow(QWidget* parent) : FramelessDialog("文字识别", parent) {
    setFixedSize(800, 500); // 增加宽度以适应三栏
    setAcceptDrops(true);

    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(100);
    connect(m_processingTimer, &QTimer::timeout, this, &OCRWindow::processNextBatch);

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(500);
    m_updateTimer->setSingleShot(true);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        updateRightDisplay();
        updateProgressLabel();
    });

    initUI();
    onClearResults();

    connect(&OCRManager::instance(), &OCRManager::recognitionFinished, this, &OCRWindow::onRecognitionFinished, Qt::QueuedConnection);
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

    m_progressLabel = new QLabel();
    m_progressLabel->setStyleSheet("color: #888; font-size: 11px;");
    leftLayout->addWidget(m_progressLabel);

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

    m_ocrResult = new QPlainTextEdit();
    m_ocrResult->setReadOnly(true);
    m_ocrResult->setPlaceholderText("选择左侧项目查看识别结果...");
    m_ocrResult->setStyleSheet("QPlainTextEdit { background: #1a1a1a; border: 1px solid #333; border-radius: 6px; color: #eee; font-size: 13px; padding: 10px; }");
    rightLayout->addWidget(m_ocrResult);

    mainHLayout->addWidget(rightPanel);
}

void OCRWindow::onPasteAndRecognize() {
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime) return;

    QList<PendingOCRTask> tasks;

    if (mime->hasImage()) {
        QImage img = qvariant_cast<QImage>(mime->imageData());
        if (!img.isNull()) {
            PendingOCRTask t;
            t.image = img;
            t.path = "来自剪贴板的图片";
            t.isPath = false;
            tasks << t;
        }
    }

    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                PendingOCRTask t;
                t.path = path;
                t.isPath = true;
                tasks << t;
            }
        }
    }

    if (tasks.size() > MAX_BATCH_SIZE) {
        QMessageBox::warning(this, "提示", QString("单次最多只能处理 %1 张图片").arg(MAX_BATCH_SIZE));
        tasks = tasks.mid(0, MAX_BATCH_SIZE);
    }

    if (!tasks.isEmpty()) {
        {
            QMutexLocker locker(&m_itemsMutex);
            for (auto& t : tasks) {
                OCRItem item;
                item.name = t.isPath ? QFileInfo(t.path).fileName() : t.path;
                item.id = ++m_lastUsedId;
                m_items.append(item);

                t.id = item.id;
                m_pendingTasks.enqueue(t);

                auto* listItem = new QListWidgetItem(item.name, m_itemList);
                listItem->setData(Qt::UserRole, item.id);
                listItem->setIcon(IconHelper::getIcon("image", "#888"));
            }
        }
        
        if (!m_processingTimer->isActive()) m_processingTimer->start();
        if (!m_updateTimer->isActive()) m_updateTimer->start();

        m_itemList->setCurrentRow(m_itemList->count() - tasks.size());
    }
}

void OCRWindow::onBrowseAndRecognize() {
    QStringList files = QFileDialog::getOpenFileNames(this, "选择识别图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (files.isEmpty()) return;

    if (files.size() > MAX_BATCH_SIZE) {
        QMessageBox::warning(this, "提示", QString("单次最多只能选择 %1 张图片").arg(MAX_BATCH_SIZE));
        files = files.mid(0, MAX_BATCH_SIZE);
    }

    {
        QMutexLocker locker(&m_itemsMutex);
        for (const QString& file : std::as_const(files)) {
            OCRItem item;
            item.name = QFileInfo(file).fileName();
            item.id = ++m_lastUsedId;
            m_items.append(item);

            PendingOCRTask t;
            t.path = file;
            t.id = item.id;
            t.isPath = true;
            m_pendingTasks.enqueue(t);

            auto* listItem = new QListWidgetItem(item.name, m_itemList);
            listItem->setData(Qt::UserRole, item.id);
            listItem->setIcon(IconHelper::getIcon("image", "#888"));
        }
    }

    if (!m_processingTimer->isActive()) m_processingTimer->start();
    if (!m_updateTimer->isActive()) m_updateTimer->start();

    m_itemList->setCurrentRow(m_itemList->count() - files.size());
}

void OCRWindow::onClearResults() {
    QMutexLocker locker(&m_itemsMutex);
    m_itemList->clear();
    m_items.clear();
    m_ocrResult->clear();
    m_pendingTasks.clear();
    m_activeCount = 0;
    if (m_processingTimer) m_processingTimer->stop();
    if (m_updateTimer) m_updateTimer->stop();
    updateProgressLabel();

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
    const QMimeData* mime = event->mimeData();
    int newItemsCount = 0;
    {
        QMutexLocker locker(&m_itemsMutex);
        if (mime->hasImage()) {
            QImage img = qvariant_cast<QImage>(mime->imageData());
            if (!img.isNull()) {
                OCRItem item;
                item.name = "拖入的图片";
                item.id = ++m_lastUsedId;
                m_items.append(item);

                PendingOCRTask t;
                t.image = img;
                t.id = item.id;
                t.isPath = false;
                m_pendingTasks.enqueue(t);
                newItemsCount++;

                auto* listItem = new QListWidgetItem(item.name, m_itemList);
                listItem->setData(Qt::UserRole, item.id);
                listItem->setIcon(IconHelper::getIcon("image", "#888"));
            }
        }

        if (mime->hasUrls()) {
            for (const QUrl& url : mime->urls()) {
                QString path = url.toLocalFile();
                if (!path.isEmpty()) {
                    OCRItem item;
                    item.name = QFileInfo(path).fileName();
                    item.id = ++m_lastUsedId;
                    m_items.append(item);

                    PendingOCRTask t;
                    t.path = path;
                    t.id = item.id;
                    t.isPath = true;
                    m_pendingTasks.enqueue(t);
                    newItemsCount++;

                    auto* listItem = new QListWidgetItem(item.name, m_itemList);
                    listItem->setData(Qt::UserRole, item.id);
                    listItem->setIcon(IconHelper::getIcon("image", "#888"));
                }
            }
        }
    }

    if (newItemsCount > 0) {
        if (!m_processingTimer->isActive()) m_processingTimer->start();
        if (!m_updateTimer->isActive()) m_updateTimer->start();
        m_itemList->setCurrentRow(m_itemList->count() - newItemsCount);
        event->acceptProposedAction();
    }
}

void OCRWindow::processImages(const QList<QImage>& images) {
    // 此函数在当前重构中不再直接使用，通过 onPaste/onBrowse/drop 直接入队
}

void OCRWindow::processNextBatch() {
    QMutexLocker locker(&m_itemsMutex);
    while (m_activeCount < MAX_CONCURRENT_OCR && !m_pendingTasks.isEmpty()) {
        auto task = m_pendingTasks.dequeue();
        m_activeCount++;
        if (task.isPath) {
            OCRManager::instance().recognizeAsync(task.path, task.id);
        } else {
            OCRManager::instance().recognizeAsync(task.image, task.id);
        }
    }

    if (m_pendingTasks.isEmpty() && m_activeCount == 0) {
        m_processingTimer->stop();
    }

    if (!m_updateTimer->isActive()) {
        m_updateTimer->start();
    }
}

void OCRWindow::onRecognitionFinished(const QString& text, int contextId) {
    {
        QMutexLocker locker(&m_itemsMutex);
        bool found = false;
        for (auto& item : m_items) {
            if (item.id == contextId) {
                item.result = text.trimmed();
                item.isFinished = true;
                found = true;
                break;
            }
        }

        if (!found) return; // 不是本窗口的 ID，直接忽略

        m_activeCount--;
    }

    if (!m_updateTimer->isActive()) {
        m_updateTimer->start();
    }
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
        QStringList parts;
        parts.reserve(m_items.size() * 4);
        int totalLen = 0;
        for (const auto& item : std::as_const(m_items)) {
            parts << QString("【%1】").arg(item.name);
            QString res = item.isFinished ? item.result : "正在识别队列中...";
            parts << res;
            parts << "-----------------------------------";
            parts << "";
            totalLen += res.length();
            if (totalLen > 100000) { // 限制汇总显示长度，防止 UI 卡死
                parts << "\n... (内容过多，仅显示部分。请点击左侧单个项目查看完整结果) ...";
                break;
            }
        }
        m_ocrResult->setPlainText(parts.join("\n"));
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

void OCRWindow::updateProgressLabel() {
    int total = m_items.size();
    int finished = 0;
    for (const auto& item : std::as_const(m_items)) {
        if (item.isFinished) finished++;
    }

    if (m_progressLabel) {
        m_progressLabel->setText(QString("进度: %1/%2\n队列: %3\n处理中: %4")
            .arg(finished).arg(total)
            .arg(m_pendingTasks.size())
            .arg(m_activeCount));
    }
}

void OCRWindow::onCopyResult() {
    QApplication::clipboard()->setText(m_ocrResult->toPlainText());
}

