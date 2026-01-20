#include "MainWindow.h"
#include "GraphWidget.h"
#include "NoteDelegate.h"
#include "../core/DatabaseManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTabWidget>
#include <QStandardItemModel>
#include <QLabel>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("极速灵感 (RapidNotes) - 开发版");
    resize(1200, 800);
    initUI();
}

void MainWindow::initUI() {
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);

    // 1. 左侧导航 (Pro 级分类树)
    m_sideBar = new QTreeView();
    auto* sideModel = new QStandardItemModel(this);
    auto* root = sideModel->invisibleRootItem();
    root->appendRow(new QStandardItem("全部笔记"));
    root->appendRow(new QStandardItem("今日计划"));
    root->appendRow(new QStandardItem("未分类"));
    root->appendRow(new QStandardItem("回收站"));

    auto* userFolder = new QStandardItem("我的文件夹");
    userFolder->appendRow(new QStandardItem("工作项目"));
    userFolder->appendRow(new QStandardItem("灵感草稿"));
    root->appendRow(userFolder);

    m_sideBar->setModel(sideModel);
    m_sideBar->setHeaderHidden(true);
    m_sideBar->expandAll();
    m_sideBar->setStyleSheet("background: #252526; border: none; color: #CCCCCC;");

    // 2. 中间列表 (商业级卡片式设计)
    m_noteList = new QListView();
    m_model = new NoteModel(this);
    m_noteList->setModel(m_model);
    m_noteList->setItemDelegate(new NoteDelegate(this));
    m_noteList->setSpacing(5);
    m_noteList->setFrameShape(QFrame::NoFrame);
    m_noteList->setStyleSheet("background: #1E1E1E; border: none;");

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

    // 4. 右侧元数据面板
    m_metaPanel = new QWidget();
    m_metaPanel->setFixedWidth(250);
    m_metaPanel->setStyleSheet("background: #252526; border-left: 1px solid #333;");
    auto* metaLayout = new QVBoxLayout(m_metaPanel);
    metaLayout->setContentsMargins(15, 20, 15, 20);

    auto* titleLabel = new QLabel("元数据详情");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: #007ACC;");
    metaLayout->addWidget(titleLabel);
    metaLayout->addSpacing(20);

    metaLayout->addWidget(new QLabel("标签管理:"));
    metaLayout->addWidget(new QLineEdit());
    metaLayout->addSpacing(10);

    metaLayout->addWidget(new QLabel("星级评分:"));
    auto* starLayout = new QHBoxLayout();
    for(int i=0; i<5; ++i) starLayout->addWidget(new QPushButton("★"));
    metaLayout->addLayout(starLayout);

    metaLayout->addStretch();
    auto* pinBtn = new QPushButton("置顶笔记");
    metaLayout->addWidget(pinBtn);

    splitter->addWidget(m_sideBar);
    splitter->addWidget(m_noteList);
    splitter->addWidget(rightTab);
    splitter->addWidget(m_metaPanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 4);
    splitter->setStretchFactor(3, 1);

    mainLayout->addWidget(splitter);

    // 加载数据
    m_model->setNotes(DatabaseManager::instance().getAllNotes());
    connect(&DatabaseManager::instance(), &DatabaseManager::noteAdded, [this](){
        m_model->setNotes(DatabaseManager::instance().getAllNotes());
    });
}
