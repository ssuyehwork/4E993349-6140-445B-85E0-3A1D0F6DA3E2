#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QTimer>

class NodeItem;
class EdgeItem;

class GraphWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphWidget(QWidget* parent = nullptr);

    void itemMoved();

protected:
    void timerEvent(QTimerEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    int m_timerId = 0;
};

class NodeItem : public QGraphicsItem {
public:
    NodeItem(GraphWidget* graphWidget);
    void addEdge(EdgeItem* edge);

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    void calculateForces();
    bool advancePosition();

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QList<EdgeItem*> m_edgeList;
    QPointF m_newPos;
    GraphWidget* m_graph;
};

#endif // GRAPHWIDGET_H
