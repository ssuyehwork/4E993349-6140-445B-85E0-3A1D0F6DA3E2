#ifndef QUICKWINDOW_H
#define QUICKWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QListView>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../models/NoteModel.h"
#include "../models/CategoryModel.h"

class QuickWindow : public QWidget {
    Q_OBJECT
public:
    explicit QuickWindow(QWidget* parent = nullptr);
    void showCentered();

public slots:
    void refreshData();

protected:
    bool event(QEvent* event) override;

private:
    void initUI();
    
    QLineEdit* m_searchEdit;
    QListView* m_listView;
    NoteModel* m_model;
    
    QTreeView* m_sideBar;
    CategoryModel* m_sideModel;
};

#endif // QUICKWINDOW_H