#ifndef QUICKPREVIEW_H
#define QUICKPREVIEW_H

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>

class QuickPreview : public QWidget {
    Q_OBJECT
public:
    explicit QuickPreview(QWidget* parent = nullptr) : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint) {
        setAttribute(Qt::WA_TranslucentBackground);

        auto* layout = new QVBoxLayout(this);
        m_container = new QFrame();
        m_container->setStyleSheet("background-color: #1e1e1e; border: 1px solid #444; border-radius: 8px;");

        auto* containerLayout = new QVBoxLayout(m_container);
        m_textEdit = new QTextEdit();
        m_textEdit->setReadOnly(true);
        m_textEdit->setStyleSheet("background: transparent; border: none; color: #ddd; font-size: 14px;");
        containerLayout->addWidget(m_textEdit);

        layout->addWidget(m_container);

        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(20);
        shadow->setColor(QColor(0, 0, 0, 150));
        shadow->setOffset(0, 5);
        m_container->setGraphicsEffect(shadow);

        resize(500, 600);
    }

    void showPreview(const QString& title, const QString& content, const QPoint& pos) {
        QString formattedContent = content;
        formattedContent.replace("\n", "<br>");
        QString html = QString("<h2>%1</h2><hr/><div style='line-height: 1.5;'>%2</div>")
                       .arg(title, formattedContent);
        m_textEdit->setHtml(html);
        move(pos);
        show();
    }

protected:
    void keyReleaseEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape) {
            hide();
        }
        QWidget::keyReleaseEvent(event);
    }

private:
    QFrame* m_container;
    QTextEdit* m_textEdit;
};

#endif // QUICKPREVIEW_H
