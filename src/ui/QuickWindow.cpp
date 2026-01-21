#include "QuickWindow.h"
#include "NoteEditWindow.h"
#include "QuickNoteDelegate.h"
#include "IconHelper.h"
#include "../core/DatabaseManager.h"
#include "../core/ClipboardMonitor.h"
#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QSettings>
#include <QMenu>
#include <QWindow>
#include <QShortcut>
#include <QKeySequence>
#include <QClipboard>
#include <QMimeData>
#include <QTimer>
#include <QApplication>
#include <QActionGroup>
#include <QAction>
#include <QUrl>
#include <QToolTip>
#include <QImage>
#include <QMap>
#include <QFileInfo>
#include <QInputDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QRandomGenerator>

QuickWindow::QuickWindow(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    initUI();

    // 修复：由于信号增加了参数，这里使用 lambda 忽略参数即可
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, [this](const QVariantMap&){
        refreshData();
    });

    // 连接剪贴板监控，实现 Python 版的实时捕获反馈
    connect(&ClipboardMonitor::instance(), &ClipboardMonitor::newContentDetected, [this](){
        refreshData();
    });

    connect(&DatabaseManager::instance(), &DatabaseManager::categoriesChanged, [this](){
        m_systemModel->refresh();
        m_partitionModel->refresh();
        m_model->updateCategoryMap();
        refreshData();
    });

#ifdef Q_OS_WIN
    m_monitorTimer = new QTimer(this);
    connect(m_monitorTimer, &QTimer::timeout, [this]() {
        HWND currentHwnd = GetForegroundWindow();
        if (currentHwnd == 0 || currentHwnd == (HWND)winId()) return;
        if (currentHwnd != m_lastActiveHwnd) {
            m_lastActiveHwnd = currentHwnd;
            m_lastThreadId = GetWindowThreadProcessId(m_lastActiveHwnd, nullptr);
            
            GUITHREADINFO gti;
            gti.cbSize = sizeof(GUITHREADINFO);
            if (GetGUIThreadInfo(m_lastThreadId, &gti)) {
                m_lastFocusHwnd = gti.hwndFocus;
            } else {
                m_lastFocusHwnd = nullptr;
            }
        }
    });
    m_monitorTimer->start(200);
#endif
}

