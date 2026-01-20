#ifndef QUICKWINDOW_H
#define QUICKWINDOW_H

#include <QWidget>
#include "SearchLineEdit.h"
#include <QListView>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../models/NoteModel.h"
#include "../models/CategoryModel.h"
#include "QuickPreview.h"

class QuickWindow : public QWidget {
    Q_OBJECT
public:
    explicit QuickWindow(QWidget* parent = nullptr);
    void showCentered();

public slots:
    void refreshData();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void initUI();

    SearchLineEdit* m_searchEdit;
    QListView* m_listView;
    NoteModel* m_model;
    QuickPreview* m_quickPreview;

    QTreeView* m_sideBar;
    CategoryModel* m_sideModel;
};

#endif // QUICKWINDOW_H