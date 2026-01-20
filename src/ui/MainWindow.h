#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QTextEdit>
#include <QSplitter>
#include "../models/NoteModel.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void initUI();

    QTreeView* m_sideBar;
    QListView* m_noteList;
    QWidget* m_editorArea;
    QWidget* m_metaPanel;
    NoteModel* m_model;
};

#endif // MAINWINDOW_H
