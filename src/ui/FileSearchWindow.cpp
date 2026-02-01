#include "FileSearchWindow.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QClipboard>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QDir>
#include <QSettings>
#include <QSplitter>
#include <QMenu>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <functional>

// ----------------------------------------------------------------------------
// PathHistory 相关辅助类 (复刻 SearchHistoryPopup 逻辑)
// ----------------------------------------------------------------------------
class PathChip : public QFrame {
    Q_OBJECT
public:
    PathChip(const QString& text, QWidget* parent = nullptr) : QFrame(parent), m_text(text) {
        setAttribute(Qt::WA_StyledBackground);
        setCursor(Qt::PointingHandCursor);
        setObjectName("PathChip");
        
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(10);
        
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet("border: none; background: transparent; color: #DDD; font-size: 13px;");
        layout->addWidget(lbl);
        layout->addStretch();
        
        auto* btnDel = new QPushButton();
        btnDel->setIcon(IconHelper::getIcon("close", "#666", 16));
        btnDel->setIconSize(QSize(10, 10));
        btnDel->setFixedSize(16, 16);
        btnDel->setCursor(Qt::PointingHandCursor);
        btnDel->setStyleSheet(
            "QPushButton { background-color: transparent; border-radius: 4px; padding: 0px; }"
            "QPushButton:hover { background-color: #E74C3C; }"
        );
        
        connect(btnDel, &QPushButton::clicked, this, [this](){ emit deleted(m_text); });
        layout->addWidget(btnDel);

        setStyleSheet(
            "#PathChip { background-color: transparent; border: none; border-radius: 4px; }"
            "#PathChip:hover { background-color: #3E3E42; }"
        );
    }
    
    void mousePressEvent(QMouseEvent* e) override { 
        if(e->button() == Qt::LeftButton) emit clicked(m_text); 
        QFrame::mousePressEvent(e);
    }

signals:
    void clicked(const QString& text);
    void deleted(const QString& text);
private:
    QString m_text;
};

// ----------------------------------------------------------------------------
// Sidebar ListWidget subclass for Drag & Drop
// ----------------------------------------------------------------------------
class FileSidebarListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit FileSidebarListWidget(QWidget* parent = nullptr) : QListWidget(parent) {
        setAcceptDrops(true);
    }
signals:
    void folderDropped(const QString& path);
protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
            event->acceptProposedAction();
        }
    }
    void dragMoveEvent(QDragMoveEvent* event) override {
        event->acceptProposedAction();
    }
    void dropEvent(QDropEvent* event) override {
        QString path;
        if (event->mimeData()->hasUrls()) {
            path = event->mimeData()->urls().at(0).toLocalFile();
        } else if (event->mimeData()->hasText()) {
            path = event->mimeData()->text();
        }
        
        if (!path.isEmpty() && QDir(path).exists()) {
            emit folderDropped(path);
            event->acceptProposedAction();
        }
    }
};

class FileSearchHistoryPopup : public QWidget {
    Q_OBJECT
public:
    enum Type { Path, Filename };

