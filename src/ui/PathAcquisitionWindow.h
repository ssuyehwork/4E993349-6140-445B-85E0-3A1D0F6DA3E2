#ifndef PATHACQUISITIONWINDOW_H
#define PATHACQUISITIONWINDOW_H

#include "FramelessDialog.h"
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

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
    QPoint m_dragPos;
};

#endif // PATHACQUISITIONWINDOW_H
