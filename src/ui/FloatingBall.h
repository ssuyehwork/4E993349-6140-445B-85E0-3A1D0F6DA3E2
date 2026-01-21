#ifndef FLOATINGBALL_H
#define FLOATINGBALL_H

#include <QWidget>
#include <QPoint>
#include <QPropertyAnimation>
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include "WritingAnimation.h"

class FloatingBall : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QPoint pos READ pos WRITE move)

public:
    explicit FloatingBall(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void switchSkin(const QString& name);
    void burstParticles();
    void startInertiaAnimation();
    void checkEdgeAdsorption();
    void updateParticleEffect();

    QPoint m_dragPosition;
    bool m_isDragging = false;
    QPoint m_velocity;
    QTimer* m_inertiaTimer;
    QTimer* m_particleTimer;
    WritingAnimation* m_writingAnim;
    
    struct Particle {
        QPointF pos;
        QPointF velocity;
        double life;
        QColor color;
    };
    QList<Particle> m_particles;

    QColor m_color1 = QColor("#4FACFE");
    QColor m_color2 = QColor("#00F2FE");

signals:
    void doubleClicked();
    void requestMainWindow();
    void requestQuickWindow();
};

#endif // FLOATINGBALL_H