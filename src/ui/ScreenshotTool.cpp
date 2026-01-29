#include "ScreenshotTool.h"
#include "IconHelper.h"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QFileDialog>
#include <QClipboard>
#include <QMimeData>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QToolTip>
#include <QKeyEvent>
#include <QDateTime>
#include <functional>
#include <QBuffer>
#include <QInputDialog>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- ScreenshotToolbar 实现 ---
ScreenshotToolbar::ScreenshotToolbar(ScreenshotTool* tool) 
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    m_tool = tool;
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("ScreenshotToolbar");
    setStyleSheet(
        "#ScreenshotToolbar { background-color: #1E1E1E; border: 1px solid #333333; border-radius: 6px; }"
        "QPushButton { border: none; border-radius: 4px; padding: 6px; background: transparent; min-width: 28px; min-height: 28px; }"
        "QPushButton:hover { background-color: #3e3e42; }"
        "QPushButton:checked { background-color: #007acc; }"
    );

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(6);

    addToolButton("screenshot_rect", "矩形", ScreenshotToolType::Rect);
    addToolButton("screenshot_ellipse", "椭圆", ScreenshotToolType::Ellipse);
    addToolButton("screenshot_arrow", "箭头", ScreenshotToolType::Arrow);
    addToolButton("screenshot_pen", "画笔", ScreenshotToolType::Pen);
    addToolButton("screenshot_marker", "序号", ScreenshotToolType::Marker);
    addToolButton("screenshot_text", "文字", ScreenshotToolType::Text);
    addToolButton("screenshot_mosaic", "马赛克", ScreenshotToolType::Mosaic);

    auto* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setStyleSheet("background-color: #444;");
    line->setFixedWidth(1);
    layout->addWidget(line);

    addActionButton("undo", "撤销 (Ctrl+Z)", [tool]{ tool->undo(); });
    addActionButton("save", "保存 (Ctrl+S)", [tool]{ tool->save(); });
    addActionButton("close", "退出 (Esc/Ctrl+W)", [tool]{ tool->cancel(); }, "#e74c3c");
    addActionButton("screenshot_confirm", "完成 (双击或回车)", [tool]{ tool->confirm(); }, "#2ecc71");

    adjustSize();
}

void ScreenshotToolbar::addToolButton(const QString& icon, const QString& tip, ScreenshotToolType type) {
    auto* btn = new QPushButton(IconHelper::getIcon(icon, "#ccc", 20), "");
    btn->setCheckable(true);
    btn->setAutoDefault(false);
    btn->setToolTip(tip);
    layout()->addWidget(btn);
    m_buttons[type] = btn;
    connect(btn, &QPushButton::clicked, [this, type]{
        for (auto* b : m_buttons.values()) b->setChecked(false);
        m_buttons[type]->setChecked(true);
        m_tool->setTool(type);
    });
}

void ScreenshotToolbar::addActionButton(const QString& icon, const QString& tip, std::function<void()> func, const QString& color) {
    auto* btn = new QPushButton(IconHelper::getIcon(icon, color, 20), "");
    btn->setAutoDefault(false);
    btn->setToolTip(tip);
    layout()->addWidget(btn);
    connect(btn, &QPushButton::clicked, func);
}

// --- ScreenshotTool 实现 ---

ScreenshotTool::ScreenshotTool(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowState(Qt::WindowFullScreen);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    // 抓取全屏
    QScreen *screen = QGuiApplication::primaryScreen();
    m_screenPixmap = screen->grabWindow(0);

    // 生成马赛克背景
    QImage mosaicImg = m_screenPixmap.toImage();
    QImage small = mosaicImg.scaled(mosaicImg.width() / 10, mosaicImg.height() / 10, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    m_mosaicPixmap = QPixmap::fromImage(small.scaled(mosaicImg.width(), mosaicImg.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation));

    m_toolbar = new ScreenshotToolbar(this);
    m_toolbar->hide();
}

ScreenshotTool::~ScreenshotTool() {
    if (m_toolbar) m_toolbar->deleteLater();
}

void ScreenshotTool::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. 绘制底图
    painter.drawPixmap(0, 0, m_screenPixmap);

    // 2. 绘制遮罩
    QRect selRect = selectionRect();
    QPainterPath path;
    path.addRect(rect());
    if (selRect.isValid()) {
        path.addRect(selRect);
    }
    painter.fillPath(path, QColor(0, 0, 0, 100));

    if (!selRect.isValid()) return;

    // 3. 绘制选区边框
    painter.setPen(QPen(QColor(0, 122, 204), 2));
    painter.drawRect(selRect);

    // 4. 绘制控制点 (编辑模式或选择模式且不在拖动中)
    if (m_state == ScreenshotState::Editing || (m_state == ScreenshotState::Selecting && !m_isDragging)) {
        auto handles = getHandleRects();
        painter.setBrush(QColor(0, 122, 204));
        for (const auto& r : handles) {
            painter.drawRect(r);
        }
    }

    // 5. 绘制标注
    painter.save();
    painter.setClipRect(selRect);
    for (const auto& ann : m_annotations) {
        drawAnnotation(painter, ann);
    }
    if (m_isDrawing) {
        drawAnnotation(painter, m_currentAnnotation);
    }
    painter.restore();
}

