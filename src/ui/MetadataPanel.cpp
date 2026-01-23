#include "MetadataPanel.h"
#include "AdvancedTagSelector.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>
#include <QDateTime>

MetadataPanel::MetadataPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(240);
    // 移除 border-left 以消除“虚线”感，背景使用 Python 版的 bg_mid (#252526)
    setStyleSheet("QWidget#MetadataPanel { background-color: #252526; border: none; }");
    setObjectName("MetadataPanel");
    initUI();
}

void MetadataPanel::initUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);

    // 1. 顶部标题
    QWidget* titleContainer = new QWidget();
    titleContainer->setStyleSheet("background-color: transparent;");
    QHBoxLayout* titleLayout = new QHBoxLayout(titleContainer);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    QLabel* icon = new QLabel();
    icon->setPixmap(IconHelper::getIcon("all_data", "#4a90e2", 18).pixmap(18, 18));
    icon->setStyleSheet("background: transparent; border: none;");

    QLabel* lbl = new QLabel("元数据");
    lbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #4a90e2; background: transparent; border: none;");

    titleLayout->addWidget(icon);
    titleLayout->addWidget(lbl);
    titleLayout->addStretch();
    layout->addWidget(titleContainer);

    // 2. 状态栈 (无选择 / 多选 / 详情)
    m_stack = new QStackedWidget();
    m_stack->setStyleSheet("background-color: transparent;");

    m_stack->addWidget(createInfoWidget("filter", "未选择项目", "请选择一个项目以查看其元数据"));
    m_stack->addWidget(createInfoWidget("all_data", "已选择多个项目", "请仅选择一项以查看其元数据"));

    // 详情显示区域 (MetadataDisplay 等效)
    QWidget* displayWidget = new QWidget();
    auto* displayLayout = new QVBoxLayout(displayWidget);
    displayLayout->setContentsMargins(0, 5, 0, 5);
    displayLayout->setSpacing(8);
    displayLayout->setAlignment(Qt::AlignTop);

    displayLayout->addWidget(createCapsule("创建于", "created"));
    displayLayout->addWidget(createCapsule("更新于", "updated"));
    displayLayout->addWidget(createCapsule("分类", "category"));
    displayLayout->addWidget(createCapsule("状态", "status"));
    displayLayout->addWidget(createCapsule("星级", "rating"));
    displayLayout->addWidget(createCapsule("标签", "tags_display"));

    m_stack->addWidget(displayWidget);
    layout->addWidget(m_stack);

    // 3. 标题输入框 (CapsuleInput)
    m_titleEdit = new ClickableLineEdit();
    m_titleEdit->setPlaceholderText("标题");
    m_titleEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: rgba(255, 255, 255, 0.05);"
        "    border: 1px solid rgba(255, 255, 255, 0.1);"
        "    border-radius: 10px;"
        "    color: #EEE;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    padding: 8px 12px;"
        "    margin-top: 10px;"
        "    outline: none;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #4a90e2;"
        "    background-color: rgba(255, 255, 255, 0.08);"
        "}"
    );
    connect(m_titleEdit, &QLineEdit::editingFinished, [this](){
        if(m_currentNoteId != -1) {
            DatabaseManager::instance().updateNoteState(m_currentNoteId, "title", m_titleEdit->text());
            emit noteUpdated();
        }
    });
    layout->addWidget(m_titleEdit);

    layout->addStretch(1);

    // 4. 分割线
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet("background-color: #505050; border: none; max-height: 1px; margin-bottom: 5px;");
    layout->addWidget(line);

    // 5. 标签输入框 (CapsuleTagInput)
    m_tagEdit = new ClickableLineEdit();
    m_tagEdit->setPlaceholderText("输入标签添加... (双击更多)");
    m_tagEdit->setStyleSheet(
        "QLineEdit {"
        "    background-color: rgba(255, 255, 255, 0.05);"
        "    border: 1px solid rgba(255, 255, 255, 0.1);"
        "    border-radius: 10px;"
        "    padding: 8px 12px;"
        "    font-size: 12px;"
        "    color: #eee;"
        "    outline: none;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #4a90e2;"
        "    background-color: rgba(255, 255, 255, 0.08);"
        "}"
    );
    connect(m_tagEdit, &QLineEdit::editingFinished, [this](){
        if(m_currentNoteId != -1) {
            DatabaseManager::instance().updateNoteState(m_currentNoteId, "tags", m_tagEdit->text());
            emit noteUpdated();
        }
    });
    connect(m_tagEdit, &ClickableLineEdit::doubleClicked, [this](){
        if (m_currentNoteId != -1) {
            QStringList currentTags = m_tagEdit->text().split(",", Qt::SkipEmptyParts);
            for (QString& t : currentTags) t = t.trimmed();

            auto* selector = new AdvancedTagSelector(this);
            selector->setup(DatabaseManager::instance().getAllTags(), currentTags);
            connect(selector, &AdvancedTagSelector::tagsConfirmed, [this](const QStringList& tags){
                m_tagEdit->setText(tags.join(", "));
                if(m_currentNoteId != -1) {
                    DatabaseManager::instance().updateNoteState(m_currentNoteId, "tags", m_tagEdit->text());
                    emit noteUpdated();
                }
            });
            selector->showAtCursor();
        }
    });
    layout->addWidget(m_tagEdit);

    // 默认显示无选择
    m_stack->setCurrentIndex(0);
}

