#include "QuickWindow.h"
#include "NoteEditWindow.h"
#include "../core/DatabaseManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>

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
    mainLayout->setContentsMargins(10, 10, 10, 10);

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
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 5);
    container->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索笔记或按回车保存...");
    layout->addWidget(m_searchEdit);

    auto* hLayout = new QHBoxLayout();
    
    m_sideBar = new QTreeView();
    m_sideModel = new CategoryModel(this);
    m_sideBar->setModel(m_sideModel);
    m_sideBar->setHeaderHidden(true);
    m_sideBar->setFixedWidth(180);
    m_sideBar->expandAll();
    connect(m_sideBar, &QTreeView::clicked, this, [this](const QModelIndex& index){
        QString type = index.data(Qt::UserRole).toString();
        if (type == "category") {
            int catId = index.data(Qt::UserRole + 1).toInt();
            m_model->setNotes(DatabaseManager::instance().searchNotes("", "category", catId));
        } else {
            m_model->setNotes(DatabaseManager::instance().searchNotes("", type));
        }
    });
    hLayout->addWidget(m_sideBar);

    m_listView = new QListView();
    m_model = new NoteModel(this);
    m_listView->setModel(m_model);
    connect(m_listView, &QListView::doubleClicked, this, [this](const QModelIndex& index){
        if (!index.isValid()) return;
        int id = index.data(NoteModel::IdRole).toInt();
        NoteEditWindow* win = new NoteEditWindow(id);
        connect(win, &NoteEditWindow::noteSaved, this, &QuickWindow::refreshData);
        win->show();
    });
    hLayout->addWidget(m_listView);
    
    layout->addLayout(hLayout);
    
    mainLayout->addWidget(container);
    setFixedSize(800, 500);
    
    // 搜索逻辑
    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text){
        m_model->setNotes(DatabaseManager::instance().searchNotes(text));
    });

    // 回车保存逻辑
    connect(m_searchEdit, &QLineEdit::returnPressed, [this](){
        QString text = m_searchEdit->text();
        if (!text.isEmpty()) {
            DatabaseManager::instance().addNoteAsync("快速记录", text, {"Quick"});
            m_searchEdit->clear();
            hide();
        }
    });

    refreshData();
}

void QuickWindow::refreshData() {
    if (m_searchEdit->text().isEmpty()) {
        m_model->setNotes(DatabaseManager::instance().getAllNotes());
    }
}

void QuickWindow::showCentered() {
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