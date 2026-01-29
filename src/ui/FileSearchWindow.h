#ifndef FILESEARCHWINDOW_H
#define FILESEARCHWINDOW_H

#include "FramelessDialog.h"
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include <QPair>
#include <atomic>

/**
 * @brief 扫描线程：实现增量扫描与目录剪枝
 */
class ScannerThread : public QThread {
    Q_OBJECT
public:
    explicit ScannerThread(const QString& folderPath, QObject* parent = nullptr);
    void stop();

signals:
    void fileFound(const QString& name, const QString& path);
    void finished(int count);

protected:
    void run() override;

private:
    QString m_folderPath;
    std::atomic<bool> m_isRunning{true};
};

/**
 * @brief 隐形调整大小手柄
 */
class ResizeHandle : public QWidget {
    Q_OBJECT
public:
    explicit ResizeHandle(QWidget* target, QWidget* parent = nullptr);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
private:
    QWidget* m_target;
    QPoint m_startPos;
    QSize m_startSize;
};

/**
 * @brief 文件查找窗口：1:1 复刻 Python 版逻辑与视觉
 */
class FileSearchWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit FileSearchWindow(QWidget* parent = nullptr);
    ~FileSearchWindow();

private slots:
    void selectFolder();
    void onPathReturnPressed();
    void startScan(const QString& path);
    void onFileFound(const QString& name, const QString& path);
    void onScanFinished(int count);
    void refreshList();
    void openFileLocation(QListWidgetItem* item);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void initUI();
    void setupStyles();

    QLineEdit* m_pathInput;
    QLineEdit* m_searchInput;
    QLineEdit* m_extInput;
    QLabel* m_infoLabel;
    QListWidget* m_fileList;
    
    ResizeHandle* m_resizeHandle;
    ScannerThread* m_scanThread = nullptr;
    
    struct FileData {
        QString name;
        QString path;
    };
    QList<FileData> m_filesData;
};

#endif // FILESEARCHWINDOW_H
