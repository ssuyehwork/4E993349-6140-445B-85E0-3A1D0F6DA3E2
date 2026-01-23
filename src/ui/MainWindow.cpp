#include "MainWindow.h"
#include "../core/DatabaseManager.h"
#include "NoteDelegate.h"
#include "Toolbox.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCursor>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent, Qt::FramelessWindowHint) {
    setWindowTitle("极速灵感 (RapidNotes) - 开发版");
    resize(1200, 800);
    initUI();
    refreshData();

    // 【关键修改】区分两种信号
    // 1. 增量更新：添加新笔记时不刷新全表
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, this, &MainWindow::onNoteAdded);
    
    // 2. 全量刷新：修改、删除时才刷新全表
    connect(&DatabaseManager::instance(), &DatabaseManager::noteUpdated, this, &MainWindow::refreshData);
}

void MainWindow::initUI() {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. HeaderBar
    m_header = new HeaderBar(this);
    connect(m_header, &HeaderBar::searchChanged, this, [this](const QString& text){
        m_noteModel->setNotes(DatabaseManager::instance().searchNotes(text));
    });
    connect(m_header, &HeaderBar::newNoteRequested, this, [this](){
        NoteEditWindow* win = new NoteEditWindow();
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });
    connect(m_header, &HeaderBar::toggleSidebar, this, [this](){
        m_sideBar->setVisible(!m_sideBar->isVisible());
    });
    connect(m_header, &HeaderBar::toolboxRequested, this, &MainWindow::toolboxRequested);
    connect(m_header, &HeaderBar::previewToggled, this, [this](bool checked){
        m_editor->togglePreview(checked);
    });
    connect(m_header, &HeaderBar::windowClose, this, &MainWindow::close);
    connect(m_header, &HeaderBar::windowMinimize, this, &MainWindow::showMinimized);
    connect(m_header, &HeaderBar::windowMaximize, this, [this](){
        if (isMaximized()) showNormal();
        else showMaximized();
    });
    mainLayout->addWidget(m_header);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);

    // 2. 左侧侧边栏 (固定最小宽度)
    m_sideBar = new DropTreeView();
    m_sideBar->setFixedWidth(220); // 彻底固定
    m_sideModel = new CategoryModel(CategoryModel::Both, this);
    m_sideBar->setModel(m_sideModel);
    m_sideBar->setHeaderHidden(true);
    m_sideBar->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sideBar->setStyleSheet("background-color: #252526; border: none; color: #CCC;");
    m_sideBar->expandAll();
    m_sideBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sideBar, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos){
        QModelIndex index = m_sideBar->indexAt(pos);
        QMenu menu(this);
        if (index.isValid()) {
            QString type = index.data(Qt::UserRole).toString();
            if (type == "category") {
                int id = index.data(Qt::UserRole + 1).toInt();
                menu.addAction(IconHelper::getIcon("edit", "#aaaaaa"), "重命名分类");
                menu.addAction(IconHelper::getIcon("trash", "#aaaaaa"), "删除分类", [this, id](){
                    DatabaseManager::instance().deleteCategory(id);
                    refreshData();
                });
            } else if (type == "trash") {
                menu.addAction(IconHelper::getIcon("trash", "#aaaaaa"), "清空回收站");
            }
        } else {
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "新建分类", [this](){
                DatabaseManager::instance().addCategory("新建分类");
                refreshData();
            });
        }
        menu.exec(m_sideBar->mapToGlobal(pos));
    });
    connect(m_sideBar, &QTreeView::clicked, this, &MainWindow::onTagSelected);
    
    // 连接拖拽信号
    connect(m_sideBar, &DropTreeView::notesDropped, this, [this](const QList<int>& ids, const QModelIndex& targetIndex){
        if (!targetIndex.isValid()) return;
        QString type = targetIndex.data(Qt::UserRole).toString();
        for (int id : ids) {
            if (type == "category") {
                int catId = targetIndex.data(Qt::UserRole + 1).toInt();
                DatabaseManager::instance().updateNoteState(id, "category_id", catId);
            } else if (type == "bookmark") {
                DatabaseManager::instance().updateNoteState(id, "is_favorite", 1);
            } else if (type == "trash") {
                DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
            } else if (type == "uncategorized") {
                DatabaseManager::instance().updateNoteState(id, "category_id", QVariant());
            }
        }
        refreshData();
    });
    splitter->addWidget(m_sideBar);

    // 3. 中间列表
    m_noteList = new QListView();
    m_noteModel = new NoteModel(this);
    m_noteList->setModel(m_noteModel);
    m_noteList->setItemDelegate(new NoteDelegate(m_noteList));
    m_noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_noteList, &QListView::customContextMenuRequested, this, &MainWindow::showContextMenu);
    m_noteList->setSpacing(2);
    m_noteList->setStyleSheet("background: #1E1E1E; border: none;");
    connect(m_noteList, &QListView::clicked, this, &MainWindow::onNoteSelected);
    connect(m_noteList, &QListView::doubleClicked, this, [this](const QModelIndex& index){
        if (!index.isValid()) return;
        int id = index.data(NoteModel::IdRole).toInt();
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });
    splitter->addWidget(m_noteList);
    
    // 4. 右侧内容容器 (编辑器 + 元数据)
    // 为了实现“固定”感，这里不再使用嵌套 Splitter，而是使用 QHBoxLayout
    QWidget* rightContainer = new QWidget();
    QHBoxLayout* rightLayout = new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_editor = new Editor();
    m_editor->togglePreview(false);
    rightLayout->addWidget(m_editor, 4); // 编辑器占 4 份

    m_metaPanel = new MetadataPanel(this);
    m_metaPanel->setFixedWidth(240); // 元数据面板强制固定宽度
    connect(m_metaPanel, &MetadataPanel::noteUpdated, this, &MainWindow::refreshData);
    rightLayout->addWidget(m_metaPanel, 1);

    splitter->addWidget(rightContainer);

    // 快捷键注册
    auto* actionFilter = new QAction(this);
    actionFilter->setShortcut(QKeySequence("Ctrl+G"));
    connect(actionFilter, &QAction::triggered, this, [this](){
        m_header->toggleSidebar(); // 暂时用切换侧边栏模拟
    });
    addAction(actionFilter);

    auto* actionMeta = new QAction(this);
    actionMeta->setShortcut(QKeySequence("Ctrl+I"));
    connect(actionMeta, &QAction::triggered, this, [this](){
        m_metaPanel->setVisible(!m_metaPanel->isVisible());
    });
    addAction(actionMeta);

    auto* actionRefresh = new QAction(this);
    actionRefresh->setShortcut(QKeySequence("F5"));
    connect(actionRefresh, &QAction::triggered, this, &MainWindow::refreshData);
    addAction(actionRefresh);

    splitter->setStretchFactor(0, 1); // 侧边栏
    splitter->setStretchFactor(1, 2); // 笔记列表
    splitter->setStretchFactor(2, 8); // 内容区
    
    // 显式设置初始大小比例
    splitter->setSizes({220, 300, 800});

    mainLayout->addWidget(splitter);

    m_quickPreview = new QuickPreview(this);
    connect(m_quickPreview, &QuickPreview::editRequested, this, [this](int id){
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });
    m_noteList->installEventFilter(this);
}

