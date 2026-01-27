#ifndef TAGMANAGERWINDOW_H
#define TAGMANAGERWINDOW_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QPoint>

class TagManagerWindow : public QWidget {
    Q_OBJECT
public:
    explicit TagManagerWindow(QWidget* parent = nullptr);
    ~TagManagerWindow();

public slots:
    void refreshData();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void initUI();
    void handleRename();
    void handleDelete();
    void handleSearch(const QString& text);

    QTableWidget* m_tagTable;
    QLineEdit* m_searchEdit;
    QPoint m_dragPos;
};

#endif // TAGMANAGERWINDOW_H
