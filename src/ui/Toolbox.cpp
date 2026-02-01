#include "Toolbox.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

Toolbox::Toolbox(QWidget* parent) : FramelessDialog("工具箱 (功能已更新)", parent) {
    setObjectName("ToolboxLauncher");
    setFixedSize(300, 540); // 增加高度以容纳新按钮

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

    // 关键字搜索工具
    auto* btnSearchKeyword = createToolButton("查找关键字", "edit", "#3498db");
    connect(btnSearchKeyword, &QPushButton::clicked, this, &Toolbox::showKeywordSearchRequested);
    contentLayout->addWidget(btnSearchKeyword);

    auto* btnPicker = createToolButton("吸取颜色", "screen_picker", "#ff6b81");
    connect(btnPicker, &QPushButton::clicked, this, &Toolbox::showColorPickerRequested);
    contentLayout->addWidget(btnPicker);

    contentLayout->addStretch();
}

QPushButton* Toolbox::createToolButton(const QString& text, const QString& iconName, const QString& color) {
    auto* btn = new QPushButton(text);
    btn->setIcon(IconHelper::getIcon(iconName, color));
    btn->setIconSize(QSize(20, 20));
    btn->setFixedHeight(44);
    
    // 使用 qss 直接控制对齐和边距，避免子控件导致的背景分割
    btn->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: #2D2D2D;"
        "  border: 1px solid #333;"
        "  border-radius: 8px;"
        "  color: #E0E0E0;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  text-align: left;"
        "  padding-left: 15px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #3D3D3D;"
        "  border-color: #444;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #252525;"
        "}"
    ));
    
    return btn;
}
