#include "QuickWindow.h"
#include "NoteEditWindow.h"
#include "IconHelper.h"
#include "QuickNoteDelegate.h"
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

// 定义调整大小的边缘触发区域宽度
#define RESIZE_MARGIN 20 

QuickWindow::QuickWindow(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    
    // 关键修复：开启鼠标追踪，否则不按住鼠标时无法检测边缘
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    
    initUI();

    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, [this](const QVariantMap&){
        refreshData();
    });

    connect(&ClipboardMonitor::instance(), &ClipboardMonitor::newContentDetected, [this](){
        refreshData();
    });

    connect(&DatabaseManager::instance(), &DatabaseManager::categoriesChanged, [this](){
        m_systemModel->refresh();
        m_partitionModel->refresh();
        m_model->updateCategoryMap();
        refreshData();
        m_partitionTree->expandAll();
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
    container->setMouseTracking(true); // 确保容器不阻断鼠标追踪
    container->setStyleSheet(
        "QWidget#container { background: #1E1E1E; border-radius: 10px; border: 1px solid #333; }"
        "QListView, QTreeView { background: transparent; border: none; color: #BBB; outline: none; }"
        "QTreeView::item { height: 25px; padding: 0px 4px; }"
        "QListView::item { padding: 6px; border-bottom: 1px solid #2A2A2A; }"
        "QListView::item:selected, QTreeView::item:selected { background: #4a90e2; color: white; }"
    );
    
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 4);
    container->setGraphicsEffect(shadow);

    auto* containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // --- 左侧内容区域 ---
    auto* leftContent = new QWidget();
    leftContent->setMouseTracking(true);
    auto* leftLayout = new QVBoxLayout(leftContent);
    leftLayout->setContentsMargins(10, 10, 10, 5);
    leftLayout->setSpacing(8);
    
    m_searchEdit = new SearchLineEdit();
    m_searchEdit->setPlaceholderText("搜索灵感 (双击查看历史)");
    m_searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchEdit);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setHandleWidth(4);
    m_splitter->setChildrenCollapsible(false);
    
    m_listView = new QListView();
    m_listView->setDragEnabled(true);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setIconSize(QSize(28, 28));
    m_listView->setAlternatingRowColors(true);
    m_listView->setMouseTracking(true);
    m_listView->setItemDelegate(new QuickNoteDelegate(this));
    m_model = new NoteModel(this);
    m_listView->setModel(m_model);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, &QuickWindow::showListContextMenu);
    connect(m_listView, &QListView::doubleClicked, this, [this](const QModelIndex& index){
        activateNote(index);
    });

    auto* sidebarContainer = new QWidget();
    auto* sidebarLayout = new QVBoxLayout(sidebarContainer);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    m_systemTree = new DropTreeView();
    m_systemModel = new CategoryModel(CategoryModel::System, this);
    m_systemTree->setModel(m_systemModel);
    m_systemTree->setHeaderHidden(true);
    m_systemTree->setFixedHeight(150);
    m_systemTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_systemTree, &QTreeView::customContextMenuRequested, this, &QuickWindow::showSidebarMenu);

    m_partitionTree = new DropTreeView();
    m_partitionModel = new CategoryModel(CategoryModel::User, this);
    m_partitionTree->setModel(m_partitionModel);
    m_partitionTree->setHeaderHidden(true);
    m_partitionTree->expandAll();
    m_partitionTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_partitionTree, &QTreeView::customContextMenuRequested, this, &QuickWindow::showSidebarMenu);

    sidebarLayout->addWidget(m_systemTree);
    sidebarLayout->addWidget(m_partitionTree);

    // 树形菜单点击逻辑...
    auto onSelectionChanged = [this](DropTreeView* tree, const QModelIndex& index) {
        if (!index.isValid()) return;
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

    // 拖拽逻辑...
    auto onNotesDropped = [this](const QList<int>& ids, const QModelIndex& targetIndex) {
        if (!targetIndex.isValid()) return;
        QString type = targetIndex.data(Qt::UserRole).toString();
        for (int id : ids) {
            if (type == "category") {
                int catId = targetIndex.data(Qt::UserRole + 1).toInt();
                DatabaseManager::instance().updateNoteState(id, "category_id", catId);
            } else if (type == "bookmark") DatabaseManager::instance().updateNoteState(id, "is_favorite", 1);
            else if (type == "trash") DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
            else if (type == "uncategorized") DatabaseManager::instance().updateNoteState(id, "category_id", QVariant());
        }
        refreshData();
    };
    connect(m_systemTree, &DropTreeView::notesDropped, [this, onNotesDropped](const QList<int>& ids, const QModelIndex& idx){ onNotesDropped(ids, idx); });
    connect(m_partitionTree, &DropTreeView::notesDropped, [this, onNotesDropped](const QList<int>& ids, const QModelIndex& idx){ onNotesDropped(ids, idx); });

    // 右键菜单...
    // (此处省略部分右键菜单代码以保持简洁，逻辑与原版保持一致)
    // 主要是 showSidebarMenu 的实现...

    m_splitter->addWidget(m_listView);
    m_splitter->addWidget(sidebarContainer);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setSizes({550, 150});
    leftLayout->addWidget(m_splitter);

    applyListTheme(""); // 【核心修复】初始化时即应用深色主题

    m_statusLabel = new QLabel("当前分区: 全部数据");
    m_statusLabel->setStyleSheet("font-size: 11px; color: #888; padding-left: 2px;");
    m_statusLabel->setFixedHeight(32);
    leftLayout->addWidget(m_statusLabel);

    containerLayout->addWidget(leftContent);

    // --- 右侧垂直工具栏 (Custom Toolbar Implementation) ---
    // 【核心修正】根据图二 1:1 还原，压缩宽度，修正图标名，重构分页布局
    
    QWidget* customToolbar = new QWidget(this);
    customToolbar->setFixedWidth(40); // 压缩至 40px
    customToolbar->setStyleSheet(
        "QWidget { background-color: #252526; border-top-right-radius: 10px; border-bottom-right-radius: 10px; border-left: 1px solid #333; }"
        "QPushButton { border: none; border-radius: 4px; background: transparent; padding: 0px; }"
        "QPushButton:hover { background-color: #3e3e42; }"
        "QPushButton:pressed { background-color: #2d2d2d; }"
        "QLabel { color: #888; font-size: 11px; }"
        "QLineEdit { background: transparent; border: 1px solid #444; border-radius: 4px; color: white; font-size: 11px; font-weight: bold; padding: 0; }"
    );
    
    QVBoxLayout* toolLayout = new QVBoxLayout(customToolbar);
    toolLayout->setContentsMargins(4, 8, 4, 8); // 对齐 Python 版边距
    toolLayout->setSpacing(4); // 紧凑间距，匹配图二

    // 辅助函数：创建图标按钮，支持旋转
    auto createToolBtn = [](QString iconName, QString color, QString tooltip, int rotate = 0) {
        QPushButton* btn = new QPushButton();
        QIcon icon = IconHelper::getIcon(iconName, color);
        if (rotate != 0) {
            QPixmap pix = icon.pixmap(32, 32);
            QTransform trans;
            trans.rotate(rotate);
            btn->setIcon(QIcon(pix.transformed(trans, Qt::SmoothTransformation)));
        } else {
            btn->setIcon(icon);
        }
        btn->setIconSize(QSize(18, 18));
        btn->setFixedSize(32, 32);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    // 1. 顶部窗口控制区 (修正图标名为 SvgIcons 中存在的名称)
    QPushButton* btnClose = createToolBtn("close", "#aaaaaa", "关闭");
    connect(btnClose, &QPushButton::clicked, this, &QuickWindow::hide);

    QPushButton* btnFull = createToolBtn("maximize", "#aaaaaa", "切换主窗口");
    connect(btnFull, &QPushButton::clicked, [this](){ emit toggleMainWindowRequested(); hide(); });

    QPushButton* btnMin = createToolBtn("minimize", "#aaaaaa", "最小化");
    connect(btnMin, &QPushButton::clicked, this, &QuickWindow::showMinimized);

    toolLayout->addWidget(btnClose, 0, Qt::AlignHCenter);
    toolLayout->addWidget(btnFull, 0, Qt::AlignHCenter);
    toolLayout->addWidget(btnMin, 0, Qt::AlignHCenter);

    toolLayout->addSpacing(8);

    // 2. 功能按钮区
    QPushButton* btnPin = createToolBtn("pin_tilted", "#aaaaaa", "置顶");
    btnPin->setCheckable(true);
    btnPin->setObjectName("btnPin");
    btnPin->setStyleSheet("QPushButton:checked { background-color: #3A90FF; }");
    if (windowFlags() & Qt::WindowStaysOnTopHint) {
        btnPin->setChecked(true);
        btnPin->setIcon(IconHelper::getIcon("pin_vertical", "#ffffff"));
    }
    connect(btnPin, &QPushButton::toggled, this, &QuickWindow::toggleStayOnTop);

    QPushButton* btnSidebar = createToolBtn("eye", "#aaaaaa", "显示/隐藏侧边栏");
    btnSidebar->setObjectName("btnSidebar");
    btnSidebar->setCheckable(true);
    btnSidebar->setChecked(true);
    btnSidebar->setStyleSheet("QPushButton:checked { background-color: #3A90FF; }");
    connect(btnSidebar, &QPushButton::clicked, this, &QuickWindow::toggleSidebar);

    QPushButton* btnRefresh = createToolBtn("refresh", "#aaaaaa", "刷新");
    connect(btnRefresh, &QPushButton::clicked, this, &QuickWindow::refreshData);

    QPushButton* btnToolbox = createToolBtn("toolbox", "#aaaaaa", "工具箱");
    connect(btnToolbox, &QPushButton::clicked, this, &QuickWindow::toolboxRequested);

    toolLayout->addWidget(btnPin, 0, Qt::AlignHCenter);
    toolLayout->addWidget(btnSidebar, 0, Qt::AlignHCenter);
    toolLayout->addWidget(btnRefresh, 0, Qt::AlignHCenter);
    toolLayout->addWidget(btnToolbox, 0, Qt::AlignHCenter);

    toolLayout->addStretch();

    // 3. 分页区 (完全复刻图二布局：箭头+输入框+下方总数)
    QPushButton* btnPrev = createToolBtn("nav_prev", "#aaaaaa", "上一页", 90);
    btnPrev->setFixedSize(32, 20);
    connect(btnPrev, &QPushButton::clicked, [this](){
        if (m_currentPage > 1) { m_currentPage--; refreshData(); }
    });

    QLineEdit* pageInput = new QLineEdit("1");
    pageInput->setObjectName("pageInput");
    pageInput->setAlignment(Qt::AlignCenter);
    pageInput->setFixedSize(28, 20);
    connect(pageInput, &QLineEdit::returnPressed, [this, pageInput](){
        int p = pageInput->text().toInt();
        if (p > 0 && p <= m_totalPages) { m_currentPage = p; refreshData(); }
    });

    QLabel* totalLabel = new QLabel("1");
    totalLabel->setObjectName("totalLabel");
    totalLabel->setAlignment(Qt::AlignCenter);
    totalLabel->setStyleSheet("color: #666; font-size: 10px; border: none; background: transparent;");

    QPushButton* btnNext = createToolBtn("nav_next", "#aaaaaa", "下一页", 90);
    btnNext->setFixedSize(32, 20);
    connect(btnNext, &QPushButton::clicked, [this](){
        if (m_currentPage < m_totalPages) { m_currentPage++; refreshData(); }
    });

    toolLayout->addWidget(btnPrev, 0, Qt::AlignHCenter);
    toolLayout->addWidget(pageInput, 0, Qt::AlignHCenter);
    toolLayout->addWidget(totalLabel, 0, Qt::AlignHCenter);
    toolLayout->addWidget(btnNext, 0, Qt::AlignHCenter);

    toolLayout->addSpacing(20); // 增加分页与标题间距

    // 4. 垂直标题 "快速笔记"
    QLabel* verticalTitle = new QLabel("快\n速\n笔\n记");
    verticalTitle->setAlignment(Qt::AlignCenter);
    verticalTitle->setStyleSheet("color: #444; font-size: 11px; font-weight: bold; border: none; background: transparent; line-height: 1.1;");
    toolLayout->addWidget(verticalTitle, 0, Qt::AlignHCenter);

    toolLayout->addSpacing(12);

    // 5. 底部 Logo (修正为 zap 图标以匹配图二蓝闪电)
    QPushButton* btnLogo = createToolBtn("zap", "#3A90FF", "RapidNotes");
    btnLogo->setCursor(Qt::ArrowCursor);
    btnLogo->setStyleSheet("background: transparent; border: none;");
    toolLayout->addWidget(btnLogo, 0, Qt::AlignHCenter);

    containerLayout->addWidget(customToolbar);
    
    // m_toolbar = new QuickToolbar(this); // 移除旧代码
    // containerLayout->addWidget(m_toolbar); // 移除旧代码
    
    mainLayout->addWidget(container);
    
    // 初始大小和最小大小
    resize(830, 630);
    setMinimumSize(400, 300);

    m_quickPreview = new QuickPreview(this);
    m_listView->installEventFilter(this);

    // 搜索逻辑
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &QuickWindow::refreshData);
    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text){
        m_currentPage = 1;
        m_searchTimer->start(300);
    });

    connect(m_searchEdit, &QLineEdit::returnPressed, [this](){
        QString text = m_searchEdit->text().trimmed();
        if (text.isEmpty()) return;
        m_searchEdit->addHistoryEntry(text);
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
    // settings.setValue("stayOnTop", m_toolbar->isStayOnTop()); // Old
    settings.setValue("stayOnTop", (windowFlags() & Qt::WindowStaysOnTopHint) ? true : false);
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
    
    // Alt+D Toggle Stay on Top
    new QShortcut(QKeySequence("Alt+D"), this, [this](){ 
        toggleStayOnTop(!(windowFlags() & Qt::WindowStaysOnTopHint)); 
    });
    
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
    
    int totalCount = DatabaseManager::instance().getNotesCount(keyword, m_currentFilterType, m_currentFilterValue);
    
    const int pageSize = 100; // 对齐 Python 版
    m_totalPages = qMax(1, (totalCount + pageSize - 1) / pageSize); 
    if (m_currentPage > m_totalPages) m_currentPage = m_totalPages;
    if (m_currentPage < 1) m_currentPage = 1;

    m_model->setNotes(DatabaseManager::instance().searchNotes(keyword, m_currentFilterType, m_currentFilterValue, m_currentPage, pageSize));
    
    // 更新工具栏页码 (对齐新版 1:1 布局)
    auto* pageInput = findChild<QLineEdit*>("pageInput");
    if (pageInput) pageInput->setText(QString::number(m_currentPage));

    auto* totalLabel = findChild<QLabel*>("totalLabel");
    if (totalLabel) totalLabel->setText(QString::number(m_totalPages));
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
        QString altBgColor = c.darker(450).name();
        QString selColor = c.darker(110).name();
        // 对齐 Python 版，QPalette::Highlight 对应选中色，Base/AlternateBase 对应斑马纹
        style = QString("QListView { "
                        "  border: none; "
                        "  background-color: %1; "
                        "  alternate-background-color: %2; "
                        "  selection-background-color: %3; "
                        "  color: #eee; "
                        "  outline: none; "
                        "}")
                .arg(bgColor, altBgColor, selColor);
    } else {
        style = "QListView { "
                "  border: none; "
                "  background-color: #1e1e1e; "
                "  alternate-background-color: #151515; "
                "  selection-background-color: #4a90e2; "
                "  color: #eee; "
                "  outline: none; "
                "}";
    }
    m_listView->setStyleSheet(style);
}

