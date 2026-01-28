#ifndef COLORPICKERWINDOW_H
#define COLORPICKERWINDOW_H

#include "FramelessDialog.h"
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDragEnterEvent>
#include <QDropEvent>

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
    void onFavoriteSelected(QListWidgetItem* item);
    void clearFavorites();

private:
    void initUI();
    void updateColorDisplay(const QColor& color);
    void extractColorsFromImage(const QImage& img);
    void saveFavorite(const QString& hex);
    void loadFavorites();

    QLabel* m_colorPreview;
    QLineEdit* m_hexEdit;
    QListWidget* m_favoriteList;
    QListWidget* m_extractedList;

    QColor m_currentColor;
};

#endif // COLORPICKERWINDOW_H
