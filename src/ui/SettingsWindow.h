#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include "FramelessDialog.h"
#include <QSettings>
#include <QKeySequence>

class QLineEdit;

class HotkeyEdit;

class SettingsWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

private slots:
    void handleSetPassword();
    void handleModifyPassword();
    void handleRemovePassword();
    void saveHotkeys();

private:
    void initSettingsUI();
    
    // UI elements for Hotkeys
    HotkeyEdit* m_hkQuickWin;
    HotkeyEdit* m_hkFavorite;
    HotkeyEdit* m_hkScreenshot;
};

#endif // SETTINGSWINDOW_H
