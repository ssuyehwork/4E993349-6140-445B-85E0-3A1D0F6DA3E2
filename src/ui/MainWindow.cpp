#include "MainWindow.h"
#include "../core/DatabaseManager.h"
#include "NoteDelegate.h"
#include "CategoryDelegate.h"
#include "IconHelper.h"
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
#include <QMouseEvent>
#include <QCloseEvent>
#include <QItemSelection>
#include <QActionGroup>
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
#include <QClipboard>
#include <QMimeData>
#include <QPlainTextEdit>
#include "CleanListView.h"
#include "NoteEditWindow.h"
#include "FramelessDialog.h"
#include "CategoryPasswordDialog.h"
#include "SettingsWindow.h"
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#define RESIZE_MARGIN 10
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent, Qt::FramelessWindowHint) {
    setWindowTitle("极速灵感 (RapidNotes) - 开发版");
    resize(1200, 800);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
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
    centralWidget->setMouseTracking(true);
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
#ifdef Q_OS_WIN
            HWND hwnd = (HWND)win->winId();
            if (checked) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            } else {
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            }
#else
            Qt::WindowFlags flags = win->windowFlags();
            if (checked) flags |= Qt::WindowStaysOnTopHint;
            else flags &= ~Qt::WindowStaysOnTopHint;
            
            if (flags != win->windowFlags()) {
                win->setWindowFlags(flags);
                win->show();
            }
