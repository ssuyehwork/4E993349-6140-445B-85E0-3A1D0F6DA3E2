#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <QWidget>
#include <QPushButton>
#include "FramelessDialog.h"
#include <QPoint>

class Toolbox : public FramelessDialog {
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
    void showColorPickerRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void toggleStayOnTop(bool checked);

private:
    void initUI();
    QPushButton* createToolButton(const QString& text, const QString& iconName, const QString& color);

    QPoint m_dragPos;
};

#endif // TOOLBOX_H