void QuickWindow::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15); // 给阴影留出空间

    auto* container = new QWidget();
    container->setObjectName("container");
    container->setStyleSheet(
        "QWidget#container { background: #1E1E1E; border-radius: 10px; border: 1px solid #333; }"
        "QLineEdit { background: transparent; border: none; color: white; font-size: 18px; padding: 10px; border-bottom: 1px solid #333; }"
        "QListView, QTreeView { background: transparent; border: none; color: #BBB; outline: none; }"
        "QListView::item, QTreeView::item { padding: 8px; }"
        "QListView::item:selected, QTreeView::item:selected { background: #37373D; border-radius: 4px; }"
    );
    
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 4);
    container->setGraphicsEffect(shadow);

    auto* containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto* leftContent = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftContent);
    leftLayout->setContentsMargins(10, 10, 10, 5);
    leftLayout->setSpacing(8);
    
    m_searchEdit = new SearchLineEdit();
    m_searchEdit->setPlaceholderText("搜索灵感 (双击查看历史)");
    m_searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchEdit);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setHandleWidth(4);
    
    // Python 对齐：侧边栏在左
    auto* sidebarContainer = new QWidget();
    auto* sidebarLayout = new QVBoxLayout(sidebarContainer);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    m_systemTree = new DropTreeView();
    m_systemModel = new CategoryModel(CategoryModel::System, this);
    m_systemTree->setModel(m_systemModel);
    m_systemTree->setHeaderHidden(true);
    m_systemTree->setFixedHeight(150); // Python 版固定高度

    m_partitionTree = new DropTreeView();
    m_partitionModel = new CategoryModel(CategoryModel::User, this);
    m_partitionTree->setModel(m_partitionModel);
    m_partitionTree->setHeaderHidden(true);
    m_partitionTree->expandAll();

    sidebarLayout->addWidget(m_systemTree);
    sidebarLayout->addWidget(m_partitionTree);

    // 列表在右
    m_listView = new QListView();
    m_listView->setDragEnabled(true);
    m_listView->setAlternatingRowColors(false); // 禁用斑马纹
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setIconSize(QSize(28, 28));
    m_model = new NoteModel(this);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(new QuickNoteDelegate(m_listView));
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, &QuickWindow::showListContextMenu);
    connect(m_listView, &QListView::doubleClicked, this, [this](const QModelIndex& index){
        activateNote(index);
    });

    auto onSelectionChanged = [this](DropTreeView* tree, const QModelIndex& index) {
        if (!index.isValid()) return;

        // 互斥选中
        if (tree == m_systemTree) {
            m_partitionTree->selectionModel()->clearSelection();
            m_partitionTree->setCurrentIndex(QModelIndex());
        } else {
            m_systemTree->selectionModel()->clearSelection();
            m_systemTree->setCurrentIndex(QModelIndex());
        }

        m_currentFilterType = index.data(Qt::UserRole).toString();
        QString name = index.data(Qt::DisplayRole).toString();
        updatePartitionStatus(name);

        if (m_currentFilterType == "category") {
            m_currentFilterValue = index.data(Qt::UserRole + 1).toInt();
            applyListTheme(index.data(Qt::UserRole + 2).toString());
        } else {
            m_currentFilterValue = -1;
            applyListTheme("");
        }
        m_currentPage = 1;
        refreshData();
    };

    connect(m_systemTree, &QTreeView::clicked, [this, onSelectionChanged](const QModelIndex& idx){ onSelectionChanged(m_systemTree, idx); });
    connect(m_partitionTree, &QTreeView::clicked, [this, onSelectionChanged](const QModelIndex& idx){ onSelectionChanged(m_partitionTree, idx); });

    auto onNotesDropped = [this](const QList<int>& ids, const QModelIndex& targetIndex) {
        if (!targetIndex.isValid()) return;
        QString type = targetIndex.data(Qt::UserRole).toString();
        for (int id : ids) {
            if (type == "category") {
                int catId = targetIndex.data(Qt::UserRole + 1).toInt();
                DatabaseManager::instance().updateNoteState(id, "category_id", catId);

                QSettings settings("RapidNotes", "QuickWindow");
                QVariantList recentCats = settings.value("recentCategories").toList();
                recentCats.removeAll(catId);
                recentCats.prepend(catId);
                while (recentCats.size() > 15) recentCats.removeLast();
                settings.setValue("recentCategories", recentCats);
            } else if (type == "bookmark") {
                DatabaseManager::instance().updateNoteState(id, "is_favorite", 1);
            } else if (type == "trash") {
                DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
            } else if (type == "uncategorized") {
                DatabaseManager::instance().updateNoteState(id, "category_id", QVariant());
            }
        }
        refreshData();
    };

    connect(m_systemTree, &DropTreeView::notesDropped, [this, onNotesDropped](const QList<int>& ids, const QModelIndex& idx){ onNotesDropped(ids, idx); });
    connect(m_partitionTree, &DropTreeView::notesDropped, [this, onNotesDropped](const QList<int>& ids, const QModelIndex& idx){ onNotesDropped(ids, idx); });

    // 右键菜单逻辑
    auto showSidebarMenu = [this](DropTreeView* tree, const QPoint& pos) {
        QModelIndex index = tree->indexAt(pos);
        QMenu menu(this);
        menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; }");
        
        menu.addAction(IconHelper::getIcon("refresh", "#aaaaaa"), "刷新", [this](){
            m_systemModel->refresh();
            m_partitionModel->refresh();
            refreshData();
        });
        menu.addSeparator();

        if (tree == m_partitionTree && (!index.isValid() || index.data(Qt::DisplayRole).toString() == "我的分区")) {
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "➕ 新建分组", [this](){
                bool ok;
                QString text = QInputDialog::getText(this, "新建分组", "分组名称:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) DatabaseManager::instance().addCategory(text);
            });
            menu.exec(tree->mapToGlobal(pos));
            return;
        }

        QString type = index.data(Qt::UserRole).toString();
        if (type == "category") {
            int catId = index.data(Qt::UserRole + 1).toInt();
            QString name = index.data(Qt::DisplayRole).toString();

            menu.addAction(IconHelper::getIcon("add", "#4a90e2"), "新建灵感", [this, catId](){
                NoteEditWindow* win = new NoteEditWindow();
                win->setDefaultCategory(catId); 
                connect(win, &NoteEditWindow::noteSaved, this, &QuickWindow::refreshData);
                win->show();
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("palette", "#aaaaaa"), "设置颜色", [this, catId](){
                QColor color = QColorDialog::getColor(Qt::gray, this, "选择分类颜色");
                if (color.isValid()) DatabaseManager::instance().setCategoryColor(catId, color.name());
            });
            menu.addAction(IconHelper::getIcon("text", "#aaaaaa"), "设置预设标签", [this, catId](){
                QString current = DatabaseManager::instance().getCategoryPresetTags(catId);
                bool ok;
                QString tags = QInputDialog::getText(this, "预设标签", "自动绑定的标签 (逗号分隔):", QLineEdit::Normal, current, &ok);
                if (ok) DatabaseManager::instance().setCategoryPresetTags(catId, tags);
            });
            menu.addSeparator();
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "新建分组", [this](){
                bool ok;
                QString text = QInputDialog::getText(this, "新建分组", "分组名称:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) DatabaseManager::instance().addCategory(text);
            });
            menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "新建分区", [this, catId](){
                bool ok;
                QString text = QInputDialog::getText(this, "新建分区", "分区名称:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) DatabaseManager::instance().addCategory(text, catId);
            });
            menu.addAction(IconHelper::getIcon("edit", "#aaaaaa"), "重命名", [this, catId, name](){
                bool ok;
                QString text = QInputDialog::getText(this, "重命名", "新名称:", QLineEdit::Normal, name, &ok);
                if (ok && !text.isEmpty()) DatabaseManager::instance().renameCategory(catId, text);
            });
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "删除", [this, catId](){
                if (QMessageBox::Yes == QMessageBox::question(this, "确认删除", "确认删除此分类？")) DatabaseManager::instance().deleteCategory(catId);
            });
        } else if (type == "trash") {
            menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "清空回收站", [this](){
                if (QMessageBox::Yes == QMessageBox::warning(this, "警告", "清空回收站不可恢复，确定吗？")) {
                    DatabaseManager::instance().emptyTrash(); refreshData();
                }
            });
        }
        menu.exec(tree->mapToGlobal(pos));
    };

    m_systemTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_partitionTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_systemTree, &QTreeView::customContextMenuRequested, [this, showSidebarMenu](const QPoint& p){ showSidebarMenu(m_systemTree, p); });
    connect(m_partitionTree, &QTreeView::customContextMenuRequested, [this, showSidebarMenu](const QPoint& p){ showSidebarMenu(m_partitionTree, p); });

    m_splitter->addWidget(sidebarContainer);
    m_splitter->addWidget(m_listView);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({180, 600});
    leftLayout->addWidget(m_splitter);

    m_statusLabel = new QLabel("当前分区: 全部数据");
    m_statusLabel->setStyleSheet("font-size: 11px; color: #888; padding-left: 2px;");
    m_statusLabel->setFixedHeight(32);
    leftLayout->addWidget(m_statusLabel);

    containerLayout->addWidget(leftContent);

    // 右侧垂直工具栏
    m_toolbar = new QuickToolbar(this);
    containerLayout->addWidget(m_toolbar);
    
    mainLayout->addWidget(container);
    resize(830, 630);

    m_quickPreview = new QuickPreview(this);
    m_listView->installEventFilter(this);

    // 工具栏信号
    connect(m_toolbar, &QuickToolbar::closeRequested, this, &QuickWindow::hide);
    connect(m_toolbar, &QuickToolbar::openFullRequested, this, [this](){
        emit toggleMainWindowRequested();
        hide();
    });
    connect(m_toolbar, &QuickToolbar::minimizeRequested, this, &QuickWindow::showMinimized);
    connect(m_toolbar, &QuickToolbar::toggleStayOnTop, this, &QuickWindow::toggleStayOnTop);
    connect(m_toolbar, &QuickToolbar::toggleSidebar, this, &QuickWindow::toggleSidebar);
    connect(m_toolbar, &QuickToolbar::refreshRequested, this, &QuickWindow::refreshData);
    connect(m_toolbar, &QuickToolbar::toolboxRequested, this, &QuickWindow::toolboxRequested);

    // 分页
    connect(m_toolbar, &QuickToolbar::prevPage, this, [this](){
        if (m_currentPage > 1) { m_currentPage--; refreshData(); }
    });
    connect(m_toolbar, &QuickToolbar::nextPage, this, [this](){
        if (m_currentPage < m_totalPages) { m_currentPage++; refreshData(); }
    });
    connect(m_toolbar, &QuickToolbar::jumpToPage, this, [this](int page){
        if (page >= 1 && page <= m_totalPages) { m_currentPage = page; refreshData(); }
    });
    
    // 搜索逻辑 (带 300ms 延迟，对标 Python)
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &QuickWindow::refreshData);
    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text){
        m_currentPage = 1;
        m_searchTimer->start(300);
    });

    // 回车逻辑优化：仅在空搜索或特定条件下新增，否则保存历史
    connect(m_searchEdit, &QLineEdit::returnPressed, [this](){
        QString text = m_searchEdit->text().trimmed();
        if (text.isEmpty()) return;
        
        m_searchEdit->addHistoryEntry(text);
        
        // 如果列表中没有匹配项，或者用户确实想新增（这里简单判断，如果完全不匹配则新增）
        if (m_model->rowCount() == 0) {
            DatabaseManager::instance().addNoteAsync("快速记录", text, {"Quick"});
            m_searchEdit->clear();
            hide();
        }
    });

    setupShortcuts();
    restoreState();
    refreshData();
}

