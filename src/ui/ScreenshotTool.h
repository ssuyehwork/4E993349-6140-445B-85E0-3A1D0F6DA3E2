#ifndef SCREENSHOTTOOL_H
#define SCREENSHOTTOOL_H

#include <QWidget>
#include <QRubberBand>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>

class ScreenshotTool : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotTool(QWidget* parent = nullptr) : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowState(Qt::WindowFullScreen);
        setCursor(Qt::CrossCursor);
        m_screenPixmap = QGuiApplication::primaryScreen()->grabWindow(0);
    }

signals:
    void screenshotCaptured(const QImage& image);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_screenPixmap);
        painter.fillRect(rect(), QColor(0, 0, 0, 100));
    }

    void mousePressEvent(QMouseEvent* event) override {
        m_origin = event->pos();
        if (!m_rubberBand) m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        m_rubberBand->setGeometry(QRect(m_origin, QSize()));
        m_rubberBand->show();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        m_rubberBand->setGeometry(QRect(m_origin, event->pos()).normalized());
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        m_rubberBand->hide();
        QRect rect = QRect(m_origin, event->pos()).normalized();
        if (rect.width() > 5 && rect.height() > 5) {
            QImage img = m_screenPixmap.copy(rect).toImage();
            emit screenshotCaptured(img);
        }
        close();
    }

private:
    QPixmap m_screenPixmap;
    QRubberBand* m_rubberBand = nullptr;
    QPoint m_origin;
};

#endif // SCREENSHOTTOOL_H
