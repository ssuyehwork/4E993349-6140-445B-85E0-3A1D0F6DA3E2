#include "ScreenshotTool.h"
#include <QApplication>
#include <QScreen>
#include <QPainterPathStroker>
#include <QFileDialog>
#include <QClipboard>
#include <QMenu>
#include <QDateTime>
#include <QInputDialog>
#include <QStyle>
#include <QStyleOption>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==========================================
// IconFactory
// ==========================================
class IconFactory {
public:
    static QIcon createIcon(const QString& type, const QColor& color = Qt::white) {
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(color, 2));
        p.setBrush(Qt::NoBrush);

        if (type == "rect") p.drawRect(6, 8, 20, 16);
        else if (type == "ellipse") p.drawEllipse(6, 8, 20, 16);
        else if (type == "arrow") {
            p.drawLine(6, 26, 24, 8); p.drawLine(14, 8, 24, 8); p.drawLine(24, 18, 24, 8);
        } else if (type == "line") p.drawLine(6, 26, 26, 6);
        else if (type == "pen") {
            QPainterPath path; path.moveTo(6, 26); path.quadTo(12, 10, 26, 6); p.drawPath(path);
        } else if (type == "marker") {
            p.drawEllipse(4, 4, 24, 24);
            p.setFont(QFont("Arial", 12, QFont::Bold)); p.drawText(pix.rect(), Qt::AlignCenter, "1");
        } else if (type == "text") {
            p.setFont(QFont("Times", 18, QFont::Bold)); p.drawText(pix.rect(), Qt::AlignCenter, "T");
        } else if (type == "mosaic") {
            p.setPen(Qt::NoPen); p.setBrush(color);
            p.drawRect(6, 6, 8, 8); p.drawRect(18, 6, 8, 8);
            p.drawRect(6, 18, 8, 8); p.drawRect(18, 18, 8, 8);
        } else if (type == "undo") {
            QPainterPath path; path.moveTo(24, 12); path.arcTo(6, 6, 20, 20, 90, 180); p.drawPath(path);
            QPolygonF head; head << QPointF(24, 12) << QPointF(18, 8) << QPointF(18, 16);
            p.setBrush(color); p.setPen(Qt::NoPen); p.drawPolygon(head);
        } else if (type == "redo") {
            QPainterPath path; path.moveTo(8, 12); path.arcTo(6, 6, 20, 20, 90, -180); p.drawPath(path);
            QPolygonF head; head << QPointF(8, 12) << QPointF(14, 8) << QPointF(14, 16);
            p.setBrush(color); p.setPen(Qt::NoPen); p.drawPolygon(head);
        } else if (type == "save") {
            p.drawRect(6, 6, 20, 20); p.fillRect(10, 6, 12, 8, color); p.drawRect(8, 18, 16, 8);
        } else if (type == "copy") {
            p.drawRect(12, 6, 14, 14);
            p.setBrush(QColor(45, 45, 45)); p.drawRect(6, 12, 14, 14);
            p.setBrush(Qt::NoBrush); p.setPen(QPen(color, 2)); p.drawRect(6, 12, 14, 14);
        } else if (type == "close") {
            p.setPen(QPen(Qt::red, 3)); p.drawLine(8, 8, 24, 24); p.drawLine(24, 8, 8, 24);
        } else if (type == "confirm") {
            p.setPen(QPen(Qt::green, 3)); p.drawLine(6, 16, 12, 24); p.drawLine(12, 24, 26, 8);
        }
        return QIcon(pix);
    }
    
    static QIcon createArrowStyleIcon(ArrowStyle style) {
        QPixmap pix(40, 20);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::white);
        
        QPointF start(5, 10), end(35, 10);
        if (style == ArrowStyle::Solid) {
            p.drawLine(start, end);
            QPolygonF head; head << end << QPointF(25, 5) << QPointF(25, 15); p.drawPolygon(head);
        } else if (style == ArrowStyle::Thin) {
            p.drawLine(start, end); p.drawLine(end, QPointF(28, 4)); p.drawLine(end, QPointF(28, 16));
        } else if (style == ArrowStyle::Outline) {
            p.setBrush(Qt::transparent);
            QPolygonF poly; poly << QPointF(5, 8) << QPointF(25, 8) << QPointF(25, 4) << QPointF(35, 10) << QPointF(25, 16) << QPointF(25, 12) << QPointF(5, 12);
            p.drawPolygon(poly);
        } else if (style == ArrowStyle::Double) {
            p.drawLine(start, end);
            QPolygonF h1; h1 << end << QPointF(28, 5) << QPointF(28, 15);
            QPolygonF h2; h2 << start << QPointF(12, 5) << QPointF(12, 15);
            p.drawPolygon(h1); p.drawPolygon(h2);
        } else if (style == ArrowStyle::DotStart) {
            p.drawLine(start, end); p.drawEllipse(start, 3, 3);
            QPolygonF head; head << end << QPointF(28, 5) << QPointF(28, 15); p.drawPolygon(head);
        }
        return QIcon(pix);
    }
};

