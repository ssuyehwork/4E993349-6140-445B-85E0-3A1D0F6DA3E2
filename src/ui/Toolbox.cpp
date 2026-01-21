#include "Toolbox.h"
#include "IconHelper.h"
#include "../core/KeyboardHook.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QClipboard>
#include <QApplication>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRadioButton>
#include <QButtonGroup>
#include "../core/Utils.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Toolbox::Toolbox(QWidget* parent) : QDialog(parent) {
    setWindowTitle("工具箱");
    resize(450, 450);
    setStyleSheet("QDialog { background-color: #1e1e1e; color: #ccc; } QTabWidget::pane { border: 1px solid #333; background: #252526; } QTabBar::tab { background: #2d2d2d; padding: 10px 20px; border-right: 1px solid #1e1e1e; } QTabBar::tab:selected { background: #252526; color: #4a90e2; }");

    QVBoxLayout* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    QWidget* timeTab = new QWidget();
    initTimePasteTab(timeTab);
    m_tabs->addTab(timeTab, IconHelper::getIcon("clock", "#aaaaaa"), " 时间助手");

    QWidget* pwdTab = new QWidget();
    initPasswordGenTab(pwdTab);
    m_tabs->addTab(pwdTab, IconHelper::getIcon("lock", "#aaaaaa"), " 密码生成");

    layout->addWidget(m_tabs);

    connect(&KeyboardHook::instance(), &KeyboardHook::digitPressed, this, &Toolbox::onDigitPressed);
}

Toolbox::~Toolbox() {
    KeyboardHook::instance().stop();
}

void Toolbox::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (m_tabs->currentIndex() == 0) {
        KeyboardHook::instance().start();
    }
}

void Toolbox::hideEvent(QHideEvent* event) {
    KeyboardHook::instance().stop();
    QDialog::hideEvent(event);
}

void Toolbox::initTimePasteTab(QWidget* tab) {
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    QLabel* info = new QLabel("🕒 快速格式化输出当前时间");
    info->setStyleSheet("font-weight: bold; color: #4a90e2;");
    layout->addWidget(info);

    // 模式选择
    QHBoxLayout* modeLayout = new QHBoxLayout();
    QRadioButton* rbRetreat = new QRadioButton("退 (往前 N 分钟)");
    QRadioButton* rbAdvance = new QRadioButton("进 (往后 N 分钟)");
    rbRetreat->setChecked(true);
    modeLayout->addWidget(rbRetreat);
    modeLayout->addWidget(rbAdvance);
    layout->addLayout(modeLayout);

    connect(rbRetreat, &QRadioButton::toggled, [this](bool checked){ if(checked) m_timeMode = 0; });
    connect(rbAdvance, &QRadioButton::toggled, [this](bool checked){ if(checked) m_timeMode = 1; });

    QLabel* tip = new QLabel("提示: 开启此页面后，按主键盘数字键 0-9 可直接输出偏移时间。");
    tip->setStyleSheet("color: #888; font-size: 11px;");
    tip->setWordWrap(true);
    layout->addWidget(tip);

    auto addTimeBtn = [&](const QString& text, const QString& format) {
        QPushButton* btn = new QPushButton(text);
        btn->setStyleSheet("QPushButton { background: #333; border: 1px solid #444; border-radius: 6px; padding: 10px; color: #ddd; text-align: left; } QPushButton:hover { background: #3e3e42; border-color: #4a90e2; }");
        connect(btn, &QPushButton::clicked, [format](){
            QApplication::clipboard()->setText(QDateTime::currentDateTime().toString(format));
        });
        layout->addWidget(btn);
    };

    addTimeBtn("标准格式 (2025-01-20 17:00:35)", "yyyy-MM-dd HH:mm:ss");
    addTimeBtn("短日期 (2025/01/20)", "yyyy/MM/dd");
    addTimeBtn("紧凑格式 (202501201700)", "yyyyMMddHHmm");
    addTimeBtn("仅时间 (17:00:35)", "HH:mm:ss");
    
    layout->addStretch();
}

