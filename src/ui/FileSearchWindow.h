#ifndef FILESEARCHWINDOW_H
#define FILESEARCHWINDOW_H

#include "FramelessDialog.h"
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QFutureWatcher>

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

class FileSearchWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit FileSearchWindow(QWidget* parent = nullptr);

private slots:
    void browseFolder();
    void startSearch();
    void onSearchFinished();
    void filterResults();
    void locateFile(QListWidgetItem* item);

private:
    void initUI();

    QLineEdit* m_pathEdit;
    QLineEdit* m_searchEdit;
    QLineEdit* m_extEdit;
    QListWidget* m_fileList;
    QLabel* m_infoLabel;
    QPushButton* m_searchBtn;

    QStringList m_allFiles; // 存储所有找到的文件完整路径
    QFutureWatcher<QStringList> m_watcher;
    ResizeHandle* m_resizeHandle;

    void resizeEvent(QResizeEvent* event) override;
};

#endif // FILESEARCHWINDOW_H
