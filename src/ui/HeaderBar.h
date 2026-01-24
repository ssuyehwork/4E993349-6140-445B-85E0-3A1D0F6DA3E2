#ifndef HEADERBAR_H
#define HEADERBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QLineEdit>
#include "SearchLineEdit.h"

class HeaderBar : public QWidget {
    Q_OBJECT
public:
    explicit HeaderBar(QWidget* parent = nullptr);

signals:
    void searchChanged(const QString& text);
    void newNoteRequested();
    void toggleSidebar();
    void pageChanged(int page);
    void toolboxRequested();
    void metadataToggled(bool checked);
    void refreshRequested();
    void filterRequested();
    void windowClose();
    void windowMinimize();
    void windowMaximize();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

public:
    void updatePagination(int current, int total);
    void setFilterActive(bool active);
    void setMetadataActive(bool active);

private:
    SearchLineEdit* m_searchEdit;
    QLineEdit* m_pageInput;
    QLabel* m_totalPageLabel;
    QPushButton* m_btnFilter;
    QPushButton* m_btnMeta;

    int m_currentPage = 1;
    int m_totalPages = 1;
    QPoint m_dragPos;
};

#endif // HEADERBAR_H
