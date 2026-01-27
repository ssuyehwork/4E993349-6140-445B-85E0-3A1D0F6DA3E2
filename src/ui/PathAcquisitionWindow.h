#ifndef PATHACQUISITIONWINDOW_H
#define PATHACQUISITIONWINDOW_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

class PathAcquisitionWindow : public QWidget {
    Q_OBJECT
public:
    explicit PathAcquisitionWindow(QWidget* parent = nullptr);
    ~PathAcquisitionWindow();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void initUI();

    QListWidget* m_pathList;
    QLabel* m_dropHint;
    QPoint m_dragPos;
};

#endif // PATHACQUISITIONWINDOW_H