void QuickWindow::saveState() {
    QSettings settings("RapidNotes", "QuickWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("splitter", m_splitter->saveState());
    settings.setValue("sidebarHidden", m_systemTree->parentWidget()->isHidden());
    settings.setValue("stayOnTop", m_toolbar->isStayOnTop());
}

void QuickWindow::restoreState() {
    QSettings settings("RapidNotes", "QuickWindow");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("splitter")) {
        m_splitter->restoreState(settings.value("splitter").toByteArray());
    }
    if (settings.contains("sidebarHidden")) {
        m_systemTree->parentWidget()->setHidden(settings.value("sidebarHidden").toBool());
    }
    if (settings.contains("stayOnTop")) {
        toggleStayOnTop(settings.value("stayOnTop").toBool());
    }
}

void QuickWindow::setupShortcuts() {
    new QShortcut(QKeySequence("Ctrl+F"), this, [this](){ m_searchEdit->setFocus(); m_searchEdit->selectAll(); });
    new QShortcut(QKeySequence("Delete"), this, [this](){ doDeleteSelected(); });
    new QShortcut(QKeySequence("Ctrl+E"), this, [this](){ doToggleFavorite(); });
    new QShortcut(QKeySequence("Ctrl+P"), this, [this](){ doTogglePin(); });
    new QShortcut(QKeySequence("Ctrl+W"), this, [this](){ hide(); });
    new QShortcut(QKeySequence("Ctrl+S"), this, [this](){ doLockSelected(); });
    new QShortcut(QKeySequence("Ctrl+N"), this, [this](){ doNewIdea(); });
    new QShortcut(QKeySequence("Ctrl+A"), this, [this](){ m_listView->selectAll(); });
    new QShortcut(QKeySequence("Ctrl+T"), this, [this](){ doExtractContent(); });
    new QShortcut(QKeySequence("Alt+D"), this, [this](){ toggleStayOnTop(!m_toolbar->isStayOnTop()); });
    new QShortcut(QKeySequence("Alt+W"), this, [this](){ emit toggleMainWindowRequested(); hide(); });
    new QShortcut(QKeySequence("Ctrl+B"), this, [this](){ doEditSelected(); });
    new QShortcut(QKeySequence("Ctrl+Q"), this, [this](){ toggleSidebar(); });
    new QShortcut(QKeySequence("Alt+S"), this, [this](){ if(m_currentPage > 1) { m_currentPage--; refreshData(); } });
    new QShortcut(QKeySequence("Alt+X"), this, [this](){ if(m_currentPage < m_totalPages) { m_currentPage++; refreshData(); } });
    
    for (int i = 0; i <= 5; ++i) {
        new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i)), this, [this, i](){ doSetRating(i); });
    }
    
    new QShortcut(QKeySequence(Qt::Key_Space), this, [this](){ doPreview(); });
}

