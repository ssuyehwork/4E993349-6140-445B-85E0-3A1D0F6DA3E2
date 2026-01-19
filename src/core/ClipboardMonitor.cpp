#include "ClipboardMonitor.h"
#include <QMimeData>
#include <QDebug>

ClipboardMonitor& ClipboardMonitor::instance() {
    static ClipboardMonitor inst;
    return inst;
}

ClipboardMonitor::ClipboardMonitor(QObject* parent) : QObject(parent) {
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardMonitor::onClipboardChanged);
}

void ClipboardMonitor::onClipboardChanged() {
    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData->hasText()) {
        QString text = mimeData->text();
        if (text.isEmpty()) return;

        // 计算 SHA256 去重
        QString currentHash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex();
        if (currentHash != m_lastHash) {
            m_lastHash = currentHash;
            emit newContentDetected(text);
        }
    }
}
