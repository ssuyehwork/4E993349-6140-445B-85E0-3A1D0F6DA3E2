#include "AdvancedTagSelector.h"
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QGraphicsDropShadowEffect>

AdvancedTagSelector::AdvancedTagSelector(QWidget* parent) : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground); // 透明背景以便绘制圆角和阴影
    setFixedSize(360, 450); // 对齐 Python 版尺寸

    // 主布局
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10); // 预留阴影空间

    // 内部容器 (模拟 Python #mainContainer)
    auto* container = new QWidget(this);
    container->setObjectName("mainContainer");
    container->setStyleSheet(
        "#mainContainer {"
        "  background-color: #1E1E1E;"
        "  border: 1px solid #333;"
        "  border-radius: 8px;"
        "}"
    );

    // 阴影效果
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 100));
    container->setGraphicsEffect(shadow);

    mainLayout->addWidget(container);

    // 容器布局
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // 1. 搜索框 (对齐 Python 样式: 无边框，底部下划线风格)
    m_search = new QLineEdit();
    m_search->setPlaceholderText("🔍 搜索或新建...");
    m_search->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2D2D2D;"
        "  border: none;"
        "  border-bottom: 1px solid #444;"
        "  border-radius: 4px;"
        "  padding: 8px;"
        "  font-size: 13px;"
        "  color: #DDD;"
        "}"
        "QLineEdit:focus { border-bottom: 1px solid #4a90e2; }"
    );
    connect(m_search, &QLineEdit::textChanged, this, &AdvancedTagSelector::updateList);
    connect(m_search, &QLineEdit::returnPressed, this, [this](){
        QString text = m_search->text().trimmed();
        if (!text.isEmpty()) {
            if (!m_selected.contains(text)) {
                m_selected.append(text);
                emit tagsChanged();
                updateList();
            }
            m_search->clear();
        } else {
            emit tagsConfirmed(m_selected);
            hide();
        }
    });
    layout->addWidget(m_search);

    // 2. 提示标签
    m_tipsLabel = new QLabel("最近使用");
    m_tipsLabel->setStyleSheet("color: #888; font-size: 12px; font-weight: bold; margin-top: 5px;");
    layout->addWidget(m_tipsLabel);

    // 3. 滚动区域
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #444; border-radius: 3px; }"
    );

    m_tagContainer = new QWidget();
    m_tagContainer->setStyleSheet("background: transparent;");
    
    // FlowLayout 参数: margin=0, hSpacing=8, vSpacing=8
    m_flow = new FlowLayout(m_tagContainer, 0, 8, 8);
    scroll->setWidget(m_tagContainer);
    layout->addWidget(scroll);
}

void AdvancedTagSelector::setup(const QStringList& allTags, const QStringList& selectedTags) {
    setTags(allTags, selectedTags);
    m_tipsLabel->setText(QString("所有标签 (%1)").arg(allTags.count()));
}

void AdvancedTagSelector::setTags(const QStringList& allTags, const QStringList& selectedTags) {
    m_all = allTags;
    m_selected = selectedTags;
    updateList();
}

void AdvancedTagSelector::updateList() {
    // 清空现有项
    QLayoutItem* child;
    while ((child = m_flow->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    QString filter = m_search->text().toLower();
    
    // 合并列表逻辑：为了更好的视觉效果，我们将“已选”置顶，
    // 但样式上保持统一的 Chip 风格，而不是像之前那样分两块区域。
    // 如果想要完全还原 Python 的混排逻辑，这里应该不分离。
    // 但 C++ 版目前 AllTags 包含了 SelectedTags 吗？通常是的。
    // 我们先去重合并，保证顺序体验。
    
    QStringList displayList;
    // 1. 先加入已选的 (避免被过滤掉)
    for(const auto& t : m_selected) {
        if (!displayList.contains(t)) displayList.append(t);
    }
    // 2. 再加入其他的
    for(const auto& t : m_all) {
        if (!displayList.contains(t)) displayList.append(t);
    }

    for (const QString& tag : displayList) {
        // 过滤逻辑
        if (!filter.isEmpty() && !tag.toLower().contains(filter)) {
            // 如果是已选中的，即使不匹配也显示？通常是都过滤。
            // 但用户可能想取消勾选。这里为了简单，遵循过滤。
            // 除非是完全匹配。
            continue; 
        }

        bool isSelected = m_selected.contains(tag);
        
        auto* btn = new QPushButton();
        btn->setCheckable(true);
        btn->setChecked(isSelected);
        btn->setCursor(Qt::PointingHandCursor);
        
        // 文本：选中时添加 "✓ " 前缀
        QString text = tag;
        if (isSelected) text = "✓ " + text;
        btn->setText(text);

        // 样式：完全复刻 Python _update_chip_state 逻辑
        if (isSelected) {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #4a90e2;" // Primary Blue
                "  color: white;"
                "  border: 1px solid #4a90e2;"
                "  border-radius: 14px;" // Python 用的 14px
                "  padding: 6px 14px;"    // 增加 padding
                "  font-size: 12px;"
                "  font-family: 'Microsoft YaHei';"
                "}"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: #2D2D2D;"
                "  color: #BBB;"
                "  border: 1px solid #444;"
                "  border-radius: 14px;"
                "  padding: 6px 14px;"
                "  font-size: 12px;"
                "  font-family: 'Microsoft YaHei';"
                "}"
                "QPushButton:hover {"
                "  background-color: #383838;"
                "  border-color: #666;"
                "  color: white;"
                "}"
            );
        }
        
        connect(btn, &QPushButton::clicked, this, [this, tag](){ toggleTag(tag); });
        m_flow->addWidget(btn);
    }
}

void AdvancedTagSelector::toggleTag(const QString& tag) {
    if (m_selected.contains(tag)) {
        m_selected.removeAll(tag);
    } else {
        m_selected.append(tag);
    }
    emit tagsChanged();
    updateList();
    m_search->setFocus(); // 保持焦点以便继续打字
}

void AdvancedTagSelector::showAtCursor() {
    QPoint pos = QCursor::pos();
    // 稍微偏移，使得鼠标位于面板内但遮挡输入框
    // Python 逻辑: move(pos.x() - 300, pos.y() - 20) -> 这里的 300 可能是为了向左对齐???
    // 这里我们做智能调整
    
    QScreen *screen = QGuiApplication::screenAt(pos);
    if (screen) {
        QRect geo = screen->geometry();
        // 尝试将窗口显示在鼠标右下方，如果超出的左移上移
        int x = pos.x();
        int y = pos.y() + 20;

        if (x + width() > geo.right()) x = geo.right() - width() - 10;
        if (y + height() > geo.bottom()) y = geo.bottom() - height() - 10;
        
        move(x, y);
    }
    show();
    activateWindow();
    m_search->setFocus();
}

void AdvancedTagSelector::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit tagsConfirmed(m_selected); // Esc 关闭时也确认？或者取消？这里设为确认并关闭
        hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}