// 【新增】增量更新逻辑
void MainWindow::onNoteAdded(const QVariantMap& note) {
    // 1. 只在 Model 头部插入一条数据 (瞬间完成)
    m_noteModel->prependNote(note);
    
    // 2. 列表滚动到顶部
    m_noteList->scrollToTop();
}

void MainWindow::refreshData() {
    auto allNotes = DatabaseManager::instance().getAllNotes();
    m_noteModel->setNotes(allNotes);
    m_sideModel->refresh();
}

void MainWindow::onNoteSelected(const QModelIndex& index) {
    if (!index.isValid()) return;
    int id = index.data(NoteModel::IdRole).toInt();
    QVariantMap note = DatabaseManager::instance().getNoteById(id);
    m_editor->setPlainText(QString("# %1\n\n%2").arg(note["title"].toString(), note["content"].toString()));
    m_metaPanel->setNote(note);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_noteList && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Space && !keyEvent->isAutoRepeat()) {
            QModelIndex index = m_noteList->currentIndex();
            if (index.isValid()) {
                int id = index.data(NoteModel::IdRole).toInt();
                QVariantMap note = DatabaseManager::instance().getNoteById(id);
                QPoint globalPos = m_noteList->mapToGlobal(m_noteList->rect().center()) - QPoint(250, 300);
                m_quickPreview->showPreview(
                    id,
                    note["title"].toString(), 
                    note["content"].toString(), 
                    note["item_type"].toString(),
                    note["data_blob"].toByteArray(),
                    globalPos
                );
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onTagSelected(const QModelIndex& index) {
    QString type = index.data(Qt::UserRole).toString();
    if (type == "category") {
        int catId = index.data(Qt::UserRole + 1).toInt();
        m_noteModel->setNotes(DatabaseManager::instance().searchNotes("", "category", catId));
    } else {
        m_noteModel->setNotes(DatabaseManager::instance().searchNotes("", type));
    }
}

void MainWindow::showContextMenu(const QPoint& pos) {
    QModelIndex index = m_noteList->indexAt(pos);
    if (!index.isValid()) return;

    int id = index.data(NoteModel::IdRole).toInt();
    bool isPinned = index.data(NoteModel::PinnedRole).toBool();
    bool isLocked = index.data(NoteModel::LockedRole).toBool();
    int favorite = index.data(NoteModel::FavoriteRole).toInt();

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item { padding: 8px 25px; } QMenu::item:selected { background: #3E3E42; }");

    QAction* actEdit = menu.addAction(IconHelper::getIcon("edit", "#aaaaaa"), "编辑");
    connect(actEdit, &QAction::triggered, [this, id](){
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });

    menu.addSeparator();

    QMenu* starMenu = menu.addMenu(IconHelper::getIcon("star", "#aaaaaa"), "设置星级");
    starMenu->setStyleSheet("QMenu { background: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item:selected { background: #3E3E42; }");
    for(int i=0; i<=5; ++i) {
        QString label = (i == 0) ? "无星级" : QString("%1 星").arg(i);
        QAction* act = starMenu->addAction(label);
        if (favorite == i) act->setCheckable(true);
        if (favorite == i) act->setChecked(true);
        connect(act, &QAction::triggered, [id, i](){
            DatabaseManager::instance().updateNoteState(id, "is_favorite", i);
        });
    }

    QAction* actLock = menu.addAction(IconHelper::getIcon("lock", "#aaaaaa"), isLocked ? "解锁" : "锁定");
    connect(actLock, &QAction::triggered, [id, isLocked](){
        DatabaseManager::instance().updateNoteState(id, "is_locked", !isLocked);
    });

    menu.addSeparator();

    QAction* actPin = menu.addAction(IconHelper::getIcon("pin", "#aaaaaa"), isPinned ? "取消置顶" : "置顶");
    connect(actPin, &QAction::triggered, [id, isPinned](){
        DatabaseManager::instance().updateNoteState(id, "is_pinned", !isPinned);
    });

    menu.addSeparator();

    QAction* actDel = menu.addAction(IconHelper::getIcon("trash", "#aaaaaa"), "移至回收站");
    connect(actDel, &QAction::triggered, [this, id](){
        DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
    });

    menu.exec(QCursor::pos());
}