void Toolbox::onDigitPressed(int digit) {
    // 只有在时间助手标签页处于激活状态时才响应
    if (m_tabs->currentIndex() != 0) return;

    QDateTime target = QDateTime::currentDateTime();
    if (m_timeMode == 0) // 退
        target = target.addSecs(-digit * 60);
    else // 进
        target = target.addSecs(digit * 60);
    
    QString timeStr = target.toString("HH:mm");
    QApplication::clipboard()->setText(timeStr);
    
#ifdef Q_OS_WIN
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
#endif
}

void Toolbox::initPasswordGenTab(QWidget* tab) {
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    QLabel* title = new QLabel("🔐 安全密码生成器");
    title->setStyleSheet("font-weight: bold; color: #2cc985;");
    layout->addWidget(title);

    // 结果显示
    QLineEdit* resEdit = new QLineEdit();
    resEdit->setReadOnly(true);
    resEdit->setAlignment(Qt::AlignCenter);
    resEdit->setStyleSheet("background: #1a1a1a; border: 1px solid #444; border-radius: 6px; color: #2cc985; font-size: 18px; font-family: Consolas; padding: 10px;");
    layout->addWidget(resEdit);

    // 强度条
    QProgressBar* strengthBar = new QProgressBar();
    strengthBar->setFixedHeight(4);
    strengthBar->setTextVisible(false);
    strengthBar->setStyleSheet("QProgressBar { border: none; background: #333; } QProgressBar::chunk { background: #2cc985; }");
    layout->addWidget(strengthBar);

    // 设置区
    QHBoxLayout* settingsLayout = new QHBoxLayout();
    settingsLayout->addWidget(new QLabel("长度:"));
    QSpinBox* spinLen = new QSpinBox();
    spinLen->setRange(6, 64);
    spinLen->setValue(16);
    spinLen->setStyleSheet("QSpinBox { background: #333; border: 1px solid #444; padding: 5px; color: white; }");
    settingsLayout->addWidget(spinLen);
    layout->addLayout(settingsLayout);

    QCheckBox* chkUpper = new QCheckBox("包含大写字母 (A-Z)");
    QCheckBox* chkLower = new QCheckBox("包含小写字母 (a-z)");
    QCheckBox* chkDigit = new QCheckBox("包含数字 (0-9)");
    QCheckBox* chkSymbol = new QCheckBox("包含符号 (@#$)");
    chkUpper->setChecked(true); chkLower->setChecked(true); chkDigit->setChecked(true); chkSymbol->setChecked(true);
    layout->addWidget(chkUpper); layout->addWidget(chkLower); layout->addWidget(chkDigit); layout->addWidget(chkSymbol);

    QPushButton* btnGen = new QPushButton("生成并复制");
    btnGen->setFixedHeight(40);
    btnGen->setStyleSheet("QPushButton { background: #2cc985; color: white; font-weight: bold; border-radius: 20px; } QPushButton:hover { background: #229c67; }");
    
    auto generate = [=]() {
        QString pool = "";
        if (chkUpper->isChecked()) pool += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        if (chkLower->isChecked()) pool += "abcdefghijklmnopqrstuvwxyz";
        if (chkDigit->isChecked()) pool += "0123456789";
        if (chkSymbol->isChecked()) pool += "!@#$%^&*()-_=+";

        if (pool.isEmpty()) return;

        QString pwd = "";
        for (int i = 0; i < spinLen->value(); ++i) {
            pwd += pool.at(QRandomGenerator::global()->bounded(pool.length()));
        }
        resEdit->setText(pwd);
        QApplication::clipboard()->setText(pwd);
        
        // 简单强度估算
        int strength = (pwd.length() * 100) / 32;
        strengthBar->setValue(qMin(100, strength));
    };

    connect(btnGen, &QPushButton::clicked, generate);
    layout->addWidget(btnGen);
    layout->addStretch();
}
