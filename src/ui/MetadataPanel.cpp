#include "MetadataPanel.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QPushButton>

MetadataPanel::MetadataPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(240);
    setStyleSheet("background-color: #252526; border-left: 1px solid #333;");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);

    // 顶部标题
    QWidget* headerArea = new QWidget();
    QHBoxLayout* headerLayout = new QHBoxLayout(headerArea);
    headerLayout->setContentsMargins(0,0,0,0);
    QLabel* headerIcon = new QLabel();
    headerIcon->setPixmap(IconHelper::getIcon("all_data", "#4a90e2", 18).pixmap(18, 18));
    headerLayout->addWidget(headerIcon);
    QLabel* header = new QLabel("元数据");
    header->setStyleSheet("font-weight: bold; font-size: 14px; color: #4a90e2; background: transparent;");
    headerLayout->addWidget(header);
    headerLayout->addStretch();
    layout->addWidget(headerArea);

    // 标题输入
    m_titleEdit = new QLineEdit();
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
    layout->addWidget(createCapsule("星级", "rating"));
    layout->addWidget(createCapsule("状态", "status"));

    layout->addStretch();

    // 标签编辑
    m_tagEdit = new QLineEdit();
    m_tagEdit->setPlaceholderText("标签 (逗号分隔)");
    m_tagEdit->setStyleSheet("background: rgba(255,255,255,0.05); border: 1px solid #444; border-radius: 8px; color: #eee; padding: 8px; font-size: 12px;");
    connect(m_tagEdit, &QLineEdit::editingFinished, [this](){
        if(m_currentNoteId != -1) {
            DatabaseManager::instance().updateNoteState(m_currentNoteId, "tags", m_tagEdit->text());
            emit noteUpdated();
        }
    });
    layout->addWidget(new QLabel("标签:"));
    layout->addWidget(m_tagEdit);
}

QWidget* MetadataPanel::createCapsule(const QString& label, const QString& key) {
    QWidget* row = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);

    row->setStyleSheet("background: rgba(255,255,255,0.03); border: 1px solid #3a3a3a; border-radius: 8px;");

    QLabel* lblLabel = new QLabel(label);
    lblLabel->setStyleSheet("color: #888; font-size: 11px; min-width: 40px; border: none; background: transparent;");

    QLabel* lblValue = new QLabel("-");
    lblValue->setStyleSheet("color: #ddd; font-size: 12px; font-weight: bold; border: none; background: transparent;");
    lblValue->setWordWrap(true);

    layout->addWidget(lblLabel);
    layout->addWidget(lblValue);

    m_capsules[key] = lblValue;
    return row;
}

void MetadataPanel::setNote(const QVariantMap& note) {
    if(note.isEmpty()) {
        m_currentNoteId = -1;
        m_titleEdit->clear();
        m_tagEdit->clear();
        for(auto* lbl : m_capsules) lbl->setText("-");
        return;
    }
    m_currentNoteId = note["id"].toInt();
    m_titleEdit->setText(note["title"].toString());
    m_tagEdit->setText(note["tags"].toString());

    m_capsules["created"]->setText(note["created_at"].toString().left(16).replace("T", " "));
    m_capsules["updated"]->setText(note["updated_at"].toString().left(16).replace("T", " "));

    int rating = note["rating"].toInt();
    QString stars = QString("★").repeated(rating) + QString("☆").repeated(5 - rating);
    m_capsules["rating"]->setText(stars);

    QStringList status;
    if (note["is_pinned"].toBool()) status << "置顶";
    if (note["is_favorite"].toBool()) status << "收藏";
    if (note["is_locked"].toBool()) status << "锁定";
    m_capsules["status"]->setText(status.isEmpty() ? "常规" : status.join(", "));
}