// ==========================================
// SelectionInfoBar
// ==========================================
SelectionInfoBar::SelectionInfoBar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFixedSize(180, 28);
    hide();
}
void SelectionInfoBar::updateInfo(const QRect& rect) {
    m_text = QString("%1, %2 | %3 x %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    update();
}
void SelectionInfoBar::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(0, 0, 0, 200)); p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 4, 4);
    p.setPen(Qt::white); p.setFont(QFont("Arial", 9));
    p.drawText(rect(), Qt::AlignCenter, m_text);
}

// ==========================================
// ScreenshotToolbar
// ==========================================
ScreenshotToolbar::ScreenshotToolbar(ScreenshotTool* tool) 
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    m_tool = tool;
    setObjectName("ScreenshotToolbar");
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_AlwaysShowToolTips);
    setMouseTracking(true);

    setStyleSheet(R"(
        #ScreenshotToolbar { background-color: #2D2D2D; border: 1px solid #555; border-radius: 6px; }
        QPushButton { background: transparent; border: none; border-radius: 4px; padding: 4px; }
        QPushButton:hover { background-color: #444; }
        QPushButton:checked { background-color: #007ACC; }
        QWidget#OptionWidget { background-color: #333; border-top: 1px solid #555; border-bottom-left-radius: 6px; border-bottom-right-radius: 6px; }
        QPushButton[colorBtn="true"] { border: 2px solid transparent; border-radius: 2px; }
        QPushButton[colorBtn="true"]:checked { border-color: white; }
        QPushButton[sizeBtn="true"] { background-color: #777; border-radius: 50%; }
        QPushButton[sizeBtn="true"]:checked { background-color: #007ACC; }
        QMenu { background: #2D2D2D; color: white; border: 1px solid #555; }
        QMenu::item:selected { background: #007ACC; }
    )");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0); mainLayout->setSpacing(0);

    QWidget* toolRow = new QWidget;
    auto* layout = new QHBoxLayout(toolRow);
    layout->setContentsMargins(8, 6, 8, 6); layout->setSpacing(8);

    addToolButton(layout, ScreenshotToolType::Rect, "rect", "矩形 (R)");
    addToolButton(layout, ScreenshotToolType::Ellipse, "ellipse", "椭圆 (E)");
    addToolButton(layout, ScreenshotToolType::Arrow, "arrow", "箭头 (A)");
    addToolButton(layout, ScreenshotToolType::Line, "line", "直线 (L)");
    
    auto* line = new QFrame; line->setFrameShape(QFrame::VLine); line->setStyleSheet("color: #666;"); layout->addWidget(line);
    
    addToolButton(layout, ScreenshotToolType::Pen, "pen", "画笔 (P)");
    addToolButton(layout, ScreenshotToolType::Marker, "marker", "记号笔 (M)");
    addToolButton(layout, ScreenshotToolType::Text, "text", "文字 (T)");
    addToolButton(layout, ScreenshotToolType::Mosaic, "mosaic", "马赛克 (Z)");

    layout->addStretch();
    
    addActionButton(layout, "undo", "撤销", [tool]{ tool->undo(); });
    addActionButton(layout, "redo", "重做", [tool]{ tool->redo(); });
    addActionButton(layout, "save", "保存", [tool]{ tool->save(); });
    addActionButton(layout, "copy", "复制", [tool]{ tool->copyToClipboard(); });
    addActionButton(layout, "close", "取消", [tool]{ tool->cancel(); });
    addActionButton(layout, "confirm", "完成", [tool]{ tool->confirm(); });

    mainLayout->addWidget(toolRow);
    createOptionWidget();
    mainLayout->addWidget(m_optionWidget);
}

void ScreenshotToolbar::addToolButton(QBoxLayout* layout, ScreenshotToolType type, const QString& iconType, const QString& tip) {
    auto* btn = new QPushButton();
    btn->setIcon(IconFactory::createIcon(iconType));
    btn->setIconSize(QSize(20, 20));
    btn->setToolTip(tip);
    btn->setCheckable(true); btn->setFixedSize(32, 32);
    layout->addWidget(btn);
    m_buttons[type] = btn;
    connect(btn, &QPushButton::clicked, [this, type]{ selectTool(type); });
}

void ScreenshotToolbar::addActionButton(QBoxLayout* layout, const QString& iconName, const QString& tip, std::function<void()> func) {
    auto* btn = new QPushButton();
    btn->setIcon(IconFactory::createIcon(iconName));
    btn->setIconSize(QSize(20, 20));
    btn->setToolTip(tip); btn->setFixedSize(32, 32);
    layout->addWidget(btn);
    connect(btn, &QPushButton::clicked, func);
}

void ScreenshotToolbar::createOptionWidget() {
    m_optionWidget = new QWidget;
    m_optionWidget->setObjectName("OptionWidget");
    auto* layout = new QHBoxLayout(m_optionWidget);
    layout->setContentsMargins(10, 6, 10, 8); layout->setSpacing(12);

    m_arrowStyleBtn = new QPushButton();
    m_arrowStyleBtn->setFixedSize(40, 24);
    m_arrowStyleBtn->setToolTip("切换箭头样式");
    updateArrowButtonIcon(ArrowStyle::Solid);
    connect(m_arrowStyleBtn, &QPushButton::clicked, this, &ScreenshotToolbar::showArrowMenu);
    layout->addWidget(m_arrowStyleBtn);

    int sizes[] = {2, 4, 8};
    QString sizeTips[] = {"细", "中", "粗"};
    auto* sizeGrp = new QButtonGroup(this);
    for(int i = 0; i < 3; ++i) {
        int s = sizes[i];
        auto* btn = new QPushButton;
        btn->setProperty("sizeBtn", true); btn->setFixedSize(14 + s, 14 + s);
        btn->setToolTip(sizeTips[i]);
        btn->setCheckable(true);
        if(s == 4) btn->setChecked(true);
        layout->addWidget(btn);
        sizeGrp->addButton(btn);
        connect(btn, &QPushButton::clicked, [this, s]{ m_tool->setDrawWidth(s); });
    }

    QList<QColor> colors = {Qt::red, QColor(255, 165, 0), Qt::green, QColor(0, 120, 255), Qt::white};
    QString colorTips[] = {"红色", "橙色", "绿色", "蓝色", "白色"};
    auto* colGrp = new QButtonGroup(this);
    for(int i = 0; i < colors.size(); ++i) {
        const auto& c = colors[i];
        auto* btn = new QPushButton;
        btn->setProperty("colorBtn", true); btn->setFixedSize(20, 20);
        btn->setToolTip(colorTips[i]);
        btn->setStyleSheet(QString("background-color: %1;").arg(c.name()));
        btn->setCheckable(true);
        if(c == Qt::red) btn->setChecked(true);
        layout->addWidget(btn);
        colGrp->addButton(btn);
        connect(btn, &QPushButton::clicked, [this, c]{ m_tool->setDrawColor(c); });
    }
    
    auto* pickerBtn = new QPushButton("...");
    pickerBtn->setFixedSize(20, 20);
    pickerBtn->setToolTip("更多颜色");
    pickerBtn->setStyleSheet("border: 1px solid #777; border-radius: 4px; color: #ccc;");
    layout->addWidget(pickerBtn);
    connect(pickerBtn, &QPushButton::clicked, [this]{
        QColor c = QColorDialog::getColor(Qt::red, this);
        if(c.isValid()) m_tool->setDrawColor(c);
    });

    layout->addStretch();
    m_optionWidget->setVisible(false);
}

void ScreenshotToolbar::showArrowMenu() {
    QMenu menu(this);
    auto addAct = [&](ArrowStyle s, const QString& txt) {
        QAction* act = menu.addAction(IconFactory::createArrowStyleIcon(s), txt);
        connect(act, &QAction::triggered, [this, s]{
            m_tool->setArrowStyle(s);
            updateArrowButtonIcon(s);
        });
    };
    addAct(ArrowStyle::Solid, "实心箭头");
    addAct(ArrowStyle::Thin, "细线箭头");
    addAct(ArrowStyle::Outline, "空心箭头");
    addAct(ArrowStyle::Double, "双向箭头");
    addAct(ArrowStyle::DotStart, "圆点起点");
    menu.exec(mapToGlobal(m_arrowStyleBtn->geometry().bottomLeft()));
}

void ScreenshotToolbar::updateArrowButtonIcon(ArrowStyle style) {
    m_arrowStyleBtn->setIcon(IconFactory::createArrowStyleIcon(style));
    m_arrowStyleBtn->setIconSize(QSize(32, 16));
}

void ScreenshotToolbar::selectTool(ScreenshotToolType type) {
    for(auto* b : m_buttons) b->setChecked(false);
    if(m_buttons.contains(type)) m_buttons[type]->setChecked(true);

    bool showOpts = (type != ScreenshotToolType::None);
    m_optionWidget->setVisible(showOpts);
    
    bool isArrow = (type == ScreenshotToolType::Arrow);
    m_arrowStyleBtn->setVisible(isArrow);

    m_tool->setTool(type);
    adjustSize();
    m_tool->updateToolbarPosition();
}

void ScreenshotToolbar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}
void ScreenshotToolbar::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging) move(event->globalPosition().toPoint() - m_dragPosition);
    else QWidget::mouseMoveEvent(event);
}
void ScreenshotToolbar::mouseReleaseEvent(QMouseEvent *) { m_isDragging = false; }
void ScreenshotToolbar::paintEvent(QPaintEvent *) {
    QStyleOption opt; opt.initFrom(this);
    QPainter p(this); style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

// ==========================================
// ScreenshotTool Implementation
// ==========================================
ScreenshotTool::ScreenshotTool(QWidget* parent) 
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    
    // !!! 核心修复：防止关闭此窗口时Qt认为主程序也该退出了 !!!
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动释放内存

    setWindowState(Qt::WindowFullScreen);
    setMouseTracking(true);
    
    m_screenPixmap = QGuiApplication::primaryScreen()->grabWindow(0);
    
    QImage img = m_screenPixmap.toImage();
    QImage small = img.scaled(img.width()/15, img.height()/15, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    m_mosaicPixmap = QPixmap::fromImage(small.scaled(img.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation));

    m_toolbar = new ScreenshotToolbar(this);
    m_toolbar->hide();
    
    m_infoBar = new SelectionInfoBar(this);
    
    m_textInput = new QLineEdit(this);
    m_textInput->hide();
    m_textInput->setFrame(false);
    connect(m_textInput, &QLineEdit::editingFinished, this, &ScreenshotTool::commitTextInput);
}

void ScreenshotTool::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    detectWindows();
}

void ScreenshotTool::cancel() {
    emit screenshotCanceled();
    if (m_toolbar) m_toolbar->close();
    close(); // 因为设置了 WA_QuitOnClose = false，这里只会关闭截图层，不会退出主程序
}

void ScreenshotTool::drawArrow(QPainter& p, const QPointF& start, const QPointF& end, const DrawingAnnotation& ann) {
    double angle = std::atan2(end.y() - start.y(), end.x() - start.x());
    double arrowSize = 15 + ann.strokeWidth;
    
    if (ann.arrowStyle != ArrowStyle::Outline) p.drawLine(start, end);
    
    auto getTriPoints = [&](QPointF tip, double ang) {
        QPointF p1 = tip - QPointF(arrowSize * std::cos(ang - M_PI/6), arrowSize * std::sin(ang - M_PI/6));
        QPointF p2 = tip - QPointF(arrowSize * std::cos(ang + M_PI/6), arrowSize * std::sin(ang + M_PI/6));
        return QPolygonF() << tip << p1 << p2;
    };

    if (ann.arrowStyle == ArrowStyle::Solid) {
        p.setBrush(ann.color); p.setPen(Qt::NoPen);
        p.drawPolygon(getTriPoints(end, angle));
    } else if (ann.arrowStyle == ArrowStyle::Thin) {
        p.drawLine(end, end - QPointF(arrowSize * std::cos(angle - M_PI/6), arrowSize * std::sin(angle - M_PI/6)));
        p.drawLine(end, end - QPointF(arrowSize * std::cos(angle + M_PI/6), arrowSize * std::sin(angle + M_PI/6)));
    } else if (ann.arrowStyle == ArrowStyle::Outline) {
        QPointF dir(std::cos(angle), std::sin(angle));
        QPointF norm(-dir.y(), dir.x());
        double w = ann.strokeWidth / 2.0 + 2; 
        
        QPointF neck = end - dir * (arrowSize * 0.8);
        QPointF p1 = neck + norm * w;
        QPointF p2 = start + norm * w;
        QPointF p3 = start - norm * w;
        QPointF p4 = neck - norm * w;
        QPointF head1 = neck + norm * (arrowSize * 0.5); 
        QPointF head2 = neck - norm * (arrowSize * 0.5);
        
        QPolygonF poly; poly << end << head1 << p1 << p2 << p3 << p4 << head2;
        p.setBrush(Qt::transparent); p.setPen(QPen(ann.color, 2)); p.drawPolygon(poly);
    } else if (ann.arrowStyle == ArrowStyle::Double) {
        p.setBrush(ann.color); p.setPen(Qt::NoPen);
        p.drawPolygon(getTriPoints(end, angle));
        p.drawPolygon(getTriPoints(start, angle + M_PI));
    } else if (ann.arrowStyle == ArrowStyle::DotStart) {
        p.setBrush(ann.color); p.setPen(Qt::NoPen);
        p.drawEllipse(start, 4 + ann.strokeWidth, 4 + ann.strokeWidth);
        p.drawPolygon(getTriPoints(end, angle));
    }
}

void ScreenshotTool::drawAnnotation(QPainter& p, const DrawingAnnotation& ann) {
    if (ann.points.size() < 2 && ann.type != ScreenshotToolType::Text && ann.type != ScreenshotToolType::Marker) return;
    p.setPen(QPen(ann.color, ann.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    if (ann.type == ScreenshotToolType::Rect) p.drawRect(QRectF(ann.points[0], ann.points[1]).normalized());
    else if (ann.type == ScreenshotToolType::Ellipse) p.drawEllipse(QRectF(ann.points[0], ann.points[1]).normalized());
    else if (ann.type == ScreenshotToolType::Line) p.drawLine(ann.points[0], ann.points[1]);
    else if (ann.type == ScreenshotToolType::Arrow) drawArrow(p, ann.points[0], ann.points[1], ann);
    else if (ann.type == ScreenshotToolType::Pen) {
        QPainterPath path; path.moveTo(ann.points[0]);
        for(int i=1; i<ann.points.size(); ++i) path.lineTo(ann.points[i]);
        p.drawPath(path);
    } else if (ann.type == ScreenshotToolType::Marker) {
        p.setBrush(ann.color); p.setPen(Qt::NoPen);
        int r = 12 + ann.strokeWidth;
        p.drawEllipse(ann.points[0], r, r);
        p.setPen(Qt::white); p.setFont(QFont("Arial", r, QFont::Bold));
        p.drawText(QRectF(ann.points[0].x()-r, ann.points[0].y()-r, r*2, r*2), Qt::AlignCenter, ann.text);
    } else if (ann.type == ScreenshotToolType::Mosaic) {
        QPainterPath path; path.moveTo(ann.points[0]);
        for(int i=1; i<ann.points.size(); ++i) path.lineTo(ann.points[i]);
        QPainterPathStroker s; s.setWidth(ann.strokeWidth * 6);
        p.save(); p.setClipPath(s.createStroke(path));
        p.drawPixmap(0, 0, m_mosaicPixmap); p.restore();
    } else if (ann.type == ScreenshotToolType::Text && !ann.text.isEmpty()) {
        p.setPen(ann.color); p.setFont(QFont("Microsoft YaHei", 12 + ann.strokeWidth*2, QFont::Bold));
        p.drawText(ann.points[0], ann.text);
    }
}

void ScreenshotTool::mousePressEvent(QMouseEvent* e) {
    if(m_textInput->isVisible() && !m_textInput->geometry().contains(e->pos())) commitTextInput();
    if(e->button() != Qt::LeftButton) return;

    if (m_state == ScreenshotState::Selecting) {
        m_dragHandle = -1;
        m_startPoint = e->pos();
        m_endPoint = m_startPoint;
        m_isDragging = true;
        m_toolbar->hide();
        m_infoBar->hide();
    } else {
        if (selectionRect().contains(e->pos()) && m_currentTool != ScreenshotToolType::None) {
            if (m_currentTool == ScreenshotToolType::Text) { showTextInput(e->pos()); return; }
            m_isDrawing = true; m_toolbar->hide(); 
            m_currentAnnotation = {m_currentTool, {e->pos()}, m_currentColor, "", m_currentStrokeWidth, LineStyle::Solid, m_currentArrowStyle};
            if(m_currentTool == ScreenshotToolType::Marker) {
                int c = 1; for(auto& a: m_annotations) if(a.type == ScreenshotToolType::Marker) c++;
                m_currentAnnotation.text = QString::number(c);
            }
        } else if (getHandleAt(e->pos()) != -1) {
            m_dragHandle = getHandleAt(e->pos()); m_isDragging = true; m_toolbar->hide();
        } else if (selectionRect().contains(e->pos())) {
             m_dragHandle = 8; m_startPoint = e->pos() - m_startPoint; m_isDragging = true; m_toolbar->hide();
        } else {
             m_state = ScreenshotState::Selecting; m_startPoint = e->pos(); m_endPoint = m_startPoint; m_isDragging = true; m_toolbar->hide();
        }
    }
    update();
}

void ScreenshotTool::mouseMoveEvent(QMouseEvent* e) {
    if (m_state == ScreenshotState::Selecting && !m_isDragging) {
        QRect smallest;
        long long minArea = -1;
        for (const QRect& r : m_detectedRects) {
            if (r.contains(e->pos())) {
                long long area = (long long)r.width() * r.height();
                if (minArea == -1 || area < minArea) {
                    minArea = area;
                    smallest = r;
                }
            }
        }
        if (m_highlightedRect != smallest) {
            m_highlightedRect = smallest;
            update();
        }
    }

    if (m_isDragging) {
        if (m_state == ScreenshotState::Selecting || m_dragHandle == -1) m_endPoint = e->pos();
        else if (m_dragHandle == 8) {
            QRect r = selectionRect(); r.moveTo(e->pos() - m_startPoint);
            m_startPoint = r.topLeft(); m_endPoint = r.bottomRight();
        } else {
            // 简单处理8点拉伸
            QPoint p = e->pos();
            if(m_dragHandle==0) m_startPoint = p;
            else if(m_dragHandle==1) m_startPoint.setY(p.y());
            else if(m_dragHandle==2) { m_startPoint.setY(p.y()); m_endPoint.setX(p.x()); }
            else if(m_dragHandle==3) m_endPoint.setX(p.x());
            else if(m_dragHandle==4) m_endPoint = p;
            else if(m_dragHandle==5) m_endPoint.setY(p.y());
            else if(m_dragHandle==6) { m_endPoint.setY(p.y()); m_startPoint.setX(p.x()); }
            else if(m_dragHandle==7) m_startPoint.setX(p.x());
        }
        
        if (!selectionRect().isEmpty()) {
            m_infoBar->updateInfo(selectionRect());
            m_infoBar->show();
            m_infoBar->move(selectionRect().left(), selectionRect().top() - 35);
            m_infoBar->raise();
        }
    } else if (m_isDrawing) {
        if (m_currentTool == ScreenshotToolType::Arrow || m_currentTool == ScreenshotToolType::Line || 
            m_currentTool == ScreenshotToolType::Rect || m_currentTool == ScreenshotToolType::Ellipse) {
            if (m_currentAnnotation.points.size() > 1) m_currentAnnotation.points[1] = e->pos();
            else m_currentAnnotation.points.append(e->pos());
        } else {
            m_currentAnnotation.points.append(e->pos());
        }
    }
    updateCursor(e->pos());
    update();
}

void ScreenshotTool::mouseReleaseEvent(QMouseEvent* e) {
    if (m_isDrawing) {
        m_isDrawing = false;
        m_annotations.append(m_currentAnnotation);
        m_redoStack.clear();
    } else if (m_isDragging) {
        m_isDragging = false;
        if (m_state == ScreenshotState::Selecting) {
            // 点击判定：如果移动距离很小，且当前在高亮矩形上，则自动吸附
            if ((e->pos() - m_startPoint).manhattanLength() < 5) {
                if (!m_highlightedRect.isEmpty()) {
                    m_startPoint = m_highlightedRect.topLeft();
                    m_endPoint = m_highlightedRect.bottomRight();
                }
            }
            if (selectionRect().isValid() && selectionRect().width() > 2 && selectionRect().height() > 2) {
                m_state = ScreenshotState::Editing;
            }
        }
    }

    if (m_state == ScreenshotState::Editing) {
        updateToolbarPosition();
        m_toolbar->show();
        m_infoBar->updateInfo(selectionRect());
        m_infoBar->show();
        m_infoBar->move(selectionRect().left(), selectionRect().top() - 35);
    }
    update();
}

void ScreenshotTool::updateToolbarPosition() {
    QRect r = selectionRect();
    if(r.isEmpty()) return;
    int x = r.right() - m_toolbar->width();
    int y = r.bottom() + 10;
    if (x < 0) x = 0;
    if (y + m_toolbar->height() > height()) y = r.top() - m_toolbar->height() - 10;
    m_toolbar->move(x, y);
}

void ScreenshotTool::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.drawPixmap(0,0,m_screenPixmap);
    QRect r = selectionRect();
    QPainterPath path; path.addRect(rect());
    if(r.isValid()) path.addRect(r);
    p.fillPath(path, QColor(0,0,0,120));

    if (m_state == ScreenshotState::Selecting && !m_isDragging && !m_highlightedRect.isEmpty()) {
        p.setPen(QPen(QColor(0, 120, 255, 200), 2));
        p.setBrush(QColor(0, 120, 255, 30));
        p.drawRect(m_highlightedRect);
    }

    if(r.isValid()) {
        p.setPen(QPen(QColor(0, 120, 255), 2)); p.drawRect(r);
        auto h = getHandleRects();
        p.setBrush(Qt::white);
        for(auto& hr : h) p.drawEllipse(hr);
        p.setClipRect(r);
        for(auto& a : m_annotations) drawAnnotation(p, a);
        if(m_isDrawing) drawAnnotation(p, m_currentAnnotation);
    }
}

void ScreenshotTool::setTool(ScreenshotToolType t) { if(m_textInput->isVisible()) commitTextInput(); m_currentTool = t; }
void ScreenshotTool::setDrawColor(const QColor& c) { m_currentColor = c; }
void ScreenshotTool::setDrawWidth(int w) { m_currentStrokeWidth = w; }
void ScreenshotTool::setArrowStyle(ArrowStyle s) { m_currentArrowStyle = s; }
void ScreenshotTool::undo() { if(!m_annotations.isEmpty()) { m_redoStack.append(m_annotations.takeLast()); update(); } }
void ScreenshotTool::redo() { if(!m_redoStack.isEmpty()) { m_annotations.append(m_redoStack.takeLast()); update(); } }
void ScreenshotTool::copyToClipboard() { QApplication::clipboard()->setImage(generateFinalImage()); cancel(); }
void ScreenshotTool::save() { 
    QString f = QFileDialog::getSaveFileName(this, "Save", "cap.png", "PNG(*.png)");
    if(!f.isEmpty()) generateFinalImage().save(f);
    cancel();
}
void ScreenshotTool::confirm() { emit screenshotCaptured(generateFinalImage()); cancel(); }
QRect ScreenshotTool::selectionRect() const { return QRect(m_startPoint, m_endPoint).normalized(); }
QList<QRect> ScreenshotTool::getHandleRects() const {
    QRect r = selectionRect(); QList<QRect> l;
    if(r.isEmpty()) return l;
    int s = 8;
    l << QRect(r.left()-s/2, r.top()-s/2, s, s) << QRect(r.center().x()-s/2, r.top()-s/2, s, s)
      << QRect(r.right()-s/2, r.top()-s/2, s, s) << QRect(r.right()-s/2, r.center().y()-s/2, s, s)
      << QRect(r.right()-s/2, r.bottom()-s/2, s, s) << QRect(r.center().x()-s/2, r.bottom()-s/2, s, s)
      << QRect(r.left()-s/2, r.bottom()-s/2, s, s) << QRect(r.left()-s/2, r.center().y()-s/2, s, s);
    return l;
}
int ScreenshotTool::getHandleAt(const QPoint& p) const {
    auto l = getHandleRects();
    for(int i=0; i<l.size(); ++i) if(l[i].contains(p)) return i;
    return -1;
}
void ScreenshotTool::updateCursor(const QPoint& p) {
    if(selectionRect().contains(p) && m_currentTool != ScreenshotToolType::None) setCursor(Qt::CrossCursor);
    else setCursor(Qt::ArrowCursor);
}
void ScreenshotTool::showTextInput(const QPoint& p) {
    m_textInput->move(p); m_textInput->resize(100, 30); m_textInput->show(); m_textInput->setFocus();
}
void ScreenshotTool::commitTextInput() {
    if(m_textInput->text().isEmpty()) { m_textInput->hide(); return; }
    m_annotations.append({ScreenshotToolType::Text, {m_textInput->pos()}, m_currentColor, m_textInput->text(), m_currentStrokeWidth});
    m_textInput->hide(); m_textInput->clear(); update();
}
#ifdef Q_OS_WIN
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
    QList<QRect>* rects = reinterpret_cast<QList<QRect>*>(lParam);
    if (IsWindowVisible(hwnd)) {
        RECT rect;
        if (GetWindowRect(hwnd, &rect)) {
            QRect qr(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
            if (qr.width() > 5 && qr.height() > 5) {
                rects->append(qr);
            }
        }
    }
    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    QList<QRect>* rects = reinterpret_cast<QList<QRect>*>(lParam);
    if (IsWindowVisible(hwnd)) {
        // 排除掉一些无效窗口，比如带缩放动画的 DWM 阴影等
        int cloaked = 0;
        DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        if (cloaked) return TRUE;

        RECT rect;
        if (GetWindowRect(hwnd, &rect)) {
            QRect qr(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
            if (qr.width() > 10 && qr.height() > 10) {
                rects->append(qr);
                // 递归查找子控件
                EnumChildWindows(hwnd, EnumChildProc, lParam);
            }
        }
    }
    return TRUE;
}
#endif

void ScreenshotTool::detectWindows() {
    m_detectedRects.clear();
#ifdef Q_OS_WIN
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&m_detectedRects));
    // 转换为本地坐标以支持多显示器
    QPoint offset = mapToGlobal(QPoint(0,0));
    for(QRect& r : m_detectedRects) {
        r.translate(-offset);
    }
#endif
}

QImage ScreenshotTool::generateFinalImage() {
    QRect r = selectionRect();
    QPixmap p = m_screenPixmap.copy(r);
    QPainter painter(&p);
    painter.translate(-r.topLeft());
    for(auto& a : m_annotations) drawAnnotation(painter, a);
    return p.toImage();
}
void ScreenshotTool::keyPressEvent(QKeyEvent* e) {
    if(e->key() == Qt::Key_Escape) {
        cancel();
    } else if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) {
        if(m_state == ScreenshotState::Editing) copyToClipboard();
    } else if (e->modifiers() == Qt::ControlModifier && e->key() == Qt::Key_Z) {
        undo();
    } else if (e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) && e->key() == Qt::Key_Z) {
        redo();
    }
}
void ScreenshotTool::mouseDoubleClickEvent(QMouseEvent* e) { if(selectionRect().contains(e->pos())) confirm(); }