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
#include <QSettings>

FloatingBall::FloatingBall(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    setFixedSize(120, 120); // 1:1 复刻 Python 版尺寸
    
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &FloatingBall::updatePhysics);
    m_timer->start(16);

    restorePosition();
    switchSkin("mocha");
}

void FloatingBall::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    float cx = width() / 2.0f;
    float cy = height() / 2.0f;

    // 1. 绘制阴影
    painter.save();
    painter.translate(cx, cy + m_bookY + 15);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawEllipse(QRectF(-35, -10, 70, 20));
    painter.restore();

    // 2. 绘制粒子
    for (const auto& p : m_particles) {
        QColor c = p.color;
        c.setAlphaF(p.life);
        painter.setBrush(c);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(p.pos, p.size, p.size);
    }

    // 3. 绘制笔记本
    painter.save();
    painter.translate(cx, cy + m_bookY);
    drawBook(&painter);
    painter.restore();

    // 4. 绘制钢笔
    painter.save();
    painter.translate(cx + m_penX, cy + m_penY - 5);
    painter.rotate(m_penAngle);
    drawPen(&painter);
    painter.restore();
}

void FloatingBall::drawBook(QPainter* p) {
    p->setPen(Qt::NoPen);
    if (m_skinName == "open") {
        float w = 80, h = 50;
        p->rotate(-5);
        QPainterPath path;
        path.moveTo(-w/2, -h/2); path.lineTo(0, -h/2 + 4);
        path.lineTo(w/2, -h/2); path.lineTo(w/2, h/2);
        path.lineTo(0, h/2 + 4); path.lineTo(-w/2, h/2); path.closeSubpath();
        p->setBrush(QColor("#f8f8f5"));
        p->drawPath(path);
        // 中缝阴影
        QLinearGradient grad(-10, 0, 10, 0);
        grad.setColorAt(0, QColor(0,0,0,0)); grad.setColorAt(0.5, QColor(0,0,0,20)); grad.setColorAt(1, QColor(0,0,0,0));
        p->setBrush(grad);
        p->drawRect(QRectF(-5, -h/2+4, 10, h-4));
        // 横线
        p->setPen(QPen(QColor(200, 200, 200), 1));
        for (int y = (int)(-h/2)+15; y < (int)(h/2); y += 7) {
            p->drawLine(int(-w/2+5), y, -5, y+2);
            p->drawLine(5, y+2, int(w/2-5), y);
        }
    } else {
        float w = 56, h = 76;
        if (m_skinName == "classic") {
            p->setBrush(QColor("#ebebe6"));
            p->drawRoundedRect(QRectF(-w/2+6, -h/2+6, w, h), 3, 3);
            QLinearGradient grad(-w, -h, w, h);
            grad.setColorAt(0, QColor("#3c3c41")); grad.setColorAt(1, QColor("#141419"));
            p->setBrush(grad);
            p->drawRoundedRect(QRectF(-w/2, -h/2, w, h), 3, 3);
            p->setBrush(QColor(10, 10, 10, 200));
            p->drawRect(QRectF(w/2 - 12, -h/2, 6, h));
        } else if (m_skinName == "royal") {
            p->setBrush(QColor("#f0f0eb"));
            p->drawRoundedRect(QRectF(-w/2+6, -h/2+6, w, h), 2, 2);
            QLinearGradient grad(-w, -h, w, 0);
            grad.setColorAt(0, QColor("#282864")); grad.setColorAt(1, QColor("#0a0a32"));
            p->setBrush(grad);
            p->drawRoundedRect(QRectF(-w/2, -h/2, w, h), 2, 2);
            p->setBrush(QColor(218, 165, 32));
            float c_size = 12;
            QPolygonF poly; poly << QPointF(w/2, -h/2) << QPointF(w/2-c_size, -h/2) << QPointF(w/2, -h/2+c_size);
            p->drawPolygon(poly);
        } else if (m_skinName == "matcha") {
            p->setBrush(QColor("#fafaf5"));
            p->drawRoundedRect(QRectF(-w/2+5, -h/2+5, w, h), 3, 3);
            QLinearGradient grad(-w, -h, w, h);
            grad.setColorAt(0, QColor("#a0be96")); grad.setColorAt(1, QColor("#64825a"));
            p->setBrush(grad);
            p->drawRoundedRect(QRectF(-w/2, -h/2, w, h), 3, 3);
            p->setBrush(QColor(255, 255, 255, 200));
            p->drawRoundedRect(QRectF(-w/2+10, -20, 34, 15), 2, 2);
        } else { // mocha / default
            p->setBrush(QColor("#f5f0e1"));
            p->drawRoundedRect(QRectF(-w/2+6, -h/2+6, w, h), 3, 3);
            QLinearGradient grad(-w, -h, w, h);
            grad.setColorAt(0, QColor("#5a3c32")); grad.setColorAt(1, QColor("#321e19"));
            p->setBrush(grad);
            p->drawRoundedRect(QRectF(-w/2, -h/2, w, h), 3, 3);
            p->setBrush(QColor(120, 20, 30));
            p->drawRect(QRectF(w/2 - 15, -h/2, 8, h));
        }
    }
}