void QuickWindow::refreshData() {
    QString keyword = m_searchEdit->text();
    
    // 1. 获取当前筛选条件下的总数
    int totalCount = DatabaseManager::instance().getNotesCount(keyword, m_currentFilterType, m_currentFilterValue);
    
    // 2. 计算总页数 (每页100条，对标 Python 版)
    const int pageSize = 100;
    m_totalPages = qMax(1, (totalCount + pageSize - 1) / pageSize); 
    if (m_currentPage > m_totalPages) m_currentPage = m_totalPages;
    if (m_currentPage < 1) m_currentPage = 1;

    // 3. 执行分页搜索
    m_model->setNotes(DatabaseManager::instance().searchNotes(keyword, m_currentFilterType, m_currentFilterValue, m_currentPage, pageSize));
    
    // 4. 更新工具栏
    m_toolbar->setPageInfo(m_currentPage, m_totalPages);
}

void QuickWindow::updatePartitionStatus(const QString& name) {
    if (m_systemTree->parentWidget()->isHidden()) {
        m_statusLabel->setText(QString("当前分区: %1").arg(name.isEmpty() ? "全部数据" : name));
        m_statusLabel->show();
    } else {
        m_statusLabel->hide();
    }
}

void QuickWindow::applyListTheme(const QString& colorHex) {
    QString style;
    if (!colorHex.isEmpty()) {
        QColor c(colorHex);
        QString bgColor = c.darker(350).name();
        QString selColor = c.darker(110).name();
        style = QString("QListView { border: none; background-color: %1; color: #eee; outline: none; } "
                        "QListView::item { padding: 8px; border-bottom: 1px solid rgba(0,0,0,0.2); } "
                        "QListView::item:selected { background-color: %2; color: white; border-radius: 4px; } "
                        "QListView::item:hover { background-color: rgba(255,255,255,0.1); }")
                .arg(bgColor, selColor);
    } else {
        style = "QListView { border: none; background-color: #1e1e1e; color: #eee; outline: none; } "
                "QListView::item { padding: 8px; border-bottom: 1px solid #2a2a2a; } "
                "QListView::item:selected { background-color: #4a90e2; color: white; border-radius: 4px; } "
                "QListView::item:hover { background-color: #333333; }";
    }
    m_listView->setStyleSheet(style);
}

