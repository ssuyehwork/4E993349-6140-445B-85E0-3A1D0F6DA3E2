#ifndef SEARCHLINEEDIT_H
#define SEARCHLINEEDIT_H

#include <QLineEdit>
#include <QMouseEvent>

class SearchLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit SearchLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {}

signals:
    void doubleClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            emit doubleClicked();
        }
        QLineEdit::mouseDoubleClickEvent(e);
    }
};

#endif // SEARCHLINEEDIT_H
