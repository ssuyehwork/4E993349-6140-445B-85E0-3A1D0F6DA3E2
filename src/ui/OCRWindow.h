#ifndef OCRWINDOW_H
#define OCRWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class OCRWindow : public QWidget {
    Q_OBJECT
public:
    explicit OCRWindow(QWidget* parent = nullptr);
    ~OCRWindow();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

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
