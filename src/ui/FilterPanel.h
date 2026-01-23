#ifndef FILTERPANEL_H
#define FILTERPANEL_H

#include <QWidget>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

class FilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit FilterPanel(QWidget* parent = nullptr);
    void updateStats(const QString& keyword, const QString& type, const QVariant& value);
    QVariantMap getCheckedCriteria() const;
    void resetFilters();

signals:
    void criteriaChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void initUI();
    void setupTree();
    void addFixedDateOptions(const QString& key);
    void onItemChanged(QTreeWidgetItem* item, int column);

    QWidget* m_container;
    QWidget* m_header;
    QTreeWidget* m_tree;
    QPushButton* m_btnReset;
    QLabel* m_resizeHandle;

    QMap<QString, QTreeWidgetItem*> m_roots;
    bool m_blockItemClick = false;

    // Drag & Resize state
    QPoint m_dragStartPos;
    bool m_isDragging = false;
    bool m_isResizing = false;
    QRect m_resizeStartGeometry;
};

#endif // FILTERPANEL_H
