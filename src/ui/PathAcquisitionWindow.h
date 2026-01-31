#ifndef PATHACQUISITIONWINDOW_H
#define PATHACQUISITIONWINDOW_H

#include "FramelessDialog.h"
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QUrl>
#include <QCheckBox>

class PathAcquisitionWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit PathAcquisitionWindow(QWidget* parent = nullptr);
    ~PathAcquisitionWindow();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void initUI();

    QListWidget* m_pathList;
    QLabel* m_dropHint;
    QCheckBox* m_recursiveCheck;
    QPoint m_dragPos;
    
    QList<QUrl> m_currentUrls;  // 缓存当前拖入的 URL，用于自动刷新
    void processStoredUrls();   // 处理缓存的 URL
    void onShowContextMenu(const QPoint& pos); // 右键菜单
};

#endif // PATHACQUISITIONWINDOW_H
