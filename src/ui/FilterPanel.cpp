#include "FilterPanel.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QApplication>

FilterPanel::FilterPanel(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setMinimumSize(250, 350);
    resize(280, 450);

    initUI();
    setupTree();
}

void FilterPanel::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    m_container = new QWidget();
    m_container->setObjectName("FilterPanelContainer");
    m_container->setStyleSheet(
        "#FilterPanelContainer {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #333333;"
        "  border-radius: 12px;"
        "}"
    );

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 150));
    m_container->setGraphicsEffect(shadow);

    auto* containerLayout = new QVBoxLayout(m_container);
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(8);

    // Header
    m_header = new QWidget();
    m_header->setFixedHeight(32);
    m_header->setStyleSheet("background-color: #252526; border-radius: 6px;");
    m_header->setCursor(Qt::SizeAllCursor);

    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(10, 0, 10, 0);

    auto* headerIcon = new QLabel();
    headerIcon->setPixmap(IconHelper::getIcon("select", "#007ACC").pixmap(16, 16));
    headerLayout->addWidget(headerIcon);

    auto* headerTitle = new QLabel("高级筛选");
    headerTitle->setStyleSheet("color: #007ACC; font-size: 13px; font-weight: bold; background: transparent;");
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch();

    auto* closeBtn = new QPushButton();
    closeBtn->setIcon(IconHelper::getIcon("close", "#888888"));
    closeBtn->setFixedSize(24, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background-color: transparent; border: none; border-radius: 4px; }"
        "QPushButton:hover { background-color: #e74c3c; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &FilterPanel::hide);
    headerLayout->addWidget(closeBtn);

    containerLayout->addWidget(m_header);

    // Tree
    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(20);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(true);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setStyleSheet(
        "QTreeWidget {"
        "  background-color: #1e1e1e;"
        "  color: #ddd;"
        "  border: none;"
        "  font-size: 12px;"
        "}"
        "QTreeWidget::item {"
        "  height: 28px;"
        "  border-radius: 4px;"
        "  padding: 2px 5px;"
        "}"
        "QTreeWidget::item:hover { background-color: #2a2d2e; }"
        "QTreeWidget::item:selected { background-color: #37373d; color: white; }"
        "QTreeWidget::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "}"
        "QScrollBar:vertical { border: none; background: transparent; width: 6px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #444; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: #555; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
    connect(m_tree, &QTreeWidget::itemChanged, this, &FilterPanel::onItemChanged);
    containerLayout->addWidget(m_tree);

    // Bottom
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);

    m_btnReset = new QPushButton(" 重置");
    m_btnReset->setIcon(IconHelper::getIcon("refresh", "white"));
    m_btnReset->setCursor(Qt::PointingHandCursor);
    m_btnReset->setFixedWidth(80);
    m_btnReset->setStyleSheet(
        "QPushButton {"
        "  background-color: #252526;"
        "  border: 1px solid #444;"
        "  color: #888;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover { color: #ddd; background-color: #333; }"
    );
    connect(m_btnReset, &QPushButton::clicked, this, &FilterPanel::resetFilters);
    bottomLayout->addWidget(m_btnReset);
    bottomLayout->addStretch();

    containerLayout->addLayout(bottomLayout);

    // Resize handle
    m_resizeHandle = new QLabel(m_container);
    m_resizeHandle->setPixmap(IconHelper::getIcon("grip_diagonal", "#666666").pixmap(20, 20));
    m_resizeHandle->setFixedSize(20, 20);
    m_resizeHandle->setCursor(Qt::SizeFDiagCursor);
    m_resizeHandle->setStyleSheet("background: transparent; border: none;");
    m_resizeHandle->raise();

    mainLayout->addWidget(m_container);
}

void FilterPanel::setupTree() {
    struct Section {
        QString key;
        QString label;
        QString icon;
        QString color;
    };

    QList<Section> sections = {
        {"stars", "评级", "star_filled", "#f39c12"},
        {"colors", "颜色", "palette", "#e91e63"},
        {"types", "类型", "folder", "#3498db"},
        {"date_create", "创建时间", "calendar", "#2ecc71"},
        {"tags", "标签", "tag", "#e67e22"}
    };

    QFont headerFont = m_tree->font();
    headerFont.setBold(true);

    for (const auto& sec : sections) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, sec.label);
        item->setIcon(0, IconHelper::getIcon(sec.icon, sec.color));
        item->setExpanded(true);
        item->setFlags(Qt::ItemIsEnabled);
        item->setFont(0, headerFont);
        item->setForeground(0, QBrush(Qt::gray));
        m_roots[sec.key] = item;
    }

    addFixedDateOptions("date_create");
}

void FilterPanel::addFixedDateOptions(const QString& key) {
    if (!m_roots.contains(key)) return;
    auto* root = m_roots[key];

    struct Option {
        QString id;
        QString label;
        QString icon;
    };

    QList<Option> options = {
        {"today", "今日", "today"},
        {"yesterday", "昨日", "clock"},
        {"this_week", "本周", "calendar"},
        {"this_month", "本月", "calendar"}
    };

    m_blockItemClick = true;
    for (const auto& opt : options) {
        auto* item = new QTreeWidgetItem(root);
        item->setText(0, opt.label);
        item->setData(0, Qt::UserRole, opt.id);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setCheckState(0, Qt::Unchecked);
        item->setIcon(0, IconHelper::getIcon(opt.icon, "#888"));
    }
    m_blockItemClick = false;
}

