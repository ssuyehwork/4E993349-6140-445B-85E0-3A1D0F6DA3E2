#include "SettingsWindow.h"
#include "IconHelper.h"
#include "CategoryPasswordDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QToolTip>

SettingsWindow::SettingsWindow(QWidget* parent) : FramelessDialog("系统设置", parent) {
    setFixedSize(450, 400);
    initSettingsUI();
}

void SettingsWindow::initSettingsUI() {
    auto* layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(20);

    // 1. 密码管理部分
    auto* pwdGroup = new QGroupBox("安全设置", m_contentArea);
    pwdGroup->setStyleSheet("QGroupBox { color: #aaa; font-weight: bold; border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    auto* pwdLayout = new QVBoxLayout(pwdGroup);

    QSettings settings("RapidNotes", "QuickWindow");
    bool hasPwd = !settings.value("appPassword").toString().isEmpty();

    auto* btnSet = new QPushButton(IconHelper::getIcon("lock", "#aaa"), " 设置启动密码");
    auto* btnModify = new QPushButton(IconHelper::getIcon("edit", "#aaa"), " 修改启动密码");
    auto* btnRemove = new QPushButton(IconHelper::getIcon("trash", "#e74c3c"), " 移除启动密码");

    QString btnStyle = "QPushButton { background-color: #2D2D2D; color: #EEE; border: 1px solid #444; border-radius: 5px; padding: 8px; text-align: left; } QPushButton:hover { background-color: #3E3E42; }";
    btnSet->setStyleSheet(btnStyle);
    btnModify->setStyleSheet(btnStyle);
    btnRemove->setStyleSheet(btnStyle);

    btnSet->setVisible(!hasPwd);
    btnModify->setVisible(hasPwd);
    btnRemove->setVisible(hasPwd);

    connect(btnSet, &QPushButton::clicked, this, &SettingsWindow::handleSetPassword);
    connect(btnModify, &QPushButton::clicked, this, &SettingsWindow::handleModifyPassword);
    connect(btnRemove, &QPushButton::clicked, this, &SettingsWindow::handleRemovePassword);

    pwdLayout->addWidget(btnSet);
    pwdLayout->addWidget(btnModify);
    pwdLayout->addWidget(btnRemove);
    layout->addWidget(pwdGroup);

    // 2. 快捷键设置部分 (目前仅展示，或实现简单的显示)
    auto* hkGroup = new QGroupBox("全局快捷键 (当前仅供查看)", m_contentArea);
    hkGroup->setStyleSheet("QGroupBox { color: #aaa; font-weight: bold; border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    auto* hkLayout = new QFormLayout(hkGroup);
    hkLayout->setLabelAlignment(Qt::AlignRight);

    auto addHkRow = [&](const QString& label, const QString& current) {
        auto* edit = new QLineEdit(current);
        edit->setReadOnly(true);
        edit->setStyleSheet("background-color: #1e1e1e; border: 1px solid #333; color: #888; padding: 4px; border-radius: 4px;");
        hkLayout->addRow(label, edit);
    };

    addHkRow("激活极速窗口:", "Alt + Space");
    addHkRow("快速收藏/加星:", "Ctrl + Shift + E");
    addHkRow("屏幕截图识别:", "Ctrl + Alt + A");

    layout->addWidget(hkGroup);
    layout->addStretch();

    // 底部关闭按钮
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    auto* btnClose = new QPushButton("关闭");
    btnClose->setFixedSize(80, 30);
    btnClose->setStyleSheet("QPushButton { background-color: #4a90e2; color: white; border: none; border-radius: 4px; } QPushButton:hover { background-color: #357abd; }");
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(btnClose);
    layout->addLayout(bottomLayout);
}

void SettingsWindow::handleSetPassword() {
    auto* dlg = new CategoryPasswordDialog("设置启动密码", this);
    if (dlg->exec() == QDialog::Accepted) {
        if (dlg->password() == dlg->confirmPassword()) {
            QSettings s("RapidNotes", "QuickWindow");
            s.setValue("appPassword", dlg->password());
            s.setValue("appPasswordHint", dlg->passwordHint());
            QMessageBox::information(this, "成功", "启动密码已设置，下次启动时生效。");
            accept(); // 关闭设置窗口以刷新状态（简单处理）
        } else {
            QMessageBox::warning(this, "错误", "两次输入的密码不一致。");
        }
    }
    dlg->deleteLater();
}

void SettingsWindow::handleModifyPassword() {
    QSettings s("RapidNotes", "QuickWindow");
    auto* verifyDlg = new FramelessInputDialog("身份验证", "请输入当前启动密码:", "", this);
    verifyDlg->setEchoMode(QLineEdit::Password);
    if (verifyDlg->exec() == QDialog::Accepted) {
        if (verifyDlg->text() == s.value("appPassword").toString()) {
            auto* dlg = new CategoryPasswordDialog("修改启动密码", this);
            dlg->setInitialData(s.value("appPasswordHint").toString());
            if (dlg->exec() == QDialog::Accepted) {
                if (dlg->password() == dlg->confirmPassword()) {
                    s.setValue("appPassword", dlg->password());
                    s.setValue("appPasswordHint", dlg->passwordHint());
                    QMessageBox::information(this, "成功", "启动密码已更新。");
                    accept();
                } else {
                    QMessageBox::warning(this, "错误", "两次输入的密码不一致。");
                }
            }
            dlg->deleteLater();
        } else {
            QMessageBox::warning(this, "错误", "密码错误，无法修改。");
        }
    }
    verifyDlg->deleteLater();
}

void SettingsWindow::handleRemovePassword() {
    auto* verifyDlg = new FramelessInputDialog("身份验证", "请输入启动密码以移除保护:", "", this);
    verifyDlg->setEchoMode(QLineEdit::Password);
    if (verifyDlg->exec() == QDialog::Accepted) {
        QSettings s("RapidNotes", "QuickWindow");
        if (verifyDlg->text() == s.value("appPassword").toString()) {
            s.remove("appPassword");
            s.remove("appPasswordHint");
            QMessageBox::information(this, "成功", "启动密码已移除。");
            accept();
        } else {
            QMessageBox::warning(this, "错误", "密码错误，操作取消。");
        }
    }
    verifyDlg->deleteLater();
}

void SettingsWindow::saveHotkeys() {
    // 预留以后实现
}