void FloatingBall::drawPen(QPainter* p) {
    p->setPen(Qt::NoPen);
    QColor c_light, c_mid, c_dark;
    if (m_skinName == "royal") {
        c_light = QColor(60, 60, 70); c_mid = QColor(20, 20, 25); c_dark = QColor(0, 0, 0);
    } else if (m_skinName == "classic") {
        c_light = QColor(80, 80, 80); c_mid = QColor(30, 30, 30); c_dark = QColor(10, 10, 10);
    } else if (m_skinName == "matcha") {
        c_light = QColor(255, 255, 250); c_mid = QColor(240, 240, 230); c_dark = QColor(200, 200, 190);
    } else {
        c_light = QColor(180, 60, 70); c_mid = QColor(140, 20, 30); c_dark = QColor(60, 5, 10);
    }

    QLinearGradient bodyGrad(-6, 0, 6, 0);
    bodyGrad.setColorAt(0.0, c_light); bodyGrad.setColorAt(0.5, c_mid); bodyGrad.setColorAt(1.0, c_dark);
    QPainterPath path_body; path_body.addRoundedRect(QRectF(-6, -23, 12, 46), 5, 5);
    p->setBrush(bodyGrad); p->drawPath(path_body);
    
    QPainterPath tipPath;
    tipPath.moveTo(-3, 23); tipPath.lineTo(3, 23); tipPath.lineTo(0, 37); tipPath.closeSubpath();
    QLinearGradient tipGrad(-5, 0, 5, 0);
    tipGrad.setColorAt(0, QColor(240, 230, 180)); tipGrad.setColorAt(1, QColor(190, 170, 100));
    p->setBrush(tipGrad); p->drawPath(tipPath);
    
    p->setBrush(QColor(220, 200, 140)); p->drawRect(QRectF(-6, 19, 12, 4));
    p->setBrush(QColor(210, 190, 130)); p->drawRoundedRect(QRectF(-1.5, -17, 3, 24), 1.5, 1.5);
}

void FloatingBall::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_offset = event->pos();
        m_penY += 3.0f; // 1:1 复刻 Python 按下弹性反馈
        update();
    }
}

void FloatingBall::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        move(event->globalPosition().toPoint() - m_offset);
    }
}

void FloatingBall::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        savePosition();
    }
}

void FloatingBall::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
    }
}

void FloatingBall::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    m_isHovering = true;
}

void FloatingBall::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    m_isHovering = false;
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
    if (event->mimeData()->hasText()) {
        event->accept();
        m_isHovering = true;
    } else {
        event->ignore();
    }
}

void FloatingBall::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    m_isHovering = false;
}