    explicit FileSearchHistoryPopup(FileSearchWindow* window, QLineEdit* edit, Type type) 
        : QWidget(window->window(), Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) 
    {
        m_window = window;
        m_edit = edit;
        m_type = type;
        setAttribute(Qt::WA_TranslucentBackground);
        
        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(12, 12, 12, 12);
        
        auto* container = new QFrame();
        container->setObjectName("PopupContainer");
        container->setStyleSheet(
            "#PopupContainer { background-color: #252526; border: 1px solid #444; border-radius: 10px; }"
        );
        rootLayout->addWidget(container);

        auto* shadow = new QGraphicsDropShadowEffect(container);
        shadow->setBlurRadius(20); shadow->setXOffset(0); shadow->setYOffset(5);
        shadow->setColor(QColor(0, 0, 0, 120));
        container->setGraphicsEffect(shadow);

        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(10);

        auto* top = new QHBoxLayout();
        auto* icon = new QLabel();
        icon->setPixmap(IconHelper::getIcon("clock", "#888").pixmap(14, 14));
        icon->setStyleSheet("border: none; background: transparent;");
        top->addWidget(icon);

        auto* title = new QLabel(m_type == Path ? "最近扫描路径" : "最近搜索文件名");
        title->setStyleSheet("color: #888; font-weight: bold; font-size: 11px; background: transparent; border: none;");
        top->addWidget(title);
        top->addStretch();
        auto* clearBtn = new QPushButton("清空");
        clearBtn->setCursor(Qt::PointingHandCursor);
        clearBtn->setStyleSheet("QPushButton { background: transparent; color: #666; border: none; font-size: 11px; } QPushButton:hover { color: #E74C3C; }");
        connect(clearBtn, &QPushButton::clicked, [this](){
            if (m_type == Path) m_window->clearHistory();
            else m_window->clearSearchHistory();
            refreshUI();
        });
        top->addWidget(clearBtn);
        layout->addLayout(top);

        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet(
            "QScrollArea { background-color: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background-color: transparent; }"
        );
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        m_chipsWidget = new QWidget();
        m_chipsWidget->setStyleSheet("background-color: transparent;");
        m_vLayout = new QVBoxLayout(m_chipsWidget);
        m_vLayout->setContentsMargins(0, 0, 0, 0);
        m_vLayout->setSpacing(2);
        m_vLayout->addStretch();
        scroll->setWidget(m_chipsWidget);
        layout->addWidget(scroll);

        m_opacityAnim = new QPropertyAnimation(this, "windowOpacity");
        m_opacityAnim->setDuration(200);
    }

    void refreshUI() {
        QLayoutItem* item;
        while ((item = m_vLayout->takeAt(0))) {
            if(item->widget()) item->widget()->deleteLater();
            delete item;
        }
        m_vLayout->addStretch();
        
        QStringList history = (m_type == Path) ? m_window->getHistory() : m_window->getSearchHistory();
        if(history.isEmpty()) {
            auto* lbl = new QLabel("暂无历史记录");
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet("color: #555; font-style: italic; margin: 20px; border: none;");
            m_vLayout->insertWidget(0, lbl);
        } else {
            for(const QString& val : history) {
                auto* chip = new PathChip(val);
                chip->setFixedHeight(32);
                connect(chip, &PathChip::clicked, this, [this](const QString& v){ 
                    if (m_type == Path) m_window->useHistoryPath(v);
                    else m_edit->setText(v);
                    close(); 
                });
                connect(chip, &PathChip::deleted, this, [this](const QString& v){ 
                    if (m_type == Path) m_window->removeHistoryEntry(v);
                    else m_window->removeSearchHistoryEntry(v);
                    refreshUI(); 
                });
                m_vLayout->insertWidget(m_vLayout->count() - 1, chip);
            }
        }
        
        int targetWidth = m_edit->width();
        int contentHeight = qMin(410, (int)history.size() * 34 + 60);
        resize(targetWidth + 24, contentHeight);
    }

    void showAnimated() {
        refreshUI();
        QPoint pos = m_edit->mapToGlobal(QPoint(0, m_edit->height()));
        move(pos.x() - 12, pos.y() - 7);
        setWindowOpacity(0);
        show();
        m_opacityAnim->setStartValue(0);
        m_opacityAnim->setEndValue(1);
        m_opacityAnim->start();
    }

private:
    FileSearchWindow* m_window;
    QLineEdit* m_edit;
    Type m_type;
    QWidget* m_chipsWidget;
    QVBoxLayout* m_vLayout;
    QPropertyAnimation* m_opacityAnim;
};

// ----------------------------------------------------------------------------
// ScannerThread 实现
// ----------------------------------------------------------------------------
ScannerThread::ScannerThread(const QString& folderPath, QObject* parent)
    : QThread(parent), m_folderPath(folderPath) {}

void ScannerThread::stop() {
    m_isRunning = false;
    wait();
}

