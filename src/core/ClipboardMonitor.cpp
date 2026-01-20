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

void ClipboardMonitor::onClipboardChanged() {
    // 1. 过滤本程序自身的复制
    QWidget* activeWin = QApplication::activeWindow();
    if (activeWin) return;

    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData) return;

    QString type = "text";
    QString content;
    QByteArray dataBlob;

    if (mimeData->hasImage()) {
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
    } else if (mimeData->hasText()) {
        content = mimeData->text();
        if (content.trimmed().isEmpty()) return;
    } else {
        return;
    }

    // SHA256 去重
    QByteArray hashData = dataBlob.isEmpty() ? content.toUtf8() : dataBlob;
    QString currentHash = QCryptographicHash::hash(hashData, QCryptographicHash::Sha256).toHex();

    if (currentHash == m_lastHash) return;
    m_lastHash = currentHash;

    qDebug() << "[ClipboardMonitor] 捕获新内容:" << type << content.left(30);
    emit newContentDetected(content, type, dataBlob);
}