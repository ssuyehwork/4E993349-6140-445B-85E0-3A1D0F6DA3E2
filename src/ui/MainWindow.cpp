#include "MainWindow.h"
#include "../core/DatabaseManager.h"
#include "NoteDelegate.h"
#include "CategoryDelegate.h"
#include <QHBoxLayout>
#include <utility>
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
#include <QSettings>
#include <QRandomGenerator>
#include <QLineEdit>
#include <QTextEdit>
#include <QToolTip>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QApplication>
#include <QPlainTextEdit>
#include "CleanListView.h"
#include "FramelessDialog.h"
#include "CategoryPasswordDialog.h"
#include "SettingsWindow.h"
#include <functional>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent, Qt::FramelessWindowHint) {
    setWindowTitle("极速灵感 (RapidNotes) - 开发版");
    resize(1200, 800);
    initUI();
    refreshData();

    // 【关键修改】区分两种信号
    // 1. 增量更新：添加新笔记时不刷新全表
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, this, &MainWindow::onNoteAdded);
    
    // 2. 全量刷新：修改、删除、分类变化（锁定状态）时才刷新全表
    connect(&DatabaseManager::instance(), &DatabaseManager::noteUpdated, this, &MainWindow::refreshData);
    connect(&DatabaseManager::instance(), &DatabaseManager::categoriesChanged, this, &MainWindow::refreshData);

    restoreLayout(); // 恢复布局
}

