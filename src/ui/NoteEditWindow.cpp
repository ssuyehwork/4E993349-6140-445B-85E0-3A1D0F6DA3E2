#include "NoteEditWindow.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QMessageBox>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QWindow>
#include <QMouseEvent>

NoteEditWindow::NoteEditWindow(int noteId, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint), m_noteId(noteId)
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(900, 600);
    initUI();

    if (m_noteId > 0) {
        loadNoteData(m_noteId);
    }
}

void NoteEditWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(QColor("#252526"));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10);
}

void NoteEditWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (auto* handle = windowHandle()) {
            handle->startSystemMove();
        }
        event->accept();
    }
}

void NoteEditWindow::initUI() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 左侧面板
    QWidget* leftPanel = new QWidget();
    leftPanel->setStyleSheet("background-color: #1E1E1E; border-top-left-radius: 10px; border-bottom-left-radius: 10px; border-right: 1px solid #333;");
    leftPanel->setFixedWidth(280);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(20, 20, 20, 20);
    setupLeftPanel(leftLayout);

    // 右侧面板
    QWidget* rightPanel = new QWidget();
    rightPanel->setStyleSheet("background-color: #252526; border-top-right-radius: 10px; border-bottom-right-radius: 10px;");
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(20, 10, 20, 20);
    setupRightPanel(rightLayout);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 5);
    setGraphicsEffect(shadow);

    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(rightPanel);

    // 关闭按钮
    QPushButton* closeBtn = new QPushButton(this);
    closeBtn->setIcon(IconHelper::getIcon("close", "#aaaaaa"));
    closeBtn->setGeometry(width() - 40, 10, 30, 30);
    closeBtn->setStyleSheet("QPushButton { background: transparent; border: none; } QPushButton:hover { background: #c42b1c; border-radius: 5px; }");
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
}

void NoteEditWindow::setupLeftPanel(QVBoxLayout* layout) {
    QString labelStyle = "color: #888; font-size: 12px; margin-bottom: 5px; margin-top: 10px;";
    QString inputStyle = "QLineEdit, QComboBox { background: #2D2D2D; border: 1px solid #3E3E42; border-radius: 4px; padding: 8px; color: #EEE; font-size: 13px; }";

    QWidget* titleArea = new QWidget();
    QHBoxLayout* titleLayout = new QHBoxLayout(titleArea);
    titleLayout->setContentsMargins(0,0,0,0);
    QLabel* titleIcon = new QLabel();
    titleIcon->setPixmap(IconHelper::getIcon("edit", "#4FACFE", 24).pixmap(24, 24));
    titleLayout->addWidget(titleIcon);
    QLabel* winTitle = new QLabel("记录灵感");
    winTitle->setStyleSheet("color: #EEE; font-size: 16px; font-weight: bold;");
    titleLayout->addWidget(winTitle);
    titleLayout->addStretch();
    layout->addWidget(titleArea);
    layout->addSpacing(10);

    QLabel* lblCat = new QLabel("分区");
    lblCat->setStyleSheet(labelStyle);
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItem("未分类", -1);
    auto categories = DatabaseManager::instance().getAllCategories();
    for (const auto& cat : categories) {
        m_categoryCombo->addItem(cat["name"].toString(), cat["id"]);
    }
    m_categoryCombo->setStyleSheet(inputStyle);
    layout->addWidget(lblCat);
    layout->addWidget(m_categoryCombo);

    QLabel* lblTitle = new QLabel("标题");
    lblTitle->setStyleSheet(labelStyle);
    m_titleEdit = new QLineEdit();
    m_titleEdit->setPlaceholderText("请输入灵感标题...");
    m_titleEdit->setStyleSheet(inputStyle);
    layout->addWidget(lblTitle);
    layout->addWidget(m_titleEdit);

    QLabel* lblTags = new QLabel("标签");
    lblTags->setStyleSheet(labelStyle);
    m_tagEdit = new QLineEdit();
    m_tagEdit->setPlaceholderText("使用逗号分隔");
    m_tagEdit->setStyleSheet(inputStyle);
    layout->addWidget(lblTags);
    layout->addWidget(m_tagEdit);

    QLabel* lblColor = new QLabel("标记颜色");
    lblColor->setStyleSheet(labelStyle);
    layout->addWidget(lblColor);

    QWidget* colorGrid = new QWidget();
    QGridLayout* grid = new QGridLayout(colorGrid);
    grid->setContentsMargins(0, 10, 0, 10);

    m_colorGroup = new QButtonGroup(this);
    QStringList colors = {"#FF9800", "#444444", "#2196F3", "#4CAF50", "#F44336", "#9C27B0"};
    for(int i=0; i<colors.size(); ++i) {
        QPushButton* btn = createColorBtn(colors[i], i);
        grid->addWidget(btn, i/3, i%3);
        m_colorGroup->addButton(btn, i);
    }
    if(m_colorGroup->button(0)) m_colorGroup->button(0)->setChecked(true);

    layout->addWidget(colorGrid);

    layout->addStretch();

    QPushButton* saveBtn = new QPushButton();
    saveBtn->setIcon(IconHelper::getIcon("save", "#ffffff"));
    saveBtn->setText(" 保存 (Ctrl+S)");
    saveBtn->setShortcut(QKeySequence("Ctrl+S"));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setFixedHeight(45);
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #409EFF; color: white; border-radius: 4px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #66B1FF; }"
    );
    connect(saveBtn, &QPushButton::clicked, [this](){
        QString title = m_titleEdit->text();
        if(title.isEmpty()) title = "未命名灵感";
        QString content = m_contentEdit->toPlainText();
        QString tags = m_tagEdit->text();
        int catId = m_categoryCombo->currentData().toInt();
        QString color = m_colorGroup->checkedButton() ? m_colorGroup->checkedButton()->property("color").toString() : "";

        if (m_noteId == 0) {
            DatabaseManager::instance().addNoteAsync(title, content, tags.split(","), color, catId);
        } else {
            DatabaseManager::instance().updateNote(m_noteId, title, content, tags.split(","), color, catId);
        }
        emit noteSaved();
        close();
    });
    layout->addWidget(saveBtn);
}

