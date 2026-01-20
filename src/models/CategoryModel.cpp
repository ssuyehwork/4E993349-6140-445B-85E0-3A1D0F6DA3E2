#include "CategoryModel.h"
#include "../core/DatabaseManager.h"

CategoryModel::CategoryModel(QObject* parent) : QStandardItemModel(parent) {
    refresh();
}

void CategoryModel::refresh() {
    clear();
    QStandardItem* root = invisibleRootItem();

    // 系统分类
    QStandardItem* systemGroup = new QStandardItem("系统分类");
    systemGroup->setSelectable(false);
    root->appendRow(systemGroup);

    auto addSystemItem = [&](const QString& name, const QString& type) {
        QStandardItem* item = new QStandardItem(name);
        item->setData(type, Qt::UserRole);
        systemGroup->appendRow(item);
    };

    addSystemItem("📂 全部笔记", "all");
    addSystemItem("📅 今日笔记", "today");
    addSystemItem("📑 未分类", "uncategorized");
    addSystemItem("🏷️ 未标签", "untagged");
    addSystemItem("⭐ 书签", "bookmark");
    addSystemItem("🗑️ 回收站", "trash");

    // 用户分类
    QStandardItem* userGroup = new QStandardItem("我的分区");
    userGroup->setSelectable(false);
    root->appendRow(userGroup);

    auto categories = DatabaseManager::instance().getAllCategories();
    QMap<int, QStandardItem*> itemMap;

    for (const auto& cat : categories) {
        QStandardItem* item = new QStandardItem(cat["name"].toString());
        item->setData("category", Qt::UserRole);
        item->setData(cat["id"], Qt::UserRole + 1);
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
