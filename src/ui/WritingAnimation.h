#ifndef WRITINGANIMATION_H
#define WRITINGANIMATION_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QtMath>

class WritingAnimation : public QWidget {
    Q_OBJECT
public:
    explicit WritingAnimation(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(40, 40);
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &WritingAnimation::updatePhysics);
    }

    void start() {
        m_time = 0;
        m_timer->start(20);
        show();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 绘制简单的笔和纸动画
        p.translate(width()/2, height()/2);

        // 纸
        p.setBrush(QColor("#f5f0e1"));
        p.drawRoundedRect(-15, -15, 30, 30, 2, 2);

        // 笔
        p.rotate(m_angle);
        p.setBrush(QColor("#8b4513"));
        p.drawRect(-2, -10, 4, 20);
    }

private slots:
    void updatePhysics() {
        m_time += 0.1;
        m_angle = -45 + 20 * qSin(m_time * 5);
        update();
        if (m_time > 3.0) {
            m_timer->stop();
            hide();
        }
    }

private:
    QTimer* m_timer;
    float m_time = 0;
    float m_angle = -45;
};

#endif // WRITINGANIMATION_H
