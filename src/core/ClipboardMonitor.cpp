#include "ClipboardMonitor.h"
#include <QMimeData>
#include <QDebug>

ClipboardMonitor& ClipboardMonitor::instance() {
    static ClipboardMonitor inst;
    return inst;
}

ClipboardMonitor::ClipboardMonitor(QObject* parent) : QObject(parent) {
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardMonitor::onClipboardChanged);
    qDebug() << "[ClipboardMonitor] 初始化完成，开始监听...";
}

#include <QApplication>
#include <QImage>
#include <QBuffer>
#include <QUrl>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

void ClipboardMonitor::onClipboardChanged() {
    qDebug() << "[ClipboardMonitor] 剪贴板变动事件触发";
    emit clipboardChanged();

    if (m_skipNext) {
        qDebug() << "[ClipboardMonitor] 跳过本次变动 (m_skipNext == true)";
        m_skipNext = false;
        return;
    }

    // 抓取来源窗口信息 (对标 Ditto)
    QString sourceApp = "未知应用";
    QString sourceTitle = "未知窗口";

    // 1. 过滤本程序自身的复制 (通过进程 ID 判定，比 activeWindow 更可靠)
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);
        
        // 如果来源是自身进程，直接拦截
        if (processId == GetCurrentProcessId()) {
            qDebug() << "[ClipboardMonitor] 拦截自身进程的剪贴板变动";
            return;
        }

        // 既然已经拿到了 HWND 和 PID，直接抓取标题和应用名，避免重复调用系统 API
        wchar_t title[512];
        if (GetWindowTextW(hwnd, title, 512)) {
            sourceTitle = QString::fromWCharArray(title);
        }

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH)) {
                sourceApp = QFileInfo(QString::fromWCharArray(exePath)).baseName();
            }
            CloseHandle(hProcess);
        }
    }
#else
    QWidget* activeWin = QApplication::activeWindow();
    if (activeWin) return;
#endif

    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData) return;

    QString type = "text";
    QString content;
    QByteArray dataBlob;

    if (mimeData->hasText() && !mimeData->text().trimmed().isEmpty()) {
        content = mimeData->text();
        type = "text";
    } else if (mimeData->hasImage()) {
        QImage img = qvariant_cast<QImage>(mimeData->imageData());
        if (!img.isNull()) {
            type = "image";
            content = "[图片]";
            QBuffer buffer(&dataBlob);
            buffer.open(QIODevice::WriteOnly);
            img.save(&buffer, "PNG");
        }
    } else if (mimeData->hasUrls()) {
        QStringList paths;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) paths << url.toLocalFile();
        }
        if (!paths.isEmpty()) {
            type = "file";
            content = paths.join(";");
        }
    } else {
        return;
    }

    // SHA256 去重
    QElapsedTimer hashTimer;
    hashTimer.start();
    QByteArray hashData = dataBlob.isEmpty() ? content.toUtf8() : dataBlob;

    // 如果数据块非常大（大于 5MB），哈希计算可能会卡顿主线程
    if (hashData.size() > 5 * 1024 * 1024) {
        qDebug() << "[ClipboardMonitor] 检测到大数据块 (" << hashData.size() / 1024 / 1024 << "MB)，正在异步处理或限制频率...";
    }

    QString currentHash = QCryptographicHash::hash(hashData, QCryptographicHash::Sha256).toHex();
    
    if (hashTimer.elapsed() > 50) {
        qDebug() << "[Perf] ClipboardMonitor 哈希计算耗时较长:" << hashTimer.elapsed() << "ms";
    }

    if (currentHash == m_lastHash) return;
    m_lastHash = currentHash;

    qDebug() << "[ClipboardMonitor] 捕获新内容 (来自:" << sourceApp << "):" << type << content.left(30);
    emit newContentDetected(content, type, dataBlob, sourceApp, sourceTitle);
}