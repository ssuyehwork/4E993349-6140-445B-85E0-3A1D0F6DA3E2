#include "TimePasteWindow.h"
#include "IconHelper.h"
#include "../core/KeyboardHook.h"
#include <QDateTime>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QPushButton>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TimePasteWindow::TimePasteWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle("时间输出工具");
    setFixedSize(320, 270);

    initUI();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TimePasteWindow::updateDateTime);
    m_timer->start(1000);
    updateDateTime();

    connect(&KeyboardHook::instance(), &KeyboardHook::digitPressed, this, &TimePasteWindow::onDigitPressed);
}

TimePasteWindow::~TimePasteWindow() {
}

void TimePasteWindow::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Title Bar
    auto* titleBar = new QWidget();
    titleBar->setFixedHeight(35);
    titleBar->setStyleSheet("background-color: #2D2D2D; border-top-left-radius: 10px; border-top-right-radius: 10px;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 5, 10, 5);

    auto* titleLabel = new QLabel("时间输出工具");
    titleLabel->setStyleSheet("color: #B0B0B0; font-size: 13px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto* btnMin = new QPushButton("—");
    btnMin->setFixedSize(30, 25);
    btnMin->setStyleSheet("QPushButton { background: transparent; color: #B0B0B0; border: none; font-size: 16px; font-weight: bold; } QPushButton:hover { background: #404040; color: #FFFFFF; border-radius: 3px; }");
    connect(btnMin, &QPushButton::clicked, this, &TimePasteWindow::showMinimized);
    titleLayout->addWidget(btnMin);

    auto* btnClose = new QPushButton("✕");
    btnClose->setFixedSize(30, 25);
    btnClose->setStyleSheet("QPushButton { background: transparent; color: #B0B0B0; border: none; font-size: 16px; } QPushButton:hover { background: #E81123; color: #FFFFFF; border-radius: 3px; }");
    connect(btnClose, &QPushButton::clicked, this, &TimePasteWindow::hide);
    titleLayout->addWidget(btnClose);

    mainLayout->addWidget(titleBar);

    // 2. Content
    auto* content = new QWidget();
    content->setStyleSheet("background-color: #2D2D2D; border-bottom-left-radius: 10px; border-bottom-right-radius: 10px;");
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
    m_dateLabel->setText(now.toString("yyyy年MM月dd日"));
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
    QApplication::clipboard()->setText(timeStr);

#ifdef Q_OS_WIN
    INPUT inputs[4];
    memset(inputs, 0, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_LCONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_LCONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
#endif
}

void TimePasteWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void TimePasteWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void TimePasteWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    KeyboardHook::instance().setDigitInterceptEnabled(true);
}

void TimePasteWindow::hideEvent(QHideEvent* event) {
    KeyboardHook::instance().setDigitInterceptEnabled(false);
    QWidget::hideEvent(event);
}
