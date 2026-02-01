#ifndef TOOLBOX_H
#define TOOLBOX_H

#include "FramelessDialog.h"
#include <QPushButton>
#include <QPoint>

class Toolbox : public FramelessWindow {
    Q_OBJECT
public:
    explicit Toolbox(QWidget* parent = nullptr);
    ~Toolbox();

signals:
    void showTimePasteRequested();
    void showPasswordGeneratorRequested();
    void showOCRRequested();
    void showPathAcquisitionRequested();
    void showTagManagerRequested();
    void showFileStorageRequested();
    void showFileSearchRequested();
    void showKeywordSearchRequested();
    void showColorPickerRequested();

private:
    void initUI();
    QPushButton* createToolButton(const QString& text, const QString& iconName, const QString& color);
};

#endif // TOOLBOX_H
