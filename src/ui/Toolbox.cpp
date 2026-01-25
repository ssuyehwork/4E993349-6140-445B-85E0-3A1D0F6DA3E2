#include "Toolbox.h"
#include "IconHelper.h"
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>

Toolbox::Toolbox(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setWindowTitle("工具箱");
    setObjectName("ToolboxLauncher");
    resize(300, 400);

    initUI();
}

Toolbox::~Toolbox() {
}

void Toolbox::initUI() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(15, 15, 15, 15);

    auto* container = new QFrame();
    container->setObjectName("ToolboxContainer");
    container->setStyleSheet("#ToolboxContainer { background-color: #2D2D2D; border-radius: 12px; border: 1px solid #444; }");
    rootLayout->addWidget(container);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 120));
    container->setGraphicsEffect(shadow);

    auto* contentLayout = new QVBoxLayout(container);
    contentLayout->setContentsMargins(10, 5, 10, 20);
    contentLayout->setSpacing(15);

    // Title Bar with Close and Pin buttons
    auto* titleHeader = new QFrame();
    titleHeader->setObjectName("ToolboxTitleBar");
    titleHeader->setFixedHeight(40);
    titleHeader->setAttribute(Qt::WA_StyledBackground);
    titleHeader->setStyleSheet("QFrame#ToolboxTitleBar { background: transparent; border: none; }");
    auto* titleLayout = new QHBoxLayout(titleHeader);
    titleLayout->setContentsMargins(10, 0, 5, 0);
    titleLayout->setSpacing(0);

    auto* titleLabel = new QLabel("工具箱");
    titleLabel->setStyleSheet("color: #888; font-size: 12px; font-weight: bold; background: transparent; border: none;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto* btnPin = new QPushButton();
    btnPin->setObjectName("btnPin");
    btnPin->setFixedSize(32, 32);
    btnPin->setIconSize(QSize(20, 20));
    btnPin->setCheckable(true);
    btnPin->setChecked(true);
    m_isStayOnTop = true;
    btnPin->setIcon(IconHelper::getIcon("pin_vertical", "#ffffff"));
    btnPin->setStyleSheet("QPushButton { border: none; background: transparent; border-radius: 5px; } "
                          "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); } "
                          "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.2); } "
                          "QPushButton:checked { background-color: #0078d4; }");
    btnPin->setToolTip("置顶");
    connect(btnPin, &QPushButton::toggled, this, &Toolbox::toggleStayOnTop);
    titleLayout->addWidget(btnPin);

    auto* btnClose = new QPushButton();
    btnClose->setFixedSize(32, 32);
    btnClose->setIconSize(QSize(20, 20));
    btnClose->setIcon(IconHelper::getIcon("close", "#888888"));
    btnClose->setStyleSheet("QPushButton { border: none; background: transparent; border-radius: 5px; } "
                            "QPushButton:hover { background-color: #e74c3c; } "
                            "QPushButton:pressed { background-color: #c0392b; }");
    btnClose->setToolTip("关闭");
    connect(btnClose, &QPushButton::clicked, this, &Toolbox::hide);
    titleLayout->addWidget(btnClose);

    contentLayout->addWidget(titleHeader);

    auto* btnTime = createToolButton("时间输出");
    connect(btnTime, &QPushButton::clicked, this, &Toolbox::showTimePasteRequested);
    contentLayout->addWidget(btnTime);

    auto* btnPwd = createToolButton("密码生成器");
    connect(btnPwd, &QPushButton::clicked, this, &Toolbox::showPasswordGeneratorRequested);
    contentLayout->addWidget(btnPwd);

    auto* btnOCR = createToolButton("文字识别 (OCR)");
    connect(btnOCR, &QPushButton::clicked, this, &Toolbox::showOCRRequested);
    contentLayout->addWidget(btnOCR);

    contentLayout->addStretch();
}

QPushButton* Toolbox::createToolButton(const QString& text) {
    auto* btn = new QPushButton(text);
    btn->setStyleSheet("QPushButton { background-color: #4A4A4A; color: #E0E0E0; border: 1px solid #666; border-radius: 5px; padding: 10px; font-size: 14px; } "
                       "QPushButton:hover { background-color: #5A5A5A; border-color: #888; } "
                       "QPushButton:pressed { background-color: #3A3A3A; }");
    return btn;
}

void Toolbox::toggleStayOnTop(bool checked) {
    m_isStayOnTop = checked;
    Qt::WindowFlags flags = windowFlags();
    if (checked) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }
    setWindowFlags(flags);

    // 更新图标
    if (auto* btnPin = findChild<QPushButton*>("btnPin")) {
        btnPin->setIcon(IconHelper::getIcon(checked ? "pin_vertical" : "pin_tilted", checked ? "#ffffff" : "#888888"));
    }

    show(); // Important to show again after changing flags
}

void Toolbox::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->pos().y() <= 60) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void Toolbox::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void Toolbox::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_dragPos = QPoint();
}
