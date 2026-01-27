#ifndef FILESTORAGEWINDOW_H
#define FILESTORAGEWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPoint>

class FileStorageWindow : public QWidget {
    Q_OBJECT
public:
    explicit FileStorageWindow(QWidget* parent = nullptr);
    void setCurrentCategory(int catId) { m_categoryId = catId; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void initUI();
    void processStorage(const QStringList& paths);
    void storeFile(const QString& path);
    void storeFolder(const QString& path);

    QLabel* m_dropHint;
    QListWidget* m_statusList;
    QPoint m_dragPos;
    int m_categoryId = -1;
};

#endif // FILESTORAGEWINDOW_H
