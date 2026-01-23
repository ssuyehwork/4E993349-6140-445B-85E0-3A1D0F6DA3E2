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
#include <QInputDialog>
#include <QColorDialog>
#include <QSet>
#include <functional>

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
        m_currentKeyword = text;
        m_currentPage = 1;
        refreshData();
    });
    connect(m_header, &HeaderBar::pageChanged, this, [this](int page){
        m_currentPage = page;
        refreshData();
    });
    connect(m_header, &HeaderBar::refreshRequested, this, &MainWindow::refreshData);
    connect(m_header, &HeaderBar::filterRequested, this, [this](){
        if (m_filterPanel->isVisible()) m_filterPanel->hide();
        else {
            m_filterPanel->updateStats(m_currentKeyword, m_currentFilterType, m_currentFilterValue);
            QPoint pos = m_header->mapToGlobal(QPoint(m_header->width() - 320, m_header->height() + 5));
            m_filterPanel->move(pos);
            m_filterPanel->show();
        }
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
    m_sideBar->setRootIsDecorated(false);
    m_sideBar->setIndentation(12);
    m_sideBar->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sideBar->setStyleSheet(
        "QTreeView { background-color: #252526; border: none; color: #CCC; outline: none; }"
        "QTreeView::branch { image: none; border: none; width: 0px; }"
        "QTreeView::branch:has-children:closed, QTreeView::branch:has-children:open, "
        "QTreeView::branch:has-siblings:has-children:closed, QTreeView::branch:has-siblings:has-children:open { image: none; width: 0px; }"
        "QTreeView::item { height: 22px; padding-left: 10px; }"
    );
    m_sideBar->expandAll();
    m_sideBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sideBar, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos){
        QModelIndex index = m_sideBar->indexAt(pos);
        QMenu menu(this);
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 8px; border-radius: 3px; } "
                           "QMenu::item:selected { background-color: #4a90e2; color: white; }");

        if (!index.isValid() || index.data().toString() == "我的分区") {
            menu.addAction("➕ 新建分组", [this]() {
                bool ok;
                QString text = QInputDialog::getText(this, "新建组", "组名称:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) {
                    DatabaseManager::instance().addCategory(text);
                    refreshData();
                }
            });
            menu.exec(m_sideBar->mapToGlobal(pos));
            return;
        }

        QString type = index.data(CategoryModel::TypeRole).toString();
        if (type == "category") {
            int catId = index.data(CategoryModel::IdRole).toInt();
            QString currentName = index.data(CategoryModel::NameRole).toString();

            menu.addAction(IconHelper::getIcon("add", "#3498db"), "新建数据", [this, catId]() {
                auto* win = new NoteEditWindow();
                win->setDefaultCategory(catId);
                connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
                win->show();
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("palette", "#e67e22"), "设置颜色", [this, catId]() {
                QColor color = QColorDialog::getColor(Qt::gray, this, "选择分类颜色");
                if (color.isValid()) {
                    DatabaseManager::instance().setCategoryColor(catId, color.name());
                    refreshData();
                }
            });
            menu.addAction(IconHelper::getIcon("tag", "#FFAB91"), "设置预设标签", [this, catId]() {
                QString currentTags = DatabaseManager::instance().getCategoryPresetTags(catId);
                bool ok;
                QString text = QInputDialog::getText(this, "预设标签", "标签 (逗号分隔):", QLineEdit::Normal, currentTags, &ok);
                if (ok) {
                    DatabaseManager::instance().setCategoryPresetTags(catId, text);
                }
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "新建分组", [this]() {
                bool ok;
                QString text = QInputDialog::getText(this, "新建组", "组名称:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) {
                    DatabaseManager::instance().addCategory(text);
                    refreshData();
                }
            });
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "新建子分区", [this, catId]() {
                bool ok;
                QString text = QInputDialog::getText(this, "新建区", "区名称:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) {
                    DatabaseManager::instance().addCategory(text, catId);
                    refreshData();
                }
            });
            menu.addAction(IconHelper::getIcon("edit", "#aaaaaa"), "重命名分类", [this, catId, currentName]() {
                bool ok;
                QString text = QInputDialog::getText(this, "重命名", "新名称:", QLineEdit::Normal, currentName, &ok);
                if (ok && !text.isEmpty()) {
                    DatabaseManager::instance().renameCategory(catId, text);
                    refreshData();
                }
            });
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "删除分类", [this, catId]() {
                if (QMessageBox::question(this, "确认删除", "确定要删除此分类吗？内容将移至未分类。") == QMessageBox::Yes) {
                    DatabaseManager::instance().deleteCategory(catId);
                    refreshData();
                }
            });
        } else if (type == "trash") {
            menu.addAction(IconHelper::getIcon("refresh", "#2ecc71"), "全部恢复 (到未分类)", [this](){
                DatabaseManager::instance().restoreAllFromTrash();
                refreshData();
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "清空回收站", [this]() {
                if (QMessageBox::question(this, "确认清空", "确定要永久删除回收站中的所有非保护内容吗？\n(受保护的项将继续保留)") == QMessageBox::Yes) {
                    DatabaseManager::instance().emptyTrash();
                    refreshData();
                }
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
    m_noteList->setMinimumWidth(268); // 设置最小宽度为268px
    m_noteList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 隐藏垂直滚动条
    m_noteList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 隐藏水平滚动条
    m_noteModel = new NoteModel(this);
    m_noteList->setModel(m_noteModel);
    m_noteList->setItemDelegate(new NoteDelegate(m_noteList));
    m_noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_noteList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_noteList, &QListView::customContextMenuRequested, this, &MainWindow::showContextMenu);
    m_noteList->setSpacing(2);
    m_noteList->setStyleSheet("background: #1E1E1E; border: none;");
    connect(m_noteList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSelectionChanged);
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

    m_filterPanel = new FilterPanel(this);
    connect(m_filterPanel, &FilterPanel::criteriaChanged, this, &MainWindow::refreshData);

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
    // 保存当前展开状态
    QSet<QString> expandedPaths;
    std::function<void(const QModelIndex&)> checkChildren = [&](const QModelIndex& parent) {
        for (int j = 0; j < m_sideModel->rowCount(parent); ++j) {
            QModelIndex child = m_sideModel->index(j, 0, parent);
            if (m_sideBar->isExpanded(child)) {
                QString type = child.data(CategoryModel::TypeRole).toString();
                if (type == "category") {
                    expandedPaths.insert("cat_" + QString::number(child.data(CategoryModel::IdRole).toInt()));
                } else {
                    expandedPaths.insert(child.data(CategoryModel::NameRole).toString());
                }
            }
            if (m_sideModel->rowCount(child) > 0) checkChildren(child);
        }
    };

    for (int i = 0; i < m_sideModel->rowCount(); ++i) {
        QModelIndex index = m_sideModel->index(i, 0);
        if (m_sideBar->isExpanded(index)) {
            expandedPaths.insert(index.data(CategoryModel::NameRole).toString());
        }
        checkChildren(index);
    }

    auto notes = DatabaseManager::instance().searchNotes(m_currentKeyword, m_currentFilterType, m_currentFilterValue, m_currentPage, m_pageSize);
    int totalCount = DatabaseManager::instance().getNotesCount(m_currentKeyword, m_currentFilterType, m_currentFilterValue);

    // 应用高级筛选器的筛选 (前端过滤)
    QVariantMap criteria = m_filterPanel->getCheckedCriteria();
    if (!criteria.isEmpty()) {
        QList<QVariantMap> filtered;
        for (const auto& note : notes) {
            bool match = true;
            if (criteria.contains("stars")) {
                if (!criteria["stars"].toStringList().contains(QString::number(note["rating"].toInt()))) match = false;
            }
            if (match && criteria.contains("types")) {
                if (!criteria["types"].toStringList().contains(note["item_type"].toString())) match = false;
            }
            if (match && criteria.contains("colors")) {
                if (!criteria["colors"].toStringList().contains(note["color"].toString())) match = false;
            }
            if (match && criteria.contains("tags")) {
                QStringList noteTags = note["tags"].toString().split(",", Qt::SkipEmptyParts);
                bool tagMatch = false;
                for (const QString& t : criteria["tags"].toStringList()) {
                    if (noteTags.contains(t.trimmed())) { tagMatch = true; break; }
                }
                if (!tagMatch) match = false;
            }
            // 可以继续添加日期过滤
            if (match) filtered.append(note);
        }
        notes = filtered;
        totalCount = notes.size(); // 简化处理，分页在有高级筛选时表现可能不一
    }

    m_noteModel->setNotes(notes);
    m_sideModel->refresh();

    int totalPages = (totalCount + m_pageSize - 1) / m_pageSize;
    if (totalPages < 1) totalPages = 1;
    m_header->updatePagination(m_currentPage, totalPages);

    // 恢复展开状态，并确保“我的分区”默认展开
    for (int i = 0; i < m_sideModel->rowCount(); ++i) {
        QModelIndex index = m_sideModel->index(i, 0);
        QString name = index.data(CategoryModel::NameRole).toString();
        if (name == "我的分区" || expandedPaths.contains(name)) {
            m_sideBar->setExpanded(index, true);
        }

        std::function<void(const QModelIndex&)> restoreChildren = [&](const QModelIndex& parent) {
            for (int j = 0; j < m_sideModel->rowCount(parent); ++j) {
                QModelIndex child = m_sideModel->index(j, 0, parent);
                QString type = child.data(CategoryModel::TypeRole).toString();
                QString identifier = (type == "category") ?
                    ("cat_" + QString::number(child.data(CategoryModel::IdRole).toInt())) :
                    child.data(CategoryModel::NameRole).toString();

                if (expandedPaths.contains(identifier) || (parent.data(CategoryModel::NameRole).toString() == "我的分区")) {
                    m_sideBar->setExpanded(child, true);
                }
                if (m_sideModel->rowCount(child) > 0) restoreChildren(child);
            }
        };
        restoreChildren(index);
    }
}

void MainWindow::onNoteSelected(const QModelIndex& index) {
    // 转发给 selectionChanged 处理
}

void MainWindow::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    QModelIndexList indices = m_noteList->selectionModel()->selectedIndexes();
    if (indices.isEmpty()) {
        m_metaPanel->clearSelection();
    } else if (indices.size() == 1) {
        int id = indices.first().data(NoteModel::IdRole).toInt();
        QVariantMap note = DatabaseManager::instance().getNoteById(id);
        m_editor->setPlainText(QString("# %1\n\n%2").arg(note["title"].toString(), note["content"].toString()));
        m_metaPanel->setNote(note);
    } else {
        m_metaPanel->setMultipleNotes(indices.size());
    }
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
    m_currentFilterType = index.data(CategoryModel::TypeRole).toString();
    if (m_currentFilterType == "category") {
        m_currentFilterValue = index.data(CategoryModel::IdRole).toInt();
    } else {
        m_currentFilterValue = -1;
    }
    m_currentPage = 1;
    refreshData();
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