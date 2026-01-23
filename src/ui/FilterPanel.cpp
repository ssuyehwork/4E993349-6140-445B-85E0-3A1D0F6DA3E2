#include "FilterPanel.h"
#include "../core/DatabaseManager.h"
#include <QScrollArea>

FilterPanel::FilterPanel(QWidget* parent) : QWidget(parent, Qt::Popup) {
    setFixedWidth(300);
    setFixedHeight(400);
    setStyleSheet("QWidget { background: #2D2D2D; color: #CCC; border: 1px solid #444; }");

    auto* layout = new QVBoxLayout(this);
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    
    auto* container = new QWidget();
    m_mainLayout = new QVBoxLayout(container);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(15);
    
    scroll->setWidget(container);
    layout->addWidget(scroll);
}

void FilterPanel::updateStats(const QString& keyword, const QString& type, const QVariant& value) {
    // 清除旧内容
    QLayoutItem* item;
    while ((item = m_mainLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    m_checkboxes.clear();

    QVariantMap stats = DatabaseManager::instance().getFilterStats(keyword, type, value.toInt());

    createSection("星级", "stars", stats["stars"].toMap());
    createSection("类型", "types", stats["types"].toMap());
    createSection("颜色", "colors", stats["colors"].toMap());
    createSection("标签", "tags", stats["tags"].toMap());
    createSection("创建时间", "date_create", stats["date_create"].toMap());
}

void FilterPanel::createSection(const QString& title, const QString& key, const QVariantMap& data) {
    if (data.isEmpty()) return;

    auto* group = new QWidget();
    auto* v = new QVBoxLayout(group);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(5);

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet("font-weight: bold; color: #888;");
    v->addWidget(lbl);

    auto* flow = new FlowLayout(0, 5, 5);
    for (auto it = data.begin(); it != data.end(); ++it) {
        QString label = QString("%1 (%2)").arg(it.key()).arg(it.value().toInt());
        auto* cb = new QCheckBox(label);
        cb->setProperty("key", it.key());
        cb->setStyleSheet("QCheckBox { font-size: 11px; }");
        connect(cb, &QCheckBox::toggled, this, &FilterPanel::criteriaChanged);
        flow->addWidget(cb);
        m_checkboxes[key].append(cb);
    }
    v->addLayout(flow);
    m_mainLayout->addWidget(group);
}

QVariantMap FilterPanel::getCheckedCriteria() const {
    QVariantMap criteria;
    for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
        QStringList checked;
        for (QCheckBox* cb : it.value()) {
            if (cb->isChecked()) checked << cb->property("key").toString();
        }
        if (!checked.isEmpty()) criteria[it.key()] = checked;
    }
    return criteria;
}
