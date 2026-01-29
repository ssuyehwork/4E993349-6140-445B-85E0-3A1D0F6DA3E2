#ifndef SCREENSHOTTOOL_H
#define SCREENSHOTTOOL_H

#include <QWidget>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QPainter>
#include <QPaintEvent>
#include <QList>
#include <QStack>
#include <QPointF>
#include <QRectF>

class ScreenshotToolbar;

enum class ScreenshotState {
    Selecting,
    Editing
};

enum class ScreenshotToolType {
    None,
    Rect,
    Ellipse,
    Arrow,
    Pen,
    Marker,
    Text,
    Mosaic
};

struct DrawingAnnotation {
    ScreenshotToolType type;
    QList<QPointF> points;
    QColor color;
    QString text;
    int strokeWidth;
};

class ScreenshotTool : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotTool(QWidget* parent = nullptr);
    ~ScreenshotTool();

signals:
    void screenshotCaptured(const QImage& image);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void setTool(ScreenshotToolType type);
    void undo();
    void save();
    void confirm();
    void cancel();

private:
    void updateCursor(const QPoint& pos);
    QRect selectionRect() const;
    QList<QRect> getHandleRects() const;
    int getHandleAt(const QPoint& pos) const;
    void drawAnnotation(QPainter& painter, const DrawingAnnotation& ann);
    void finishCapture();
    QImage generateFinalImage();

    QPixmap m_screenPixmap;
    QPixmap m_mosaicPixmap;
    ScreenshotState m_state = ScreenshotState::Selecting;
    ScreenshotToolType m_currentTool = ScreenshotToolType::None;

    QPoint m_startPoint;
    QPoint m_endPoint;
    bool m_isDragging = false;
    int m_dragHandle = -1; // -1: none, 0-7: handles, 8: move selection

    QList<DrawingAnnotation> m_annotations;
    DrawingAnnotation m_currentAnnotation;
    bool m_isDrawing = false;

    ScreenshotToolbar* m_toolbar = nullptr;
};

#endif // SCREENSHOTTOOL_H
