#include "DatabaseLockWidget.h"
#include "IconHelper.h"
#include <QPushButton>

DatabaseLockWidget::DatabaseLockWidget(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setAlignment(Qt::AlignCenter);

    // 半透明背景遮罩效果
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(30, 30, 30));
    setPalette(pal);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(15);

    // 1. 大锁图标
    auto* lockIcon = new QLabel();
    lockIcon->setPixmap(IconHelper::getIcon("lock", "#4a90e2").pixmap(80, 80));
    lockIcon->setAlignment(Qt::AlignCenter);
    layout->addWidget(lockIcon);

    layout->addSpacing(10);

    // 2. 标题
    auto* titleLabel = new QLabel("灵感库已安全锁定");
    titleLabel->setStyleSheet("color: white; font-size: 20px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // 3. 提示
    auto* tipLabel = new QLabel("请输入主密码以解密并访问您的灵感数据");
    tipLabel->setStyleSheet("color: #888; font-size: 13px;");
    tipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(tipLabel);

    layout->addSpacing(10);

    // 4. 密码框
    m_pwdEdit = new QLineEdit();
    m_pwdEdit->setPlaceholderText("主密码");
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setFixedWidth(240);
    m_pwdEdit->setFixedHeight(36);
    m_pwdEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #252526; border: 1px solid #444; border-radius: 6px;"
        "  padding: 0 12px; color: white; font-size: 14px;"
        "}"
        "QLineEdit:focus { border: 1px solid #4a90ff; }"
    );
    connect(m_pwdEdit, &QLineEdit::returnPressed, this, &DatabaseLockWidget::onVerify);
    layout->addWidget(m_pwdEdit, 0, Qt::AlignHCenter);

    m_errorLabel = new QLabel("");
    m_errorLabel->setStyleSheet("color: #e74c3c; font-size: 12px;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_errorLabel);

    layout->addSpacing(10);

    // 5. 按钮
    auto* btnConfirm = new QPushButton("立即解锁");
    btnConfirm->setFixedWidth(240);
    btnConfirm->setFixedHeight(36);
    btnConfirm->setCursor(Qt::PointingHandCursor);
    btnConfirm->setStyleSheet(
        "QPushButton { background-color: #007acc; color: white; border: none; border-radius: 6px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background-color: #0062a3; }"
    );
    connect(btnConfirm, &QPushButton::clicked, this, &DatabaseLockWidget::onVerify);
    layout->addWidget(btnConfirm, 0, Qt::AlignHCenter);

    auto* btnQuit = new QPushButton("退出应用");
    btnQuit->setFlat(true);
    btnQuit->setStyleSheet("QPushButton { color: #666; font-size: 12px; } QPushButton:hover { color: #888; }");
    connect(btnQuit, &QPushButton::clicked, this, &DatabaseLockWidget::quitRequested);
    layout->addWidget(btnQuit, 0, Qt::AlignHCenter);

    mainLayout->addWidget(container);
}

void DatabaseLockWidget::showError(const QString& msg) {
    m_errorLabel->setText(msg);
    m_pwdEdit->setStyleSheet(m_pwdEdit->styleSheet() + "border: 1px solid #e74c3c;");
    m_pwdEdit->selectAll();
    m_pwdEdit->setFocus();
}

void DatabaseLockWidget::onVerify() {
    QString pwd = m_pwdEdit->text();
    if (pwd.isEmpty()) return;

    m_errorLabel->setText("");
    emit unlocked(pwd);
}