QPushButton* NoteEditWindow::createColorBtn(const QString& color, int id) {
    QPushButton* btn = new QPushButton();
    btn->setCheckable(true);
    btn->setFixedSize(30, 30);
    btn->setProperty("color", color);
    btn->setStyleSheet(QString(
        "QPushButton { background-color: %1; border-radius: 15px; border: 2px solid transparent; }"
        "QPushButton:checked { border: 2px solid white; }"
    ).arg(color));
    return btn;
}

void NoteEditWindow::setupRightPanel(QVBoxLayout* layout) {
    QHBoxLayout* toolBar = new QHBoxLayout();

    auto addTool = [&](const QString& iconName, const QString& tip) {
        QPushButton* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon(iconName, "#aaaaaa"));
        btn->setToolTip(tip);
        btn->setFixedSize(35, 35);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("QPushButton { background: transparent; border: 1px solid #333; border-radius: 4px; } QPushButton:hover { background: #333; }");
        toolBar->addWidget(btn);
        return btn;
    };

    addTool("undo", "撤销");
    addTool("redo", "重做");
    addTool("list_ul", "无序列表");
    addTool("list_ol", "有序列表");
    addTool("todo", "待办事项");
    addTool("trash", "删除内容");

    toolBar->addStretch();
    layout->addLayout(toolBar);

    layout->addSpacing(10);
    m_contentEdit = new Editor();
    m_contentEdit->setPlaceholderText("在这里记录详细内容...");
    layout->addWidget(m_contentEdit);
}

void NoteEditWindow::loadNoteData(int id) {
    auto notes = DatabaseManager::instance().getAllNotes();
    for(const auto& note : notes) {
        if (note["id"].toInt() == id) {
            m_titleEdit->setText(note["title"].toString());
            m_contentEdit->setPlainText(note["content"].toString());
            m_tagEdit->setText(note["tags"].toString());
            break;
        }
    }
}