void FloatingBall::dropEvent(QDropEvent* event) {
    m_isHovering = false;
    QString text = event->mimeData()->text();
    if (!text.trimmed().isEmpty()) {
        DatabaseManager::instance().addNoteAsync("拖拽投喂", text, {"投喂"}, "", -1, "text");
        burstParticles();
        m_isWriting = true;
        m_writeTimer = 0;
        event->acceptProposedAction();
    }
}

QIcon FloatingBall::generateBallIcon() {
    FloatingBall temp(nullptr);
    temp.m_timer->stop(); // 停止动画
    temp.setAttribute(Qt::WA_DontShowOnScreen);
    temp.m_particles.clear();
    temp.switchSkin("mocha");
    temp.m_timeStep = 0; // 静态
    
    QPixmap pixmap(temp.size());
    pixmap.fill(Qt::transparent);
    temp.render(&pixmap);
    return QIcon(pixmap);
}

void FloatingBall::switchSkin(const QString& name) {
    m_skinName = name;
    update();
}

void FloatingBall::burstParticles() {
    // 逻辑保持
}

void FloatingBall::updatePhysics() {
    m_timeStep += 0.05f;
    
    // 1. 待机呼吸
    float idlePenY = qSin(m_timeStep * 0.5f) * 4.0f;
    float idleBookY = qSin(m_timeStep * 0.5f - 1.0f) * 2.0f;
    
    float targetPenAngle = -45.0f;
    float targetPenX = 0.0f;
    float targetPenY = idlePenY;
    float targetBookY = idleBookY;
    
    // 2. 书写/悬停动画
    if (m_isWriting || m_isHovering) {
        m_writeTimer++;
        targetPenAngle = -65.0f;
        float writeSpeed = m_timeStep * 3.0f;
        targetPenX = qSin(writeSpeed) * 8.0f;
        targetPenY = 5.0f + qCos(writeSpeed * 2.0f) * 2.0f;
        targetBookY = -3.0f;
        
        if (m_isWriting && m_writeTimer > 90) {
            m_isWriting = false;
        }
    }
    
    // 3. 物理平滑
    float easing = 0.1f;
    m_penAngle += (targetPenAngle - m_penAngle) * easing;
    m_penX += (targetPenX - m_penX) * easing;
    m_penY += (targetPenY - m_penY) * easing;
    m_bookY += (targetBookY - m_bookY) * easing;

    updateParticles();
    update();
}

void FloatingBall::updateParticles() {
    if ((m_isWriting || m_isHovering) && m_particles.size() < 15) {
        if (QRandomGenerator::global()->generateDouble() < 0.3) {
            float rad = qDegreesToRadians(m_penAngle);
            float tipLen = 35.0f;
            Particle p;
            p.pos = QPointF(width()/2.0f + m_penX - qSin(rad)*tipLen, height()/2.0f + m_penY + qCos(rad)*tipLen);
            p.velocity = QPointF(QRandomGenerator::global()->generateDouble() - 0.5, QRandomGenerator::global()->generateDouble() + 0.5);
            p.life = 1.0;
            p.size = 1.0f + QRandomGenerator::global()->generateDouble() * 2.0f;
            p.color = QColor::fromHsv(QRandomGenerator::global()->bounded(360), 150, 255);
            m_particles.append(p);
        }
    }
    for (int i = 0; i < m_particles.size(); ++i) {
        m_particles[i].pos += m_particles[i].velocity;
        m_particles[i].life -= 0.03;
        m_particles[i].size *= 0.96f;
        if (m_particles[i].life <= 0) {
            m_particles.removeAt(i);
            --i;
        }
    }
}

void FloatingBall::savePosition() {
    QSettings settings("RapidNotes", "FloatingBall");
    settings.setValue("pos", pos());
}

void FloatingBall::restorePosition() {
    QSettings settings("RapidNotes", "FloatingBall");
    if (settings.contains("pos")) {
        move(settings.value("pos").toPoint());
    } else {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeom = screen->geometry();
            move(screenGeom.width() - 150, screenGeom.height() / 2);
        }
    }
}