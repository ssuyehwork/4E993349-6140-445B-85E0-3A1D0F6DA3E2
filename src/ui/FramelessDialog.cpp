#include "FramelessDialog.h"
#include "IconHelper.h"
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QTimer>
#include <QPainter>
#include <QPen>

FramelessDialog::FramelessDialog(const QString& title, QWidget* parent) 
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool) 
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumWidth(320);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(10, 10, 10, 10);

    auto* container = new QWidget(this);
    container->setObjectName("DialogContainer");
    container->setStyleSheet(
        "#DialogContainer {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #333333;"
        "  border-radius: 12px;"
        "}"
    );
    outerLayout->addWidget(container);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 150));
    container->setGraphicsEffect(shadow);

    m_mainLayout = new QVBoxLayout(container);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 标题栏
    auto* titleBar = new QWidget();
    titleBar->setFixedHeight(32);
    titleBar->setStyleSheet("background-color: transparent; border-bottom: 1px solid #2D2D2D;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(12, 0, 0, 0);

    m_titleLabel = new QLabel(title);
    m_titleLabel->setStyleSheet("color: #888; font-size: 11px; font-weight: bold; border: none;");
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    auto* minBtn = new QPushButton("─");
    minBtn->setObjectName("minBtn");
    minBtn->setFixedSize(40, 32);
    minBtn->setAutoDefault(false);
    minBtn->setCursor(Qt::PointingHandCursor);
    minBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: #888888; font-family: 'Segoe UI Symbol', 'Microsoft YaHei'; font-size: 10px; } "
        "QPushButton:hover { background-color: #3E3E42; color: white; }"
    );
    connect(minBtn, &QPushButton::clicked, this, &QDialog::showMinimized);
    titleLayout->addWidget(minBtn);

    auto* closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(40, 32);
    closeBtn->setAutoDefault(false);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; color: #888888; border-top-right-radius: 12px; font-family: 'Segoe UI Symbol', 'Microsoft YaHei'; font-size: 12px; } "
        "QPushButton:hover { background-color: #E81123; color: white; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(closeBtn);

    m_mainLayout->addWidget(titleBar);

    m_contentArea = new QWidget();
    m_mainLayout->addWidget(m_contentArea);
}

void FramelessDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void FramelessDialog::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void FramelessDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
}

// ----------------------------------------------------------------------------
// FramelessInputDialog
// ----------------------------------------------------------------------------
FramelessInputDialog::FramelessInputDialog(const QString& title, const QString& label, 
                                           const QString& initial, QWidget* parent)
    : FramelessDialog(title, parent) 
{
    auto* layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(20, 15, 20, 20);
    layout->setSpacing(12);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet("color: #eee; font-size: 13px;");
    layout->addWidget(lbl);

    m_edit = new QLineEdit(initial);
    m_edit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2D2D2D; border: 1px solid #444; border-radius: 4px;"
        "  padding: 8px; color: white; selection-background-color: #4a90e2;"
        "}"
        "QLineEdit:focus { border: 1px solid #4a90e2; }"
    );
    layout->addWidget(m_edit);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    auto* btnOk = new QPushButton("确定");
    btnOk->setAutoDefault(false);
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setStyleSheet("QPushButton { background-color: #4a90e2; color: white; border: none; border-radius: 4px; padding: 6px 20px; font-weight: bold; } QPushButton:hover { background-color: #357abd; }");
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnOk);

    layout->addLayout(btnLayout);

    m_edit->setFocus();
    m_edit->selectAll();
}

void FramelessInputDialog::showEvent(QShowEvent* event) {
    FramelessDialog::showEvent(event);
    // 增加延迟至 100ms 确保焦点稳定
    QTimer::singleShot(100, m_edit, qOverload<>(&QWidget::setFocus));
}

// ----------------------------------------------------------------------------
// FramelessMessageBox
// ----------------------------------------------------------------------------
FramelessMessageBox::FramelessMessageBox(const QString& title, const QString& text, QWidget* parent)
    : FramelessDialog(title, parent)
{
    auto* layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(25, 20, 25, 25);
    layout->setSpacing(20);

    auto* lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setStyleSheet("color: #eee; font-size: 14px; line-height: 150%;");
    layout->addWidget(lbl);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* btnCancel = new QPushButton("取消");
    btnCancel->setAutoDefault(false);
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setStyleSheet("QPushButton { background-color: transparent; color: #888; border: 1px solid #555; border-radius: 4px; padding: 6px 15px; } QPushButton:hover { color: #eee; border-color: #888; }");
    connect(btnCancel, &QPushButton::clicked, this, [this](){ emit cancelled(); reject(); });
    btnLayout->addWidget(btnCancel);

    auto* btnOk = new QPushButton("确定");
    btnOk->setAutoDefault(false);
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border: none; border-radius: 4px; padding: 6px 20px; font-weight: bold; } QPushButton:hover { background-color: #c0392b; }");
    connect(btnOk, &QPushButton::clicked, this, [this](){ emit confirmed(); accept(); });
    btnLayout->addWidget(btnOk);

    layout->addLayout(btnLayout);
}
