#include "MainWindow.h"
#include "../core/DatabaseManager.h"
#include "NoteDelegate.h"
#include "Toolbox.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTabWidget>
#include <QLabel>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCursor>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
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
    connect(m_header, &HeaderBar::toolboxRequested, this, [this](){
        Toolbox dlg(this);
        dlg.exec();
    });
    connect(m_header, &HeaderBar::previewToggled, this, [this](bool checked){
        m_editor->togglePreview(checked);
    });
    mainLayout->addWidget(m_header);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // 2. 左侧侧边栏
    m_sideBar = new QTreeView();
    m_sideModel = new CategoryModel(this);
    m_sideBar->setModel(m_sideModel);
    m_sideBar->setHeaderHidden(true);
    m_sideBar->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sideBar->setStyleSheet("background-color: #252526; border: none; color: #CCC;");
    m_sideBar->expandAll();
    connect(m_sideBar, &QTreeView::clicked, this, &MainWindow::onTagSelected);
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

    // 4. 右侧主展示区
    auto* mainTabSplitter = new QSplitter(Qt::Horizontal);

    auto* rightTab = new QTabWidget();
    m_editor = new Editor();
    m_editor->togglePreview(true); // 默认开启预览模式
    m_graphWidget = new GraphWidget();
    rightTab->addTab(m_editor, IconHelper::getIcon("eye", "#aaaaaa"), "预览");
    rightTab->addTab(m_graphWidget, IconHelper::getIcon("branch", "#aaaaaa"), "知识图谱");
    mainTabSplitter->addWidget(rightTab);

    // 5. 元数据面板
    m_metaPanel = new MetadataPanel(this);
    connect(m_metaPanel, &MetadataPanel::noteUpdated, this, &MainWindow::refreshData);
    mainTabSplitter->addWidget(m_metaPanel);

    splitter->addWidget(mainTabSplitter);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 6);

    mainLayout->addWidget(splitter);
}

// 【新增】增量更新逻辑
void MainWindow::onNoteAdded(const QVariantMap& note) {
    // 1. 只在 Model 头部插入一条数据 (瞬间完成)
    m_noteModel->prependNote(note);

    // 2. 列表滚动到顶部
    m_noteList->scrollToTop();

    // 3. (可选) 如果你想图谱也增量更新，可以在 GraphWidget 加 addSingleNode 接口
    // 这里暂时不做，因为图谱不一定开着
}

void MainWindow::refreshData() {
    auto allNotes = DatabaseManager::instance().getAllNotes();
    m_noteModel->setNotes(allNotes);
    m_graphWidget->loadNotes(allNotes);
    m_sideModel->refresh();
}

void MainWindow::onNoteSelected(const QModelIndex& index) {
    if (!index.isValid()) return;
    int id = index.data(NoteModel::IdRole).toInt();
    QVariantMap note = DatabaseManager::instance().getNoteById(id);
    m_editor->setPlainText(QString("# %1\n\n%2").arg(note["title"].toString(), note["content"].toString()));
    m_metaPanel->setNote(note);
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

    QAction* actEdit = menu.addAction("📝 编辑");
    connect(actEdit, &QAction::triggered, [this, id](){
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });

    menu.addSeparator();

    QMenu* starMenu = menu.addMenu("⭐ 设置星级");
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

    QAction* actLock = menu.addAction(isLocked ? "🔓 解锁" : "🔒 锁定");
    connect(actLock, &QAction::triggered, [id, isLocked](){
        DatabaseManager::instance().updateNoteState(id, "is_locked", !isLocked);
    });

    menu.addSeparator();

    QAction* actPin = menu.addAction(isPinned ? "🚫 取消置顶" : "📌 置顶");
    connect(actPin, &QAction::triggered, [id, isPinned](){
        DatabaseManager::instance().updateNoteState(id, "is_pinned", !isPinned);
    });

    menu.addSeparator();

    QAction* actDel = menu.addAction("🗑️ 移至回收站");
    connect(actDel, &QAction::triggered, [this, id](){
        DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
    });

    menu.exec(QCursor::pos());
}