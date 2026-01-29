#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QObject>
#include <QImage>

class OCRManager : public QObject {
    Q_OBJECT
public:
    static OCRManager& instance();
    void recognizeAsync(const QImage& image, int contextId = -1);

private:
    void recognizeSync(const QImage& image, int contextId);

signals:
    void recognitionFinished(const QString& text, int contextId);

private:
    OCRManager(QObject* parent = nullptr);
};

#endif // OCRMANAGER_H
