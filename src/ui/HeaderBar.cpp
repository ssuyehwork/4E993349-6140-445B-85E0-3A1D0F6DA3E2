#include "HeaderBar.h"
#include "IconHelper.h"
#include <QHBoxLayout>

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(50);
    setStyleSheet("background-color: #2D2D2D; border-bottom: 1px solid #333;");

    QHBoxLayout* layout = new QHBoxLayout(this);

    QPushButton* btnSidebar = new QPushButton();
    btnSidebar->setIcon(IconHelper::getIcon("sidebar", "#aaaaaa"));
    btnSidebar->setFixedSize(32, 32);
    btnSidebar->setStyleSheet("background: transparent; border: none;");
    connect(btnSidebar, &QPushButton::clicked, this, &HeaderBar::toggleSidebar);
    layout->addWidget(btnSidebar);

    m_searchEdit = new SearchLineEdit();
    m_searchEdit->setPlaceholderText("搜索灵感 (双击历史)...");
    m_searchEdit->setFixedWidth(300);
    m_searchEdit->setStyleSheet("background: #1e1e1e; border-radius: 15px; padding: 5px 15px; border: 1px solid #444; color: white;");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HeaderBar::searchChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, [this](){
        QString text = m_searchEdit->text().trimmed();
        if (text.isEmpty()) return;
        QSettings settings("RapidNotes", "SearchHistory");
        QStringList history = settings.value("list").toStringList();
        history.removeAll(text);
        history.prepend(text);
        if (history.size() > 20) history = history.mid(0, 20);
        settings.setValue("list", history);
    });
    connect(m_searchEdit, &SearchLineEdit::doubleClicked, this, &HeaderBar::setupSearchHistory);
    layout->addWidget(m_searchEdit);

    layout->addStretch();

    m_pageLabel = new QLabel("1 / 1");
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
    btnTool->setIcon(IconHelper::getIcon("toolbox", "#aaaaaa"));
    btnTool->setFixedSize(32, 32);
    btnTool->setStyleSheet("background: transparent; border: none;");
    connect(btnTool, &QPushButton::clicked, this, &HeaderBar::toolboxRequested);
    layout->addWidget(btnTool);

    QPushButton* btnPreview = new QPushButton();
    btnPreview->setIcon(IconHelper::getIcon("eye", "#aaaaaa"));
    btnPreview->setFixedSize(32, 32);
    btnPreview->setCheckable(true);
    btnPreview->setStyleSheet("QPushButton { background: transparent; border: none; } QPushButton:checked { background: #444; border-radius: 4px; }");
    connect(btnPreview, &QPushButton::toggled, this, &HeaderBar::previewToggled);
    layout->addWidget(btnPreview);
}

#include <QSettings>

void HeaderBar::setupSearchHistory() {
    QSettings settings("RapidNotes", "SearchHistory");
    QStringList history = settings.value("list").toStringList();

    if (history.isEmpty()) return;

    QMenu* menu = new QMenu(this);
    menu->setStyleSheet("QMenu { background-color: #2D2D2D; color: white; border: 1px solid #444; } QMenu::item:selected { background-color: #37373D; }");

    for (const QString& item : history) {
        QAction* act = menu->addAction(item);
        connect(act, &QAction::triggered, [this, item](){
            m_searchEdit->setText(item);
        });
    }

    menu->addSeparator();
    QAction* clearAct = menu->addAction("清空历史");
    connect(clearAct, &QAction::triggered, [](){
        QSettings settings("RapidNotes", "SearchHistory");
        settings.setValue("list", QStringList());
    });

    menu->exec(m_searchEdit->mapToGlobal(QPoint(0, m_searchEdit->height())));
}
