#include "Toolbox.h"
#include "IconHelper.h"
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
#include "../core/Utils.h"

Toolbox::Toolbox(QWidget* parent) : QDialog(parent) {
    setWindowTitle("工具箱");
    resize(450, 400);
    setStyleSheet("QDialog { background-color: #1e1e1e; color: #ccc; } QTabWidget::pane { border: 1px solid #333; background: #252526; } QTabBar::tab { background: #2d2d2d; padding: 10px 20px; border-right: 1px solid #1e1e1e; } QTabBar::tab:selected { background: #252526; color: #4a90e2; }");

    QVBoxLayout* layout = new QVBoxLayout(this);
    QTabWidget* tabs = new QTabWidget(this);

    QWidget* timeTab = new QWidget();
    initTimePasteTab(timeTab);
    tabs->addTab(timeTab, IconHelper::getIcon("clock", "#aaaaaa"), " 时间助手");

    QWidget* pwdTab = new QWidget();
    initPasswordGenTab(pwdTab);
    tabs->addTab(pwdTab, IconHelper::getIcon("lock", "#aaaaaa"), " 密码生成");

    layout->addWidget(tabs);
}

void Toolbox::initTimePasteTab(QWidget* tab) {
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    QLabel* info = new QLabel("🕒 快速格式化输出当前时间");
    info->setStyleSheet("font-weight: bold; color: #4a90e2;");
    layout->addWidget(info);

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
