#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QObject>
#include <QImage>
#include <QtConcurrent>

class OCRManager : public QObject {
    Q_OBJECT
public:
    static OCRManager& instance();

    void recognizeAsync(const QImage& image) {
        (void)QtConcurrent::run([this, image]() {
            // 这里通常调用 Windows.Media.Ocr 或 Tesseract
            // 示例逻辑：
            QString result = "识别出的文字示例"; 
            emit recognitionFinished(result);
        });
    }

signals:
    void recognitionFinished(const QString& text);

private:
    OCRManager(QObject* parent = nullptr) : QObject(parent) {}
};

#endif // OCRMANAGER_H