void QuickWindow::activateNote(const QModelIndex& index) {
    if (!index.isValid()) return;

    int id = index.data(NoteModel::IdRole).toInt();
    QVariantMap note = DatabaseManager::instance().getNoteById(id);

    // 1. 准备剪贴板数据
    QString itemType = note["item_type"].toString();
    QString content = note["content"].toString();
    
    if (itemType == "image") {
        QImage img;
        img.loadFromData(note["data_blob"].toByteArray());
        QApplication::clipboard()->setImage(img);
    } else if (itemType != "text" && !itemType.isEmpty()) {
        // 对标 Python：处理文件路径 URLs
        QStringList rawPaths = content.split(';', Qt::SkipEmptyParts);
        QList<QUrl> validUrls;
        QStringList missingFiles;
        
        for (const QString& p : rawPaths) {
            QString path = p.trimmed().remove('\"');
            if (QFileInfo::exists(path)) {
                validUrls << QUrl::fromLocalFile(path);
            } else {
                missingFiles << QFileInfo(path).fileName();
            }
        }
        
        if (!validUrls.isEmpty()) {
            QMimeData* mimeData = new QMimeData();
            mimeData->setUrls(validUrls);
            QApplication::clipboard()->setMimeData(mimeData);
        } else {
            QApplication::clipboard()->setText(content);
            if (!missingFiles.isEmpty()) {
                QToolTip::showText(QCursor::pos(), "⚠️ 原文件已丢失，已复制路径文本", this);
            }
        }
    } else {
        QApplication::clipboard()->setText(content);
    }

    // 2. 隐藏当前窗口
    hide();

    // 3. 执行“Ditto式”自动粘贴
#ifdef Q_OS_WIN
    if (m_lastActiveHwnd && IsWindow(m_lastActiveHwnd)) {
        DWORD currThread = GetCurrentThreadId();
        bool attached = false;
        if (m_lastThreadId != 0 && m_lastThreadId != currThread) {
            attached = AttachThreadInput(currThread, m_lastThreadId, TRUE);
        }

        if (IsIconic(m_lastActiveHwnd)) {
            ShowWindow(m_lastActiveHwnd, SW_RESTORE);
        }
        SetForegroundWindow(m_lastActiveHwnd);
        
        if (m_lastFocusHwnd && IsWindow(m_lastFocusHwnd)) {
            SetFocus(m_lastFocusHwnd);
        }

        // 延迟 200ms，确保目标窗口已准备好接收输入 (对标 Python 版 0.1s + 额外缓冲)
        DWORD lastThread = m_lastThreadId;
        QTimer::singleShot(200, [lastThread, attached]() {
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('V', 0, 0, 0);
            keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);

            if (attached) {
                AttachThreadInput(GetCurrentThreadId(), lastThread, FALSE);
            }
        });
    }
