#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPoint>

class Toolbox : public QWidget {
    Q_OBJECT
public:
    explicit Toolbox(QWidget* parent = nullptr);
    ~Toolbox();

signals:
    void showTimePasteRequested();
    void showPasswordGeneratorRequested();
    void showOCRRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void initUI();
    QPushButton* createToolButton(const QString& text);

    QPoint m_dragPos;
};

#endif // TOOLBOX_H
