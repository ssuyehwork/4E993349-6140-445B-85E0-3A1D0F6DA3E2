#ifndef QUICKWINDOW_H
#define QUICKWINDOW_H

#include <QWidget>
#include "SearchLineEdit.h"
#include <QListView>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>
#include "../models/NoteModel.h"
#include "../models/CategoryModel.h"
#include "QuickPreview.h"
#include "QuickToolbar.h"
#include "DropTreeView.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class QuickWindow : public QWidget {
    Q_OBJECT
public:
    explicit QuickWindow(QWidget* parent = nullptr);
    void showCentered();
    void saveState();
    void restoreState();

public slots:
    void refreshData();

signals:
    void toggleMainWindowRequested();
    void toolboxRequested();

protected:
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void initUI();
    void activateNote(const QModelIndex& index);
    void setupShortcuts();
    void updatePartitionStatus(const QString& name);
    void refreshSidebar();
    void applyListTheme(const QString& colorHex);
    int getResizeArea(const QPoint& pos);
    void setCursorShape(int area);

public:
    QString currentCategoryColor() const { return m_currentCategoryColor; }

    // 快捷键处理函数
    void doDeleteSelected();
    void doToggleFavorite();
    void doTogglePin();
    void doLockSelected();
    void doNewIdea();
    void doExtractContent();
    void doEditSelected();
    void doSetRating(int rating);
    void doMoveToCategory(int catId);
    void doPreview();
    void toggleStayOnTop(bool checked);
    void toggleSidebar();
    void showListContextMenu(const QPoint& pos);
    void showSidebarMenu(const QPoint& pos);
    
    SearchLineEdit* m_searchEdit;
    QListView* m_listView;
    NoteModel* m_model;
    QuickPreview* m_quickPreview;
    
    DropTreeView* m_systemTree;
    DropTreeView* m_partitionTree;
    CategoryModel* m_systemModel;
    CategoryModel* m_partitionModel;
    
    QTimer* m_searchTimer;
    QTimer* m_monitorTimer;
    QSplitter* m_splitter;
    QuickToolbar* m_toolbar;
    QLabel* m_statusLabel;

    int m_currentPage = 1;
    int m_totalPages = 1;
    QString m_currentFilterType = "all";
    int m_currentFilterValue = -1;
    QString m_currentCategoryColor = "#4a90e2"; // 默认蓝色

    int m_resizeArea = 0;
    QPoint m_resizeStartPos;
    QRect m_resizeStartGeometry;
    static const int RESIZE_MARGIN = 15;

#ifdef Q_OS_WIN
    HWND m_lastActiveHwnd = nullptr;
    HWND m_lastFocusHwnd = nullptr;
    DWORD m_lastThreadId = 0;
#endif
};

#endif // QUICKWINDOW_H