#endif
}

void QuickWindow::doDeleteSelected() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    QList<int> ids;
    for (const auto& index : selected) {
        if (!index.data(NoteModel::LockedRole).toBool()) {
            ids << index.data(NoteModel::IdRole).toInt();
        }
    }
    DatabaseManager::instance().updateNoteStateBatch(ids, "is_deleted", 1);
    refreshData();
}

void QuickWindow::doToggleFavorite() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        DatabaseManager::instance().toggleNoteState(id, "is_favorite");
    }
    refreshData();
}

void QuickWindow::doTogglePin() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        DatabaseManager::instance().toggleNoteState(id, "is_pinned");
    }
    refreshData();
}

void QuickWindow::doLockSelected() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    
    int firstId = selected.first().data(NoteModel::IdRole).toInt();
    bool firstState = selected.first().data(NoteModel::LockedRole).toBool();
    bool targetState = !firstState;

    QList<int> ids;
    for (const auto& index : selected) ids << index.data(NoteModel::IdRole).toInt();
    
    DatabaseManager::instance().updateNoteStateBatch(ids, "is_locked", targetState);
    refreshData();
}

void QuickWindow::doNewIdea() {
    NoteEditWindow* win = new NoteEditWindow();
    connect(win, &NoteEditWindow::noteSaved, this, &QuickWindow::refreshData);
    win->show();
}

void QuickWindow::doExtractContent() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    QStringList texts;
    for (const auto& index : selected) {
        QString type = index.data(NoteModel::TypeRole).toString();
        if (type == "text" || type.isEmpty()) {
            texts << index.data(NoteModel::ContentRole).toString();
        }
    }
    if (!texts.isEmpty()) {
        QApplication::clipboard()->setText(texts.join("\n---\n"));
    }
}

void QuickWindow::doEditSelected() {
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(NoteModel::IdRole).toInt();
    NoteEditWindow* win = new NoteEditWindow(id);
    connect(win, &NoteEditWindow::noteSaved, this, &QuickWindow::refreshData);
    win->show();
}

void QuickWindow::doSetRating(int rating) {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        DatabaseManager::instance().updateNoteState(id, "rating", rating);
    }
    refreshData();
}

void QuickWindow::doPreview() {
    QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) return;
    int id = index.data(NoteModel::IdRole).toInt();
    QVariantMap note = DatabaseManager::instance().getNoteById(id);
    QPoint globalPos = m_listView->mapToGlobal(m_listView->rect().center()) - QPoint(250, 300);
    m_quickPreview->showPreview(note["title"].toString(), note["content"].toString(), globalPos);
}

void QuickWindow::toggleStayOnTop(bool checked) {
#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    if (checked) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    } else {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
#else
    Qt::WindowFlags flags = windowFlags();
    if (checked) flags |= Qt::WindowStaysOnTopHint;
    else flags &= ~Qt::WindowStaysOnTopHint;
    
    if (flags != windowFlags()) {
        setWindowFlags(flags);
        show();
    }
#endif
    m_toolbar->setStayOnTop(checked);
}

void QuickWindow::toggleSidebar() {
    bool hidden = !m_systemTree->parentWidget()->isVisible();
    m_systemTree->parentWidget()->setVisible(hidden);

    QString name;
    if (m_systemTree->currentIndex().isValid()) name = m_systemTree->currentIndex().data().toString();
    else name = m_partitionTree->currentIndex().data().toString();

    updatePartitionStatus(name);
}

