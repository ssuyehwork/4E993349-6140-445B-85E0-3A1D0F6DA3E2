#ifndef TIMEPASTEWINDOW_H
#define TIMEPASTEWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

class TimePasteWindow : public QWidget {
    Q_OBJECT
public:
    explicit TimePasteWindow(QWidget* parent = nullptr);
    ~TimePasteWindow();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void updateDateTime();
    void onDigitPressed(int digit);

private:
    void initUI();
    QString getRadioStyle();

    QLabel* m_dateLabel;
    QLabel* m_timeLabel;
    QRadioButton* m_radioPrev;
    QRadioButton* m_radioNext;
    QButtonGroup* m_buttonGroup;
    QTimer* m_timer;
    QPoint m_dragPos;
};

#endif // TIMEPASTEWINDOW_H
