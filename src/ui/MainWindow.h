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
#include "NoteEditWindow.h"
#include "HeaderBar.h"
#include "MetadataPanel.h"
#include "QuickPreview.h"
#include "DropTreeView.h"
#include "FilterPanel.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

signals:
    void toolboxRequested();

private slots:
    void onNoteSelected(const QModelIndex& index);
    void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onTagSelected(const QModelIndex& index);
    void showContextMenu(const QPoint& pos);
    
    // 【新增】处理单条笔记添加，不刷新全表
    void onNoteAdded(const QVariantMap& note);
    
    void refreshData();
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void initUI();
    
    DropTreeView* m_sideBar;
    CategoryModel* m_sideModel;
    
    QListView* m_noteList;
    NoteModel* m_noteModel;

    HeaderBar* m_header;
    MetadataPanel* m_metaPanel;
    QuickPreview* m_quickPreview;
    FilterPanel* m_filterPanel;
    
    Editor* m_editor;

    QString m_currentKeyword;
    QString m_currentFilterType = "all";
    int m_currentFilterValue = -1;
    int m_currentPage = 1;
    int m_pageSize = 50;
};

#endif // MAINWINDOW_H