QWidget* MetadataPanel::createInfoWidget(const QString& icon, const QString& title, const QString& subtitle) {
    QWidget* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 40, 0, 0);
    layout->setSpacing(15);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* iconLabel = new QLabel();
    iconLabel->setPixmap(IconHelper::getIcon(icon, "#555", 64).pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("background: transparent; border: none;");
    layout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel(title);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #e0e0e0; background: transparent; border: none;");
    layout->addWidget(titleLabel);

    QLabel* subLabel = new QLabel(subtitle);
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setStyleSheet("font-size: 12px; color: #888; background: transparent; border: none;");
    subLabel->setWordWrap(true);
    layout->addWidget(subLabel);

    layout->addStretch();
    return w;
}

QWidget* MetadataPanel::createCapsule(const QString& label, const QString& key) {
    QWidget* row = new QWidget();
    row->setObjectName("CapsuleRow");
    // 对齐 Python 版 CapsuleRow 样式
    row->setStyleSheet(
        "QWidget#CapsuleRow {"
        "    background-color: rgba(255, 255, 255, 0.05);"
        "    border: 1px solid rgba(255, 255, 255, 0.1);"
        "    border-radius: 10px;"
        "}"
    );

    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);
    
    QLabel* lblLabel = new QLabel(label);
    lblLabel->setStyleSheet("font-size: 11px; color: #AAA; border: none; min-width: 45px; background: transparent;");
    
    QLabel* lblValue = new QLabel("-");
    lblValue->setWordWrap(true);
    lblValue->setStyleSheet("font-size: 12px; color: #FFF; border: none; font-weight: bold; background: transparent;");
    
    layout->addWidget(lblLabel);
    layout->addWidget(lblValue);
    
    m_capsules[key] = lblValue;
    return row;
}

void MetadataPanel::setNote(const QVariantMap& note) {
    if(note.isEmpty()) {
        clearSelection();
        return;
    }
    m_stack->setCurrentIndex(2); // 详情页
    m_currentNoteId = note["id"].toInt();
    m_titleEdit->setText(note["title"].toString());
    m_tagEdit->setText(note["tags"].toString());
    m_capsules["tags_display"]->setText(note["tags"].toString().isEmpty() ? "-" : note["tags"].toString());
    
    // 格式化时间
    auto formatTime = [](const QVariant& v) {
        QString s = v.toString();
        if (s.contains("T")) return s.left(16).replace("T", " ");
        return s.left(16);
    };

    m_capsules["created"]->setText(formatTime(note["created_at"]));
    m_capsules["updated"]->setText(formatTime(note["updated_at"]));
    
    int rating = note["rating"].toInt();
    QString stars = QString("★").repeated(rating) + QString("☆").repeated(5 - rating);
    m_capsules["rating"]->setText(stars);
    
    QStringList status;
    if (note["is_pinned"].toInt() > 0) status << "置顶";
    if (note["is_locked"].toInt() > 0) status << "锁定";
    if (note["is_favorite"].toInt() > 0) status << "书签";
    m_capsules["status"]->setText(status.isEmpty() ? "常规" : status.join(", "));

    // 分类
    int catId = note["category_id"].toInt();
    if (catId > 0) {
        auto categories = DatabaseManager::instance().getAllCategories();
        for (const auto& cat : categories) {
            if (cat["id"].toInt() == catId) {
                m_capsules["category"]->setText(cat["name"].toString());
                break;
            }
        }
    } else {
        m_capsules["category"]->setText("未分类");
    }
}

void MetadataPanel::setMultipleNotes(int count) {
    m_stack->setCurrentIndex(1); // 多选页
    m_currentNoteId = -1;
    m_titleEdit->clear();
    m_tagEdit->clear();
}

void MetadataPanel::clearSelection() {
    m_stack->setCurrentIndex(0); // 无选择页
    m_currentNoteId = -1;
    m_titleEdit->clear();
    m_tagEdit->clear();
}
