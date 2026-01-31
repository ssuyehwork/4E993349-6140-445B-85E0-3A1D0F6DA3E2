#ifndef OCRWINDOW_H
#define OCRWINDOW_H

#include "FramelessDialog.h"
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QListWidget>
#include <QMutex>

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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void initUI();
    void processImages(const QList<QImage>& images);
    void updateRightDisplay();

    struct OCRItem {
        QImage image;
        QString name;
        QString result;
        bool isFinished = false;
        int id = -1;
    };

    QListWidget* m_itemList = nullptr;
    QTextEdit* m_ocrResult = nullptr;
    
    QList<OCRItem> m_items;
    int m_lastUsedId = 0;
    mutable QMutex m_itemsMutex;
};

#endif // OCRWINDOW_H