void ScannerThread::run() {
    int count = 0;
    if (m_folderPath.isEmpty() || !QDir(m_folderPath).exists()) {
        emit finished(0);
        return;
    }

    QStringList ignored = {".git", ".idea", "__pycache__", "node_modules", "$RECYCLE.BIN", "System Volume Information"};
    
    // 使用 std::function 实现递归扫描，支持目录剪枝
    std::function<void(const QString&)> scanDir = [&](const QString& currentPath) {
        if (!m_isRunning) return;

        QDir dir(currentPath);
        // 1. 获取当前目录下所有文件
        QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const auto& fi : files) {
            if (!m_isRunning) return;
            emit fileFound(fi.fileName(), fi.absoluteFilePath());
            count++;
        }

        // 2. 获取子目录并递归 (排除忽略列表)
        QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const auto& di : subDirs) {
            if (!m_isRunning) return;
            if (!ignored.contains(di.fileName())) {
                scanDir(di.absoluteFilePath());
            }
        }
    };

    scanDir(m_folderPath);
    emit finished(count);
}

// ----------------------------------------------------------------------------
// ResizeHandle 实现
// ----------------------------------------------------------------------------
ResizeHandle::ResizeHandle(QWidget* target, QWidget* parent) 
    : QWidget(parent), m_target(target) 
{
    setFixedSize(20, 20);
    setCursor(Qt::SizeFDiagCursor);
}

void ResizeHandle::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_startPos = event->globalPosition().toPoint();
        m_startSize = m_target->size();
        event->accept();
    }
}

void ResizeHandle::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        QPoint delta = event->globalPosition().toPoint() - m_startPos;
        int newW = qMax(m_startSize.width() + delta.x(), 600);
        int newH = qMax(m_startSize.height() + delta.y(), 400);
        m_target->resize(newW, newH);
        event->accept();
    }
}

// ----------------------------------------------------------------------------
// FileSearchWindow 实现
// ----------------------------------------------------------------------------
FileSearchWindow::FileSearchWindow(QWidget* parent) 
    : FramelessDialog("查找文件", parent) 
{
    resize(1000, 680);
    setupStyles();
    initUI();
    loadFavorites();
    m_resizeHandle = new ResizeHandle(this, this);
    m_resizeHandle->raise();
}

FileSearchWindow::~FileSearchWindow() {
    if (m_scanThread) {
        m_scanThread->stop();
        m_scanThread->deleteLater();
    }
}

void FileSearchWindow::setupStyles() {
    // 1:1 复刻 Python 脚本中的 STYLESHEET
    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            font-size: 14px;
            color: #E0E0E0;
            outline: none;
        }
        QSplitter::handle {
            background-color: #333;
        }
        QListWidget {
            background-color: #252526; 
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 4px;
        }
        QListWidget::item {
            height: 30px;
            padding-left: 8px;
            border-radius: 4px;
            color: #CCCCCC;
        }
        QListWidget::item:selected {
            background-color: #37373D;
            border-left: 3px solid #007ACC;
            color: #FFFFFF;
        }
        QListWidget::item:hover {
            background-color: #2A2D2E;
        }
        QLineEdit {
            background-color: #333333;
            border: 1px solid #444444;
            color: #FFFFFF;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #264F78;
        }
        QLineEdit:focus {
            border: 1px solid #007ACC;
            background-color: #2D2D2D;
        }
        QPushButton#ActionBtn {
            background-color: #007ACC;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 18px;
            font-weight: bold;
        }
        QPushButton#ActionBtn:hover {
            background-color: #0062A3;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #555555;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}