void FilterPanel::updateStats(const QString& keyword, const QString& type, const QVariant& value) {
    m_blockItemClick = true;

    // 清除旧的子节点（除了预定义的日期选项）
    for (auto it = m_roots.begin(); it != m_roots.end(); ++it) {
        if (it.key() == "date_create") {
            // 只保留前 4 个固定项
            while (it.value()->childCount() > 4) {
                delete it.value()->takeChild(4);
            }
        } else {
            qDeleteAll(it.value()->takeChildren());
        }
    }

    QVariantMap stats = DatabaseManager::instance().getFilterStats(keyword, type, value);

    auto addStatsToRoot = [&](const QString& key, const QString& iconName, const QString& iconColor) {
        if (!m_roots.contains(key)) return;
        auto* root = m_roots[key];
        QVariantMap data = stats[key].toMap();
        for (auto it = data.begin(); it != data.end(); ++it) {
            QString label = QString("%1 (%2)").arg(it.key()).arg(it.value().toInt());
            auto* item = new QTreeWidgetItem(root);
            item->setText(0, label);
            item->setData(0, Qt::UserRole, it.key());
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setCheckState(0, Qt::Unchecked);
            if (!iconName.isEmpty()) {
                item->setIcon(0, IconHelper::getIcon(iconName, iconColor));
            }
        }
    };

    // 特殊处理星级统计
    if (m_roots.contains("stars")) {
        auto* root = m_roots["stars"];
        QVariantMap starData = stats["stars"].toMap();
        for (int i = 5; i >= 1; --i) {
            int count = starData[QString::number(i)].toInt();
            QString label = QString(i, QChar(0x2605)); // Unicode star
            auto* item = new QTreeWidgetItem(root);
            item->setText(0, QString("%1 (%2)").arg(label).arg(count));
            item->setData(0, Qt::UserRole, QString::number(i));
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setCheckState(0, Qt::Unchecked);
            item->setIcon(0, IconHelper::getIcon("star_filled", "#f39c12"));
        }
    }

    addStatsToRoot("colors", "circle_filled", "");
    addStatsToRoot("types", "folder", "#3498db");
    addStatsToRoot("tags", "tag", "#e67e22");

    // 更新日期统计标签
    if (m_roots.contains("date_create")) {
        auto* root = m_roots["date_create"];
        QVariantMap dateData = stats["date_create"].toMap();
        for (int i = 0; i < root->childCount(); ++i) {
            auto* child = root->child(i);
            QString key = child->data(0, Qt::UserRole).toString();
            QString dbKey = key.replace("this_", "");
            int count = dateData[dbKey].toInt();
            QString label = child->text(0).split(" (")[0];
            child->setText(0, QString("%1 (%2)").arg(label).arg(count));
        }
    }

    // 特殊处理颜色图标
    auto* colorRoot = m_roots["colors"];
    for (int i = 0; i < colorRoot->childCount(); ++i) {
        auto* item = colorRoot->child(i);
        QString colorCode = item->data(0, Qt::UserRole).toString();
        item->setIcon(0, IconHelper::getIcon("circle_filled", colorCode));
    }

    m_blockItemClick = false;
}

QVariantMap FilterPanel::getCheckedCriteria() const {
    QVariantMap criteria;
    for (auto it = m_roots.begin(); it != m_roots.end(); ++it) {
        QStringList checked;
        for (int i = 0; i < it.value()->childCount(); ++i) {
            auto* item = it.value()->child(i);
            if (item->checkState(0) == Qt::Checked) {
                checked << item->data(0, Qt::UserRole).toString();
            }
        }
        if (!checked.isEmpty()) {
            criteria[it.key()] = checked;
        }
    }
    return criteria;
}

void FilterPanel::resetFilters() {
    m_blockItemClick = true;
    for (auto* root : m_roots) {
        for (int i = 0; i < root->childCount(); ++i) {
            root->child(i)->setCheckState(0, Qt::Unchecked);
        }
    }
    m_blockItemClick = false;
    emit criteriaChanged();
}

void FilterPanel::onItemChanged(QTreeWidgetItem* item, int column) {
    if (m_blockItemClick) return;
    emit criteriaChanged();
}

void FilterPanel::mousePressEvent(QMouseEvent* event) {
    QPoint pos = event->position().toPoint();
    QPoint posInContainer = m_container->mapFromParent(pos);

    if (m_header->geometry().contains(posInContainer)) {
        m_isDragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    } else if (m_resizeHandle->geometry().contains(posInContainer)) {
        m_isResizing = true;
        m_resizeStartGeometry = geometry();
        event->accept();
    }
}

void FilterPanel::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        move(event->globalPosition().toPoint() - m_dragStartPos);
        event->accept();
    } else if (m_isResizing) {
        QPoint globalPos = event->globalPosition().toPoint();
        int newW = qMax(minimumWidth(), m_resizeStartGeometry.width() + (globalPos.x() - (m_resizeStartGeometry.x() + m_resizeStartGeometry.width())));
        int newH = qMax(minimumHeight(), m_resizeStartGeometry.height() + (globalPos.y() - (m_resizeStartGeometry.y() + m_resizeStartGeometry.height())));
        resize(newW, newH);
        event->accept();
    }
}

void FilterPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_resizeHandle) {
        m_resizeHandle->move(m_container->width() - 20, m_container->height() - 20);
    }
}

void FilterPanel::mouseReleaseEvent(QMouseEvent* event) {
    m_isDragging = false;
    m_isResizing = false;
}
