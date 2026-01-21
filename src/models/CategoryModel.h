#ifndef CATEGORYMODEL_H
#define CATEGORYMODEL_H

#include <QStandardItemModel>

class CategoryModel : public QStandardItemModel {
    Q_OBJECT
public:
    enum Type { System, User, Both };
    explicit CategoryModel(Type type, QObject* parent = nullptr);
    void refresh();

private:
    Type m_type;
};

#endif // CATEGORYMODEL_H
