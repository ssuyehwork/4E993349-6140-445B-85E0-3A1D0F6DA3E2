#ifndef COLORPICKERWINDOW_H
#define COLORPICKERWINDOW_H

#include "FramelessDialog.h"
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QButtonGroup>

/**
 * @brief 专业拾色器窗口：支持屏幕吸色、色彩理论建议、变体生成及多格式导出
 */
class ColorPickerWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit ColorPickerWindow(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void startPickColor();
    void openColorWheel();
    void addCurrentToFavorites();
    void onColorSelected(QListWidgetItem* item);
    void clearFavorites();
    void copySpecificFormat();

private:
    void initUI();
    void updateColorDisplay(const QColor& color, bool addToHistory = true);
    void extractColorsFromImage(const QImage& img);

    // 数据持久化
    void saveFavorite(const QString& hex);
    void loadFavorites();
    void addToHistory(const QString& hex);
    void loadHistory();

    // 色彩辅助逻辑
    void updateVariations(const QColor& color);
    void updateHarmony(const QColor& color);
    void updateFormats(const QColor& color);

    // UI 组件
    QLabel* m_colorPreview;
    QLineEdit* m_hexEdit;
    QLabel* m_rgbLabel;
    QLabel* m_hsvLabel;
    QLabel* m_cmykLabel;

    QListWidget* m_favoriteList;
    QListWidget* m_historyList;
    QListWidget* m_extractedList;

    QWidget* m_shadesContainer;
    QWidget* m_harmonyContainer;

    QColor m_currentColor;
};

#endif // COLORPICKERWINDOW_H
