#include "MetadataPanel.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QPushButton>

MetadataPanel::MetadataPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(250);
    setStyleSheet("background-color: #252526; border-left: 1px solid #333;");

    QVBoxLayout* layout = new QVBoxLayout(this);

    QWidget* headerArea = new QWidget();
    QHBoxLayout* headerLayout = new QHBoxLayout(headerArea);
    headerLayout->setContentsMargins(0,0,0,0);
    QLabel* headerIcon = new QLabel();
    headerIcon->setPixmap(IconHelper::getIcon("all_data", "#4FACFE", 18).pixmap(18, 18));
    headerLayout->addWidget(headerIcon);
    QLabel* header = new QLabel("元数据");
    header->setStyleSheet("font-weight: bold; font-size: 14px; color: #4FACFE;");
    headerLayout->addWidget(header);
    headerLayout->addStretch();
    layout->addWidget(headerArea);

    m_titleEdit = new QLineEdit();
    m_titleEdit->setPlaceholderText("标题");
    connect(m_titleEdit, &QLineEdit::editingFinished, [this](){
        if(m_currentNoteId != -1) {
            DatabaseManager::instance().updateNoteState(m_currentNoteId, "title", m_titleEdit->text());
            emit noteUpdated();
        }
    });
    layout->addWidget(new QLabel("标题:"));
    layout->addWidget(m_titleEdit);

    m_tagEdit = new QLineEdit();
    m_tagEdit->setPlaceholderText("标签 (逗号分隔)");
    connect(m_tagEdit, &QLineEdit::editingFinished, [this](){
        if(m_currentNoteId != -1) {
            DatabaseManager::instance().updateNoteState(m_currentNoteId, "tags", m_tagEdit->text());
            emit noteUpdated();
        }
    });
    layout->addWidget(new QLabel("标签:"));
    layout->addWidget(m_tagEdit);

    m_createdLabel = new QLabel("创建于: -");
    m_updatedLabel = new QLabel("更新于: -");
    m_ratingLabel = new QLabel("星级: -");
    layout->addWidget(m_createdLabel);
    layout->addWidget(m_updatedLabel);
    layout->addWidget(m_ratingLabel);

    layout->addStretch();
}

void MetadataPanel::setNote(const QVariantMap& note) {
    if(note.isEmpty()) {
        m_currentNoteId = -1;
        m_titleEdit->clear();
        m_tagEdit->clear();
        m_createdLabel->setText("创建于: -");
        m_updatedLabel->setText("更新于: -");
        return;
    }
    m_currentNoteId = note["id"].toInt();
    m_titleEdit->setText(note["title"].toString());
    m_tagEdit->setText(note["tags"].toString());
    m_createdLabel->setText("创建于: " + note["created_at"].toString());
    m_updatedLabel->setText("更新于: " + note["updated_at"].toString());
    m_ratingLabel->setText(QString("星级: %1").arg(note["rating"].toInt()));
}