void QuickWindow::showListContextMenu(const QPoint& pos) {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) {
        QModelIndex index = m_listView->indexAt(pos);
        if (index.isValid()) {
            m_listView->setCurrentIndex(index);
            selected << index;
        } else {
            return;
        }
    }

    int selCount = selected.size();
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                       "QMenu::item { padding: 6px 10px 6px 28px; border-radius: 3px; } "
                       "QMenu::item:selected { background-color: #4a90e2; color: white; }");

    if (selCount == 1) {
        menu.addAction(IconHelper::getIcon("eye", "#1abc9c"), "预览 (Space)", this, &QuickWindow::doPreview);
    }
    
    menu.addAction(IconHelper::getIcon("file", "#1abc9c"), QString("复制内容 (%1)").arg(selCount), this, &QuickWindow::doExtractContent);
    menu.addSeparator();

    if (selCount == 1) {
        menu.addAction(IconHelper::getIcon("edit", "#4a90e2"), "编辑 (Ctrl+B)", this, &QuickWindow::doEditSelected);
        menu.addSeparator();
    }

    auto* ratingMenu = menu.addMenu(IconHelper::getIcon("star", "#f39c12"), QString("设置星级 (%1)").arg(selCount));
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

    bool isFavorite = selected.first().data(NoteModel::FavoriteRole).toBool();
    menu.addAction(IconHelper::getIcon(isFavorite ? "bookmark_filled" : "bookmark", "#ff6b81"), 
                   isFavorite ? "取消书签" : "添加书签 (Ctrl+E)", this, &QuickWindow::doToggleFavorite);

    bool isPinned = selected.first().data(NoteModel::PinnedRole).toBool();
    menu.addAction(IconHelper::getIcon(isPinned ? "pin_vertical" : "pin_tilted", isPinned ? "#e74c3c" : "#aaaaaa"), 
                   isPinned ? "取消置顶" : "置顶选中项 (Ctrl+P)", this, &QuickWindow::doTogglePin);
    
    bool isLocked = selected.first().data(NoteModel::LockedRole).toBool();
    menu.addAction(IconHelper::getIcon("lock", isLocked ? "#2ecc71" : "#aaaaaa"), 
                   isLocked ? "解锁选中项" : "锁定选中项 (Ctrl+S)", this, &QuickWindow::doLockSelected);
    
    menu.addSeparator();

    auto* catMenu = menu.addMenu(IconHelper::getIcon("branch", "#cccccc"), QString("移动选中项到分类 (%1)").arg(selCount));
    catMenu->addAction("⚠️ 未分类", [this]() { doMoveToCategory(-1); });
    
    QSettings settings("RapidNotes", "QuickWindow");
    QVariantList recentCats = settings.value("recentCategories").toList();
    auto allCategories = DatabaseManager::instance().getAllCategories();
    QMap<int, QVariantMap> catMap;
    for (const auto& cat : allCategories) catMap[cat["id"].toInt()] = cat;

    int count = 0;
    for (const auto& v : recentCats) {
        if (count >= 15) break;
        int cid = v.toInt();
        if (catMap.contains(cid)) {
            const auto& cat = catMap[cid];
            catMenu->addAction(IconHelper::getIcon("branch", cat["color"].toString()), cat["name"].toString(), [this, cid]() {
                doMoveToCategory(cid);
            });
            count++;
        }
    }

    menu.addSeparator();
    menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "移至回收站 (Delete)", this, &QuickWindow::doDeleteSelected);

    menu.exec(m_listView->mapToGlobal(pos));
}

void QuickWindow::doMoveToCategory(int catId) {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    if (catId != -1) {
        QSettings settings("RapidNotes", "QuickWindow");
        QVariantList recentCats = settings.value("recentCategories").toList();
        recentCats.removeAll(catId);
        recentCats.prepend(catId);
        while (recentCats.size() > 15) recentCats.removeLast();
        settings.setValue("recentCategories", recentCats);
    }

    QList<int> ids;
    for (const auto& index : selected) ids << index.data(NoteModel::IdRole).toInt();
    
    DatabaseManager::instance().moveNotesToCategory(ids, catId);
    refreshData();
}

