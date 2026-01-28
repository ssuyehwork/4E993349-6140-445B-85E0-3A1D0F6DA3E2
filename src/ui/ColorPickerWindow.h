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
 * @brief 专业拾色器窗口：支持屏幕吸色、色彩理论建议、变体生成、多格式导出、无障碍检查及色盲模拟
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
    void exportPaletteAsCSS();

private:
    void initUI();
    void updateColorDisplay(const QColor& color, bool addToHistory = true);
    void extractColorsFromImage(const QImage& img);

    // 数据持久化
    void saveFavorite(const QString& hex);
    void loadFavorites();
    void addToHistory(const QString& hex);
    void loadHistory();

    // 专业色彩辅助逻辑
    void updateVariations(const QColor& color);
    void updateHarmony(const QColor& color);
    void updateFormats(const QColor& color);
    void updateAccessibility(const QColor& color);
    void updateBlindnessSimulation(const QColor& color);

    // 辅助计算
    double getLuminance(const QColor& c);
    double getContrastRatio(const QColor& c1, const QColor& c2);
    QColor simulateBlindness(const QColor& color, const double matrix[3][3]);

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

    // 无障碍与模拟组件
    QLabel* m_contrastLabel;
    QLabel* m_aaLabel;
    QLabel* m_aaaLabel;
    QWidget* m_blindnessContainer;

    QColor m_currentColor;
};

#endif // COLORPICKERWINDOW_H
