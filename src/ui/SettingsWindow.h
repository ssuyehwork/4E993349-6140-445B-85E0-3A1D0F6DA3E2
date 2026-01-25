#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include "FramelessDialog.h"
#include <QSettings>
#include <QKeySequence>

class QLineEdit;

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
    QLineEdit* m_hkQuickWin;
    QLineEdit* m_hkFavorite;
    QLineEdit* m_hkScreenshot;
};

#endif // SETTINGSWINDOW_H
