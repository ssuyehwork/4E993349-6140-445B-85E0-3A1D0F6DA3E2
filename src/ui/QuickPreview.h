#ifndef QUICKPREVIEW_H
#define QUICKPREVIEW_H

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QApplication>
#include <QFrame>
#include <QShortcut>
#include <QAction>
#include "IconHelper.h"

class QuickPreview : public QWidget {
    Q_OBJECT
signals:
    void editRequested(int noteId);

public:
    explicit QuickPreview(QWidget* parent = nullptr) : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setFocusPolicy(Qt::StrongFocus);
        
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);

        m_container = new QFrame();
        m_container->setObjectName("previewContainer");
        m_container->setStyleSheet(
            "QFrame#previewContainer { background-color: #1e1e1e; border: 1px solid #444; border-radius: 8px; }"
            "QFrame#previewTitleBar { background-color: #1e1e1e; border-top-left-radius: 7px; border-top-right-radius: 7px; border-bottom: 1px solid #333; }"
            "QTextEdit { border-bottom-left-radius: 7px; border-bottom-right-radius: 7px; background: transparent; border: none; color: #ddd; font-size: 14px; padding: 10px; }"
            "QPushButton { border: none; border-radius: 4px; background: transparent; padding: 4px; }"
            "QPushButton:hover { background-color: #3e3e42; }"
            "QPushButton#btnClose:hover { background-color: #E81123; }"
        );
        
        auto* containerLayout = new QVBoxLayout(m_container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // --- 标题栏 ---
        m_titleBar = new QFrame();
        m_titleBar->setObjectName("previewTitleBar");
        m_titleBar->setFixedHeight(36);
        m_titleBar->setAttribute(Qt::WA_StyledBackground);
        auto* titleLayout = new QHBoxLayout(m_titleBar);
        titleLayout->setContentsMargins(10, 0, 5, 0);
        titleLayout->setSpacing(5);

        QLabel* titleLabel = new QLabel("预览");
        titleLabel->setStyleSheet("color: #888; font-size: 12px; font-weight: bold;");
        titleLayout->addWidget(titleLabel);
        titleLayout->addStretch();

        auto createBtn = [this](const QString& icon, const QString& tooltip, const QString& objName = "") {
            QPushButton* btn = new QPushButton();
            btn->setIcon(IconHelper::getIcon(icon, "#aaaaaa"));
            btn->setIconSize(QSize(16, 16));
            btn->setFixedSize(32, 32);
            btn->setToolTip(tooltip);
            if (!objName.isEmpty()) btn->setObjectName(objName);
            return btn;
        };

        QPushButton* btnEdit = createBtn("edit", "编辑 (Ctrl+B)");
        btnEdit->setFocusPolicy(Qt::NoFocus);
        QPushButton* btnMin = createBtn("minimize", "最小化");
        btnMin->setFocusPolicy(Qt::NoFocus);
        QPushButton* btnMax = createBtn("maximize", "最大化");
        btnMax->setFocusPolicy(Qt::NoFocus);
        QPushButton* btnClose = createBtn("close", "关闭", "btnClose");
        btnClose->setFocusPolicy(Qt::NoFocus);

        connect(btnEdit, &QPushButton::clicked, [this]() {
            emit editRequested(m_currentNoteId);
        });
        connect(btnMin, &QPushButton::clicked, this, &QuickPreview::showMinimized);
        connect(btnMax, &QPushButton::clicked, [this]() {
            if (isMaximized()) showNormal();
            else showMaximized();
        });
        connect(btnClose, &QPushButton::clicked, this, &QuickPreview::hide);

        titleLayout->addWidget(btnEdit);
        titleLayout->addWidget(btnMin);
        titleLayout->addWidget(btnMax);
        titleLayout->addWidget(btnClose);

        containerLayout->addWidget(m_titleBar);

        m_textEdit = new QTextEdit();
        m_textEdit->setReadOnly(true);
        m_textEdit->setFocusPolicy(Qt::NoFocus); // 防止拦截空格键
        containerLayout->addWidget(m_textEdit);
        
        mainLayout->addWidget(m_container);
        
        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(15);
        shadow->setColor(QColor(0, 0, 0, 150));
        shadow->setOffset(0, 5);
        m_container->setGraphicsEffect(shadow);
        
        resize(900, 700);

    }

    void showPreview(int noteId, const QString& title, const QString& content, const QPoint& pos) {
        showPreview(noteId, title, content, "text", QByteArray(), pos);
    }

    void showPreview(int noteId, const QString& title, const QString& content, const QString& type, const QByteArray& data, const QPoint& pos) {
        m_currentNoteId = noteId;
        QString html;
        QString titleHtml = QString("<h3 style='color: #eee; margin-bottom: 5px;'>%1</h3>").arg(title.toHtmlEscaped());
        QString hrHtml = "<hr style='border: 0; border-top: 1px solid #444; margin: 10px 0;'>";

        if (type == "image" && !data.isEmpty()) {
            html = QString("%1%2<div style='text-align: center;'><img src='data:image/png;base64,%3' width='450'></div>")
                   .arg(titleHtml, hrHtml, QString(data.toBase64()));
        } else {
            QString formattedContent = content.toHtmlEscaped();
            formattedContent.replace("\n", "<br>");
            html = QString("%1%2<div style='line-height: 1.6; color: #ccc; font-size: 13px;'>%3</div>")
                   .arg(titleHtml, hrHtml, formattedContent);
        }
        m_textEdit->setHtml(html);
        move(pos);
        show();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_titleBar->rect().contains(m_titleBar->mapFrom(this, event->pos()))) {
            m_dragging = true;
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_dragging && event->buttons() & Qt::LeftButton) {
            move(event->globalPosition().toPoint() - m_dragPos);
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        m_dragging = false;
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (m_titleBar->rect().contains(m_titleBar->mapFrom(this, event->pos()))) {
            if (isMaximized()) showNormal();
            else showMaximized();
            event->accept();
        }
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    QFrame* m_container;
    QWidget* m_titleBar;
    QTextEdit* m_textEdit;
    int m_currentNoteId = -1;
    bool m_dragging = false;
    QPoint m_dragPos;
};

#endif // QUICKPREVIEW_H
