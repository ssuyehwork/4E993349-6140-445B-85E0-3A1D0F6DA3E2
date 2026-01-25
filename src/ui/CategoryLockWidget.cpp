#include "CategoryLockWidget.h"
#include "IconHelper.h"
#include "../core/DatabaseManager.h"
#include <QGraphicsDropShadowEffect>

CategoryLockWidget::CategoryLockWidget(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setAlignment(Qt::AlignCenter);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(8);

    // 1. 锁图标 (1:1 还原图 3，增大至 100x100)
    auto* lockIcon = new QLabel();
    lockIcon->setPixmap(IconHelper::getIcon("lock", "#444444").pixmap(100, 100));
    lockIcon->setAlignment(Qt::AlignCenter);
    layout->addWidget(lockIcon);

    layout->addSpacing(15);

    // 2. 动态标题 (1:1 还原图 3: "<Name> 已锁定")
    m_titleLabel = new QLabel("已锁定");
    m_titleLabel->setStyleSheet("color: #FFFFFF; font-size: 18px; font-weight: bold; background: transparent;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_titleLabel);

    // 3. 密码提示 (1:1 还原图 3)
    m_hintLabel = new QLabel("密码提示: ");
    m_hintLabel->setStyleSheet("color: #888888; font-size: 13px; background: transparent;");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_hintLabel);

    layout->addSpacing(2);

    // 4. 密码输入框 (1:1 还原图 3 比例)
    m_pwdEdit = new QLineEdit();
    m_pwdEdit->setPlaceholderText("输入密码");
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setFixedWidth(220);
    m_pwdEdit->setFixedHeight(32);
    m_pwdEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #121212; border: 1px solid #333; border-radius: 6px;"
        "  padding: 0 10px; color: white; font-size: 13px;"
        "}"
        "QLineEdit:focus { border: 1px solid #3a90ff; background-color: #0a0a0a; }"
    );
    connect(m_pwdEdit, &QLineEdit::returnPressed, this, &CategoryLockWidget::onVerify);
    layout->addWidget(m_pwdEdit, 0, Qt::AlignHCenter);

    mainLayout->addWidget(container);

    // 移除强制背景色，使其自然融合到父容器 (#1e1e1e) 中
    setStyleSheet("background: transparent;");
}

void CategoryLockWidget::setCategory(int id, const QString& name, const QString& hint) {
    if (m_catId == id && isVisible()) return; // 关键修复：防止因数据刷新导致的输入框重置

    m_catId = id;
    m_titleLabel->setText(QString("%1 已锁定").arg(name));
    m_hintLabel->setText(QString("密码提示: %1").arg(hint.isEmpty() ? "无" : hint));
    m_pwdEdit->clear();
    m_pwdEdit->setFocus();
}

void CategoryLockWidget::clearInput() {
    m_pwdEdit->clear();
    m_pwdEdit->setFocus();
}

void CategoryLockWidget::onVerify() {
    if (m_catId == -1) return;

    if (DatabaseManager::instance().verifyCategoryPassword(m_catId, m_pwdEdit->text())) {
        emit unlocked(m_catId);
    } else {
        m_pwdEdit->setStyleSheet(m_pwdEdit->styleSheet() + "border: 1px solid #e74c3c;");
        m_pwdEdit->selectAll();
    }
}
