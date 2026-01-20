#include "Toolbox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QClipboard>
#include <QApplication>
#include <QDateTime>
#include "../core/Utils.h"

Toolbox::Toolbox(QWidget* parent) : QDialog(parent) {
    setWindowTitle("工具箱");
    resize(400, 300);

    QVBoxLayout* layout = new QVBoxLayout(this);
    QTabWidget* tabs = new QTabWidget(this);

    QWidget* timeTab = new QWidget();
    initTimePasteTab(timeTab);
    tabs->addTab(timeTab, "时间助手");

    QWidget* pwdTab = new QWidget();
    initPasswordGenTab(pwdTab);
    tabs->addTab(pwdTab, "密码生成");

    layout->addWidget(tabs);
}

void Toolbox::initTimePasteTab(QWidget* tab) {
    QVBoxLayout* layout = new QVBoxLayout(tab);
    QLabel* info = new QLabel("快速复制当前时间:");
    layout->addWidget(info);

    QPushButton* btnNow = new QPushButton("复制当前时间 (YYYY-MM-DD HH:MM:SS)");
    connect(btnNow, &QPushButton::clicked, [](){
        QApplication::clipboard()->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    });
    layout->addWidget(btnNow);

    QPushButton* btnDate = new QPushButton("复制当前日期 (YYYY-MM-DD)");
    connect(btnDate, &QPushButton::clicked, [](){
        QApplication::clipboard()->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd"));
    });
    layout->addWidget(btnDate);

    layout->addStretch();
}

void Toolbox::initPasswordGenTab(QWidget* tab) {
    QVBoxLayout* layout = new QVBoxLayout(tab);

    QHBoxLayout* lenLayout = new QHBoxLayout();
    lenLayout->addWidget(new QLabel("长度:"));
    QSpinBox* spinLen = new QSpinBox();
    spinLen->setRange(4, 64);
    spinLen->setValue(16);
    lenLayout->addWidget(spinLen);
    layout->addLayout(lenLayout);

    QLineEdit* resEdit = new QLineEdit();
    resEdit->setReadOnly(true);
    layout->addWidget(resEdit);

    QPushButton* btnGen = new QPushButton("生成并复制");
    connect(btnGen, &QPushButton::clicked, [spinLen, resEdit](){
        QString pwd = Utils::generatePassword(spinLen->value());
        resEdit->setText(pwd);
        QApplication::clipboard()->setText(pwd);
    });
    layout->addWidget(btnGen);

    layout->addStretch();
}
