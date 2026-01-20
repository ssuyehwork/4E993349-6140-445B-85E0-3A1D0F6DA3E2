#ifndef HEADERBAR_H
#define HEADERBAR_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
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
    void previewToggled(bool checked);

private:
    void setupSearchHistory();

    SearchLineEdit* m_searchEdit;
    QLabel* m_pageLabel;
    int m_currentPage = 1;
    int m_totalPages = 1;
};

#endif // HEADERBAR_H
