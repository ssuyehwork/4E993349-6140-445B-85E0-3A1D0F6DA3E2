#include "FramelessDialog.h"
#include "IconHelper.h"
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
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
        "  border-radius: 10px;"
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
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet("background-color: #252526; border-top-left-radius: 10px; border-top-right-radius: 10px;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 0, 8, 0);

    m_titleLabel = new QLabel(title);
    m_titleLabel->setStyleSheet("color: #aaa; font-size: 12px; font-weight: bold; border: none;");
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();

    auto* closeBtn = new QPushButton();
    closeBtn->setFixedSize(24, 24);
    closeBtn->setIcon(IconHelper::getIcon("close", "#888888", 14));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("QPushButton { border: none; border-radius: 4px; } QPushButton:hover { background-color: #e74c3c; }");
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
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setStyleSheet("QPushButton { background-color: #4a90e2; color: white; border: none; border-radius: 4px; padding: 6px 20px; font-weight: bold; } QPushButton:hover { background-color: #357abd; }");
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnOk);

    layout->addLayout(btnLayout);

    m_edit->setFocus();
    m_edit->selectAll();
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
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setStyleSheet("QPushButton { background-color: transparent; color: #888; border: 1px solid #555; border-radius: 4px; padding: 6px 15px; } QPushButton:hover { color: #eee; border-color: #888; }");
    connect(btnCancel, &QPushButton::clicked, this, [this](){ emit cancelled(); reject(); });
    btnLayout->addWidget(btnCancel);

    auto* btnOk = new QPushButton("确定");
    btnOk->setCursor(Qt::PointingHandCursor);
    btnOk->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; border: none; border-radius: 4px; padding: 6px 20px; font-weight: bold; } QPushButton:hover { background-color: #c0392b; }");
    connect(btnOk, &QPushButton::clicked, this, [this](){ emit confirmed(); accept(); });
    btnLayout->addWidget(btnOk);

    layout->addLayout(btnLayout);
}
