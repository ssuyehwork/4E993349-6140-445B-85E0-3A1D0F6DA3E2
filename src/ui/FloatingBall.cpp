#include "FloatingBall.h"
#include "../core/DatabaseManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainterPath>
#include <QtMath>
#include <QRandomGenerator>

FloatingBall::FloatingBall(QWidget* parent) : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    setFixedSize(60, 60);

    m_inertiaTimer = new QTimer(this);
    connect(m_inertiaTimer, &QTimer::timeout, this, &FloatingBall::startInertiaAnimation);

    m_particleTimer = new QTimer(this);
    connect(m_particleTimer, &QTimer::timeout, this, [this](){
        updateParticleEffect();
        update();
    });
}

void FloatingBall::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制阴影/发光
    painter.setBrush(QColor(40, 40, 40, 200));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect().adjusted(2, 2, -2, -2));

    // 绘制粒子
    for (const auto& p : m_particles) {
        QColor c = p.color;
        c.setAlphaF(p.life);
        painter.setBrush(c);
        painter.drawEllipse(p.pos, 2, 2);
    }

    // 绘制主体
    QLinearGradient gradient(0, 0, width(), height());
    gradient.setColorAt(0, QColor("#4FACFE"));
    gradient.setColorAt(1, QColor("#00F2FE"));
    painter.setBrush(gradient);
    painter.drawEllipse(rect().adjusted(5, 5, -5, -5));

    // 绘制图标感
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(rect().center(), 10, 10);
}

void FloatingBall::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_inertiaTimer->stop();
    }
}

void FloatingBall::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        QPoint oldPos = pos();
        move(event->globalPosition().toPoint() - m_dragPosition);
        m_velocity = pos() - oldPos;
    }
}

void FloatingBall::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        if (m_velocity.manhattanLength() > 5) {
            m_inertiaTimer->start(16);
        } else {
            checkEdgeAdsorption();
        }
    }
}

void FloatingBall::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    emit doubleClicked();
}

void FloatingBall::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    // 弹出效果
    if (pos().x() < 0) move(0, pos().y());
    if (pos().x() > screen()->geometry().width() - width()) move(screen()->geometry().width() - width(), pos().y());
}

void FloatingBall::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    checkEdgeAdsorption();
}

void FloatingBall::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.addAction("摩卡·勃艮第", [this](){ switchSkin("mocha"); });
    menu.addAction("经典黑金", [this](){ switchSkin("gold"); });
    menu.addAction("皇家蓝", [this](){ switchSkin("blue"); });
    menu.addAction("抹茶绿", [this](){ switchSkin("green"); });
    menu.exec(event->globalPos());
}

void FloatingBall::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText() || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FloatingBall::dropEvent(QDropEvent* event) {
    QString content = event->mimeData()->text();
    DatabaseManager::instance().addNoteAsync("拖拽记录", content, {"投喂"});
    burstParticles();
    event->acceptProposedAction();
}

void FloatingBall::switchSkin(const QString& name) {
    // 实际逻辑中可以改变渐变色或贴图
    update();
}

void FloatingBall::burstParticles() {
    for(int i=0; i<20; ++i) {
        Particle p;
        p.pos = rect().center();
        double angle = QRandomGenerator::global()->generateDouble() * 2 * M_PI;
        double speed = QRandomGenerator::global()->generateDouble() * 5 + 2;
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.life = 1.0;
        p.color = QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 255);
        m_particles.append(p);
    }
    m_particleTimer->start(16);
}

void FloatingBall::startInertiaAnimation() {
    move(pos() + m_velocity);
    m_velocity *= 0.9; // 阻尼

    if (m_velocity.manhattanLength() < 1) {
        m_inertiaTimer->stop();
        checkEdgeAdsorption();
    }
}

void FloatingBall::checkEdgeAdsorption() {
    QRect screenGeom = screen()->geometry();
    int x = pos().x();
    if (x < screenGeom.width() / 2) {
        // 吸附到左侧
        QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
        anim->setDuration(300);
        anim->setEndValue(QPoint(-width() / 2, pos().y()));
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        // 吸附到右侧
        QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
        anim->setDuration(300);
        anim->setEndValue(QPoint(screenGeom.width() - width() / 2, pos().y()));
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void FloatingBall::updateParticleEffect() {
    // 简单的粒子更新逻辑
    for (int i = 0; i < m_particles.size(); ++i) {
        m_particles[i].pos += m_particles[i].velocity;
        m_particles[i].life -= 0.05;
        if (m_particles[i].life <= 0) {
            m_particles.removeAt(i);
            --i;
        }
    }
}
