#include "FloatingBall.h"
#include "../core/DatabaseManager.h"
#include "IconHelper.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainterPath>
#include <QtMath>
#include <QRandomGenerator>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>

// 修复点：移除了 Qt::Tool，防止在 MinGW 环境下与透明背景冲突导致崩溃
FloatingBall::FloatingBall(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    setFixedSize(60, 60);

    // 初始化位置在屏幕右侧
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->geometry();
        move(screenGeom.width() - 80, screenGeom.height() / 2);
    }

    m_inertiaTimer = new QTimer(this);
    connect(m_inertiaTimer, &QTimer::timeout, this, &FloatingBall::startInertiaAnimation);

    m_particleTimer = new QTimer(this);
    connect(m_particleTimer, &QTimer::timeout, this, [this](){
        updateParticleEffect();
        update();
    });

    m_writingAnim = new WritingAnimation(this);
    m_writingAnim->hide();
    m_writingAnim->move(10, 10);
}

void FloatingBall::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. 绘制粒子
    for (const auto& p : m_particles) {
        QColor c = p.color;
        c.setAlphaF(p.life);
        painter.setBrush(c);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(p.pos, 3, 3);
    }

    // 2. 绘制阴影/外发光
    painter.setBrush(QColor(0, 0, 0, 60));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect().adjusted(4, 4, -2, -2));

    // 3. 绘制球体
    QLinearGradient gradient(0, 0, width(), height());
    gradient.setColorAt(0, m_color1);
    gradient.setColorAt(1, m_color2);
    painter.setBrush(gradient);
    painter.drawEllipse(rect().adjusted(6, 6, -6, -6));

    // 绘制图标感
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(rect().center(), 8, 8);
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
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    // 弹出效果
    if (pos().x() < 0) move(0, pos().y());
    if (pos().x() > screen->geometry().width() - width()) move(screen->geometry().width() - width(), pos().y());
}

void FloatingBall::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    checkEdgeAdsorption();
}

void FloatingBall::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #2b2b2b; color: #f0f0f0; border: 1px solid #444; padding: 5px; } QMenu::item { padding: 5px 20px; } QMenu::item:selected { background: #333; }");

    QMenu* skinMenu = menu.addMenu(IconHelper::getIcon("palette", "#aaaaaa"), "切换外观");
    skinMenu->addAction("摩卡·勃艮第", [this](){ switchSkin("mocha"); });
    skinMenu->addAction("经典黑金", [this](){ switchSkin("classic"); });
    skinMenu->addAction("皇家蓝", [this](){ switchSkin("royal"); });
    skinMenu->addAction("抹茶绿", [this](){ switchSkin("matcha"); });
    skinMenu->addAction("默认天蓝", [this](){ switchSkin("default"); });

    menu.addSeparator();
    QAction* actQuick = menu.addAction(IconHelper::getIcon("zap", "#aaaaaa"), "打开快速笔记", this, &FloatingBall::requestQuickWindow);
    QAction* actMain = menu.addAction(IconHelper::getIcon("monitor", "#aaaaaa"), "打开主界面", this, &FloatingBall::requestMainWindow);
    menu.addSeparator();
    QAction* actQuit = menu.addAction(IconHelper::getIcon("power", "#aaaaaa"), "退出程序", [](){ qApp->quit(); });

    menu.exec(event->globalPos());
}

void FloatingBall::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText() || event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FloatingBall::dropEvent(QDropEvent* event) {
    QString content = event->mimeData()->text();
    QString type = "text";
    if (event->mimeData()->hasUrls()) {
        content = event->mimeData()->urls().first().toLocalFile();
        type = "file";
    }
    DatabaseManager::instance().addNoteAsync("拖拽投喂", content, {"投喂"}, "", -1, type);

    burstParticles();
    m_writingAnim->start();

    event->acceptProposedAction();
}

void FloatingBall::switchSkin(const QString& name) {
    if (name == "mocha") { m_color1 = QColor("#5D4037"); m_color2 = QColor("#8D6E63"); }
    else if (name == "classic") { m_color1 = QColor("#212121"); m_color2 = QColor("#FFD700"); }
    else if (name == "royal") { m_color1 = QColor("#1A237E"); m_color2 = QColor("#3F51B5"); }
    else if (name == "matcha") { m_color1 = QColor("#1B5E20"); m_color2 = QColor("#4CAF50"); }
    else { m_color1 = QColor("#4FACFE"); m_color2 = QColor("#00F2FE"); }
    update();
}

void FloatingBall::burstParticles() {
    for(int i=0; i<20; ++i) {
        Particle p;
        p.pos = rect().center();
        double angle = QRandomGenerator::global()->generateDouble() * 2 * 3.14159265358979323846;
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
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeom = screen->geometry();
    int x = pos().x();
    if (x < screenGeom.width() / 2) {
        // 吸附到左侧
        QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
        anim->setDuration(300);
        anim->setEndValue(QPoint(-width() / 2 + 10, pos().y()));
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        // 吸附到右侧
        QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
        anim->setDuration(300);
        anim->setEndValue(QPoint(screenGeom.width() - width() / 2 - 10, pos().y()));
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void FloatingBall::updateParticleEffect() {
    for (int i = 0; i < m_particles.size(); ++i) {
        m_particles[i].pos += m_particles[i].velocity;
        m_particles[i].life -= 0.05;
        if (m_particles[i].life <= 0) {
            m_particles.removeAt(i);
            --i;
        }
    }
    if (m_particles.isEmpty()) m_particleTimer->stop();
}