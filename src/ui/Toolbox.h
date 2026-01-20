#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <QDialog>
#include <QTabWidget>

class Toolbox : public QDialog {
    Q_OBJECT
public:
    explicit Toolbox(QWidget* parent = nullptr);

private:
    void initTimePasteTab(QWidget* tab);
    void initPasswordGenTab(QWidget* tab);
};

#endif // TOOLBOX_H