void FileSearchWindow::initUI() {
    auto* mainLayout = new QHBoxLayout(m_contentArea);
    mainLayout->setContentsMargins(15, 10, 15, 15);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(splitter);

    // --- 左侧边栏 ---
    auto* sidebarWidget = new QWidget();
    auto* sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(0, 0, 10, 0);
    sidebarLayout->setSpacing(10);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(5);
    auto* sidebarIcon = new QLabel();
    sidebarIcon->setPixmap(IconHelper::getIcon("folder", "#888").pixmap(14, 14));
    sidebarIcon->setStyleSheet("border: none; background: transparent;");
    headerLayout->addWidget(sidebarIcon);

    auto* sidebarHeader = new QLabel("收藏夹 (可拖入)");
    sidebarHeader->setStyleSheet("color: #888; font-weight: bold; font-size: 12px; border: none; background: transparent;");
    headerLayout->addWidget(sidebarHeader);
    headerLayout->addStretch();
    sidebarLayout->addLayout(headerLayout);

    auto* sidebar = new FileSidebarListWidget();
    m_sidebar = sidebar;
    m_sidebar->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebar->setMinimumWidth(200);
    m_sidebar->setDragEnabled(false);
    m_sidebar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sidebar, &FileSidebarListWidget::folderDropped, this, &FileSearchWindow::addFavorite);
    connect(m_sidebar, &QListWidget::itemClicked, this, &FileSearchWindow::onSidebarItemClicked);
    connect(m_sidebar, &QListWidget::customContextMenuRequested, this, &FileSearchWindow::showSidebarContextMenu);
    sidebarLayout->addWidget(m_sidebar);

    auto* btnAddFav = new QPushButton("一键添加当前路径");
    btnAddFav->setFixedHeight(32);
    btnAddFav->setCursor(Qt::PointingHandCursor);
    btnAddFav->setStyleSheet(
        "QPushButton { background-color: #2D2D30; border: 1px solid #444; color: #AAA; border-radius: 4px; font-size: 12px; }"
        "QPushButton:hover { background-color: #3E3E42; color: #FFF; border-color: #666; }"
    );
    connect(btnAddFav, &QPushButton::clicked, this, [this](){
        QString p = m_pathInput->text().trimmed();
        if (QDir(p).exists()) addFavorite(p);
    });
    sidebarLayout->addWidget(btnAddFav);

    splitter->addWidget(sidebarWidget);

    // --- 右侧主区域 ---
    auto* rightWidget = new QWidget();
    auto* layout = new QVBoxLayout(rightWidget);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(16);

    // 第一行：路径输入与浏览
    auto* pathLayout = new QHBoxLayout();
    m_pathInput = new QLineEdit();
    m_pathInput->setPlaceholderText("双击查看历史，或在此粘贴路径...");
    m_pathInput->setClearButtonEnabled(true);
    m_pathInput->installEventFilter(this);
    connect(m_pathInput, &QLineEdit::returnPressed, this, &FileSearchWindow::onPathReturnPressed);
    
    auto* btnBrowse = new QPushButton("浏览");
    btnBrowse->setObjectName("ActionBtn");
    btnBrowse->setAutoDefault(false);
    btnBrowse->setCursor(Qt::PointingHandCursor);
    connect(btnBrowse, &QPushButton::clicked, this, &FileSearchWindow::selectFolder);

    pathLayout->addWidget(m_pathInput);
    pathLayout->addWidget(btnBrowse);
    layout->addLayout(pathLayout);

    // 第二行：搜索过滤与后缀名
    auto* searchLayout = new QHBoxLayout();
    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("输入文件名过滤...");
    m_searchInput->installEventFilter(this);
    connect(m_searchInput, &QLineEdit::textChanged, this, &FileSearchWindow::refreshList);
    connect(m_searchInput, &QLineEdit::returnPressed, this, [this](){
        addSearchHistoryEntry(m_searchInput->text().trimmed());
    });

    m_extInput = new QLineEdit();
    m_extInput->setPlaceholderText("后缀 (如 py)");
    m_extInput->setFixedWidth(120);
    connect(m_extInput, &QLineEdit::textChanged, this, &FileSearchWindow::refreshList);

    searchLayout->addWidget(m_searchInput);
    searchLayout->addWidget(m_extInput);
    layout->addLayout(searchLayout);

    // 信息标签
    m_infoLabel = new QLabel("等待操作...");
    m_infoLabel->setStyleSheet("color: #888888; font-size: 12px;");
    layout->addWidget(m_infoLabel);

    // 文件列表
    m_fileList = new QListWidget();
    m_fileList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileList, &QListWidget::customContextMenuRequested, this, &FileSearchWindow::showFileContextMenu);
    layout->addWidget(m_fileList);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(1, 1);
}

void FileSearchWindow::selectFolder() {
    QString d = QFileDialog::getExistingDirectory(this, "选择文件夹");
    if (!d.isEmpty()) {
        m_pathInput->setText(d);
        startScan(d);
    }
}

void FileSearchWindow::onPathReturnPressed() {
    QString p = m_pathInput->text().trimmed();
    if (QDir(p).exists()) {
        startScan(p);
    } else {
        m_infoLabel->setText("路径不存在");
        m_pathInput->setStyleSheet("border: 1px solid #FF3333;");
    }
}

