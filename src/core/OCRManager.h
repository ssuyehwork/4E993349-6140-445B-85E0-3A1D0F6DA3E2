#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QObject>
#include <QImage>
#include <QtConcurrent>
#include <QThread>

class OCRManager : public QObject {
    Q_OBJECT
public:
    static OCRManager& instance();

    void recognizeAsync(const QImage& image, int contextId = -1) {
        (void)QtConcurrent::run([this, image, contextId]() {
            // 这里通常调用 Windows.Media.Ocr 或 Tesseract
            // 示例逻辑：
            QThread::msleep(1000); // 模拟耗时操作
            QString result = "识别出的文字示例 (Context ID: " + QString::number(contextId) + ")";
            emit recognitionFinished(result, contextId);
        });
    }

signals:
    void recognitionFinished(const QString& text, int contextId);

private:
    OCRManager(QObject* parent = nullptr) : QObject(parent) {}
};

#endif // OCRMANAGER_H