#ifndef FILESEARCHWINDOW_H
#define FILESEARCHWINDOW_H

#include "FramelessDialog.h"
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFutureWatcher>

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
    QComboBox* m_extFilter;
    QListWidget* m_fileList;
    QPushButton* m_searchBtn;

    QStringList m_allFiles; // 存储所有找到的文件完整路径
    QFutureWatcher<QStringList> m_watcher;
};

#endif // FILESEARCHWINDOW_H
