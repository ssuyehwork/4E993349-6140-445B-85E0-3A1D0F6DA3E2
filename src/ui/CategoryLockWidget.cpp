#include "CategoryLockWidget.h"
#include "IconHelper.h"
#include "../core/DatabaseManager.h"
#include <QGraphicsDropShadowEffect>

CategoryLockWidget::CategoryLockWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(15);

    // 大锁图标
    auto* lockIcon = new QLabel();
    lockIcon->setPixmap(IconHelper::getIcon("lock", "#555555").pixmap(80, 80));
    lockIcon->setAlignment(Qt::AlignCenter);
    layout->addWidget(lockIcon);

    // 提示文字
    auto* titleLabel = new QLabel("输入密码查看内容");
    titleLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // 密码提示
    m_hintLabel = new QLabel("密码提示: ");
    m_hintLabel->setStyleSheet("color: #888; font-size: 13px;");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_hintLabel);

    // 密码输入框
    m_pwdEdit = new QLineEdit();
    m_pwdEdit->setPlaceholderText("输入密码");
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setFixedWidth(280);
    m_pwdEdit->setFixedHeight(40);
    m_pwdEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #121212; border: 1px solid #333; border-radius: 6px;"
        "  padding: 0 12px; color: white; font-size: 14px;"
        "}"
        "QLineEdit:focus { border: 1px solid #3a90ff; }"
    );
    connect(m_pwdEdit, &QLineEdit::returnPressed, this, &CategoryLockWidget::onVerify);
    layout->addWidget(m_pwdEdit, 0, Qt::AlignHCenter);

    // 为了美观添加一点阴影
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 100));
    setGraphicsEffect(shadow);

    setStyleSheet("background: #1e1e1e;");
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
