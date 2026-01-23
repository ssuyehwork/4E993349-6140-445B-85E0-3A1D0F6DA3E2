#include "MetadataPanel.h"
#include "AdvancedTagSelector.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFrame>

MetadataPanel::MetadataPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(240);
    setStyleSheet("background-color: #252526; border-left: 1px solid #333;");
    initUI();
}

void MetadataPanel::initUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 顶部标题
    QWidget* headerArea = new QWidget();
    headerArea->setFixedHeight(40);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerArea);
    headerLayout->setContentsMargins(15, 0, 15, 0);
    QLabel* headerIcon = new QLabel();
    headerIcon->setPixmap(IconHelper::getIcon("all_data", "#4a90e2", 18).pixmap(18, 18));
    headerLayout->addWidget(headerIcon);
    QLabel* header = new QLabel("元数据");
    header->setStyleSheet("font-weight: bold; font-size: 14px; color: #4a90e2; background: transparent;");
    headerLayout->addWidget(header);
    headerLayout->addStretch();
    layout->addWidget(headerArea);

    m_stack = new QStackedWidget(this);

    // 1. 无选择项
    m_stack->addWidget(createInfoWidget("filter", "未选择项目", "请选择一个项目以查看其元数据"));

    // 2. 多项选择
    m_stack->addWidget(createInfoWidget("all_data", "已选择多个项目", "请仅选择一项以查看其元数据"));

    // 3. 详情展示
    m_stack->addWidget(createMetadataDisplay());

    layout->addWidget(m_stack);

    // 默认显示无选择
    m_stack->setCurrentIndex(0);
}

QWidget* MetadataPanel::createInfoWidget(const QString& icon, const QString& title, const QString& subtitle) {
    QWidget* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(20, 40, 20, 20);
    layout->setSpacing(15);
    layout->setAlignment(Qt::AlignCenter);

    QLabel* iconLabel = new QLabel();
    iconLabel->setPixmap(IconHelper::getIcon(icon, "#555", 64).pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    QLabel* titleLabel = new QLabel(title);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #e0e0e0;");
    layout->addWidget(titleLabel);

    QLabel* subLabel = new QLabel(subtitle);
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setStyleSheet("font-size: 12px; color: #888;");
    subLabel->setWordWrap(true);
    layout->addWidget(subLabel);

    layout->addStretch();
    return w;
}

QWidget* MetadataPanel::createMetadataDisplay() {
    QWidget* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(15, 5, 15, 15);
    layout->setSpacing(10);

    // 标题输入
    m_titleEdit = new ClickableLineEdit();
    m_titleEdit->setPlaceholderText("标题");
    m_titleEdit->setStyleSheet("background: rgba(255,255,255,0.05); border: 1px solid #444; border-radius: 8px; color: white; padding: 8px; font-weight: bold;");
    connect(m_titleEdit, &QLineEdit::editingFinished, [this](){
        if(m_currentNoteId != -1) {
            DatabaseManager::instance().updateNoteState(m_currentNoteId, "title", m_titleEdit->text());
            emit noteUpdated();
        }
    });
    layout->addWidget(m_titleEdit);

    // 胶囊展示区
    layout->addWidget(createCapsule("创建于", "created"));
    layout->addWidget(createCapsule("更新于", "updated"));
    layout->addWidget(createCapsule("分类", "category"));
    layout->addWidget(createCapsule("星级", "rating"));
    layout->addWidget(createCapsule("状态", "status"));

    layout->addSpacing(5);

    // 标签部分
    QLabel* tagTitle = new QLabel("标签 (双击更多):");
    tagTitle->setStyleSheet("color: #888; font-size: 11px; font-weight: bold;");
    layout->addWidget(tagTitle);

    m_tagEdit = new ClickableLineEdit();
    m_tagEdit->setPlaceholderText("输入标签...");
    m_tagEdit->setStyleSheet("background: rgba(255,255,255,0.05); border: 1px solid #444; border-radius: 8px; color: #eee; padding: 8px; font-size: 12px;");
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

    // 链接部分
    QLabel* linkTitle = new QLabel("链接:");
    linkTitle->setStyleSheet("color: #888; font-size: 11px; font-weight: bold; margin-top: 5px;");
    layout->addWidget(linkTitle);

    m_linkEdit = new QLineEdit();
    m_linkEdit->setPlaceholderText("链接地址");
    m_linkEdit->setStyleSheet("background: rgba(255,255,255,0.05); border: 1px solid #444; border-radius: 8px; color: #4a90e2; padding: 8px; font-size: 12px;");
    m_linkEdit->setReadOnly(true);
    layout->addWidget(m_linkEdit);

    layout->addStretch();
    return w;
}

QWidget* MetadataPanel::createCapsule(const QString& label, const QString& key) {
    QWidget* row = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(0);
    
    row->setStyleSheet("QWidget { background: rgba(255,255,255,0.04); border: 1px solid #3a3a3a; border-radius: 10px; } QWidget:hover { background: rgba(255,255,255,0.07); border-color: #4a90e2; }");
    
    QLabel* lblLabel = new QLabel(label);
    lblLabel->setStyleSheet("color: #999; font-size: 11px; font-weight: bold; border: none; background: transparent;");
    
    QLabel* lblValue = new QLabel("-");
    lblValue->setStyleSheet("color: #eee; font-size: 12px; font-weight: bold; border: none; background: transparent;");
    lblValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    layout->addWidget(lblLabel);
    layout->addStretch();
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
    
    m_capsules["created"]->setText(note["created_at"].toString().left(16).replace("T", " "));
    m_capsules["updated"]->setText(note["updated_at"].toString().left(16).replace("T", " "));
    
    int rating = note["rating"].toInt();
    QString stars = QString("★").repeated(rating) + QString("☆").repeated(5 - rating);
    m_capsules["rating"]->setText(stars);
    
    QStringList status;
    if (note["is_pinned"].toInt() > 0) status << "置顶";
    if (note["is_favorite"].toInt() > 0) status << "收藏";
    if (note["is_locked"].toInt() > 0) status << "锁定";
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

    // 链接
    QString content = note["content"].toString();
    if (content.startsWith("http://") || content.startsWith("https://") || content.startsWith("www.")) {
        m_linkEdit->setText(content);
    } else {
        m_linkEdit->setText("-");
    }
}

void MetadataPanel::setMultipleNotes(int count) {
    m_stack->setCurrentIndex(1); // 多选页
    m_currentNoteId = -1;
}

void MetadataPanel::clearSelection() {
    m_stack->setCurrentIndex(0); // 无选择页
    m_currentNoteId = -1;
}
