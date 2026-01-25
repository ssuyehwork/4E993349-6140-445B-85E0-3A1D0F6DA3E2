#include "TimePasteWindow.h"
#include "IconHelper.h"
#include "../core/KeyboardHook.h"
#include <QDateTime>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TimePasteWindow::TimePasteWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle("时间输出工具");
    setFixedSize(350, 300); // 增加边距空间 (320+30, 270+30)

    initUI();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TimePasteWindow::updateDateTime);
    m_timer->start(100);
    updateDateTime();

    // 使用 QueuedConnection 确保钩子回调立即返回，避免阻塞导致按键泄漏
    connect(&KeyboardHook::instance(), &KeyboardHook::digitPressed, this, &TimePasteWindow::onDigitPressed, Qt::QueuedConnection);
}

TimePasteWindow::~TimePasteWindow() {
}

void TimePasteWindow::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(0);

    // 1. Title Bar
    auto* titleBar = new QWidget();
    titleBar->setFixedHeight(35);
    titleBar->setStyleSheet("background-color: transparent;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 5, 10, 5);
    titleLayout->setSpacing(4);

    auto* titleLabel = new QLabel("时间输出工具");
    titleLabel->setStyleSheet("color: #B0B0B0; font-size: 13px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto* btnMin = new QPushButton();
    btnMin->setIcon(IconHelper::getIcon("minimize", "#B0B0B0"));
    btnMin->setFixedSize(28, 28);
    btnMin->setIconSize(QSize(18, 18));
    btnMin->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } "
                          "QPushButton:hover { background-color: rgba(255, 255, 255, 0.1); } "
                          "QPushButton:pressed { background-color: rgba(255, 255, 255, 0.2); }");
    connect(btnMin, &QPushButton::clicked, this, &TimePasteWindow::showMinimized);
    titleLayout->addWidget(btnMin);

    auto* btnClose = new QPushButton();
    btnClose->setIcon(IconHelper::getIcon("close", "#B0B0B0"));
    btnClose->setFixedSize(28, 28);
    btnClose->setIconSize(QSize(18, 18));
    btnClose->setStyleSheet("QPushButton { background: transparent; border: none; border-radius: 4px; } "
                            "QPushButton:hover { background-color: #e74c3c; } "
                            "QPushButton:pressed { background-color: #c0392b; }");
    connect(btnClose, &QPushButton::clicked, this, &TimePasteWindow::hide);
    titleLayout->addWidget(btnClose);

    mainLayout->addWidget(titleBar);

    // 2. Content
    auto* content = new QWidget();
    content->setStyleSheet("background-color: transparent;");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 10, 20, 20);
    layout->setSpacing(10);

    m_dateLabel = new QLabel();
    m_dateLabel->setAlignment(Qt::AlignCenter);
    m_dateLabel->setStyleSheet("color: #B0B0B0; font-size: 16px; padding: 5px;");
    layout->addWidget(m_dateLabel);

    m_timeLabel = new QLabel();
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet("color: #E0E0E0; font-size: 28px; font-weight: bold; padding: 5px; font-family: 'Consolas', 'Monaco', monospace;");
    layout->addWidget(m_timeLabel);

    auto* sep = new QLabel();
    sep->setFixedHeight(2);
    sep->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 transparent, stop:0.5 #555555, stop:1 transparent);");
    layout->addWidget(sep);

    m_buttonGroup = new QButtonGroup(this);
    m_radioPrev = new QRadioButton("退 (往前 N 分钟)");
    m_radioPrev->setChecked(true);
    m_radioPrev->setStyleSheet(getRadioStyle());
    m_buttonGroup->addButton(m_radioPrev, 0);
    layout->addWidget(m_radioPrev);

    m_radioNext = new QRadioButton("进 (往后 N 分钟)");
    m_radioNext->setStyleSheet(getRadioStyle());
    m_buttonGroup->addButton(m_radioNext, 1);
    layout->addWidget(m_radioNext);

    auto* tip = new QLabel("按主键盘数字键 0-9 输出时间");
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("color: #666666; font-size: 11px; padding: 5px;");
    layout->addWidget(tip);

    mainLayout->addWidget(content);
}

