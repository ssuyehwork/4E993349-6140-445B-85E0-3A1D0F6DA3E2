#include "QuickToolbar.h"
#include "IconHelper.h"
#include <QIntValidator>

QuickToolbar::QuickToolbar(QWidget* parent) : QWidget(parent) {
    setFixedWidth(40);
    setStyleSheet("background-color: #252526; border-left: 1px solid #333; border-top-right-radius: 10px; border-bottom-right-radius: 10px;");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 10);
    layout->setSpacing(5);
    layout->setAlignment(Qt::AlignHCenter);

    auto addBtn = [&](const QString& icon, const QString& tip, auto signal, bool checkable = false) {
        QPushButton* btn = new QPushButton();
        btn->setFixedSize(30, 30);
        btn->setIcon(IconHelper::getIcon(icon, "#aaaaaa"));
        btn->setToolTip(tip);
        btn->setCheckable(checkable);
        btn->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } "
                           "QPushButton:hover { background: #3e3e42; } "
                           "QPushButton:checked { background: #0E639C; }");
        connect(btn, &QPushButton::clicked, this, signal);
        layout->addWidget(btn);
        return btn;
    };

    addBtn("close", "关闭", &QuickToolbar::closeRequested);
    addBtn("maximize", "切换主界面", &QuickToolbar::openFullRequested);
    addBtn("minimize", "最小化", &QuickToolbar::minimizeRequested);

    layout->addSpacing(10);

    m_btnStayTop = addBtn("pin", "保持置顶", nullptr, true);
    connect(m_btnStayTop, &QPushButton::toggled, this, &QuickToolbar::toggleStayOnTop);

    addBtn("eye", "显示/隐藏侧边栏", &QuickToolbar::toggleSidebar);
    addBtn("refresh", "刷新", &QuickToolbar::refreshRequested);

    layout->addSpacing(15);

    // 分页
    m_btnPrev = addBtn("nav_prev", "上一页", &QuickToolbar::prevPage);

    m_pageEdit = new QLineEdit("1");
    m_pageEdit->setFixedWidth(28);
    m_pageEdit->setAlignment(Qt::AlignCenter);
    m_pageEdit->setValidator(new QIntValidator(1, 999, this));
    m_pageEdit->setStyleSheet("background: #1e1e1e; color: #ddd; border: 1px solid #444; border-radius: 4px; font-size: 11px;");
    connect(m_pageEdit, &QLineEdit::returnPressed, [this](){
        emit jumpToPage(m_pageEdit->text().toInt());
    });
    layout->addWidget(m_pageEdit, 0, Qt::AlignHCenter);

    m_totalPageLabel = new QLabel("1");
    m_totalPageLabel->setStyleSheet("color: #666; font-size: 10px;");
    layout->addWidget(m_totalPageLabel, 0, Qt::AlignHCenter);

    m_btnNext = addBtn("nav_next", "下一页", &QuickToolbar::nextPage);

    layout->addStretch();

    // 垂直文字
    QLabel* lblTitle = new QLabel("快\n速\n笔\n记");
    lblTitle->setStyleSheet("color: #444; font-weight: bold; font-size: 11px; line-height: 1.2;");
    lblTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(lblTitle);

    layout->addSpacing(10);

    QLabel* logo = new QLabel();
    logo->setPixmap(IconHelper::getIcon("zap", "#0E639C", 20).pixmap(20, 20));
    layout->addWidget(logo, 0, Qt::AlignHCenter);
}

void QuickToolbar::setPageInfo(int current, int total) {
    m_pageEdit->setText(QString::number(current));
    m_totalPageLabel->setText(QString::number(total));
    m_btnPrev->setEnabled(current > 1);
    m_btnNext->setEnabled(current < total);
}

void QuickToolbar::setStayOnTop(bool onTop) {
    m_btnStayTop->setChecked(onTop);
}
