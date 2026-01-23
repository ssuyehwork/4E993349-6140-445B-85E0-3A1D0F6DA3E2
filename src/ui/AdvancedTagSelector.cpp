#include "AdvancedTagSelector.h"
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>

AdvancedTagSelector::AdvancedTagSelector(QWidget* parent) : QWidget(parent, Qt::Popup) {
    setFixedWidth(240);
    setFixedHeight(300);
    setStyleSheet("QWidget { background: #2D2D2D; color: #CCC; border: 1px solid #444; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    m_search = new QLineEdit();
    m_search->setPlaceholderText("搜索或添加标签...");
    m_search->setStyleSheet("QLineEdit { background: #1E1E1E; border: 1px solid #333; color: #EEE; padding: 4px; border-radius: 4px; }");
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

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");

    m_tagContainer = new QWidget();
    m_flow = new FlowLayout(m_tagContainer, 0, 5, 5);
    scroll->setWidget(m_tagContainer);
    layout->addWidget(scroll);
}

void AdvancedTagSelector::setup(const QStringList& allTags, const QStringList& selectedTags) {
    setTags(allTags, selectedTags);
}

void AdvancedTagSelector::setTags(const QStringList& allTags, const QStringList& selectedTags) {
    m_all = allTags;
    m_selected = selectedTags;
    updateList();
}

void AdvancedTagSelector::updateList() {
    QLayoutItem* child;
    while ((child = m_flow->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    QString filter = m_search->text().toLower();

    for (const QString& tag : m_selected) {
        if (!filter.isEmpty() && !tag.toLower().contains(filter)) continue;
        auto* btn = new QPushButton(tag);
        btn->setCheckable(true);
        btn->setChecked(true);
        btn->setStyleSheet(
            "QPushButton { background: #4a90e2; color: white; border: none; padding: 3px 8px; border-radius: 10px; font-size: 11px; }"
        );
        connect(btn, &QPushButton::clicked, this, [this, tag](){ toggleTag(tag); });
        m_flow->addWidget(btn);
    }

    for (const QString& tag : m_all) {
        if (m_selected.contains(tag)) continue;
        if (!filter.isEmpty() && !tag.toLower().contains(filter)) continue;
        auto* btn = new QPushButton(tag);
        btn->setStyleSheet(
            "QPushButton { background: #333; color: #AAA; border: none; padding: 3px 8px; border-radius: 10px; font-size: 11px; }"
        );
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
}

void AdvancedTagSelector::showAtCursor() {
    QPoint pos = QCursor::pos();
    // 确保不超出屏幕
    QScreen *screen = QGuiApplication::screenAt(pos);
    if (screen) {
        QRect geo = screen->geometry();
        if (pos.x() + width() > geo.right()) pos.setX(geo.right() - width());
        if (pos.y() + height() > geo.bottom()) pos.setY(geo.bottom() - height());
    }
    move(pos);
    show();
    m_search->setFocus();
}

void AdvancedTagSelector::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}
