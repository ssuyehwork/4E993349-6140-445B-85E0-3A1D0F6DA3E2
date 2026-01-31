#ifndef OCRWINDOW_H
#define OCRWINDOW_H

#include "FramelessDialog.h"
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QListWidget>
#include <QMutex>
#include <QQueue>
#include <QTimer>

class OCRWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit OCRWindow(QWidget* parent = nullptr);
    ~OCRWindow();

private slots:
    void onPasteAndRecognize();
    void onBrowseAndRecognize();
    void onClearResults();
    void onRecognitionFinished(const QString& text, int contextId);
    void onCopyResult();
    void onItemSelectionChanged();
    void processNextBatch();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void initUI();
    void processImages(const QList<QImage>& images);
    void updateRightDisplay();
    void updateProgressLabel();

    struct OCRItem {
        QString name;
        QString result;
        bool isFinished = false;
        int id = -1;
    };

    struct PendingOCRTask {
        QImage image;
        QString path;
        int id;
        bool isPath = false;
    };

    static const int MAX_BATCH_SIZE = 10;
    static const int MAX_CONCURRENT_OCR = 3;

    QListWidget* m_itemList = nullptr;
    QPlainTextEdit* m_ocrResult = nullptr;
    QLabel* m_progressLabel = nullptr;
    
    QList<OCRItem> m_items;
    QQueue<PendingOCRTask> m_pendingTasks;
    QTimer* m_processingTimer = nullptr;
    QTimer* m_updateTimer = nullptr;
    int m_activeCount = 0;
    int m_lastUsedId = 1000000;
    mutable QMutex m_itemsMutex;
};

#endif // OCRWINDOW_H