void FileSearchWindow::startScan(const QString& path) {
    m_pathInput->setStyleSheet("");
    if (m_scanThread) {
        m_scanThread->stop();
        m_scanThread->deleteLater();
    }

    m_fileList->clear();
    m_filesData.clear();
    m_infoLabel->setText("正在扫描: " + path);

    m_scanThread = new ScannerThread(path, this);
    connect(m_scanThread, &ScannerThread::fileFound, this, &FileSearchWindow::onFileFound);
    connect(m_scanThread, &ScannerThread::finished, this, &FileSearchWindow::onScanFinished);
    m_scanThread->start();
}

void FileSearchWindow::onFileFound(const QString& name, const QString& path) {
    m_filesData.append({name, path});
    if (m_filesData.size() % 300 == 0) {
        m_infoLabel->setText(QString("已发现 %1 个文件...").arg(m_filesData.size()));
    }
}

void FileSearchWindow::onScanFinished(int count) {
    m_infoLabel->setText(QString("扫描结束，共 %1 个文件").arg(count));
    addHistoryEntry(m_pathInput->text().trimmed());
    refreshList();
}

void FileSearchWindow::refreshList() {
    m_fileList->clear();
    QString txt = m_searchInput->text().toLower();
    QString ext = m_extInput->text().toLower().trimmed();
    if (ext.startsWith(".")) ext = ext.mid(1);

    int limit = 500;
    int shown = 0;

    for (const auto& data : m_filesData) {
        if (!ext.isEmpty() && !data.name.toLower().endsWith("." + ext)) continue;
        if (!txt.isEmpty() && !data.name.toLower().contains(txt)) continue;

        auto* item = new QListWidgetItem(data.name);
        item->setData(Qt::UserRole, data.path);
        item->setToolTip(data.path);
        m_fileList->addItem(item);
        
        shown++;
        if (shown >= limit) {
            auto* warn = new QListWidgetItem("--- 结果过多，仅显示前 500 条 ---");
            warn->setForeground(QColor("#FFAA00"));
            warn->setTextAlignment(Qt::AlignCenter);
            warn->setFlags(Qt::NoItemFlags);
            m_fileList->addItem(warn);
            break;
        }
    }
}

void FileSearchWindow::showFileContextMenu(const QPoint& pos) {
    auto* item = m_fileList->itemAt(pos);
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) return;

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2D2D30; border: 1px solid #444; color: #EEE; } QMenu::item:selected { background-color: #3E3E42; }");
    
    menu.addAction(IconHelper::getIcon("folder", "#F1C40F"), "定位文件夹", [filePath](){
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
    });
    menu.addAction(IconHelper::getIcon("search", "#4A90E2"), "定位文件", [filePath](){
#ifdef Q_OS_WIN
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(filePath);
        QProcess::startDetached("explorer.exe", args);
#endif
    });
    menu.addSeparator();
    menu.addAction(IconHelper::getIcon("copy", "#2ECC71"), "复制完整路径", [filePath](){
        QApplication::clipboard()->setText(filePath);
    });
    menu.addAction(IconHelper::getIcon("file", "#4A90E2"), "复制文件", [filePath](){
        QMimeData* mimeData = new QMimeData();
        mimeData->setUrls({QUrl::fromLocalFile(filePath)});
        QApplication::clipboard()->setMimeData(mimeData);
    });

    menu.exec(m_fileList->mapToGlobal(pos));
}

void FileSearchWindow::resizeEvent(QResizeEvent* event) {
    FramelessDialog::resizeEvent(event);
    if (m_resizeHandle) {
        m_resizeHandle->move(width() - 20, height() - 20);
    }
}

// ----------------------------------------------------------------------------
// 历史记录与收藏夹 逻辑实现
// ----------------------------------------------------------------------------
void FileSearchWindow::addHistoryEntry(const QString& path) {
    if (path.isEmpty() || !QDir(path).exists()) return;
    QSettings settings("RapidNotes", "FileSearchHistory");
    QStringList history = settings.value("list").toStringList();
    history.removeAll(path);
    history.prepend(path);
    while (history.size() > 10) history.removeLast();
    settings.setValue("list", history);
}