void MainWindow::initUI() {
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    centralWidget->setAttribute(Qt::WA_StyledBackground, true);
    centralWidget->setStyleSheet("#CentralWidget { background-color: #1E1E1E; }");
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
    connect(m_header, &HeaderBar::stayOnTopRequested, this, [this](bool checked){
        if (auto* win = window()) {
            Qt::WindowFlags flags = win->windowFlags();
            if (checked) flags |= Qt::WindowStaysOnTopHint;
            else flags &= ~Qt::WindowStaysOnTopHint;
            win->setWindowFlags(flags);
            win->show(); // 修改 Flags 后需要 show 才会生效
        }
    });
    connect(m_header, &HeaderBar::filterRequested, this, [this](){
        bool visible = !m_filterWrapper->isVisible();
        m_filterWrapper->setVisible(visible);
        m_header->setFilterActive(visible);
        if (visible) {
            m_filterPanel->updateStats(m_currentKeyword, m_currentFilterType, m_currentFilterValue);
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
    connect(m_header, &HeaderBar::toolboxContextMenuRequested, this, &MainWindow::showToolboxMenu);
    connect(m_header, &HeaderBar::metadataToggled, this, [this](bool checked){
        m_metaPanel->setVisible(checked);
    });
    connect(m_header, &HeaderBar::windowClose, this, &MainWindow::close);
    connect(m_header, &HeaderBar::windowMinimize, this, &MainWindow::showMinimized);
    connect(m_header, &HeaderBar::windowMaximize, this, [this](){
        if (isMaximized()) showNormal();
        else showMaximized();
    });
    mainLayout->addWidget(m_header);

    // 核心内容容器：管理 5px 全局边距
    auto* contentWidget = new QWidget(centralWidget);
    contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    contentWidget->setStyleSheet("background: transparent; border: none;");
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(5, 5, 5, 5); // 确保顶栏下方及窗口四周均有 5px 留白
    contentLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(5); // 统一横向板块间的物理缝隙为 5px
    splitter->setChildrenCollapsible(false);
    splitter->setAttribute(Qt::WA_StyledBackground, true);
    splitter->setStyleSheet("QSplitter { background: transparent; border: none; } QSplitter::handle { background: transparent; }");

    // 1. 左侧侧边栏包装容器 (固定 230px)
    auto* sidebarWrapper = new QWidget();
    sidebarWrapper->setMinimumWidth(230);
    auto* sidebarWrapperLayout = new QVBoxLayout(sidebarWrapper);
    sidebarWrapperLayout->setContentsMargins(0, 0, 0, 0); // 彻底消除偏移边距，由全局 Layout 和 Splitter 控制

    auto* sidebarContainer = new QFrame();
    sidebarContainer->setObjectName("SidebarContainer");
    sidebarContainer->setAttribute(Qt::WA_StyledBackground, true);
    sidebarContainer->setStyleSheet(
        "#SidebarContainer {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #333333;"
        "  border-top-left-radius: 12px;"
        "  border-top-right-radius: 12px;"
        "  border-bottom-left-radius: 0px;"
        "  border-bottom-right-radius: 0px;"
        "}"
    );

    auto* sidebarShadow = new QGraphicsDropShadowEffect(sidebarContainer);
    sidebarShadow->setBlurRadius(10);
    sidebarShadow->setXOffset(0);
    sidebarShadow->setYOffset(4);
    sidebarShadow->setColor(QColor(0, 0, 0, 150));
    sidebarContainer->setGraphicsEffect(sidebarShadow);

    auto* sidebarContainerLayout = new QVBoxLayout(sidebarContainer);
    sidebarContainerLayout->setContentsMargins(0, 0, 0, 0); 
    sidebarContainerLayout->setSpacing(0);

    // 侧边栏标题栏 (全宽下划线方案)
    auto* sidebarHeader = new QWidget();
    sidebarHeader->setFixedHeight(32);
    sidebarHeader->setStyleSheet(
        "background-color: #252526; "
        "border-top-left-radius: 12px; "
        "border-top-right-radius: 12px; "
        "border-bottom: 1px solid #333;"
    );
    auto* sidebarHeaderLayout = new QHBoxLayout(sidebarHeader);
    sidebarHeaderLayout->setContentsMargins(15, 0, 15, 0);
    auto* sbIcon = new QLabel();
    sbIcon->setPixmap(IconHelper::getIcon("category", "#3498db").pixmap(18, 18));
    sidebarHeaderLayout->addWidget(sbIcon);
    auto* sbTitle = new QLabel("数据分类");
    sbTitle->setStyleSheet("color: #3498db; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    sidebarHeaderLayout->addWidget(sbTitle);
    sidebarHeaderLayout->addStretch();
    
    sidebarHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sidebarHeader, &QWidget::customContextMenuRequested, this, [this, sidebarContainer, splitter](const QPoint& pos){
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item { padding: 5px 20px; } QMenu::item:selected { background-color: #3E3E42; }");
        menu.addAction("向右移动", [this, sidebarContainer, splitter](){
            int index = splitter->indexOf(sidebarContainer);
            if (index < splitter->count() - 1) {
                splitter->insertWidget(index + 1, sidebarContainer); 
            }
        });
         menu.addAction("向左移动", [this, sidebarContainer, splitter](){
            int index = splitter->indexOf(sidebarContainer);
            if (index > 0) {
                splitter->insertWidget(index - 1, sidebarContainer);
            }
        });
        menu.exec(sidebarContainer->mapToGlobal(pos));
    });
    
    sidebarContainerLayout->addWidget(sidebarHeader);

    // 内容容器
    auto* sbContent = new QWidget();
    sbContent->setAttribute(Qt::WA_StyledBackground, true);
    sbContent->setStyleSheet("background: transparent; border: none;");
    auto* sbContentLayout = new QVBoxLayout(sbContent);
    sbContentLayout->setContentsMargins(8, 8, 8, 8);

    m_sideBar = new DropTreeView();
    m_sideBar->setMinimumWidth(200); 
    m_sideModel = new CategoryModel(CategoryModel::Both, this);
    m_sideBar->setModel(m_sideModel);
    m_sideBar->setItemDelegate(new CategoryDelegate(this));
    m_sideBar->setHeaderHidden(true);
    m_sideBar->setRootIsDecorated(false);
    m_sideBar->setIndentation(12);
    m_sideBar->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sideBar->setStyleSheet(
        "QTreeView { background-color: transparent; border: none; color: #CCC; outline: none; }"
        "QTreeView::branch { image: none; border: none; width: 0px; }"
        "QTreeView::item { height: 22px; padding-left: 10px; }"
    );
    m_sideBar->expandAll();
    m_sideBar->setContextMenuPolicy(Qt::CustomContextMenu);
    
    sbContentLayout->addWidget(m_sideBar);
    sidebarContainerLayout->addWidget(sbContent);

    // 直接放入 Splitter (移除 Wrapper)
    splitter->addWidget(sidebarContainer);

    connect(m_sideBar, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos){
        QModelIndex index = m_sideBar->indexAt(pos);
        QMenu menu(this);
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 8px; border-radius: 3px; } "
                           "QMenu::item:selected { background-color: #4a90e2; color: white; }");

        if (!index.isValid() || index.data().toString() == "我的分区") {
            menu.addAction(IconHelper::getIcon("add", "#3498db"), "新建分组", [this]() {
                auto* dlg = new FramelessInputDialog("新建分组", "组名称:", "", this);
                connect(dlg, &FramelessInputDialog::accepted, [this, dlg](){
                    QString text = dlg->text();
                    if (!text.isEmpty()) {
                        DatabaseManager::instance().addCategory(text);
                        refreshData();
                    }
                });
                dlg->show();
                dlg->activateWindow();
                dlg->raise();
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
                auto* dlg = new QColorDialog(Qt::gray, this);
                dlg->setWindowTitle("选择分类颜色");
                dlg->setWindowFlags(dlg->windowFlags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
                connect(dlg, &QColorDialog::colorSelected, [this, catId](const QColor& color){
                    if (color.isValid()) {
                        DatabaseManager::instance().setCategoryColor(catId, color.name());
                        refreshData();
                    }
                });
                connect(dlg, &QColorDialog::finished, dlg, &QObject::deleteLater);
                dlg->show();
            });
            menu.addAction(IconHelper::getIcon("random_color", "#FF6B9D"), "随机颜色", [this, catId]() {
                static const QStringList palette = {
                    "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FFEEAD",
                    "#D4A5A5", "#9B59B6", "#3498DB", "#E67E22", "#2ECC71",
                    "#E74C3C", "#F1C40F", "#1ABC9C", "#34495E", "#95A5A6"
                };
                QString chosenColor = palette.at(QRandomGenerator::global()->bounded(palette.size()));
                DatabaseManager::instance().setCategoryColor(catId, chosenColor);
                refreshData();
            });
            menu.addAction(IconHelper::getIcon("tag", "#FFAB91"), "设置预设标签", [this, catId]() {
                QString currentTags = DatabaseManager::instance().getCategoryPresetTags(catId);
                auto* dlg = new FramelessInputDialog("设置预设标签", "标签 (逗号分隔):", currentTags, this);
                connect(dlg, &FramelessInputDialog::accepted, [this, catId, dlg](){
                    DatabaseManager::instance().setCategoryPresetTags(catId, dlg->text());
                });
                dlg->show();
                dlg->activateWindow();
                dlg->raise();
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "新建分组", [this]() {
                auto* dlg = new FramelessInputDialog("新建分组", "组名称:", "", this);
                connect(dlg, &FramelessInputDialog::accepted, [this, dlg](){
                    QString text = dlg->text();
                    if (!text.isEmpty()) {
                        DatabaseManager::instance().addCategory(text);
                        refreshData();
                    }
                });
                dlg->show();
                dlg->activateWindow();
                dlg->raise();
            });
            menu.addAction(IconHelper::getIcon("add", "#3498db"), "新建子分区", [this, catId]() {
                auto* dlg = new FramelessInputDialog("新建子分区", "区名称:", "", this);
                connect(dlg, &FramelessInputDialog::accepted, [this, catId, dlg](){
                    QString text = dlg->text();
                    if (!text.isEmpty()) {
                        DatabaseManager::instance().addCategory(text, catId);
                        refreshData();
                    }
                });
                dlg->show();
                dlg->activateWindow();
                dlg->raise();
            });
            menu.addAction(IconHelper::getIcon("edit", "#aaaaaa"), "重命名分类", [this, catId, currentName]() {
                auto* dlg = new FramelessInputDialog("重命名分类", "新名称:", currentName, this);
                connect(dlg, &FramelessInputDialog::accepted, [this, catId, dlg](){
                    QString text = dlg->text();
                    if (!text.isEmpty()) {
                        DatabaseManager::instance().renameCategory(catId, text);
                        refreshData();
                    }
                });
                dlg->show();
                dlg->activateWindow();
                dlg->raise();
            });
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "删除分类", [this, catId]() {
                auto* dlg = new FramelessMessageBox("确认删除", "确定要删除此分类吗？内容将移至未分类。", this);
                connect(dlg, &FramelessMessageBox::confirmed, [this, catId](){
                    DatabaseManager::instance().deleteCategory(catId);
                    refreshData();
                });
                dlg->show();
            });

            menu.addSeparator();
            auto* pwdMenu = menu.addMenu(IconHelper::getIcon("lock", "#aaaaaa"), "密码保护");
            pwdMenu->setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item:selected { background-color: #3E3E42; }");
            
            pwdMenu->addAction("设置", [this, catId]() {
                auto* dlg = new CategoryPasswordDialog("设置密码", this);
                connect(dlg, &QDialog::accepted, [this, catId, dlg]() {
                    DatabaseManager::instance().setCategoryPassword(catId, dlg->password(), dlg->passwordHint());
                    refreshData();
                });
                dlg->show();
                dlg->activateWindow();
                dlg->raise();
            });
            pwdMenu->addAction("修改", [this, catId]() {
                auto* verifyDlg = new FramelessInputDialog("验证旧密码", "请输入当前密码:", "", this);
                connect(verifyDlg, &FramelessInputDialog::accepted, [this, catId, verifyDlg]() {
                    if (DatabaseManager::instance().verifyCategoryPassword(catId, verifyDlg->text())) {
                        auto* dlg = new CategoryPasswordDialog("修改密码", this);
                        QString currentHint;
                        auto cats = DatabaseManager::instance().getAllCategories();
                        for(const auto& c : std::as_const(cats)) if(c["id"].toInt() == catId) currentHint = c["password_hint"].toString();
                        dlg->setInitialData(currentHint);
                        connect(dlg, &QDialog::accepted, [this, catId, dlg]() {
                            DatabaseManager::instance().setCategoryPassword(catId, dlg->password(), dlg->passwordHint());
                            refreshData();
                        });
                        dlg->show();
                        dlg->activateWindow();
                        dlg->raise();
                    } else {
                        QMessageBox::warning(this, "错误", "旧密码验证失败");
                    }
                });
                verifyDlg->show();
            });
            pwdMenu->addAction("移除", [this, catId]() {
                auto* dlg = new FramelessInputDialog("验证密码", "请输入当前密码以移除保护:", "", this);
                connect(dlg, &FramelessInputDialog::accepted, [this, catId, dlg]() {
                    if (DatabaseManager::instance().verifyCategoryPassword(catId, dlg->text())) {
                        DatabaseManager::instance().removeCategoryPassword(catId);
                        refreshData();
                    } else {
                        QMessageBox::warning(this, "错误", "密码错误");
                    }
                });
                dlg->show();
            });
            pwdMenu->addAction("立即锁定", [this, catId]() {
                DatabaseManager::instance().lockCategory(catId);
                refreshData();
            })->setShortcut(QKeySequence("Ctrl+Shift+L"));
        } else if (type == "trash") {
            menu.addAction(IconHelper::getIcon("refresh", "#2ecc71"), "全部恢复 (到未分类)", [this](){
                DatabaseManager::instance().restoreAllFromTrash();
                refreshData();
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "清空回收站", [this]() {
                auto* dlg = new FramelessMessageBox("确认清空", "确定要永久删除回收站中的所有内容吗？\n(此操作不可逆)", this);
                connect(dlg, &FramelessMessageBox::confirmed, [this](){
                    DatabaseManager::instance().emptyTrash();
                    refreshData();
                });
                dlg->show();
            });
        }
        menu.exec(m_sideBar->mapToGlobal(pos));
    });
    connect(m_sideBar, &QTreeView::clicked, this, &MainWindow::onTagSelected);
    
    // 连接拖拽信号 (使用 Model 定义的枚举)
    connect(m_sideBar, &DropTreeView::notesDropped, this, [this](const QList<int>& ids, const QModelIndex& targetIndex){
        if (!targetIndex.isValid()) return;
        QString type = targetIndex.data(CategoryModel::TypeRole).toString();
        for (int id : ids) {
            if (type == "category") {
                int catId = targetIndex.data(CategoryModel::IdRole).toInt();
                DatabaseManager::instance().updateNoteState(id, "category_id", catId);
            } else if (targetIndex.data().toString() == "收藏" || type == "bookmark") { 
                DatabaseManager::instance().updateNoteState(id, "is_favorite", 1);
            } else if (type == "trash") {
                DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
            } else if (type == "uncategorized") {
                DatabaseManager::instance().updateNoteState(id, "category_id", QVariant());
            }
        }
        refreshData();
    });

    // 3. 中间列表卡片容器
    auto* listContainer = new QFrame();
    listContainer->setMinimumWidth(230); // 对齐 MetadataPanel
    listContainer->setObjectName("ListContainer");
    listContainer->setAttribute(Qt::WA_StyledBackground, true);
    listContainer->setStyleSheet(
        "#ListContainer {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #333333;"
        "  border-top-left-radius: 12px;"
        "  border-top-right-radius: 12px;"
        "  border-bottom-left-radius: 0px;"
        "  border-bottom-right-radius: 0px;"
        "}"
    );

    auto* listShadow = new QGraphicsDropShadowEffect(listContainer);
    listShadow->setBlurRadius(10);
    listShadow->setXOffset(0);
    listShadow->setYOffset(4);
    listShadow->setColor(QColor(0, 0, 0, 150));
    listContainer->setGraphicsEffect(listShadow);

    auto* listContainerLayout = new QVBoxLayout(listContainer);
    listContainerLayout->setContentsMargins(0, 0, 0, 0); 
    listContainerLayout->setSpacing(0);

    // 列表标题栏 (锁定 32px, 统一配色与分割线)
    auto* listHeader = new QWidget();
    listHeader->setFixedHeight(32);
    listHeader->setStyleSheet(
        "background-color: #252526; "
        "border-top-left-radius: 12px; "
        "border-top-right-radius: 12px; "
        "border-bottom: 1px solid #333;" 
    );
    auto* listHeaderLayout = new QHBoxLayout(listHeader);
    listHeaderLayout->setContentsMargins(15, 0, 15, 0); 
    auto* listIcon = new QLabel();
    listIcon->setPixmap(IconHelper::getIcon("list_ul", "#2ecc71").pixmap(18, 18));
    listHeaderLayout->addWidget(listIcon);
    auto* listHeaderTitle = new QLabel("笔记列表");
    listHeaderTitle->setStyleSheet("color: #2ecc71; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    listHeaderLayout->addWidget(listHeaderTitle);
    listHeaderLayout->addStretch();
    
    listHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(listHeader, &QWidget::customContextMenuRequested, this, [this, listContainer, splitter, listHeader](const QPoint& pos){
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item { padding: 5px 20px; } QMenu::item:selected { background-color: #3E3E42; }");
        menu.addAction("向左移动", [this, listContainer, splitter](){
            int index = splitter->indexOf(listContainer);
            if (index > 0) splitter->insertWidget(index - 1, listContainer);
        });
        menu.addAction("向右移动", [this, listContainer, splitter](){
            int index = splitter->indexOf(listContainer);
            if (index < splitter->count() - 1) splitter->insertWidget(index + 1, listContainer);
        });
        menu.exec(listHeader->mapToGlobal(pos));
    });
    
    listContainerLayout->addWidget(listHeader);

    // 内容容器
    auto* listContent = new QWidget();
    listContent->setAttribute(Qt::WA_StyledBackground, true);
    listContent->setStyleSheet("background: transparent; border: none;");
    auto* listContentLayout = new QVBoxLayout(listContent);
    // 恢复垂直边距为 8，保留水平边距 15 以对齐宽度
    listContentLayout->setContentsMargins(15, 8, 15, 8);
    
    m_noteList = new CleanListView();
    m_noteList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_noteList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_noteModel = new NoteModel(this);
    m_noteList->setModel(m_noteModel);
    m_noteList->setItemDelegate(new NoteDelegate(m_noteList));
    m_noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_noteList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_noteList, &QListView::customContextMenuRequested, this, &MainWindow::showContextMenu);
    
    // 恢复垂直间距为 5，垂直 Padding 为 5；仅水平 Padding 设为 0
    m_noteList->setSpacing(5); 
    m_noteList->setStyleSheet("QListView { background: transparent; border: none; padding-top: 5px; padding-bottom: 5px; padding-left: 0px; padding-right: 0px; }");
    
    // 基础拖拽使能 (其余复杂逻辑已由 CleanListView 实现)
    m_noteList->setDragEnabled(true);

    connect(m_noteList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_noteList, &QListView::doubleClicked, this, [this](const QModelIndex& index){
        if (!index.isValid()) return;
        int id = index.data(NoteModel::IdRole).toInt();
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });

    listContentLayout->addWidget(m_noteList);

    m_lockWidget = new CategoryLockWidget(this);
    m_lockWidget->setVisible(false);
    connect(m_lockWidget, &CategoryLockWidget::unlocked, this, [this](){
        refreshData();
    });
    listContentLayout->addWidget(m_lockWidget);

    listContainerLayout->addWidget(listContent);
    splitter->addWidget(listContainer);
    
    // 4. 右侧内容组合 (水平对齐编辑器与元数据)
    QWidget* rightContainer = new QWidget();
    rightContainer->setAttribute(Qt::WA_StyledBackground, true);
    rightContainer->setStyleSheet("background: transparent; border: none;");
    QHBoxLayout* rightLayout = new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(5); 

    // 4.1 编辑器容器 (Card)
    auto* editorContainer = new QFrame();
    editorContainer->setObjectName("EditorContainer");
    editorContainer->setAttribute(Qt::WA_StyledBackground, true);
    editorContainer->setStyleSheet(
        "#EditorContainer {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #333333;"
        "  border-top-left-radius: 12px;"
        "  border-top-right-radius: 12px;"
        "  border-bottom-left-radius: 0px;"
        "  border-bottom-right-radius: 0px;"
        "}"
    );

    auto* editorShadow = new QGraphicsDropShadowEffect(editorContainer);
    editorShadow->setBlurRadius(10);
    editorShadow->setXOffset(0);
    editorShadow->setYOffset(4);
    editorShadow->setColor(QColor(0, 0, 0, 150));
    editorContainer->setGraphicsEffect(editorShadow);

    auto* editorContainerLayout = new QVBoxLayout(editorContainer);
    editorContainerLayout->setContentsMargins(0, 0, 0, 0);
    editorContainerLayout->setSpacing(0);

    // 编辑器标题栏 (全宽贯穿线)
    auto* editorHeader = new QWidget();
    editorHeader->setFixedHeight(32);
    editorHeader->setStyleSheet(
        "background-color: #252526; "
        "border-top-left-radius: 12px; "
        "border-top-right-radius: 12px; "
        "border-bottom: 1px solid #333;"
    );
    auto* editorHeaderLayout = new QHBoxLayout(editorHeader);
    editorHeaderLayout->setContentsMargins(15, 0, 15, 0);
    auto* edIcon = new QLabel();
    edIcon->setPixmap(IconHelper::getIcon("eye", "#e67e22").pixmap(18, 18));
    editorHeaderLayout->addWidget(edIcon);
    auto* edTitle = new QLabel("预览数据"); // 保护用户修改的标题内容
    edTitle->setStyleSheet("color: #e67e22; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    editorHeaderLayout->addWidget(edTitle);
    editorHeaderLayout->addStretch();

    // 编辑锁定/解锁按钮
    auto* editLockBtn = new QPushButton();
    editLockBtn->setFixedSize(24, 24);
    editLockBtn->setCursor(Qt::PointingHandCursor);
    editLockBtn->setCheckable(true);
    editLockBtn->setToolTip("点击进入编辑模式");
    editLockBtn->setIcon(IconHelper::getIcon("edit", "#888888")); 
    editLockBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); }"
        "QPushButton:checked { background-color: rgba(74, 144, 226, 0.2); }"
    );
    editorHeaderLayout->addWidget(editLockBtn);
    
    editorHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editorHeader, &QWidget::customContextMenuRequested, this, [this, rightContainer, splitter, editorHeader](const QPoint& pos){
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item { padding: 5px 20px; } QMenu::item:selected { background-color: #3E3E42; }");
        menu.addAction("向左移动", [this, rightContainer, splitter](){
            int index = splitter->indexOf(rightContainer);
            if (index > 0) splitter->insertWidget(index - 1, rightContainer);
        });
        menu.addAction("向右移动", [this, rightContainer, splitter](){
            int index = splitter->indexOf(rightContainer);
            if (index < splitter->count() - 1) splitter->insertWidget(index + 1, rightContainer);
        });
        menu.exec(editorHeader->mapToGlobal(pos));
    });

    editorContainerLayout->addWidget(editorHeader);

    // 内容容器
    auto* editorContent = new QWidget();
    editorContent->setAttribute(Qt::WA_StyledBackground, true);
    editorContent->setStyleSheet("background: transparent; border: none;");
    auto* editorContentLayout = new QVBoxLayout(editorContent);
    editorContentLayout->setContentsMargins(2, 2, 2, 2); // 编辑器保留微量对齐边距

    m_editor = new Editor();
    m_editor->togglePreview(false);
    m_editor->setReadOnly(true); // 默认不可编辑

    connect(editLockBtn, &QPushButton::toggled, this, [this, editLockBtn](bool checked){
        m_editor->setReadOnly(!checked);
        if (checked) {
            editLockBtn->setIcon(IconHelper::getIcon("eye", "#4a90e2"));
            editLockBtn->setToolTip("当前：编辑模式 (点击切回预览)");
        } else {
            editLockBtn->setIcon(IconHelper::getIcon("edit", "#888888"));
            editLockBtn->setToolTip("当前：锁定模式 (点击解锁编辑)");
        }
    });
    
    editorContentLayout->addWidget(m_editor);
    editorContainerLayout->addWidget(editorContent);
    rightLayout->addWidget(editorContainer, 4); 

    m_metaPanel = new MetadataPanel(this);
    m_metaPanel->setVisible(true);
    connect(m_metaPanel, &MetadataPanel::noteUpdated, this, &MainWindow::refreshData);
    connect(m_metaPanel, &MetadataPanel::closed, this, [this](){
        m_header->setMetadataActive(false);
    });
    connect(m_metaPanel, &MetadataPanel::tagAdded, this, [this](const QStringList& tags){
        QModelIndexList indices = m_noteList->selectionModel()->selectedIndexes();
        if (indices.isEmpty()) return;
        for (const auto& index : std::as_const(indices)) {
            int id = index.data(NoteModel::IdRole).toInt();
            DatabaseManager::instance().addTagsToNote(id, tags);
        }
        refreshData();
    });
    rightLayout->addWidget(m_metaPanel, 1);

    splitter->addWidget(rightContainer);

    // 4. 高级筛选器卡片容器
    m_filterWrapper = new QWidget();
    m_filterWrapper->setMinimumWidth(230);
    auto* filterWrapperLayout = new QVBoxLayout(m_filterWrapper);
    filterWrapperLayout->setContentsMargins(0, 0, 0, 0); 

    auto* filterContainer = new QFrame();
    filterContainer->setObjectName("FilterContainer");
    filterContainer->setAttribute(Qt::WA_StyledBackground, true);
    filterContainer->setStyleSheet(
        "#FilterContainer {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #333333;"
        "  border-top-left-radius: 12px;"
        "  border-top-right-radius: 12px;"
        "  border-bottom-left-radius: 0px;"
        "  border-bottom-right-radius: 0px;"
        "}"
    );

    auto* filterShadow = new QGraphicsDropShadowEffect(filterContainer);
    filterShadow->setBlurRadius(10);
    filterShadow->setXOffset(0);
    filterShadow->setYOffset(4);
    filterShadow->setColor(QColor(0, 0, 0, 150));
    filterContainer->setGraphicsEffect(filterShadow);
    
    // Add context menu to filterContainer (wrapper) to allow moving
    filterContainer->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(filterContainer, &QWidget::customContextMenuRequested, this, [this, filterContainer, splitter](const QPoint& pos){
        // Only show if clicking top area (header simulation)
        if (filterContainer->mapFromGlobal(QCursor::pos()).y() > 40) return; 

        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; } QMenu::item { padding: 5px 20px; } QMenu::item:selected { background-color: #3E3E42; }");
        menu.addAction("向左移动", [this, filterContainer, splitter](){
            int index = splitter->indexOf(filterContainer);
            if (index > 0) splitter->insertWidget(index - 1, filterContainer);
        });
        menu.addAction("向右移动", [this, filterContainer, splitter](){
            int index = splitter->indexOf(filterContainer);
            if (index < splitter->count() - 1) splitter->insertWidget(index + 1, filterContainer);
        });
        menu.exec(QCursor::pos());
    });

    auto* fLayout = new QVBoxLayout(filterContainer);
    fLayout->setContentsMargins(0, 0, 0, 0); 

    m_filterPanel = new FilterPanel(this);
    m_filterPanel->setStyleSheet("background: transparent; border: none;");
    connect(m_filterPanel, &FilterPanel::filterChanged, this, &MainWindow::refreshData);
    fLayout->addWidget(m_filterPanel);

    // 直接放入 Splitter
    m_filterWrapper = filterContainer;
    m_filterWrapper->setMinimumWidth(230);
    m_filterWrapper->setVisible(true);
    m_header->setFilterActive(true);
    splitter->addWidget(m_filterWrapper);

    // 快捷键注册
    auto* actionFilter = new QAction(this);
    actionFilter->setShortcut(QKeySequence("Ctrl+G"));
    connect(actionFilter, &QAction::triggered, this, [this](){
        m_header->toggleSidebar(); 
    });
    addAction(actionFilter);

    auto* actionMeta = new QAction(this);
    actionMeta->setShortcut(QKeySequence("Ctrl+I"));
    connect(actionMeta, &QAction::triggered, this, [this](){
        bool visible = !m_metaPanel->isVisible();
        m_metaPanel->setVisible(visible);
        m_header->setMetadataActive(visible);
    });
    addAction(actionMeta);

    auto* actionRefresh = new QAction(this);
    actionRefresh->setShortcut(QKeySequence("F5"));
    connect(actionRefresh, &QAction::triggered, this, &MainWindow::refreshData);
    addAction(actionRefresh);

    auto* actionLockCat = new QAction(this);
    actionLockCat->setShortcut(QKeySequence("Ctrl+Shift+L"));
    connect(actionLockCat, &QAction::triggered, this, [this](){
        if (m_currentFilterType == "category" && m_currentFilterValue != -1) {
            DatabaseManager::instance().lockCategory(m_currentFilterValue.toInt());
            refreshData();
        }
    });
    addAction(actionLockCat);


    splitter->setStretchFactor(0, 1); 
    splitter->setStretchFactor(1, 2); 
    splitter->setStretchFactor(2, 8); 
    splitter->setStretchFactor(3, 0); 
    
    // 显式设置初始大小比例 (设置 NoteList 为 202)
    splitter->setSizes({220, 230, 800, 230});

    contentLayout->addWidget(splitter);
    mainLayout->addWidget(contentWidget);

    m_quickPreview = new QuickPreview(this);
    connect(m_quickPreview, &QuickPreview::editRequested, this, [this](int id){
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
        win->show();
    });

    m_noteList->installEventFilter(this);
    
    m_header->setMetadataActive(true);
}

void MainWindow::onNoteAdded(const QVariantMap& note) {
    m_noteModel->prependNote(note);
    m_noteList->scrollToTop();
}

void MainWindow::refreshData() {
    // 保存当前选中项状态以供恢复
    QString selectedType;
    QVariant selectedValue;
    QModelIndex currentIdx = m_sideBar->currentIndex();
    if (currentIdx.isValid()) {
        selectedType = currentIdx.data(CategoryModel::TypeRole).toString();
        if (selectedType == "category") {
            selectedValue = currentIdx.data(CategoryModel::IdRole);
        } else {
            selectedValue = currentIdx.data(CategoryModel::NameRole);
        }
    }

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
    
    QVariantMap criteria = m_filterPanel->getCheckedCriteria();
    if (!criteria.isEmpty()) {
        QList<QVariantMap> filtered;
        for (const auto& note : std::as_const(notes)) {
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
            
            if (match && criteria.contains("date_create")) {
                bool dateMatch = false;
                QDateTime now = QDateTime::currentDateTime();
                QDateTime createdAt;
                
                QVariant cv = note["created_at"];
                if (cv.typeId() == QMetaType::QDateTime) {
                    createdAt = cv.toDateTime();
                } else {
                    createdAt = QDateTime::fromString(cv.toString(), Qt::ISODate);
                    if (!createdAt.isValid()) {
                        createdAt = QDateTime::fromString(cv.toString(), "yyyy-MM-dd HH:mm:ss");
                    }
                }

                if (createdAt.isValid()) {
                    for (const QString& d_opt : criteria["date_create"].toStringList()) {
                        if (d_opt == "today") {
                            if (createdAt.date() == now.date()) dateMatch = true;
                        } else if (d_opt == "yesterday") {
                            if (createdAt.date() == now.date().addDays(-1)) dateMatch = true;
                        } else if (d_opt == "this_week" || d_opt == "week") {
                            if (createdAt.date() >= now.date().addDays(-6)) dateMatch = true;
                        } else if (d_opt == "this_month" || d_opt == "month") {
                            if (createdAt.date().year() == now.date().year() && createdAt.date().month() == now.date().month()) dateMatch = true;
                        }
                        if (dateMatch) break;
                    }
                    if (!dateMatch) match = false;
                }
            }

            if (match) filtered.append(note);
        }
        notes = filtered;
        totalCount = notes.size(); 
    }

    // 检查当前分类是否锁定
    bool isLocked = false;
    if (m_currentFilterType == "category" && m_currentFilterValue != -1) {
        int catId = m_currentFilterValue.toInt();
        if (DatabaseManager::instance().isCategoryLocked(catId)) {
            isLocked = true;
            QString hint;
            auto cats = DatabaseManager::instance().getAllCategories();
            for(const auto& c : std::as_const(cats)) if(c["id"].toInt() == catId) hint = c["password_hint"].toString();
            m_lockWidget->setCategory(catId, hint);
        }
    }

    m_noteList->setVisible(!isLocked);
    m_lockWidget->setVisible(isLocked);

    if (isLocked) {
        m_editor->setPlainText("");
        m_metaPanel->clearSelection();
    }

    m_noteModel->setNotes(isLocked ? QList<QVariantMap>() : notes);
    m_sideModel->refresh();

    int totalPages = (totalCount + m_pageSize - 1) / m_pageSize;
    if (totalPages < 1) totalPages = 1;
    m_header->updatePagination(m_currentPage, totalPages);

    for (int i = 0; i < m_sideModel->rowCount(); ++i) {
        QModelIndex index = m_sideModel->index(i, 0);
        QString name = index.data(CategoryModel::NameRole).toString();
        QString type = index.data(CategoryModel::TypeRole).toString();

        // 恢复选中
        if (!selectedType.isEmpty() && type == selectedType && index.data(CategoryModel::NameRole) == selectedValue) {
            m_sideBar->setCurrentIndex(index);
        }

        if (name == "我的分区" || expandedPaths.contains(name)) {
            m_sideBar->setExpanded(index, true);
        }
        
        std::function<void(const QModelIndex&)> restoreChildren = [&](const QModelIndex& parent) {
            for (int j = 0; j < m_sideModel->rowCount(parent); ++j) {
                QModelIndex child = m_sideModel->index(j, 0, parent);
                QString cType = child.data(CategoryModel::TypeRole).toString();
                QString cName = child.data(CategoryModel::NameRole).toString();
                
                // 恢复选中
                if (!selectedType.isEmpty() && cType == selectedType) {
                    if (cType == "category" && child.data(CategoryModel::IdRole) == selectedValue) {
                        m_sideBar->setCurrentIndex(child);
                    } else if (cType != "category" && cName == selectedValue) {
                        m_sideBar->setCurrentIndex(child);
                    }
                }

                QString identifier = (cType == "category") ? 
                    ("cat_" + QString::number(child.data(CategoryModel::IdRole).toInt())) : cName;

                if (expandedPaths.contains(identifier) || (parent.data(CategoryModel::NameRole).toString() == "我的分区")) {
                    m_sideBar->setExpanded(child, true);
                }
                if (m_sideModel->rowCount(child) > 0) restoreChildren(child);
            }
        };
        restoreChildren(index);
    }

    if (!m_filterWrapper->isHidden()) {
        m_filterPanel->updateStats(m_currentKeyword, m_currentFilterType, m_currentFilterValue);
    }
}

void MainWindow::onNoteSelected(const QModelIndex& index) {
}

void MainWindow::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    QModelIndexList indices = m_noteList->selectionModel()->selectedIndexes();
    if (indices.isEmpty()) {
        m_metaPanel->clearSelection();
    } else if (indices.size() == 1) {
        int id = indices.first().data(NoteModel::IdRole).toInt();
        QVariantMap note = DatabaseManager::instance().getNoteById(id);
        m_editor->setNote(note);
        m_metaPanel->setNote(note);
    } else {
        m_metaPanel->setMultipleNotes(indices.size());
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        doPreview();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_noteList && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Delete) {
            // 处理删除快捷键
            QModelIndexList indices = m_noteList->selectionModel()->selectedIndexes();
            if (indices.isEmpty()) return true;

            if (m_currentFilterType == "trash") {
                if (QMessageBox::question(this, "确认彻底删除", QString("确定要永久删除选中的 %1 条数据吗？此操作不可逆。").arg(indices.count())) == QMessageBox::Yes) {
                    QList<int> ids;
                    for (const auto& index : std::as_const(indices)) ids << index.data(NoteModel::IdRole).toInt();
                    DatabaseManager::instance().deleteNotesBatch(ids);
                    refreshData();
                }
            } else {
                QList<int> ids;
                for (const auto& index : std::as_const(indices)) ids << index.data(NoteModel::IdRole).toInt();
                DatabaseManager::instance().softDeleteNotes(ids);
                refreshData();
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Space) {
            doPreview();
            return true;
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

    QAction* actDel = menu.addAction(IconHelper::getIcon("trash", "#aaaaaa"), 
                                    (m_currentFilterType == "trash") ? "彻底删除 (不可逆)" : "移至回收站");
    connect(actDel, &QAction::triggered, [this, id](){
        if (m_currentFilterType == "trash") {
            // 回收站内直接删除，无视保护机制
            if (QMessageBox::question(this, "确认彻底删除", "确定要永久从数据库中删除这条数据吗？此操作不可逆。") == QMessageBox::Yes) {
                DatabaseManager::instance().deleteNote(id);
                refreshData();
            }
        } else {
            // 移至回收站：解除所有绑定 (置顶、收藏、分类)
            DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
            DatabaseManager::instance().updateNoteState(id, "is_pinned", 0);
            DatabaseManager::instance().updateNoteState(id, "is_favorite", 0);
            DatabaseManager::instance().updateNoteState(id, "category_id", QVariant());
            refreshData();
        }
    });

    menu.exec(QCursor::pos());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveLayout();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveLayout() {
    QSettings settings("RapidNotes", "MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    QSplitter* splitter = findChild<QSplitter*>();
    if (splitter) {
            settings.setValue("splitterState", splitter->saveState());
    }
}

void MainWindow::restoreLayout() {
    QSettings settings("RapidNotes", "MainWindow");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }
    
    QSplitter* splitter = findChild<QSplitter*>();
    if (splitter && settings.contains("splitterState")) {
        splitter->restoreState(settings.value("splitterState").toByteArray());
    }

    QSettings globalSettings("RapidNotes", "QuickWindow");
    m_autoCategorizeClipboard = globalSettings.value("autoCategorizeClipboard", false).toBool();
}

void MainWindow::showToolboxMenu(const QPoint& pos) {
    // 每次打开前刷新设置，确保与 QuickWindow 同步
    QSettings globalSettings("RapidNotes", "QuickWindow");
    m_autoCategorizeClipboard = globalSettings.value("autoCategorizeClipboard", false).toBool();

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                       "QMenu::item { padding: 6px 10px 6px 28px; border-radius: 3px; } "
                       "QMenu::item:selected { background-color: #4a90e2; color: white; }");

    QAction* autoCatAction = menu.addAction("剪贴板自动归档到当前分类");
    autoCatAction->setCheckable(true);
    autoCatAction->setChecked(m_autoCategorizeClipboard);
    connect(autoCatAction, &QAction::triggered, [this](bool checked){
        m_autoCategorizeClipboard = checked;
        QSettings settings("RapidNotes", "QuickWindow");
        settings.setValue("autoCategorizeClipboard", m_autoCategorizeClipboard);
        QToolTip::showText(QCursor::pos(), m_autoCategorizeClipboard ? "✅ 剪贴板自动归档已开启" : "❌ 剪贴板自动归档已关闭", this);
    });

    menu.addSeparator();

    menu.addAction(IconHelper::getIcon("settings", "#aaaaaa"), "更多设置...", [this]() {
        auto* dlg = new SettingsWindow(this);
        dlg->exec();
    });

    menu.exec(pos);
}

void MainWindow::doPreview() {
    // 保护：如果焦点在输入框，空格键应保留其原始打字功能
    QWidget* focusWidget = QApplication::focusWidget();
    if (focusWidget && (qobject_cast<QLineEdit*>(focusWidget) || 
                        qobject_cast<QTextEdit*>(focusWidget) ||
                        qobject_cast<QPlainTextEdit*>(focusWidget))) {
        // 允许空格键在输入框中输入
        return;
    }

    if (m_quickPreview->isVisible()) {
        m_quickPreview->hide();
        return;
    }
    QModelIndex index = m_noteList->currentIndex();
    if (!index.isValid()) return;
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
    m_quickPreview->raise();
    m_quickPreview->activateWindow();
}
