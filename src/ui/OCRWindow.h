#ifndef OCRWINDOW_H
#define OCRWINDOW_H

#include "FramelessDialog.h"
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class OCRWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit OCRWindow(QWidget* parent = nullptr);
    ~OCRWindow();


private slots:
    void onPasteAndRecognize();
    void onRecognitionFinished(const QString& text, int contextId);
    void onCopyResult();

private:
    void initUI();
    
    QTextEdit* m_ocrResult = nullptr;
    QPoint m_dragPos;
};

#endif // OCRWINDOW_H
