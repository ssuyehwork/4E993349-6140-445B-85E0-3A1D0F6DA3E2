#include "NoteModel.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

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
        case Qt::ToolTipRole: {
            QString content = note.value("content").toString();
            if (content.length() > 500) content = content.left(500) + "...";
            content.replace("\n", "<br>");
            QString title = note.value("title").toString();
            QString time = note.value("updated_at").toDateTime().toString("yyyy-MM-dd HH:mm:ss");

            return QString("<html><body style='color:#ccc;'>"
                           "<b>%1</b><br>"
                           "<small style='color:#888;'>%2</small><hr>"
                           "<div>%3</div>"
                           "</body></html>").arg(title, time, content);
        }
        case TypeRole:
            return note.value("item_type");
        case SmartIconRole: {
            QString type = note.value("item_type").toString();
            QString content = note.value("content").toString().trimmed();
            if (type == "image") return "image";
            if (type == "file" || type == "files") return "file";
            if (type == "folder") return "folder";

            // 文本智能识别
            if (content.startsWith("http://") || content.startsWith("https://") || content.startsWith("www.")) return "link";
            if (content.startsWith("#") || content.startsWith("import ") || content.startsWith("class ") ||
                content.startsWith("def ") || content.startsWith("<") || content.startsWith("{")) return "code";

            // 路径识别
            if (content.length() < 260 && (content.contains(":/") || content.contains(":\\") || content.startsWith("/"))) {
                QFileInfo info(content.remove("\"").remove("'"));
                if (info.exists()) return info.isDir() ? "folder" : "file";
            }
            return "text";
        }
        case SmartColorRole: {
            QString type = note.value("item_type").toString();
            QString content = note.value("content").toString().trimmed();
            if (type == "image") return "#9b59b6";
            if (type == "file" || type == "files") return "#f1c40f";
            if (type == "folder") return "#e67e22";

            if (content.startsWith("http://") || content.startsWith("https://") || content.startsWith("www.")) return "#3498db";
            if (content.startsWith("#") || content.startsWith("import ") || content.startsWith("class ") ||
                content.startsWith("def ") || content.startsWith("<") || content.startsWith("{")) return "#2ecc71";

            if (content.length() < 260 && (content.contains(":/") || content.contains(":\\") || content.startsWith("/"))) {
                QFileInfo info(content.remove("\"").remove("'"));
                if (info.exists()) return info.isDir() ? "#e67e22" : "#f1c40f";
            }
            return "#cccccc";
        }
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