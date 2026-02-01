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
#include <QStringList>
#include <QKeyEvent>

/**
 * @brief 专门用于捕获全局热键的输入框
 */
class HotkeyEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget* parent = nullptr) : QLineEdit(parent) {
        setReadOnly(true);
        setPlaceholderText("点击按下快捷键...");
        setAlignment(Qt::AlignCenter);
        setStyleSheet("QLineEdit { background-color: #1e1e1e; border: 1px solid #333; color: #4a90e2; font-weight: bold; padding: 4px; border-radius: 4px; } "
                      "QLineEdit:focus { border: 1px solid #4a90e2; background-color: #252526; }");
    }

    void setKeyCombination(uint mods, uint vk, const QString& desc) {
        m_mods = mods;
        m_vk = vk;
        setText(desc);
    }

    uint modifiers() const { return m_mods; }
    uint vk() const { return m_vk; }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        int key = event->key();
        if (key == Qt::Key_Escape || key == Qt::Key_Backspace) {
            m_mods = 0; m_vk = 0;
            setText("未设置");
            return;
        }

        // 忽略单独的修饰键按下
        if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
            return;
        }

        uint winMods = 0;
        QStringList modNames;
        if (event->modifiers() & Qt::ControlModifier) { winMods |= 0x0002; modNames << "Ctrl"; }
        if (event->modifiers() & Qt::AltModifier) { winMods |= 0x0001; modNames << "Alt"; }
        if (event->modifiers() & Qt::ShiftModifier) { winMods |= 0x0004; modNames << "Shift"; }
        if (event->modifiers() & Qt::MetaModifier) { winMods |= 0x0008; modNames << "Win"; }

        // 获取原生虚拟键码 (Windows VK)
        uint winVk = event->nativeVirtualKey();
        if (winVk == 0) winVk = key; // 回退

        m_mods = winMods;
        m_vk = winVk;

        QString keyName = QKeySequence(key).toString(QKeySequence::NativeText);
        if (modNames.isEmpty()) {
            setText(keyName);
        } else {
            setText(modNames.join(" + ") + " + " + keyName);
        }
    }

private:
    uint m_mods = 0;
    uint m_vk = 0;
};


SettingsWindow::SettingsWindow(QWidget* parent) : FramelessDialog("系统设置", parent) {
    setFixedSize(450, 520); // 增加高度解决截断问题
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
    
    btnSet->setAutoDefault(false);
    btnModify->setAutoDefault(false);
    btnRemove->setAutoDefault(false);

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

    // 2. 快捷键设置部分
    auto* hkGroup = new QGroupBox("全局快捷键", m_contentArea);
    hkGroup->setStyleSheet("QGroupBox { color: #aaa; font-weight: bold; border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");
    auto* hkLayout = new QFormLayout(hkGroup);
    hkLayout->setLabelAlignment(Qt::AlignRight);

    QSettings hkSettings("RapidNotes", "Hotkeys");

    m_hkQuickWin = new HotkeyEdit();
    m_hkQuickWin->setKeyCombination(
        hkSettings.value("QuickWinMods", 0x0001).toUInt(),
        hkSettings.value("QuickWinVk", 0x20).toUInt(),
        hkSettings.value("QuickWinDesc", "Alt + Space").toString()
    );
    hkLayout->addRow("激活极速窗口:", m_hkQuickWin);

    m_hkFavorite = new HotkeyEdit();
    m_hkFavorite->setKeyCombination(
        hkSettings.value("FavoriteMods", 0x0002 | 0x0004).toUInt(),
        hkSettings.value("FavoriteVk", 0x45).toUInt(),
        hkSettings.value("FavoriteDesc", "Ctrl + Shift + E").toString()
    );
    hkLayout->addRow("快速收藏/加星:", m_hkFavorite);

    m_hkScreenshot = new HotkeyEdit();
    m_hkScreenshot->setKeyCombination(
        hkSettings.value("ScreenshotMods", 0x0002 | 0x0001).toUInt(),
        hkSettings.value("ScreenshotVk", 0x41).toUInt(),
        hkSettings.value("ScreenshotDesc", "Ctrl + Alt + A").toString()
    );
    hkLayout->addRow("屏幕截图识别:", m_hkScreenshot);

    layout->addWidget(hkGroup);
    layout->addStretch();

    // 底部按钮
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    auto* btnSave = new QPushButton("保存并应用");
    btnSave->setFixedSize(110, 32);
    btnSave->setAutoDefault(false);
    btnSave->setStyleSheet("QPushButton { background-color: #4a90e2; color: white; border: none; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #357abd; }");
    connect(btnSave, &QPushButton::clicked, this, &SettingsWindow::saveHotkeys);
    bottomLayout->addWidget(btnSave);

    auto* btnCancel = new QPushButton("取消");
    btnCancel->setFixedSize(80, 32);
    btnCancel->setAutoDefault(false);
    btnCancel->setStyleSheet("QPushButton { background-color: #333; color: #ccc; border: none; border-radius: 4px; } QPushButton:hover { background-color: #444; }");
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(btnCancel);

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
    QSettings hkSettings("RapidNotes", "Hotkeys");

    hkSettings.setValue("QuickWinMods", m_hkQuickWin->modifiers());
    hkSettings.setValue("QuickWinVk", m_hkQuickWin->vk());
    hkSettings.setValue("QuickWinDesc", m_hkQuickWin->text());

    hkSettings.setValue("FavoriteMods", m_hkFavorite->modifiers());
    hkSettings.setValue("FavoriteVk", m_hkFavorite->vk());
    hkSettings.setValue("FavoriteDesc", m_hkFavorite->text());

    hkSettings.setValue("ScreenshotMods", m_hkScreenshot->modifiers());
    hkSettings.setValue("ScreenshotVk", m_hkScreenshot->vk());
    hkSettings.setValue("ScreenshotDesc", m_hkScreenshot->text());

    QMessageBox::information(this, "成功", "快捷键设置已保存，部分设置可能需要重启应用生效。");
    accept();
}
