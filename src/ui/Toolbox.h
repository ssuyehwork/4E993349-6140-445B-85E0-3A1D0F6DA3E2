#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <QDialog>
#include <QTabWidget>
#include <QTextEdit>

class Toolbox : public QDialog {
    Q_OBJECT
public:
    explicit Toolbox(QWidget* parent = nullptr);
    ~Toolbox();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void initTimePasteTab(QWidget* tab);
    void initPasswordGenTab(QWidget* tab);
    void initOCRTab(QWidget* tab);
    void onDigitPressed(int digit);

    int m_timeMode = 0; // 0: 退, 1: 进
    QTabWidget* m_tabs;
    QTextEdit* m_ocrResult;
};

#endif // TOOLBOX_H
