#include "Toolbox.h"
#include "IconHelper.h"
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Toolbox::Toolbox(QWidget* parent) : FramelessDialog("工具箱", parent) {
    setObjectName("ToolboxLauncher");
    setFixedSize(300, 480);

    initUI();
}

Toolbox::~Toolbox() {
}

void Toolbox::initUI() {
    auto* contentLayout = new QVBoxLayout(m_contentArea);
    contentLayout->setContentsMargins(20, 10, 20, 20);
    contentLayout->setSpacing(8);

    auto* btnTime = createToolButton("时间输出", "clock", "#1abc9c");
    connect(btnTime, &QPushButton::clicked, this, &Toolbox::showTimePasteRequested);
    contentLayout->addWidget(btnTime);

    auto* btnPwd = createToolButton("密码生成器", "lock", "#3498db");
    connect(btnPwd, &QPushButton::clicked, this, &Toolbox::showPasswordGeneratorRequested);
    contentLayout->addWidget(btnPwd);

    auto* btnOCR = createToolButton("文字识别", "text", "#4a90e2");
    connect(btnOCR, &QPushButton::clicked, this, &Toolbox::showOCRRequested);
    contentLayout->addWidget(btnOCR);

    auto* btnPath = createToolButton("路径提取", "branch", "#3a90ff");
    connect(btnPath, &QPushButton::clicked, this, &Toolbox::showPathAcquisitionRequested);
    contentLayout->addWidget(btnPath);

    auto* btnTags = createToolButton("标签管理", "tag", "#f1c40f");
    connect(btnTags, &QPushButton::clicked, this, &Toolbox::showTagManagerRequested);
    contentLayout->addWidget(btnTags);

    auto* btnFile = createToolButton("存储文件", "file_managed", "#e67e22");
    connect(btnFile, &QPushButton::clicked, this, &Toolbox::showFileStorageRequested);
    contentLayout->addWidget(btnFile);

    auto* btnSearchFile = createToolButton("查找文件", "search", "#95a5a6");
    connect(btnSearchFile, &QPushButton::clicked, this, &Toolbox::showFileSearchRequested);
    contentLayout->addWidget(btnSearchFile);

    auto* btnPicker = createToolButton("吸取颜色", "screen_picker", "#ff6b81");
    connect(btnPicker, &QPushButton::clicked, this, &Toolbox::showColorPickerRequested);
    contentLayout->addWidget(btnPicker);

    contentLayout->addStretch();
}

QPushButton* Toolbox::createToolButton(const QString& text, const QString& iconName, const QString& color) {
    auto* btn = new QPushButton();
    auto* layout = new QHBoxLayout(btn);
    layout->setContentsMargins(15, 0, 15, 0);
    layout->setSpacing(12);

    auto* iconLabel = new QLabel();
    iconLabel->setPixmap(IconHelper::getIcon(iconName, color).pixmap(20, 20));
    layout->addWidget(iconLabel);

    auto* textLabel = new QLabel(text);
    textLabel->setStyleSheet("color: #E0E0E0; font-size: 13px; font-weight: 500;");
    layout->addWidget(textLabel);
    layout->addStretch();

    btn->setFixedHeight(44);
    btn->setStyleSheet("QPushButton { background-color: #2D2D2D; border: 1px solid #333; border-radius: 8px; } "
                       "QPushButton:hover { background-color: #3D3D3D; border-color: #444; } "
                       "QPushButton:pressed { background-color: #252525; }");
    return btn;
}
