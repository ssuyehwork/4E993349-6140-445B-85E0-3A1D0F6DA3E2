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
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(10);

    // 1. 锁图标 (缩小至 48x48)
    auto* lockIcon = new QLabel();
    lockIcon->setPixmap(IconHelper::getIcon("lock", "#666666").pixmap(48, 48));
    lockIcon->setAlignment(Qt::AlignCenter);
    layout->addWidget(lockIcon);

    // 2. 提示文字 (缩小至 14px)
    auto* titleLabel = new QLabel("输入密码查看内容");
    titleLabel->setStyleSheet("color: #cccccc; font-size: 14px; font-weight: bold; background: transparent;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // 3. 密码提示
    m_hintLabel = new QLabel("密码提示: ");
    m_hintLabel->setStyleSheet("color: #666666; font-size: 12px; background: transparent;");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_hintLabel);

    layout->addSpacing(5);

    // 4. 密码输入框 (缩小至 220px)
    m_pwdEdit = new QLineEdit();
    m_pwdEdit->setPlaceholderText("输入密码");
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setFixedWidth(220);
    m_pwdEdit->setFixedHeight(32);
    m_pwdEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #121212; border: 1px solid #333; border-radius: 4px;"
        "  padding: 0 10px; color: white; font-size: 13px;"
        "}"
        "QLineEdit:focus { border: 1px solid #3a90ff; }"
    );
    connect(m_pwdEdit, &QLineEdit::returnPressed, this, &CategoryLockWidget::onVerify);
    layout->addWidget(m_pwdEdit, 0, Qt::AlignHCenter);

    mainLayout->addWidget(container);

    // 整体背景与边框 (改用轻量样式，避免重度阴影产生的断崖感)
    setStyleSheet("CategoryLockWidget { background-color: #1e1e1e; }");
    setAttribute(Qt::WA_StyledBackground);
}

void CategoryLockWidget::setCategory(int id, const QString& hint) {
    m_catId = id;
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
