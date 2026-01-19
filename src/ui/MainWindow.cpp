#include "MainWindow.h"
#include "GraphWidget.h"
#include "../core/DatabaseManager.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTabWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("极速灵感 (RapidNotes)");
    resize(1200, 800);
    initUI();
}

void MainWindow::initUI() {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);

    // 1. 左侧导航
    m_sideBar = new QTreeView();
    m_sideBar->setHeaderHidden(true);
    m_sideBar->setStyleSheet("background: #252526; border: none; color: #CCCCCC;");

    // 2. 中间列表
    m_noteList = new QListView();
    m_model = new NoteModel(this);
    m_noteList->setModel(m_model);
    m_noteList->setStyleSheet("background: #1E1E1E; border: none; color: #D4D4D4;");

    // 3. 右侧内容区 (编辑器 + 图谱)
    auto* rightTab = new QTabWidget();
    rightTab->setStyleSheet("QTabBar::tab { background: #2D2D2D; color: #CCC; padding: 10px; } QTabBar::tab:selected { background: #1E1E1E; }");

    m_editorArea = new QWidget();
    auto* editorLayout = new QVBoxLayout(m_editorArea);
    auto* edit = new QTextEdit();
    edit->setPlaceholderText("在这里记录你的灵感...");
    edit->setStyleSheet("border: none; font-size: 14pt;");
    editorLayout->addWidget(edit);

    auto* graphWidget = new GraphWidget();

    rightTab->addTab(m_editorArea, "编辑器");
    rightTab->addTab(graphWidget, "知识图谱");

    splitter->addWidget(m_sideBar);
    splitter->addWidget(m_noteList);
    splitter->addWidget(rightTab);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 4);

    mainLayout->addWidget(splitter);

    // 加载数据
    m_model->setNotes(DatabaseManager::instance().getAllNotes());
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, [this](){
        m_model->setNotes(DatabaseManager::instance().getAllNotes());
    });
}
