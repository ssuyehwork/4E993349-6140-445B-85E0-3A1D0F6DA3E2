#include "HeaderBar.h"
#include "IconHelper.h"
#include <QHBoxLayout>
#include <QSettings>
#include <QMouseEvent>
#include <QApplication>
#include <QWindow>

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(40);
    setStyleSheet("background-color: #252526; border-bottom: 1px solid #333;");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(4);
    
    QPushButton* btnSidebar = new QPushButton();
    btnSidebar->setIcon(IconHelper::getIcon("sidebar", "#aaaaaa", 20));
    btnSidebar->setIconSize(QSize(20, 20));
    btnSidebar->setFixedSize(32, 32);
    btnSidebar->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: rgba(255,255,255,0.1); }");
    connect(btnSidebar, &QPushButton::clicked, this, &HeaderBar::toggleSidebar);
    layout->addWidget(btnSidebar);

    m_searchEdit = new SearchLineEdit();
    m_searchEdit->setPlaceholderText("搜索灵感 (双击查看历史)...");
    m_searchEdit->setFixedWidth(300);
    // 恢复深色背景、15px圆角和紧凑高度 (5px内边距)，确保在标题栏中的精致感和对比度
    m_searchEdit->setStyleSheet("background: #1e1e1e; border-radius: 15px; padding: 5px 15px; border: 1px solid #444; color: white;");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HeaderBar::searchChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, [this](){
        m_searchEdit->addHistoryEntry(m_searchEdit->text().trimmed());
    });
    layout->addWidget(m_searchEdit);

    layout->addStretch();

    m_pageLabel = new QLabel("1 / 1");
    m_pageLabel->setStyleSheet("background: transparent;"); // 确保透明，继承全局文本颜色
    layout->addWidget(m_pageLabel);

    QPushButton* btnPrev = new QPushButton();
    btnPrev->setIcon(IconHelper::getIcon("nav_prev", "#aaaaaa"));
    btnPrev->setStyleSheet("background: transparent; border: none;");
    connect(btnPrev, &QPushButton::clicked, [this](){ if(m_currentPage > 1) emit pageChanged(--m_currentPage); });
    layout->addWidget(btnPrev);

    QPushButton* btnNext = new QPushButton();
    btnNext->setIcon(IconHelper::getIcon("nav_next", "#aaaaaa"));
    btnNext->setStyleSheet("background: transparent; border: none;");
    connect(btnNext, &QPushButton::clicked, [this](){ emit pageChanged(++m_currentPage); });
    layout->addWidget(btnNext);

    QPushButton* btnAdd = new QPushButton();
    btnAdd->setIcon(IconHelper::getIcon("add", "#ffffff"));
    btnAdd->setFixedSize(80, 32);
    btnAdd->setText(" 新建");
    btnAdd->setStyleSheet("background-color: #0E639C; color: white; font-weight: bold; border-radius: 4px;");
    connect(btnAdd, &QPushButton::clicked, this, &HeaderBar::newNoteRequested);
    layout->addWidget(btnAdd);

    QPushButton* btnTool = new QPushButton();
    btnTool->setIcon(IconHelper::getIcon("toolbox", "#aaaaaa", 20));
    btnTool->setIconSize(QSize(20, 20));
    btnTool->setFixedSize(32, 32);
    btnTool->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: rgba(255,255,255,0.1); }");
    connect(btnTool, &QPushButton::clicked, this, &HeaderBar::toolboxRequested);
    layout->addWidget(btnTool);

    QPushButton* btnPreview = new QPushButton();
    btnPreview->setIcon(IconHelper::getIcon("eye", "#aaaaaa", 20));
    btnPreview->setIconSize(QSize(20, 20));
    btnPreview->setFixedSize(32, 32);
    btnPreview->setCheckable(true);
    btnPreview->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: rgba(255,255,255,0.1); } QPushButton:checked { background: #0E639C; }");
    connect(btnPreview, &QPushButton::toggled, this, &HeaderBar::previewToggled);
    layout->addWidget(btnPreview);

    layout->addSpacing(10);

    // 窗口控制按钮容器，实现 0 间距紧凑布局
    QWidget* winCtrlWidget = new QWidget();
    winCtrlWidget->setStyleSheet("background: transparent;"); // 关键：防止显示全局 QWidget 背景色
    QHBoxLayout* winCtrlLayout = new QHBoxLayout(winCtrlWidget);
    winCtrlLayout->setContentsMargins(0, 0, 0, 0);
    winCtrlLayout->setSpacing(0);

    auto addWinBtn = [&](const QString& icon, const QString& hoverColor, auto signal) {
        QPushButton* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon(icon, "#aaaaaa", 20)); // 图标增大到 20px，锁定比例
        btn->setIconSize(QSize(20, 20));
        btn->setFixedSize(32, 32);
        btn->setStyleSheet(QString("QPushButton { background: transparent; border: none; border-radius: 0px; } QPushButton:hover { background: %1; }").arg(hoverColor));
        connect(btn, &QPushButton::clicked, this, signal);
        winCtrlLayout->addWidget(btn);
    };

    addWinBtn("minimize", "rgba(255,255,255,0.1)", &HeaderBar::windowMinimize);
    addWinBtn("maximize", "rgba(255,255,255,0.1)", &HeaderBar::windowMaximize);
    addWinBtn("close", "#e81123", &HeaderBar::windowClose);
    layout->addWidget(winCtrlWidget);
}

void HeaderBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (auto* win = window()) {
            if (auto* handle = win->windowHandle()) {
                handle->startSystemMove();
            }
        }
        event->accept();
    }
}

void HeaderBar::mouseMoveEvent(QMouseEvent* event) {
    QWidget::mouseMoveEvent(event);
}

void HeaderBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit windowMaximize();
        event->accept();
    }
}

