#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QListView>
#include <QSplitter>
#include <QStandardItemModel>
#include "../models/NoteModel.h"
#include "../models/CategoryModel.h"
#include "Editor.h"
#include "GraphWidget.h"
#include "NoteEditWindow.h"
#include "HeaderBar.h"
#include "MetadataPanel.h"
#include "QuickPreview.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNoteSelected(const QModelIndex& index);
    void onTagSelected(const QModelIndex& index);
    void showContextMenu(const QPoint& pos);

    // 【新增】处理单条笔记添加，不刷新全表
    void onNoteAdded(const QVariantMap& note);

    void refreshData();
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void initUI();

    QTreeView* m_sideBar;
    CategoryModel* m_sideModel;

    QListView* m_noteList;
    NoteModel* m_noteModel;

    HeaderBar* m_header;
    MetadataPanel* m_metaPanel;
    QuickPreview* m_quickPreview;

    Editor* m_editor;
    GraphWidget* m_graphWidget;

    QWidget* m_editorArea;
};

#endif // MAINWINDOW_H