QString TimePasteWindow::getRadioStyle() {
    return "QRadioButton { color: #E0E0E0; font-size: 14px; padding: 6px; spacing: 8px; } "
           "QRadioButton::indicator { width: 18px; height: 18px; border-radius: 9px; border: 2px solid #555555; background: #2A2A2A; } "
           "QRadioButton::indicator:checked { background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 #4A9EFF, stop:0.7 #4A9EFF, stop:1 #2A2A2A); border: 2px solid #4A9EFF; } "
           "QRadioButton::indicator:hover { border: 2px solid #4A9EFF; }";
}

void TimePasteWindow::updateDateTime() {
    QDateTime now = QDateTime::currentDateTime();
    m_dateLabel->setText(now.toString("yyyy-MM-dd"));
    m_timeLabel->setText(now.toString("HH:mm:ss"));
}

void TimePasteWindow::onDigitPressed(int digit) {
    if (!isVisible()) return;

    QDateTime target = QDateTime::currentDateTime();
    if (m_radioPrev->isChecked())
        target = target.addSecs(-digit * 60);
    else
        target = target.addSecs(digit * 60);
    
    QString timeStr = target.toString("HH:mm");

    // 异步延迟处理，确保剪贴板设置和模拟粘贴不阻塞主线程/钩子回调
    QTimer::singleShot(50, this, [timeStr]() {
        QApplication::clipboard()->setText(timeStr);

#ifdef Q_OS_WIN
        // 1. 显式释放所有 8 个修饰键 (L/R Ctrl, Shift, Alt, Win)
        INPUT releaseInputs[8];
        memset(releaseInputs, 0, sizeof(releaseInputs));
        BYTE keys[] = { VK_LCONTROL, VK_RCONTROL, VK_LSHIFT, VK_RSHIFT, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN };
        for (int i = 0; i < 8; ++i) {
            releaseInputs[i].type = INPUT_KEYBOARD;
            releaseInputs[i].ki.wVk = keys[i];
            releaseInputs[i].ki.dwFlags = KEYEVENTF_KEYUP;
        }
        SendInput(8, releaseInputs, sizeof(INPUT));

        // 2. 发送 Ctrl + V (带扫描码以提高兼容性)
        INPUT inputs[4];
        memset(inputs, 0, sizeof(inputs));

        // Ctrl 按下
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_LCONTROL;
        inputs[0].ki.wScan = MapVirtualKey(VK_LCONTROL, MAPVK_VK_TO_VSC);

        // V 按下
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 'V';
        inputs[1].ki.wScan = MapVirtualKey('V', MAPVK_VK_TO_VSC);

        // V 抬起
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'V';
        inputs[2].ki.wScan = MapVirtualKey('V', MAPVK_VK_TO_VSC);
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

        // Ctrl 抬起
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_LCONTROL;
        inputs[3].ki.wScan = MapVirtualKey(VK_LCONTROL, MAPVK_VK_TO_VSC);
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(4, inputs, sizeof(INPUT));
#endif
    });
}

void TimePasteWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && event->pos().y() <= 35) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void TimePasteWindow::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    m_dragPos = QPoint();
}

void TimePasteWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void TimePasteWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制主体
    QRectF bodyRect = QRectF(rect()).adjusted(15, 15, -15, -15);
    QPainterPath path;
    path.addRoundedRect(bodyRect, 12, 12);

    painter.fillPath(path, QColor(30, 30, 30, 250));
    painter.setPen(QColor(51, 51, 51)); // #333
    painter.drawPath(path);
}

void TimePasteWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    KeyboardHook::instance().setDigitInterceptEnabled(true);
}

void TimePasteWindow::hideEvent(QHideEvent* event) {
    KeyboardHook::instance().setDigitInterceptEnabled(false);
    QWidget::hideEvent(event);
}
