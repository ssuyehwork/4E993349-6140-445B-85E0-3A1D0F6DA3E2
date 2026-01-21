#include "NoteModel.h"
#include <QDateTime>

NoteModel::NoteModel(QObject* parent) : QAbstractListModel(parent) {}

int NoteModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_notes.count();
}

QVariant NoteModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_notes.count()) return QVariant();

    const QVariantMap& note = m_notes.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return note.value("title");
        case ContentRole:
            return note.value("content");
        case IdRole:
            return note.value("id");
        case TagsRole:
            return note.value("tags");
        case TimeRole:
            return note.value("updated_at");
        case PinnedRole:
            return note.value("is_pinned");
        case LockedRole:
            return note.value("is_locked");
        case FavoriteRole:
            return note.value("is_favorite");
        case TypeRole:
            return note.value("item_type");
        default:
            return QVariant();
    }
}

Qt::ItemFlags NoteModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::ItemIsEnabled;
    return QAbstractListModel::flags(index) | Qt::ItemIsDragEnabled;
}

QStringList NoteModel::mimeTypes() const {
    return {"application/x-note-ids"};
}

QMimeData* NoteModel::mimeData(const QModelIndexList& indexes) const {
    QMimeData* mimeData = new QMimeData();
    QStringList ids;
    for (const QModelIndex& index : indexes) {
        if (index.isValid()) {
            ids << QString::number(data(index, IdRole).toInt());
        }
    }
    mimeData->setData("application/x-note-ids", ids.join(",").toUtf8());
    return mimeData;
}

void NoteModel::setNotes(const QList<QVariantMap>& notes) {
    beginResetModel();
    m_notes = notes;
    endResetModel();
}

// 【新增】函数的具体实现
void NoteModel::prependNote(const QVariantMap& note) {
    // 通知视图：我要在第0行插入1条数据
    beginInsertRows(QModelIndex(), 0, 0);
    m_notes.prepend(note);
    endInsertRows();
}