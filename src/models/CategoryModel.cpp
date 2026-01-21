#include "CategoryModel.h"
#include "../core/DatabaseManager.h"
#include "../ui/IconHelper.h"

CategoryModel::CategoryModel(Type type, QObject* parent)
    : QStandardItemModel(parent), m_type(type)
{
    refresh();
}

void CategoryModel::refresh() {
    clear();
    QStandardItem* root = invisibleRootItem();

    if (m_type == System || m_type == Both) {
        auto addSystemItem = [&](const QString& name, const QString& type, const QString& icon, const QString& color = "#aaaaaa") {
            QStandardItem* item = new QStandardItem(name);
            item->setData(type, Qt::UserRole);
            item->setIcon(IconHelper::getIcon(icon, color));
            root->appendRow(item);
        };

        addSystemItem("全部数据", "all", "all_data", "#3498db");
        addSystemItem("今日数据", "today", "today", "#2ecc71");
        addSystemItem("未分类", "uncategorized", "uncategorized", "#e67e22");
        addSystemItem("未标签", "untagged", "text", "#95a5a6");
        addSystemItem("书签", "bookmark", "bookmark", "#e74c3c");
        addSystemItem("回收站", "trash", "trash", "#7f8c8d");
    }

    if (m_type == User || m_type == Both) {
        // 用户分类
        QStandardItem* userGroup = new QStandardItem("我的分区");
        userGroup->setSelectable(false);
        userGroup->setIcon(IconHelper::getIcon("branch", "#FFFFFF"));
        root->appendRow(userGroup);

        auto categories = DatabaseManager::instance().getAllCategories();
        QMap<int, QStandardItem*> itemMap;

        for (const auto& cat : categories) {
            QStandardItem* item = new QStandardItem(cat["name"].toString());
            item->setData("category", Qt::UserRole);
            item->setData(cat["id"], Qt::UserRole + 1);
            item->setData(cat["color"], Qt::UserRole + 2);
            item->setIcon(IconHelper::getIcon("circle_filled", cat["color"].toString()));
            itemMap[cat["id"].toInt()] = item;
        }

        for (const auto& cat : categories) {
            int id = cat["id"].toInt();
            int parentId = cat["parent_id"].toInt();
            if (parentId > 0 && itemMap.contains(parentId)) {
                itemMap[parentId]->appendRow(itemMap[id]);
            } else {
                userGroup->appendRow(itemMap[id]);
            }
        }
    }
}
