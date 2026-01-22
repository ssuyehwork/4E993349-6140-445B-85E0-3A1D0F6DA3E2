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
        showPreview(title, content, "text", QByteArray(), pos);
    }

    void showPreview(const QString& title, const QString& content, const QString& type, const QByteArray& data, const QPoint& pos) {
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
    QTextEdit* m_textEdit;
};

#endif // QUICKPREVIEW_H