void ScreenshotTool::mousePressEvent(QMouseEvent* event) {
    if (m_state == ScreenshotState::Selecting) {
        m_dragHandle = getHandleAt(event->pos());
        if (m_dragHandle == -1) {
            m_startPoint = event->pos();
            m_endPoint = m_startPoint;
            m_state = ScreenshotState::Selecting;
        }
        m_isDragging = true;
    } else if (m_state == ScreenshotState::Editing) {
        m_dragHandle = getHandleAt(event->pos());
        if (m_dragHandle != -1) {
            m_isDragging = true;
        } else if (selectionRect().contains(event->pos())) {
            if (m_currentTool != ScreenshotToolType::None) {
                // 开始绘制标注
                m_isDrawing = true;
                m_currentAnnotation.type = m_currentTool;
                m_currentAnnotation.color = QColor(231, 76, 60); // 默认红色
                m_currentAnnotation.strokeWidth = 3;
                m_currentAnnotation.points.clear();
                m_currentAnnotation.points.append(event->pos());
                
                if (m_currentTool == ScreenshotToolType::Marker) {
                    m_currentAnnotation.text = QString::number(m_annotations.size() + 1);
                } else if (m_currentTool == ScreenshotToolType::Text) {
                    bool ok;
                    QString text = QInputDialog::getText(this, "输入文本", "内容:", QLineEdit::Normal, "", &ok);
                    if (ok && !text.isEmpty()) {
                        m_currentAnnotation.text = text;
                        m_annotations.append(m_currentAnnotation);
                    }
                    m_isDrawing = false;
                    update();
                    return;
                }
            } else {
                // 移动选区
                m_dragHandle = 8;
                m_startPoint = event->pos() - m_startPoint; // 借用 startPoint 存 offset
                m_isDragging = true;
            }
        }
    }
    update();
}

void ScreenshotTool::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging) {
        if (m_dragHandle == -1) {
            m_endPoint = event->pos();
        } else if (m_dragHandle == 8) {
             QPoint offset = event->pos() - m_startPoint;
             QRect r = selectionRect();
             r.moveTo(offset);
             m_startPoint = r.topLeft();
             m_endPoint = r.bottomRight();
        } else {
            // 全 8 点拉伸逻辑
            QPoint p = event->pos();
            if (m_dragHandle == 0) { m_startPoint = p; }
            else if (m_dragHandle == 1) { m_startPoint.setY(p.y()); }
            else if (m_dragHandle == 2) { m_startPoint.setY(p.y()); m_endPoint.setX(p.x()); }
            else if (m_dragHandle == 3) { m_endPoint.setX(p.x()); }
            else if (m_dragHandle == 4) { m_endPoint = p; }
            else if (m_dragHandle == 5) { m_endPoint.setY(p.y()); }
            else if (m_dragHandle == 6) { m_endPoint.setY(p.y()); m_startPoint.setX(p.x()); }
            else if (m_dragHandle == 7) { m_startPoint.setX(p.x()); }
        }
        
        if (m_toolbar->isVisible()) {
             QRect r = selectionRect();
             m_toolbar->move(r.left(), r.bottom() + 10);
        }
    } else if (m_isDrawing) {
        if (m_currentTool == ScreenshotToolType::Pen || m_currentTool == ScreenshotToolType::Mosaic) {
            m_currentAnnotation.points.append(event->pos());
        } else {
            if (m_currentAnnotation.points.size() > 1) m_currentAnnotation.points[1] = event->pos();
            else m_currentAnnotation.points.append(event->pos());
        }
    }
    
    updateCursor(event->pos());
    update();
}

void ScreenshotTool::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isDragging) {
        m_isDragging = false;
        m_dragHandle = -1;
        if (selectionRect().width() > 10 && selectionRect().height() > 10) {
            m_state = ScreenshotState::Editing;
            m_toolbar->show();
            QRect r = selectionRect();
            m_toolbar->move(r.left(), r.bottom() + 10);
        }
    } else if (m_isDrawing) {
        m_isDrawing = false;
        m_annotations.append(m_currentAnnotation);
    }
    update();
}

void ScreenshotTool::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) cancel();
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) confirm();
    else if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Z) undo();
        else if (event->key() == Qt::Key_S) save();
        else if (event->key() == Qt::Key_W) cancel();
    }
}

void ScreenshotTool::mouseDoubleClickEvent(QMouseEvent* event) {
    if (selectionRect().contains(event->pos())) {
        confirm();
    }
}

void ScreenshotTool::setTool(ScreenshotToolType type) {
    m_currentTool = type;
    update();
}

void ScreenshotTool::undo() {
    if (!m_annotations.isEmpty()) {
        m_annotations.removeLast();
        update();
    }
}

void ScreenshotTool::save() {
    QString fileName = QFileDialog::getSaveFileName(this, "保存截图", 
        "Screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png",
        "Images (*.png *.jpg)");
    if (!fileName.isEmpty()) {
        generateFinalImage().save(fileName);
        close();
    }
}