void QuickWindow::showCentered() {
#ifdef Q_OS_WIN
    HWND myHwnd = (HWND)winId();
    HWND current = GetForegroundWindow();
    if (current != myHwnd) {
        m_lastActiveHwnd = current;
        m_lastThreadId = GetWindowThreadProcessId(m_lastActiveHwnd, nullptr);
        GUITHREADINFO gti;
        gti.cbSize = sizeof(GUITHREADINFO);
        if (GetGUIThreadInfo(m_lastThreadId, &gti)) {
            m_lastFocusHwnd = gti.hwndFocus;
        } else {
            m_lastFocusHwnd = nullptr;
        }
    }
#endif
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->geometry();
        move(screenGeom.center() - rect().center());
    }
    show();
    activateWindow();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

bool QuickWindow::event(QEvent* event) {
    return QWidget::event(event);
}

void QuickWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_resizeArea = getResizeArea(event->pos());
        if (m_resizeArea != 0) {
            m_resizeStartPos = event->globalPosition().toPoint();
            m_resizeStartGeometry = frameGeometry();
        } else {
            if (auto* handle = windowHandle()) {
                handle->startSystemMove();
            }
        }
        event->accept();
    }
}

void QuickWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() == Qt::NoButton) {
        setCursorShape(getResizeArea(event->pos()));
    } else if (event->buttons() & Qt::LeftButton) {
        if (m_resizeArea != 0) {
            QPoint delta = event->globalPosition().toPoint() - m_resizeStartPos;
            QRect newGeom = m_resizeStartGeometry;
            if (m_resizeArea & 0x1) newGeom.setLeft(m_resizeStartGeometry.left() + delta.x());
            if (m_resizeArea & 0x2) newGeom.setRight(m_resizeStartGeometry.right() + delta.x());
            if (m_resizeArea & 0x4) newGeom.setTop(m_resizeStartGeometry.top() + delta.y());
            if (m_resizeArea & 0x8) newGeom.setBottom(m_resizeStartGeometry.bottom() + delta.y());
            
            if (newGeom.width() >= 400 && newGeom.height() >= 300) {
                setGeometry(newGeom);
            }
            event->accept();
            return;
        }
    }
    QWidget::mouseMoveEvent(event);
}

void QuickWindow::mouseReleaseEvent(QMouseEvent* event) {
    m_resizeArea = 0;
    setCursor(Qt::ArrowCursor);
    QWidget::mouseReleaseEvent(event);
}

void QuickWindow::hideEvent(QHideEvent* event) {
    saveState();
    QWidget::hideEvent(event);
}

int QuickWindow::getResizeArea(const QPoint& pos) {
    int area = 0;
    // 获取可见容器的几何范围（考虑了 15px 的边距）
    QRect rect = findChild<QWidget*>("container")->geometry();
    int x = pos.x();
    int y = pos.y();
    const int m = 8; // 相对容器边缘的触发范围

    if (x >= rect.left() - m && x <= rect.left() + m) area |= 0x1;
    else if (x >= rect.right() - m && x <= rect.right() + m) area |= 0x2;

    if (y >= rect.top() - m && y <= rect.top() + m) area |= 0x4;
    else if (y >= rect.bottom() - m && y <= rect.bottom() + m) area |= 0x8;

    return area;
}

void QuickWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QuickWindow::setCursorShape(int area) {
    if (area == 0) setCursor(Qt::ArrowCursor);
    else if ((area & 0x1 && area & 0x4) || (area & 0x2 && area & 0x8)) setCursor(Qt::SizeFDiagCursor);
    else if ((area & 0x2 && area & 0x4) || (area & 0x1 && area & 0x8)) setCursor(Qt::SizeBDiagCursor);
    else if (area & 0x1 || area & 0x2) setCursor(Qt::SizeHorCursor);
    else if (area & 0x4 || area & 0x8) setCursor(Qt::SizeVerCursor);
}

bool QuickWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_listView && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            activateNote(m_listView->currentIndex());
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Space && !keyEvent->isAutoRepeat()) {
            QModelIndex index = m_listView->currentIndex();
            if (index.isValid()) {
                int id = index.data(NoteModel::IdRole).toInt();
                QVariantMap note = DatabaseManager::instance().getNoteById(id);
                QPoint globalPos = m_listView->mapToGlobal(m_listView->rect().center()) - QPoint(250, 300);
                m_quickPreview->showPreview(note["title"].toString(), note["content"].toString(), globalPos);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}