QStringList FileSearchWindow::getHistory() const {
    QSettings settings("RapidNotes", "FileSearchHistory");
    return settings.value("list").toStringList();
}

void FileSearchWindow::clearHistory() {
    QSettings settings("RapidNotes", "FileSearchHistory");
    settings.setValue("list", QStringList());
}

void FileSearchWindow::removeHistoryEntry(const QString& path) {
    QSettings settings("RapidNotes", "FileSearchHistory");
    QStringList history = settings.value("list").toStringList();
    history.removeAll(path);
    settings.setValue("list", history);
}

void FileSearchWindow::addSearchHistoryEntry(const QString& text) {
    if (text.isEmpty()) return;
    QSettings settings("RapidNotes", "FileSearchFilenameHistory");
    QStringList history = settings.value("list").toStringList();
    history.removeAll(text);
    history.prepend(text);
    while (history.size() > 10) history.removeLast();
    settings.setValue("list", history);
}

QStringList FileSearchWindow::getSearchHistory() const {
    QSettings settings("RapidNotes", "FileSearchFilenameHistory");
    return settings.value("list").toStringList();
}

void FileSearchWindow::removeSearchHistoryEntry(const QString& text) {
    QSettings settings("RapidNotes", "FileSearchFilenameHistory");
    QStringList history = settings.value("list").toStringList();
    history.removeAll(text);
    settings.setValue("list", history);
}

void FileSearchWindow::clearSearchHistory() {
    QSettings settings("RapidNotes", "FileSearchFilenameHistory");
    settings.setValue("list", QStringList());
}

void FileSearchWindow::useHistoryPath(const QString& path) {
    m_pathInput->setText(path);
    startScan(path);
}

void FileSearchWindow::onSidebarItemClicked(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    m_pathInput->setText(path);
    startScan(path);
}

void FileSearchWindow::showSidebarContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_sidebar->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #252526; border: 1px solid #444; color: #EEE; } QMenu::item:selected { background-color: #37373D; }");
    
    QAction* removeAct = menu.addAction(IconHelper::getIcon("delete", "#E74C3C"), "取消收藏");
    
    QAction* selected = menu.exec(m_sidebar->mapToGlobal(pos));
    if (selected == removeAct) {
        delete m_sidebar->takeItem(m_sidebar->row(item));
        saveFavorites();
    }
}

void FileSearchWindow::addFavorite(const QString& path) {
    // 检查是否已存在
    for (int i = 0; i < m_sidebar->count(); ++i) {
        if (m_sidebar->item(i)->data(Qt::UserRole).toString() == path) return;
    }

    QFileInfo fi(path);
    auto* item = new QListWidgetItem(IconHelper::getIcon("folder", "#F1C40F"), fi.fileName());
    item->setData(Qt::UserRole, path);
    item->setToolTip(path);
    m_sidebar->addItem(item);
    m_sidebar->sortItems(Qt::AscendingOrder); // 自动排序
    saveFavorites();
}

void FileSearchWindow::loadFavorites() {
    QSettings settings("RapidNotes", "FileSearchFavorites");
    QStringList favs = settings.value("list").toStringList();
    for (const QString& path : favs) {
        if (QDir(path).exists()) {
            QFileInfo fi(path);
            auto* item = new QListWidgetItem(IconHelper::getIcon("folder", "#F1C40F"), fi.fileName());
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            m_sidebar->addItem(item);
        }
    }
}

void FileSearchWindow::saveFavorites() {
    QStringList favs;
    for (int i = 0; i < m_sidebar->count(); ++i) {
        favs << m_sidebar->item(i)->data(Qt::UserRole).toString();
    }
    QSettings settings("RapidNotes", "FileSearchFavorites");
    settings.setValue("list", favs);
}

bool FileSearchWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == m_pathInput) {
            auto* popup = new FileSearchHistoryPopup(this, m_pathInput, FileSearchHistoryPopup::Path);
            popup->showAnimated();
            return true;
        } else if (watched == m_searchInput) {
            auto* popup = new FileSearchHistoryPopup(this, m_searchInput, FileSearchHistoryPopup::Filename);
            popup->showAnimated();
            return true;
        }
    }
    return FramelessDialog::eventFilter(watched, event);
}

#include "FileSearchWindow.moc"