void QuickWindow::activateNote(const QModelIndex& index) {
    if (!index.isValid()) return;

    int id = index.data(NoteModel::IdRole).toInt();
    QVariantMap note = DatabaseManager::instance().getNoteById(id);

    QString itemType = note["item_type"].toString();
    QString content = note["content"].toString();
    
    if (itemType == "image") {
        QImage img;
        img.loadFromData(note["data_blob"].toByteArray());
        QApplication::clipboard()->setImage(img);
    } else if (itemType != "text" && !itemType.isEmpty()) {
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

    // hide(); // 用户要求不隐藏窗口

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
    // 更新按钮状态与图标
    auto* btnPin = findChild<QPushButton*>("btnPin");
    if (btnPin) {
        if (btnPin->isChecked() != checked) btnPin->setChecked(checked);
        // 切换图标样式 (选中时白色垂直，未选中时灰色倾斜)
        btnPin->setIcon(IconHelper::getIcon(checked ? "pin_vertical" : "pin_tilted", checked ? "#ffffff" : "#aaaaaa"));
    }
}

void QuickWindow::toggleSidebar() {
    bool visible = !m_systemTree->parentWidget()->isVisible();
    m_systemTree->parentWidget()->setVisible(visible);
    
    // 更新按钮状态
    auto* btnSidebar = findChild<QPushButton*>("btnSidebar");
    if (btnSidebar) {
        btnSidebar->setChecked(visible);
        btnSidebar->setIcon(IconHelper::getIcon("eye", visible ? "#ffffff" : "#aaaaaa"));
    }

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

void QuickWindow::showSidebarMenu(const QPoint& pos) {
    auto* tree = qobject_cast<QTreeView*>(sender());
    if (!tree) return;

    QModelIndex index = tree->indexAt(pos);
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; padding: 4px; } "
                       "QMenu::item { padding: 6px 10px 6px 28px; border-radius: 3px; } "
                       "QMenu::item:selected { background-color: #4a90e2; color: white; }");

    if (!index.isValid() || index.data().toString() == "我的分区") {
        menu.addAction("➕ 新建分组", [this]() {
            bool ok;
            QString text = QInputDialog::getText(this, "新建组", "组名称:", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                DatabaseManager::instance().addCategory(text);
            }
        });
        menu.exec(tree->mapToGlobal(pos));
        return;
    }

    QString type = index.data(Qt::UserRole).toString();
    if (type == "category") {
        int catId = index.data(Qt::UserRole + 1).toInt();
        QString currentName = index.data(Qt::DisplayRole).toString();

        menu.addAction(IconHelper::getIcon("add", "#3498db"), "新建数据", [this, catId]() {
            auto* win = new NoteEditWindow();
            win->setDefaultCategory(catId);
            connect(win, &NoteEditWindow::noteSaved, this, &QuickWindow::refreshData);
            win->show();
        });
        menu.addSeparator();
        menu.addAction(IconHelper::getIcon("palette", "#e67e22"), "设置颜色", [this, catId]() {
            QColor color = QColorDialog::getColor(Qt::gray, this, "选择分类颜色");
            if (color.isValid()) {
                DatabaseManager::instance().setCategoryColor(catId, color.name());
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
        menu.addAction("新建分组", [this]() {
            bool ok;
            QString text = QInputDialog::getText(this, "新建组", "组名称:", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) DatabaseManager::instance().addCategory(text);
        });
        menu.addAction("新建子分区", [this, catId]() {
            bool ok;
            QString text = QInputDialog::getText(this, "新建区", "区名称:", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) DatabaseManager::instance().addCategory(text, catId);
        });
        menu.addAction(IconHelper::getIcon("edit", "#aaaaaa"), "重命名", [this, catId, currentName]() {
            bool ok;
            QString text = QInputDialog::getText(this, "重命名", "新名称:", QLineEdit::Normal, currentName, &ok);
            if (ok && !text.isEmpty()) DatabaseManager::instance().renameCategory(catId, text);
        });
        menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "删除", [this, catId]() {
            if (QMessageBox::question(this, "确认删除", "确定要删除此分类吗？内容将移至未分类。") == QMessageBox::Yes) {
                DatabaseManager::instance().deleteCategory(catId);
            }
        });
    } else if (type == "trash") {
        menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "清空回收站", [this]() {
            if (QMessageBox::question(this, "确认清空", "确定要永久删除回收站中的所有内容吗？") == QMessageBox::Yes) {
                DatabaseManager::instance().emptyTrash();
                refreshData();
            }
        });
    }

    menu.exec(tree->mapToGlobal(pos));
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
            event->accept();
        } else {
            if (auto* handle = windowHandle()) {
                handle->startSystemMove();
            }
            event->accept();
        }
    }
}

void QuickWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() == Qt::NoButton) {
        setCursorShape(getResizeArea(event->pos()));
    } 
    else if (event->buttons() & Qt::LeftButton) {
        if (m_resizeArea != 0) {
            QPoint delta = event->globalPosition().toPoint() - m_resizeStartPos;
            QRect newGeom = m_resizeStartGeometry;
            
            if (m_resizeArea & 0x1) newGeom.setLeft(m_resizeStartGeometry.left() + delta.x());
            if (m_resizeArea & 0x2) newGeom.setRight(m_resizeStartGeometry.right() + delta.x());
            if (m_resizeArea & 0x4) newGeom.setTop(m_resizeStartGeometry.top() + delta.y());
            if (m_resizeArea & 0x8) newGeom.setBottom(m_resizeStartGeometry.bottom() + delta.y());
            
            if (newGeom.width() >= minimumWidth() && newGeom.height() >= minimumHeight()) {
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
    int x = pos.x();
    int y = pos.y();
    if (x < RESIZE_MARGIN) area |= 0x1;
    else if (x > width() - RESIZE_MARGIN) area |= 0x2;
    if (y < RESIZE_MARGIN) area |= 0x4;
    else if (y > height() - RESIZE_MARGIN) area |= 0x8;
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