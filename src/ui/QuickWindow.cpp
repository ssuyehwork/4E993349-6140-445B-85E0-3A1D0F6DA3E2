#include "QuickWindow.h"
#include "NoteEditWindow.h"
#include "IconHelper.h"
#include "../core/DatabaseManager.h"
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

QuickWindow::QuickWindow(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    initUI();

    // 修复：由于信号增加了参数，这里使用 lambda 忽略参数即可
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, [this](const QVariantMap&){
        refreshData();
    });
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
    
    // 列表居左
    m_listView = new QListView();
    m_listView->setDragEnabled(true);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_model = new NoteModel(this);
    m_listView->setModel(m_model);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &QListView::customContextMenuRequested, this, &QuickWindow::showListContextMenu);
    connect(m_listView, &QListView::doubleClicked, this, [this](const QModelIndex& index){
        activateNote(index);
    });

    // 侧边栏居右
    m_sideBar = new DropTreeView();
    m_sideModel = new CategoryModel(this);
    m_sideBar->setModel(m_sideModel);
    m_sideBar->setHeaderHidden(true);
    m_sideBar->expandAll();
    m_sideBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sideBar, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos){
        QModelIndex index = m_sideBar->indexAt(pos);
        if (!index.isValid()) return;
        QMenu menu(this);
        QString type = index.data(Qt::UserRole).toString();
        if (type == "category") {
            menu.addAction(IconHelper::getIcon("edit", "#aaaaaa"), "重命名分类");
            menu.addAction(IconHelper::getIcon("trash", "#aaaaaa"), "删除分类");
        } else if (type == "trash") {
            menu.addAction(IconHelper::getIcon("trash", "#aaaaaa"), "清空回收站");
        }
        menu.exec(m_sideBar->mapToGlobal(pos));
    });
    connect(m_sideBar, &QTreeView::clicked, this, [this](const QModelIndex& index){
        m_currentFilterType = index.data(Qt::UserRole).toString();
        QString name = index.data(Qt::DisplayRole).toString();
        updatePartitionStatus(name);
        if (m_currentFilterType == "category") {
            m_currentFilterValue = index.data(Qt::UserRole + 1).toInt();
        } else {
            m_currentFilterValue = -1;
        }
        m_currentPage = 1;
        refreshData();
    });
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

    m_splitter->addWidget(m_listView);
    m_splitter->addWidget(m_sideBar);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setSizes({550, 180});
    leftLayout->addWidget(m_splitter);

    m_statusLabel = new QLabel("当前分区: 全部数据");
    m_statusLabel->setStyleSheet("font-size: 11px; color: #888; padding-left: 2px;");
    m_statusLabel->setFixedHeight(24);
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
    
    // 搜索逻辑
    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text){
        m_currentPage = 1;
        refreshData();
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
    refreshData();
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
    
    // 2. 计算总页数
    m_totalPages = qMax(1, (totalCount + 19) / 20); // 每页20条
    if (m_currentPage > m_totalPages) m_currentPage = m_totalPages;
    if (m_currentPage < 1) m_currentPage = 1;

    // 3. 执行分页搜索
    m_model->setNotes(DatabaseManager::instance().searchNotes(keyword, m_currentFilterType, m_currentFilterValue, m_currentPage, 20));
    
    // 4. 更新工具栏
    m_toolbar->setPageInfo(m_currentPage, m_totalPages);
}

void QuickWindow::updatePartitionStatus(const QString& name) {
    m_statusLabel->setText(QString("当前分区: %1").arg(name));
}

void QuickWindow::activateNote(const QModelIndex& index) {
    if (!index.isValid()) return;

    int id = index.data(NoteModel::IdRole).toInt();
    QVariantMap note = DatabaseManager::instance().getNoteById(id);

    // 1. 复制内容到剪贴板
    QString itemType = note["item_type"].toString();
    if (itemType == "image") {
        QImage img;
        img.loadFromData(note["data_blob"].toByteArray());
        QApplication::clipboard()->setImage(img);
    } else {
        QApplication::clipboard()->setText(note["content"].toString());
    }

    // 2. 隐藏当前窗口
    hide();

    // 3. 执行“Ditto式”自动粘贴
#ifdef Q_OS_WIN
    if (m_lastActiveHwnd && IsWindow(m_lastActiveHwnd)) {
        // 确保窗口不在最小化状态
        if (IsIconic(m_lastActiveHwnd)) {
            ShowWindow(m_lastActiveHwnd, SW_RESTORE);
        }

        SetForegroundWindow(m_lastActiveHwnd);

        // 延迟一小会儿，确保目标窗口已准备好接收输入
        QTimer::singleShot(100, [this]() {
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('V', 0, 0, 0);
            keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        });
    }
#endif
}

void QuickWindow::doDeleteSelected() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        if (!index.data(NoteModel::LockedRole).toBool()) {
            DatabaseManager::instance().updateNoteState(id, "is_deleted", 1);
        }
    }
    refreshData();
}

void QuickWindow::doToggleFavorite() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        bool fav = index.data(NoteModel::FavoriteRole).toBool();
        DatabaseManager::instance().updateNoteState(id, "is_favorite", !fav);
    }
    refreshData();
}

void QuickWindow::doTogglePin() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        bool pinned = index.data(NoteModel::PinnedRole).toBool();
        DatabaseManager::instance().updateNoteState(id, "is_pinned", !pinned);
    }
    refreshData();
}

void QuickWindow::doLockSelected() {
    auto selected = m_listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;
    for (const auto& index : selected) {
        int id = index.data(NoteModel::IdRole).toInt();
        bool locked = index.data(NoteModel::LockedRole).toBool();
        DatabaseManager::instance().updateNoteState(id, "is_locked", !locked);
    }
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
    Qt::WindowFlags flags = windowFlags();
    if (checked) flags |= Qt::WindowStaysOnTopHint;
    else flags &= ~Qt::WindowStaysOnTopHint;

    if (flags != windowFlags()) {
        setWindowFlags(flags);
        m_toolbar->setStayOnTop(checked);
        show();
    }
}

void QuickWindow::toggleSidebar() {
    m_sideBar->setVisible(!m_sideBar->isVisible());
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
    for (int i = 0; i <= 5; ++i) {
        ratingMenu->addAction(QString("%1 星").arg(i), [this, i]() { doSetRating(i); });
    }

    menu.addAction(IconHelper::getIcon("bookmark", "#ff6b81"), "收藏/取消收藏 (Ctrl+E)", this, &QuickWindow::doToggleFavorite);
    menu.addAction(IconHelper::getIcon("pin", "#e74c3c"), "置顶/取消置顶 (Ctrl+P)", this, &QuickWindow::doTogglePin);
    menu.addAction(IconHelper::getIcon("lock", "#2ecc71"), "锁定/解锁 (Ctrl+S)", this, &QuickWindow::doLockSelected);

    menu.addSeparator();
    menu.addAction(IconHelper::getIcon("trash", "#e74c3c"), "移至回收站 (Delete)", this, &QuickWindow::doDeleteSelected);

    menu.exec(m_listView->mapToGlobal(pos));
}

void QuickWindow::showCentered() {
#ifdef Q_OS_WIN
    m_lastActiveHwnd = GetForegroundWindow();
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
    if (event->type() == QEvent::WindowDeactivate) {
        hide();
    }
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