#endif
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
    sidebarContainer->setMinimumWidth(230);
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
    connect(sidebarHeader, &QWidget::customContextMenuRequested, this, [this, sidebarContainer, splitter, sidebarHeader](const QPoint& pos){
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                           "QMenu::icon { margin-left: 4px; } "
                           "QMenu::item:selected { background-color: #3E3E42; }");
        menu.addAction(IconHelper::getIcon("nav_prev", "#aaaaaa", 18), "向左移动", [this, sidebarContainer, splitter](){
            int index = splitter->indexOf(sidebarContainer);
            if (index > 0) splitter->insertWidget(index - 1, sidebarContainer);
        });
        menu.addAction("向右移动", [this, sidebarContainer, splitter](){
            int index = splitter->indexOf(sidebarContainer);
            if (index < splitter->count() - 1) splitter->insertWidget(index + 1, sidebarContainer);
        });
        menu.exec(sidebarHeader->mapToGlobal(pos));
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
                           "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                           "QMenu::icon { margin-left: 4px; } "
                           "QMenu::item:selected { background-color: #4a90e2; color: white; }");

        if (!index.isValid() || index.data().toString() == "我的分区") {
            menu.addAction(IconHelper::getIcon("add", "#3498db", 18), "新建分组", [this]() {
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

            menu.addAction(IconHelper::getIcon("add", "#3498db", 18), "新建数据", [this, catId]() {
                auto* win = new NoteEditWindow();
                win->setDefaultCategory(catId);
                connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
                win->show();
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("palette", "#e67e22", 18), "设置颜色", [this, catId]() {
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
            menu.addAction(IconHelper::getIcon("random_color", "#FF6B9D", 18), "随机颜色", [this, catId]() {
                static const QStringList palette = {
                    "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FFEEAD",
                    "#D4A5A5", "#9B59B6", "#3498DB", "#E67E22", "#2ECC71",
                    "#E74C3C", "#F1C40F", "#1ABC9C", "#34495E", "#95A5A6"
                };
                QString chosenColor = palette.at(QRandomGenerator::global()->bounded(palette.size()));
                DatabaseManager::instance().setCategoryColor(catId, chosenColor);
                refreshData();
            });
            menu.addAction(IconHelper::getIcon("tag", "#FFAB91", 18), "设置预设标签", [this, catId]() {
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
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa", 18), "新建分组", [this]() {
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
            menu.addAction(IconHelper::getIcon("add", "#3498db", 18), "新建子分区", [this, catId]() {
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
            menu.addAction(IconHelper::getIcon("edit", "#aaaaaa", 18), "重命名分类", [this, catId, currentName]() {
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
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c", 18), "删除分类", [this, catId]() {
                auto* dlg = new FramelessMessageBox("确认删除", "确定要删除此分类吗？内容将移至未分类。", this);
                connect(dlg, &FramelessMessageBox::confirmed, [this, catId](){
                    DatabaseManager::instance().deleteCategory(catId);
                    refreshData();
                });
                dlg->show();
            });

            menu.addSeparator();
            auto* pwdMenu = menu.addMenu(IconHelper::getIcon("lock", "#aaaaaa", 18), "密码保护");
            pwdMenu->setStyleSheet(menu.styleSheet());
            
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
                        for(const auto& c : std::as_const(cats)) if(c.value("id").toInt() == catId) currentHint = c.value("password_hint").toString();
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
            menu.addAction(IconHelper::getIcon("refresh", "#2ecc71", 18), "全部恢复 (到未分类)", [this](){
                DatabaseManager::instance().restoreAllFromTrash();
                refreshData();
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c", 18), "清空回收站", [this]() {
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
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                           "QMenu::icon { margin-left: 4px; } "
                           "QMenu::item:selected { background-color: #3E3E42; }");
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
    
    // 4. 编辑器容器 (Card) - 独立出来
    auto* editorContainer = new QFrame();
    editorContainer->setMinimumWidth(230);
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
    m_editLockBtn = new QPushButton();
    m_editLockBtn->setFixedSize(24, 24);
    m_editLockBtn->setCursor(Qt::PointingHandCursor);
    m_editLockBtn->setCheckable(true);
    m_editLockBtn->setEnabled(false); // 初始禁用
    m_editLockBtn->setToolTip("请先选择一条笔记以启用编辑");
    m_editLockBtn->setIcon(IconHelper::getIcon("edit", "#555555")); // 初始灰色
    m_editLockBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover:enabled { background-color: rgba(255, 255, 255, 0.1); }"
        "QPushButton:checked { background-color: rgba(74, 144, 226, 0.2); }"
        "QPushButton:disabled { opacity: 0.5; }"
    );
    editorHeaderLayout->addWidget(m_editLockBtn);
    
    editorHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editorHeader, &QWidget::customContextMenuRequested, this, [this, editorContainer, splitter, editorHeader](const QPoint& pos){
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                           "QMenu::icon { margin-left: 4px; } "
                           "QMenu::item:selected { background-color: #3E3E42; }");
        menu.addAction("向左移动", [this, editorContainer, splitter](){
            int index = splitter->indexOf(editorContainer);
            if (index > 0) splitter->insertWidget(index - 1, editorContainer);
        });
        menu.addAction("向右移动", [this, editorContainer, splitter](){
            int index = splitter->indexOf(editorContainer);
            if (index < splitter->count() - 1) splitter->insertWidget(index + 1, editorContainer);
        });
        menu.exec(editorHeader->mapToGlobal(pos));
    });

    editorContainerLayout->addWidget(editorHeader);

    // --- 编辑器工具栏 (同步 NoteEditWindow) ---
    m_editorToolbar = new QWidget();
    m_editorToolbar->setVisible(false);
    m_editorToolbar->setStyleSheet("background-color: #252526; border-bottom: 1px solid #333;");
    auto* toolBarLayout = new QHBoxLayout(m_editorToolbar);
    toolBarLayout->setContentsMargins(10, 2, 10, 2);
    toolBarLayout->setSpacing(0);

    QString toolBtnStyle = "QPushButton { background: transparent; border: none; border-radius: 4px; padding: 4px; } "
                           "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); } "
                           "QPushButton:checked { background-color: rgba(74, 144, 226, 0.2); }";

    auto addTool = [&](const QString& iconName, const QString& tip, std::function<void()> callback) {
        QPushButton* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon(iconName, "#aaaaaa", 18));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tip);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(toolBtnStyle);
        connect(btn, &QPushButton::clicked, callback);
        toolBarLayout->addWidget(btn);
        return btn;
    };

    addTool("undo", "撤销 (Ctrl+Z)", [this](){ m_editor->undo(); });
    addTool("redo", "重做 (Ctrl+Y)", [this](){ m_editor->redo(); });
    
    auto* sep1 = new QFrame();
    sep1->setFixedWidth(1); sep1->setFixedHeight(16); sep1->setStyleSheet("background-color: #444; margin: 0 4px;");
    toolBarLayout->addWidget(sep1);

    addTool("list_ul", "无序列表", [this](){ m_editor->toggleList(false); });
    addTool("list_ol", "有序列表", [this](){ m_editor->toggleList(true); });
    addTool("todo", "插入待办", [this](){ m_editor->insertTodo(); });
    
    auto* btnPre = addTool("eye", "Markdown 预览", nullptr);
    btnPre->setCheckable(true);
    connect(btnPre, &QPushButton::toggled, [this](bool checked){ m_editor->togglePreview(checked); });

    addTool("edit_clear", "清除格式", [this](){ m_editor->clearFormatting(); });

    auto* sep2 = new QFrame();
    sep2->setFixedWidth(1); sep2->setFixedHeight(16); sep2->setStyleSheet("background-color: #444; margin: 0 4px;");
    toolBarLayout->addWidget(sep2);

    // 高亮颜色
    QStringList hColors = {"#c0392b", "#f1c40f", "#27ae60", "#2980b9"};
    for (const auto& color : hColors) {
        QPushButton* hBtn = new QPushButton();
        hBtn->setFixedSize(18, 18);
        hBtn->setStyleSheet(QString("QPushButton { background-color: %1; border-radius: 4px; margin: 2px; } QPushButton:hover { border: 1px solid white; }").arg(color));
        connect(hBtn, &QPushButton::clicked, [this, color](){ m_editor->highlightSelection(QColor(color)); });
        toolBarLayout->addWidget(hBtn);
    }

    // 清除高亮按钮
    QPushButton* btnNoColor = new QPushButton();
    btnNoColor->setIcon(IconHelper::getIcon("no_color", "#aaaaaa", 14));
    btnNoColor->setIconSize(QSize(14, 14));
    btnNoColor->setFixedSize(22, 22);
    btnNoColor->setToolTip("清除高亮");
    btnNoColor->setStyleSheet("QPushButton { background: transparent; border: 1px solid #444; border-radius: 4px; margin-left: 4px; } "
                              "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); border-color: #888; }");
    btnNoColor->setCursor(Qt::PointingHandCursor);
    connect(btnNoColor, &QPushButton::clicked, [this](){ m_editor->highlightSelection(Qt::transparent); });
    toolBarLayout->addWidget(btnNoColor);

    toolBarLayout->addStretch();
    
    auto* btnSave = addTool("save", "保存修改 (Ctrl+S)", [this](){ saveCurrentNote(); });
    btnSave->setIcon(IconHelper::getIcon("save", "#2ecc71", 18));

    editorContainerLayout->addWidget(m_editorToolbar);

    // --- 编辑器搜索栏 ---
    m_editorSearchBar = new QWidget();
    m_editorSearchBar->setVisible(false);
    m_editorSearchBar->setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #333;");
    auto* esLayout = new QHBoxLayout(m_editorSearchBar);
    esLayout->setContentsMargins(15, 4, 15, 4);
    
    m_editorSearchEdit = new QLineEdit();
    m_editorSearchEdit->setPlaceholderText("在内容中查找...");
    m_editorSearchEdit->setStyleSheet("border: none; background: transparent; color: #fff; font-size: 12px;");
    connect(m_editorSearchEdit, &QLineEdit::returnPressed, [this](){ m_editor->findText(m_editorSearchEdit->text()); });
    
    auto* btnPrev = new QPushButton();
    btnPrev->setIcon(IconHelper::getIcon("nav_prev", "#ccc", 14));
    btnPrev->setFixedSize(24, 24);
    btnPrev->setStyleSheet("background: transparent; border: none;");
    connect(btnPrev, &QPushButton::clicked, [this](){ m_editor->findText(m_editorSearchEdit->text(), true); });
    
    auto* btnNext = new QPushButton();
    btnNext->setIcon(IconHelper::getIcon("nav_next", "#ccc", 14));
    btnNext->setFixedSize(24, 24);
    btnNext->setStyleSheet("background: transparent; border: none;");
    connect(btnNext, &QPushButton::clicked, [this](){ m_editor->findText(m_editorSearchEdit->text(), false); });

    auto* btnCloseSearch = new QPushButton();
    btnCloseSearch->setIcon(IconHelper::getIcon("close", "#888", 14));
    btnCloseSearch->setFixedSize(24, 24);
    btnCloseSearch->setStyleSheet("background: transparent; border: none;");
    connect(btnCloseSearch, &QPushButton::clicked, [this](){ m_editorSearchBar->hide(); });

    esLayout->addWidget(m_editorSearchEdit);
    esLayout->addWidget(btnPrev);
    esLayout->addWidget(btnNext);
    esLayout->addWidget(btnCloseSearch);
    editorContainerLayout->addWidget(m_editorSearchBar);

    // 内容容器
    auto* editorContent = new QWidget();
    editorContent->setAttribute(Qt::WA_StyledBackground, true);
    editorContent->setStyleSheet("background: transparent; border: none;");
    auto* editorContentLayout = new QVBoxLayout(editorContent);
    editorContentLayout->setContentsMargins(2, 2, 2, 2); // 编辑器保留微量对齐边距

    m_editor = new Editor();
    m_editor->togglePreview(false);
    m_editor->setReadOnly(true); // 默认不可编辑

    connect(m_editLockBtn, &QPushButton::toggled, this, [this](bool checked){
        m_editor->setReadOnly(!checked);
        m_editorToolbar->setVisible(checked);
        if (!checked) m_editorSearchBar->hide();

        // 核心修复：切换模式时重新同步内容，防止预览标题污染正文
        QModelIndex index = m_noteList->currentIndex();
        if (index.isValid()) {
            int id = index.data(NoteModel::IdRole).toInt();
            QVariantMap note = DatabaseManager::instance().getNoteById(id);
            // 模式切换：编辑模式不带标题(false)，预览模式带标题(true)
            m_editor->setNote(note, !checked);
        }

        if (checked) {
            m_editLockBtn->setIcon(IconHelper::getIcon("eye", "#4a90e2"));
            m_editLockBtn->setToolTip("当前：编辑模式 (点击切回预览)");
        } else {
            m_editLockBtn->setIcon(IconHelper::getIcon("edit", "#aaaaaa"));
            m_editLockBtn->setToolTip("当前：锁定模式 (点击解锁编辑)");
        }
    });
    
    editorContentLayout->addWidget(m_editor);
    editorContainerLayout->addWidget(editorContent);
    
    // 直接放入 Splitter
    splitter->addWidget(editorContainer);

    // 5. 元数据面板 - 独立出来
    m_metaPanel = new MetadataPanel(this);
    m_metaPanel->setMinimumWidth(230);
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
    
    // 给元数据面板添加右键移动菜单
    auto* metaHeader = m_metaPanel->findChild<QWidget*>("MetadataHeader");
    if (metaHeader) {
        metaHeader->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(metaHeader, &QWidget::customContextMenuRequested, this, [this, splitter, metaHeader](const QPoint& pos){
            QMenu menu;
            menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                               "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                               "QMenu::icon { margin-left: 4px; } "
                               "QMenu::item:selected { background-color: #3E3E42; }");
            menu.addAction("向左移动", [this, splitter](){
                int index = splitter->indexOf(m_metaPanel);
                if (index > 0) splitter->insertWidget(index - 1, m_metaPanel);
            });
            menu.addAction("向右移动", [this, splitter](){
                int index = splitter->indexOf(m_metaPanel);
                if (index < splitter->count() - 1) splitter->insertWidget(index + 1, m_metaPanel);
            });
            menu.exec(metaHeader->mapToGlobal(pos));
        });
    }

    splitter->addWidget(m_metaPanel);

    // 6. 高级筛选器卡片容器
    auto* filterContainer = new QFrame();
    filterContainer->setMinimumWidth(230);
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

    auto* filterContainerLayout = new QVBoxLayout(filterContainer);
    filterContainerLayout->setContentsMargins(0, 0, 0, 0);
    filterContainerLayout->setSpacing(0);

    // 筛选器标题栏
    auto* filterHeader = new QWidget();
    filterHeader->setFixedHeight(32);
    filterHeader->setStyleSheet(
        "background-color: #252526; "
        "border-top-left-radius: 12px; "
        "border-top-right-radius: 12px; "
        "border-bottom: 1px solid #333;"
    );
    auto* filterHeaderLayout = new QHBoxLayout(filterHeader);
    filterHeaderLayout->setContentsMargins(15, 0, 15, 0);
    auto* fiIcon = new QLabel();
    fiIcon->setPixmap(IconHelper::getIcon("filter", "#f1c40f").pixmap(18, 18));
    filterHeaderLayout->addWidget(fiIcon);
    auto* fiTitle = new QLabel("高级筛选");
    fiTitle->setStyleSheet("color: #f1c40f; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    filterHeaderLayout->addWidget(fiTitle);
    filterHeaderLayout->addStretch();
    
    filterHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(filterHeader, &QWidget::customContextMenuRequested, this, [this, filterContainer, splitter, filterHeader](const QPoint& pos){
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                           "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                           "QMenu::icon { margin-left: 4px; } "
                           "QMenu::item:selected { background-color: #3E3E42; }");
        menu.addAction("向左移动", [this, filterContainer, splitter](){
            int index = splitter->indexOf(filterContainer);
            if (index > 0) splitter->insertWidget(index - 1, filterContainer);
        });
        menu.addAction("向右移动", [this, filterContainer, splitter](){
            int index = splitter->indexOf(filterContainer);
            if (index < splitter->count() - 1) splitter->insertWidget(index + 1, filterContainer);
        });
        menu.exec(filterHeader->mapToGlobal(pos));
    });
    
    filterContainerLayout->addWidget(filterHeader);

    // 内容容器
    auto* filterContent = new QWidget();
    filterContent->setAttribute(Qt::WA_StyledBackground, true);
    filterContent->setStyleSheet("background: transparent; border: none;");
    auto* filterContentLayout = new QVBoxLayout(filterContent);
    filterContentLayout->setContentsMargins(10, 10, 10, 10);

    m_filterPanel = new FilterPanel(this);
    m_filterPanel->setStyleSheet("background: transparent; border: none;");
    connect(m_filterPanel, &FilterPanel::filterChanged, this, &MainWindow::refreshData);
    filterContentLayout->addWidget(m_filterPanel);
    filterContainerLayout->addWidget(filterContent);

    m_filterWrapper = filterContainer;
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

    auto* actionSearch = new QAction(this);
    actionSearch->setShortcut(QKeySequence("Ctrl+F"));
    connect(actionSearch, &QAction::triggered, this, [this](){
        if (m_editLockBtn->isChecked()) toggleSearchBar();
        else m_header->focusSearch();
    });
    addAction(actionSearch);

    auto* actionNew = new QAction(this);
    actionNew->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNew, &QAction::triggered, this, &MainWindow::doNewIdea);
    addAction(actionNew);

    auto* actionSave = new QAction(this);
    actionSave->setShortcut(QKeySequence("Ctrl+S"));
    connect(actionSave, &QAction::triggered, this, [this](){
        if (m_editLockBtn->isChecked()) saveCurrentNote();
        else doLockSelected();
    });
    addAction(actionSave);


    splitter->setStretchFactor(0, 1); 
    splitter->setStretchFactor(1, 2); 
    splitter->setStretchFactor(2, 8); 
    splitter->setStretchFactor(3, 1); 
    splitter->setStretchFactor(4, 1);
    
    // 显式设置初始大小比例
    splitter->setSizes({230, 230, 600, 230, 230});

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

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        int x = GET_X_LPARAM(msg->lParam);
        int y = GET_Y_LPARAM(msg->lParam);
        
        QPoint pos = mapFromGlobal(QPoint(x, y));
        int margin = RESIZE_MARGIN;
        int w = width();
        int h = height();

        bool left = pos.x() < margin;
        bool right = pos.x() > w - margin;
        bool top = pos.y() < margin;
        bool bottom = pos.y() > h - margin;

        if (top && left) *result = HTTOPLEFT;
        else if (top && right) *result = HTTOPRIGHT;
        else if (bottom && left) *result = HTBOTTOMLEFT;
        else if (bottom && right) *result = HTBOTTOMRIGHT;
        else if (top) *result = HTTOP;
        else if (bottom) *result = HTBOTTOM;
        else if (left) *result = HTLEFT;
        else if (right) *result = HTRIGHT;
        else return QMainWindow::nativeEvent(eventType, message, result);

        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

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
                if (!criteria.value("stars").toStringList().contains(QString::number(note.value("rating").toInt()))) match = false;
            }
            if (match && criteria.contains("types")) {
                if (!criteria.value("types").toStringList().contains(note.value("item_type").toString())) match = false;
            }
            if (match && criteria.contains("colors")) {
                if (!criteria.value("colors").toStringList().contains(note.value("color").toString())) match = false;
            }
            if (match && criteria.contains("tags")) {
                QStringList noteTags = note.value("tags").toString().split(",", Qt::SkipEmptyParts);
                bool tagMatch = false;
                for (const QString& t : criteria.value("tags").toStringList()) {
                    if (noteTags.contains(t.trimmed())) { tagMatch = true; break; }
                }
                if (!tagMatch) match = false;
            }
            
            if (match && criteria.contains("date_create")) {
                bool dateMatch = false;
                QDateTime now = QDateTime::currentDateTime();
                QDateTime createdAt;
                
                QVariant cv = note.value("created_at");
                if (cv.typeId() == QMetaType::QDateTime) {
                    createdAt = cv.toDateTime();
                } else {
                    createdAt = QDateTime::fromString(cv.toString(), Qt::ISODate);
                    if (!createdAt.isValid()) {
                        createdAt = QDateTime::fromString(cv.toString(), "yyyy-MM-dd HH:mm:ss");
                    }
                }

                if (createdAt.isValid()) {
                    for (const QString& d_opt : criteria.value("date_create").toStringList()) {
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
            for(const auto& c : std::as_const(cats)) if(c.value("id").toInt() == catId) hint = c.value("password_hint").toString();
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
        m_editor->setPlainText("");
        m_editLockBtn->setEnabled(false);
        m_editLockBtn->setChecked(false);
        m_editLockBtn->setIcon(IconHelper::getIcon("edit", "#555555"));
        m_editLockBtn->setToolTip("请先选择一条笔记以启用编辑");
    } else if (indices.size() == 1) {
        int id = indices.first().data(NoteModel::IdRole).toInt();
        QVariantMap note = DatabaseManager::instance().getNoteById(id);
        m_editor->setNote(note, true);
        m_metaPanel->setNote(note);
        m_editLockBtn->setEnabled(true);
        // 切换笔记时自动退出编辑模式，防止误操作或内容丢失
        m_editLockBtn->setChecked(false);
        m_editLockBtn->setIcon(IconHelper::getIcon("edit", "#aaaaaa"));
        m_editLockBtn->setToolTip("点击进入编辑模式");
    } else {
        m_metaPanel->setMultipleNotes(indices.size());
        m_editor->setPlainText(QString("已选中 %1 条笔记").arg(indices.size()));
        m_editLockBtn->setEnabled(false);
        m_editLockBtn->setChecked(false);
        m_editLockBtn->setIcon(IconHelper::getIcon("edit", "#555555"));
        m_editLockBtn->setToolTip("多选状态下不可直接编辑");
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
        int key = keyEvent->key();
        auto modifiers = keyEvent->modifiers();

        if (key == Qt::Key_Delete) {
            doDeleteSelected(modifiers & Qt::ControlModifier);
            return true;
        }
        if (key == Qt::Key_Space) {
            doPreview();
            return true;
        }
        if (key == Qt::Key_E && (modifiers & Qt::ControlModifier)) {
            doToggleFavorite();
            return true;
        }
        if (key == Qt::Key_P && (modifiers & Qt::ControlModifier)) {
            doTogglePin();
            return true;
        }
        if (key == Qt::Key_S && (modifiers & Qt::ControlModifier)) {
            if (m_editLockBtn->isChecked()) saveCurrentNote();
            else doLockSelected();
            return true;
        }
        if (key == Qt::Key_B && (modifiers & Qt::ControlModifier)) {
            doEditSelected();
            return true;
        }
        if (key == Qt::Key_T && (modifiers & Qt::ControlModifier)) {
            doExtractContent();
            return true;
        }
        if (key >= Qt::Key_0 && key <= Qt::Key_5 && (modifiers & Qt::ControlModifier)) {
            doSetRating(key - Qt::Key_0);
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
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) {
        QModelIndex index = m_noteList->indexAt(pos);
        if (index.isValid()) {
            m_noteList->setCurrentIndex(index);
            selected << index;
        } else {
            return;
        }
    }

    int selCount = selected.size();
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                       "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                       "QMenu::icon { margin-left: 4px; } "
                       "QMenu::item:selected { background-color: #4a90e2; color: white; }");

    if (selCount == 1) {
        menu.addAction(IconHelper::getIcon("eye", "#1abc9c", 18), "预览 (Space)", this, &MainWindow::doPreview);
    }
    
    menu.addAction(IconHelper::getIcon("file", "#1abc9c", 18), QString("复制内容 (%1)").arg(selCount), this, &MainWindow::doExtractContent);
    menu.addSeparator();

    if (selCount == 1) {
        menu.addAction(IconHelper::getIcon("edit", "#4a90e2", 18), "编辑 (Ctrl+B)", this, &MainWindow::doEditSelected);
        menu.addSeparator();
    }

    auto* ratingMenu = menu.addMenu(IconHelper::getIcon("star", "#f39c12", 18), QString("设置星级 (%1)").arg(selCount));
    ratingMenu->setStyleSheet(menu.styleSheet());
    auto* starGroup = new QActionGroup(this);
    int currentRating = (selCount == 1) ? selected.first().data(NoteModel::RatingRole).toInt() : -1;
    
    for (int i = 1; i <= 5; ++i) {
        QString stars = QString("★").repeated(i);
        QAction* action = ratingMenu->addAction(stars, [this, i]() { doSetRating(i); });
        action->setCheckable(true);
        if (i == currentRating) action->setChecked(true);
        starGroup->addAction(action);
    }
    ratingMenu->addSeparator();
    ratingMenu->addAction("清除评级", [this]() { doSetRating(0); });

    bool isFavorite = (selCount == 1) && selected.first().data(NoteModel::FavoriteRole).toBool();
    menu.addAction(IconHelper::getIcon(isFavorite ? "bookmark_filled" : "bookmark", "#ff6b81", 18),
                   isFavorite ? "取消书签" : "添加书签 (Ctrl+E)", this, &MainWindow::doToggleFavorite);

    bool isPinned = (selCount == 1) && selected.first().data(NoteModel::PinnedRole).toBool();
    menu.addAction(IconHelper::getIcon(isPinned ? "pin_vertical" : "pin_tilted", isPinned ? "#e74c3c" : "#aaaaaa", 18),
                   isPinned ? "取消置顶" : "置顶选中项 (Ctrl+P)", this, &MainWindow::doTogglePin);
    
    bool isLocked = (selCount == 1) && selected.first().data(NoteModel::LockedRole).toBool();
    menu.addAction(IconHelper::getIcon("lock", isLocked ? "#2ecc71" : "#aaaaaa", 18),
                   isLocked ? "解锁选中项" : "锁定选中项 (Ctrl+S)", this, &MainWindow::doLockSelected);
    
    menu.addSeparator();

    auto* catMenu = menu.addMenu(IconHelper::getIcon("branch", "#cccccc", 18), QString("移动选中项到分类 (%1)").arg(selCount));
    catMenu->setStyleSheet(menu.styleSheet());
    catMenu->addAction("⚠️ 未分类", [this]() { doMoveToCategory(-1); });
    
    QSettings settings("RapidNotes", "QuickWindow");
    QVariantList recentCats = settings.value("recentCategories").toList();
    auto allCategories = DatabaseManager::instance().getAllCategories();
    QMap<int, QVariantMap> catMap;
    for (const auto& cat : std::as_const(allCategories)) catMap[cat.value("id").toInt()] = cat;

    int count = 0;
    for (const auto& v : std::as_const(recentCats)) {
        if (count >= 15) break;
        int cid = v.toInt();
        if (catMap.contains(cid)) {
            const auto& cat = catMap.value(cid);
            catMenu->addAction(IconHelper::getIcon("branch", cat.value("color").toString(), 18), cat.value("name").toString(), [this, cid]() {
                doMoveToCategory(cid);
            });
            count++;
        }
    }

    menu.addSeparator();
    if (m_currentFilterType == "trash") {
        menu.addAction(IconHelper::getIcon("refresh", "#2ecc71", 18), "恢复 (还原到未分类)", [this, selected](){
            QList<int> ids;
            for (const auto& index : selected) ids << index.data(NoteModel::IdRole).toInt();
            DatabaseManager::instance().moveNotesToCategory(ids, -1);
            refreshData();
        });
        menu.addAction(IconHelper::getIcon("trash", "#e74c3c", 18), "彻底删除 (不可逆)", [this](){ doDeleteSelected(true); });
    } else {
        menu.addAction(IconHelper::getIcon("trash", "#e74c3c", 18), "移至回收站 (Delete)", [this](){ doDeleteSelected(false); });
    }

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
                       "QMenu::item { padding: 6px 10px 6px 32px; border-radius: 3px; } "
                       "QMenu::icon { margin-left: 4px; } "
                       "QMenu::item:selected { background-color: #4a90e2; color: white; }");

    QAction* autoCatAction = menu.addAction(IconHelper::getIcon("zap", "#aaaaaa", 18), "剪贴板自动归档到当前分类");
    autoCatAction->setCheckable(true);
    autoCatAction->setChecked(m_autoCategorizeClipboard);
    connect(autoCatAction, &QAction::triggered, [this](bool checked){
        m_autoCategorizeClipboard = checked;
        QSettings settings("RapidNotes", "QuickWindow");
        settings.setValue("autoCategorizeClipboard", m_autoCategorizeClipboard);
        QToolTip::showText(QCursor::pos(), m_autoCategorizeClipboard ? "✅ 剪贴板自动归档已开启" : "❌ 剪贴板自动归档已关闭", this);
    });

    menu.addSeparator();
    
    menu.addAction(IconHelper::getIcon("settings", "#aaaaaa", 18), "更多设置...", [this]() {
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
        note.value("title").toString(), 
        note.value("content").toString(), 
        note.value("item_type").toString(),
        note.value("data_blob").toByteArray(),
        globalPos
    );
    m_quickPreview->raise();
    m_quickPreview->activateWindow();
}

void MainWindow::doDeleteSelected(bool physical) {
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    bool inTrash = (m_currentFilterType == "trash");
    
    if (physical || inTrash) {
        QString msg = QString("确定要永久删除选中的 %1 条数据吗？此操作不可逆。").arg(selected.count());
        if (QMessageBox::question(this, "确认彻底删除", msg) == QMessageBox::Yes) {
            QList<int> ids;
            for (const auto& index : selected) ids << index.data(NoteModel::IdRole).toInt();
            DatabaseManager::instance().deleteNotesBatch(ids);
            refreshData();
        }
    } else {
        QList<int> ids;
        for (const auto& index : selected) ids << index.data(NoteModel::IdRole).toInt();
        DatabaseManager::instance().softDeleteNotes(ids);
        refreshData();
    }
}

void MainWindow::doToggleFavorite() {
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        DatabaseManager::instance().toggleNoteState(id, "is_favorite");
    }
    refreshData();
}

void MainWindow::doTogglePin() {
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        DatabaseManager::instance().toggleNoteState(id, "is_pinned");
    }
    refreshData();
}

void MainWindow::doLockSelected() {
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    
    int firstId = selected.first().data(NoteModel::IdRole).toInt();
    bool firstState = selected.first().data(NoteModel::LockedRole).toBool();
    bool targetState = !firstState;

    QList<int> ids;
    for (const auto& index : selected) ids << index.data(NoteModel::IdRole).toInt();
    
    DatabaseManager::instance().updateNoteStateBatch(ids, "is_locked", targetState);
    refreshData();
}

void MainWindow::doNewIdea() {
    NoteEditWindow* win = new NoteEditWindow();
    connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
    win->show();
}

void MainWindow::doExtractContent() {
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    QStringList texts;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        QVariantMap note = DatabaseManager::instance().getNoteById(id);
        if (note.value("item_type").toString() == "text" || note.value("item_type").toString().isEmpty()) {
            texts << note.value("content").toString();
        }
    }
    if (!texts.isEmpty()) {
        QApplication::clipboard()->setText(texts.join("\n---\n"));
    }
}

void MainWindow::doEditSelected() {
    QModelIndex index = m_noteList->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(NoteModel::IdRole).toInt();
    NoteEditWindow* win = new NoteEditWindow(id);
    connect(win, &NoteEditWindow::noteSaved, this, &MainWindow::refreshData);
    win->show();
}

void MainWindow::doSetRating(int rating) {
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        DatabaseManager::instance().updateNoteState(id, "rating", rating);
    }
    refreshData();
}

void MainWindow::doMoveToCategory(int catId) {
    auto selected = m_noteList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QList<int> ids;
    for (const auto& index : selected) ids << index.data(NoteModel::IdRole).toInt();
    
    DatabaseManager::instance().moveNotesToCategory(ids, catId);
    refreshData();
}

void MainWindow::saveCurrentNote() {
    QModelIndex index = m_noteList->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(NoteModel::IdRole).toInt();
    
    QString content = m_editor->toHtml();
    DatabaseManager::instance().updateNoteState(id, "content", content);
    
    // 退出编辑模式
    m_editLockBtn->setChecked(false);
    refreshData();
    QToolTip::showText(QCursor::pos(), "✅ 内容已保存", this);
}

void MainWindow::toggleSearchBar() {
    m_editorSearchBar->setVisible(!m_editorSearchBar->isVisible());
    if (m_editorSearchBar->isVisible()) {
        m_editorSearchEdit->setFocus();
        m_editorSearchEdit->selectAll();
    }
}
