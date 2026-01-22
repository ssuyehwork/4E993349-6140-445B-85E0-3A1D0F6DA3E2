#ifndef QUICKPREVIEW_H
#define QUICKPREVIEW_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QPushButton>
#include <QPixmap>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QShortcut>
#include "IconHelper.h"

class QuickPreview : public QWidget {
    Q_OBJECT
public:
    explicit QuickPreview(QWidget* parent = nullptr) : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setupUI();
    }

    void showPreview(const QString& title, const QString& content, const QString& type, const QByteArray& dataBlob, const QPoint& pos) {
        m_titleLabel->setText(title);
        
        bool isImage = (type == "image" && !dataBlob.isEmpty());

        if (isImage) {
            QPixmap pix;
            if (pix.loadFromData(dataBlob)) {
                // 自适应缩放图片，但不超过预览框大小
                m_imageLabel->setPixmap(pix.scaled(800, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                m_imageLabel->show();
                m_textEdit->hide();
                resize(qBound(400, m_imageLabel->pixmap().width() + 40, 900),
                       qBound(300, m_imageLabel->pixmap().height() + 80, 700));
            } else {
                showAsText(content);
            }
        } else {
            showAsText(content);
            resize(600, 500);
        }

        // 智能定位
        if (isImage) {
            centerOnScreen();
        } else {
            move(pos);
            ensureWithinScreen();
        }

        show();
        raise();
        activateWindow();
    }

private:
    void setupUI() {
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);

        m_container = new QFrame();
        m_container->setObjectName("PreviewContainer");
        m_container->setStyleSheet(
            "QFrame#PreviewContainer { background-color: #1e1e1e; border: 1px solid #444; border-radius: 10px; }"
            "QLabel { color: #eee; border: none; background: transparent; }"
        );
        
        auto* containerLayout = new QVBoxLayout(m_container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // 1. 标题栏 (支持拖动)
        auto* titleBar = new QWidget();
        titleBar->setFixedHeight(36);
        titleBar->setStyleSheet("background-color: #252526; border-top-left-radius: 10px; border-top-right-radius: 10px; border-bottom: 1px solid #333;");
        auto* titleLayout = new QHBoxLayout(titleBar);
        titleLayout->setContentsMargins(15, 0, 5, 0);

        m_titleLabel = new QLabel("预览");
        m_titleLabel->setStyleSheet("font-weight: bold; color: #4a90e2;");
        titleLayout->addWidget(m_titleLabel);
        titleLayout->addStretch();

        auto* btnClose = new QPushButton();
        btnClose->setIcon(IconHelper::getIcon("close", "#aaa"));
        btnClose->setFixedSize(28, 28);
        btnClose->setStyleSheet("QPushButton { border: none; border-radius: 4px; } QPushButton:hover { background-color: #e74c3c; }");
        connect(btnClose, &QPushButton::clicked, this, &QuickPreview::hide);
        titleLayout->addWidget(btnClose);

        containerLayout->addWidget(titleBar);

        // 2. 内容区
        m_textEdit = new QTextEdit();
        m_textEdit->setReadOnly(true);
        m_textEdit->setStyleSheet("background: transparent; border: none; color: #ddd; font-size: 14px; padding: 15px; line-height: 1.6;");
        
        m_imageLabel = new QLabel();
        m_imageLabel->setAlignment(Qt::AlignCenter);
        m_imageLabel->setStyleSheet("padding: 10px;");

        containerLayout->addWidget(m_textEdit, 1);
        containerLayout->addWidget(m_imageLabel, 1);
        m_imageLabel->hide();

        mainLayout->addWidget(m_container);

        // 绑定快捷键，确保即便焦点在 QTextEdit 上也能关闭
        new QShortcut(QKeySequence(Qt::Key_Space), this, [this](){ hide(); });
        new QShortcut(QKeySequence(Qt::Key_Escape), this, [this](){ hide(); });
        
        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(20);
        shadow->setColor(QColor(0, 0, 0, 150));
        shadow->setOffset(0, 5);
        m_container->setGraphicsEffect(shadow);
    }

    void showAsText(const QString& content) {
        QString formatted = content.toHtmlEscaped().replace("\n", "<br>");
        m_textEdit->setHtml(QString("<div style='color:#ddd; font-family:Microsoft YaHei;'>%1</div>").arg(formatted));
        m_textEdit->show();
        m_imageLabel->hide();
    }

    void ensureWithinScreen() {
        QScreen *screen = QApplication::screenAt(QCursor::pos());
        if (!screen) screen = QApplication::primaryScreen();
        if (screen) {
            QRect geom = screen->availableGeometry();
            int x = pos().x();
            int y = pos().y();
            if (x + width() > geom.right()) x = geom.right() - width();
            if (y + height() > geom.bottom()) y = geom.bottom() - height();
            if (x < geom.left()) x = geom.left();
            if (y < geom.top()) y = geom.top();
            move(x, y);
        }
    }

    void centerOnScreen() {
        QScreen *screen = QApplication::screenAt(QCursor::pos());
        if (!screen) screen = QApplication::primaryScreen();
        if (screen) {
            QRect geom = screen->availableGeometry();
            move(geom.center() - rect().center());
        }
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && event->pos().y() < 40) {
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
            move(event->globalPosition().toPoint() - m_dragPos);
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        m_dragPos = QPoint();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
        } else {
            QWidget::keyPressEvent(event);
        }
    }

private:
    QFrame* m_container;
    QLabel* m_titleLabel;
    QTextEdit* m_textEdit;
    QLabel* m_imageLabel;
    QPoint m_dragPos;
    int m_currentId = -1;
};

#endif // QUICKPREVIEW_H
