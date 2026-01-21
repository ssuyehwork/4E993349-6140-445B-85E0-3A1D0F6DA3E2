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
#include "../models/NoteModel.h"
#include "../models/CategoryModel.h"
#include "QuickPreview.h"
#include "QuickToolbar.h"
#include "DropTreeView.h"
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class QuickWindow : public QWidget {
    Q_OBJECT
public:
    explicit QuickWindow(QWidget* parent = nullptr);
    void showCentered();

public slots:
    void refreshData();

signals:
    void toggleMainWindowRequested();

protected:
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void initUI();
    void updatePartitionStatus(const QString& name);
    int getResizeArea(const QPoint& pos);
    void setCursorShape(int area);
    void monitorForegroundWindow();
    void pasteToTarget();

    SearchLineEdit* m_searchEdit;
    QListView* m_listView;
    NoteModel* m_model;
    QuickPreview* m_quickPreview;

    DropTreeView* m_sideBar;
    CategoryModel* m_sideModel;
    QSplitter* m_splitter;
    QuickToolbar* m_toolbar;
    QLabel* m_statusLabel;
    QTimer* m_monitorTimer;
    QTimer* m_searchTimer;

#ifdef Q_OS_WIN
    HWND m_lastActiveHwnd = nullptr;
    HWND m_lastFocusHwnd = nullptr;
    DWORD m_lastThreadId = 0;
#endif

    int m_currentPage = 1;
    int m_totalPages = 1;
    QString m_currentFilterType = "all";
    int m_currentFilterValue = -1;

    int m_resizeArea = 0;
    QPoint m_resizeStartPos;
    QRect m_resizeStartGeometry;
    static const int RESIZE_MARGIN = 8;
};

#endif // QUICKWINDOW_H