void ScreenshotTool::confirm() {
    QImage final = generateFinalImage();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setImage(final);
    emit screenshotCaptured(final);
    close();
}

void ScreenshotTool::cancel() {
    close();
}

QRect ScreenshotTool::selectionRect() const {
    return QRect(m_startPoint, m_endPoint).normalized();
}

QList<QRect> ScreenshotTool::getHandleRects() const {
    QRect r = selectionRect();
    int s = 6;
    QList<QRect> handles;
    handles << QRect(r.left()-s/2, r.top()-s/2, s, s);
    handles << QRect(r.center().x()-s/2, r.top()-s/2, s, s);
    handles << QRect(r.right()-s/2, r.top()-s/2, s, s);
    handles << QRect(r.right()-s/2, r.center().y()-s/2, s, s);
    handles << QRect(r.right()-s/2, r.bottom()-s/2, s, s);
    handles << QRect(r.center().x()-s/2, r.bottom()-s/2, s, s);
    handles << QRect(r.left()-s/2, r.bottom()-s/2, s, s);
    handles << QRect(r.left()-s/2, r.center().y()-s/2, s, s);
    return handles;
}

int ScreenshotTool::getHandleAt(const QPoint& pos) const {
    auto handles = getHandleRects();
    for (int i = 0; i < handles.size(); ++i) {
        if (handles[i].contains(pos)) return i;
    }
    return -1;
}

void ScreenshotTool::updateCursor(const QPoint& pos) {
    int h = getHandleAt(pos);
    if (h != -1) {
        if (h == 0 || h == 4) setCursor(Qt::SizeFDiagCursor);
        else if (h == 2 || h == 6) setCursor(Qt::SizeBDiagCursor);
        else if (h == 1 || h == 5) setCursor(Qt::SizeVerCursor);
        else setCursor(Qt::SizeHorCursor);
    } else if (m_state == ScreenshotState::Editing && selectionRect().contains(pos)) {
        if (m_currentTool == ScreenshotToolType::None) setCursor(Qt::SizeAllCursor);
        else setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
}

void ScreenshotTool::drawAnnotation(QPainter& painter, const DrawingAnnotation& ann) {
    if (ann.points.isEmpty()) return;

    painter.setPen(QPen(ann.color, ann.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (ann.type == ScreenshotToolType::Rect) {
        if (ann.points.size() < 2) return;
        painter.drawRect(QRectF(ann.points[0], ann.points[1]).normalized());
    } else if (ann.type == ScreenshotToolType::Ellipse) {
        if (ann.points.size() < 2) return;
        painter.drawEllipse(QRectF(ann.points[0], ann.points[1]).normalized());
    } else if (ann.type == ScreenshotToolType::Arrow) {
        if (ann.points.size() < 2) return;
        QPointF p1 = ann.points[0];
        QPointF p2 = ann.points[1];
        painter.drawLine(p1, p2);
        double angle = std::atan2(p2.y() - p1.y(), p2.x() - p1.x());
        double arrowLen = 15;
        QPointF a1 = p2 - QPointF(arrowLen * std::cos(angle - M_PI/6), arrowLen * std::sin(angle - M_PI/6));
        QPointF a2 = p2 - QPointF(arrowLen * std::cos(angle + M_PI/6), arrowLen * std::sin(angle + M_PI/6));
        painter.drawPolygon(QPolygonF() << p2 << a1 << a2);
    } else if (ann.type == ScreenshotToolType::Pen) {
        QPainterPath p;
        p.moveTo(ann.points[0]);
        for (int i = 1; i < ann.points.size(); ++i) p.lineTo(ann.points[i]);
        painter.drawPath(p);
    } else if (ann.type == ScreenshotToolType::Marker) {
        QPointF p = ann.points[0];
        painter.setBrush(ann.color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(p, 12, 12);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.drawText(QRectF(p.x()-12, p.y()-12, 24, 24), Qt::AlignCenter, ann.text);
    } else if (ann.type == ScreenshotToolType::Mosaic) {
        painter.save();
        QPainterPath p;
        p.moveTo(ann.points[0]);
        for (int i = 1; i < ann.points.size(); ++i) p.lineTo(ann.points[i]);
        
        QPainterPathStroker stroker;
        stroker.setWidth(20);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        QPainterPath strokePath = stroker.createStroke(p);
        
        painter.setClipPath(strokePath);
        painter.drawPixmap(0, 0, m_mosaicPixmap);
        painter.restore();
    } else if (ann.type == ScreenshotToolType::Text) {
        if (!ann.text.isEmpty()) {
            painter.setPen(ann.color);
            painter.setFont(QFont("Microsoft YaHei", 14));
            painter.drawText(ann.points[0], ann.text);
        }
    }
}

QImage ScreenshotTool::generateFinalImage() {
    QRect r = selectionRect();
    QPixmap result = m_screenPixmap.copy(r);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(-r.topLeft());
    for (const auto& ann : m_annotations) {
        drawAnnotation(painter, ann);
    }
    return result.toImage();
}
