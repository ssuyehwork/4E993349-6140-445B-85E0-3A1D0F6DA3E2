#ifndef QUICKWINDOW_H
#define QUICKWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QListView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../models/NoteModel.h"

class QuickWindow : public QWidget {
    Q_OBJECT
public:
    explicit QuickWindow(QWidget* parent = nullptr);
    void showCentered();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void initUI();

    QLineEdit* m_searchEdit;
    QListView* m_listView;
    NoteModel* m_model;
};

#endif // QUICKWINDOW_H
