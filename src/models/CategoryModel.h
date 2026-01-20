#ifndef CATEGORYMODEL_H
#define CATEGORYMODEL_H

#include <QStandardItemModel>

class CategoryModel : public QStandardItemModel {
    Q_OBJECT
public:
    explicit CategoryModel(QObject* parent = nullptr);
    void refresh();
};

#endif // CATEGORYMODEL_H
