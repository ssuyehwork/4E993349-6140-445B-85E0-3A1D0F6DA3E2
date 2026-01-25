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

// 修复点：使用 Qt::Tool 隐藏任务栏图标
FloatingBall::FloatingBall(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    setFixedSize(60, 80); // 调整尺寸以适应书籍长方形比例
    
    switchSkin("mocha"); // 默认使用复刻外观

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

    if (m_skinName == "mocha" || m_skinName == "open" || m_skinName == "default") {
        // 复刻 1:1 Python 版外观
        float cx = width() / 2.0f;
        float cy = height() / 2.0f;

        // 绘制书籍
        painter.save();
        painter.translate(cx, cy);
        painter.scale(0.8, 0.8);
        drawBook(&painter);
        painter.restore();

        // 绘制钢笔
        painter.save();
        painter.translate(cx, cy - 5);
        painter.scale(0.8, 0.8);
        painter.rotate(-45);
        drawPen(&painter);
        painter.restore();
    } else {
        // 兼容其他圆球皮肤
        painter.setBrush(QColor(0, 0, 0, 60));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(rect().adjusted(4, 4, -2, -2));

        QLinearGradient gradient(0, 0, width(), height());
        gradient.setColorAt(0, m_color1);
        gradient.setColorAt(1, m_color2);
        painter.setBrush(gradient);
        painter.drawEllipse(rect().adjusted(6, 6, -6, -6));

        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(rect().center(), 8, 8);
    }
}

void FloatingBall::drawBook(QPainter* p) {
    p->setPen(Qt::NoPen);
    // 纸张底层
    p->setBrush(QColor("#f5f0e1"));
    p->drawRoundedRect(QRectF(-22, -32, 56, 76), 3, 3);

    // 封面
    QLinearGradient grad(-28, -38, 28, 38);
    grad.setColorAt(0, QColor("#5a3c32"));
    grad.setColorAt(1, QColor("#321e19"));
    p->setBrush(grad);
    p->drawRoundedRect(QRectF(-28, -38, 56, 76), 3, 3);

    // 红带子 (书脊装饰)
    p->setBrush(QColor("#78141e"));
    p->drawRect(QRectF(13, -38, 8, 76));
}

void FloatingBall::drawPen(QPainter* p) {
    p->setPen(Qt::NoPen);
    // 钢笔杆
    QLinearGradient bodyGrad(-6, 0, 6, 0);
    bodyGrad.setColorAt(0, QColor("#b43c46"));
    bodyGrad.setColorAt(0.5, QColor("#8c141e"));
    bodyGrad.setColorAt(1, QColor("#3c050a"));
    p->setBrush(bodyGrad);
    p->drawRoundedRect(QRectF(-6, -23, 12, 46), 5, 5);

    // 钢笔尖
    QPainterPath tipPath;
    tipPath.moveTo(-3, 23);
    tipPath.lineTo(3, 23);
    tipPath.lineTo(0, 37);
    tipPath.closeSubpath();
    p->setBrush(QColor("#f0e6b4"));
    p->drawPath(tipPath);

    // 装饰线
    p->setBrush(QColor("#d2c88c"));
    p->drawRect(QRectF(-6, 19, 12, 4));
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
    menu.setStyleSheet(
        "QMenu { background-color: #2b2b2b; color: #f0f0f0; border: 1px solid #444; border-radius: 5px; padding: 5px; } "
        "QMenu::item { padding: 6px 15px 6px 5px; border-radius: 3px; } "
        "QMenu::item:selected { background-color: #5D4037; color: #fff; } "
        "QMenu::separator { background-color: #444; height: 1px; margin: 4px 0; }"
    );

    QMenu* skinMenu = menu.addMenu(IconHelper::getIcon("palette", "#aaaaaa"), "切换外观");
    skinMenu->addAction(IconHelper::getIcon("coffee", "#BCAAA4"), "摩卡·勃艮第", [this](){ switchSkin("mocha"); });
    skinMenu->addAction(IconHelper::getIcon("grid", "#90A4AE"), "经典黑金", [this](){ switchSkin("classic"); });
    skinMenu->addAction(IconHelper::getIcon("book", "#9FA8DA"), "皇家蓝", [this](){ switchSkin("royal"); });
    skinMenu->addAction(IconHelper::getIcon("leaf", "#A5D6A7"), "抹茶绿", [this](){ switchSkin("matcha"); });
    skinMenu->addAction(IconHelper::getIcon("book_open", "#FFCC80"), "摊开手稿", [this](){ switchSkin("open"); });
    skinMenu->addAction("默认天蓝", [this](){ switchSkin("default"); });

    menu.addSeparator();
    menu.addAction(IconHelper::getIcon("zap", "#aaaaaa"), "打开快速笔记", this, &FloatingBall::requestQuickWindow);
    menu.addAction(IconHelper::getIcon("monitor", "#aaaaaa"), "打开主界面", this, &FloatingBall::requestMainWindow);
    menu.addAction(IconHelper::getIcon("add", "#aaaaaa"), "新建灵感", this, &FloatingBall::requestNewIdea);
    menu.addSeparator();
    menu.addAction(IconHelper::getIcon("power", "#aaaaaa"), "退出程序", [](){ qApp->quit(); });
    
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

QIcon FloatingBall::generateBallIcon() {
    FloatingBall temp(nullptr);
    temp.setAttribute(Qt::WA_DontShowOnScreen);
    temp.m_particles.clear();
    temp.switchSkin("mocha");

    QPixmap pixmap(temp.size());
    pixmap.fill(Qt::transparent);
    temp.render(&pixmap);
    return QIcon(pixmap);
}

void FloatingBall::switchSkin(const QString& name) {
    m_skinName = name;
    if (name == "mocha") { m_color1 = QColor("#5D4037"); m_color2 = QColor("#8D6E63"); }
    else if (name == "classic") { m_color1 = QColor("#212121"); m_color2 = QColor("#FFD700"); }
    else if (name == "royal") { m_color1 = QColor("#1A237E"); m_color2 = QColor("#3F51B5"); }
    else if (name == "matcha") { m_color1 = QColor("#1B5E20"); m_color2 = QColor("#4CAF50"); }
    else if (name == "open") { /* handled in paintEvent */ }
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