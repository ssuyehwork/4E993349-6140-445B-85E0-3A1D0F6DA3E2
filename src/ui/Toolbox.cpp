#include "Toolbox.h"
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

Toolbox::Toolbox(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
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

    auto* container = new QWidget();
    container->setObjectName("ToolboxContainer");
    container->setStyleSheet("#ToolboxContainer { background-color: #2D2D2D; border-radius: 10px; border: 1px solid #444; }");
    rootLayout->addWidget(container);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 120));
    container->setGraphicsEffect(shadow);

    auto* contentLayout = new QVBoxLayout(container);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(15);

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

void Toolbox::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void Toolbox::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}
