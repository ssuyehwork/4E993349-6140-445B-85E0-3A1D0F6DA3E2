#include "HeaderBar.h"
#include <QHBoxLayout>
#include <QStyle>

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(50);
    setStyleSheet("background-color: #2D2D2D; border-bottom: 1px solid #333;");

    QHBoxLayout* layout = new QHBoxLayout(this);

    QPushButton* btnSidebar = new QPushButton("☰");
    btnSidebar->setFixedSize(30, 30);
    connect(btnSidebar, &QPushButton::clicked, this, &HeaderBar::toggleSidebar);
    layout->addWidget(btnSidebar);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索灵感...");
    m_searchEdit->setFixedWidth(300);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HeaderBar::searchChanged);
    layout->addWidget(m_searchEdit);

    layout->addStretch();

    m_pageLabel = new QLabel("1 / 1");
    layout->addWidget(m_pageLabel);

    QPushButton* btnPrev = new QPushButton("<");
    connect(btnPrev, &QPushButton::clicked, [this](){ if(m_currentPage > 1) emit pageChanged(--m_currentPage); });
    layout->addWidget(btnPrev);

    QPushButton* btnNext = new QPushButton(">");
    connect(btnNext, &QPushButton::clicked, [this](){ emit pageChanged(++m_currentPage); });
    layout->addWidget(btnNext);

    QPushButton* btnAdd = new QPushButton("+ 新建");
    btnAdd->setStyleSheet("background-color: #0E639C; color: white; font-weight: bold;");
    connect(btnAdd, &QPushButton::clicked, this, &HeaderBar::newNoteRequested);
    layout->addWidget(btnAdd);

    QPushButton* btnTool = new QPushButton("🛠️");
    connect(btnTool, &QPushButton::clicked, this, &HeaderBar::toolboxRequested);
    layout->addWidget(btnTool);

    QPushButton* btnPreview = new QPushButton("👁️");
    btnPreview->setCheckable(true);
    connect(btnPreview, &QPushButton::toggled, this, &HeaderBar::previewToggled);
    layout->addWidget(btnPreview);
}
