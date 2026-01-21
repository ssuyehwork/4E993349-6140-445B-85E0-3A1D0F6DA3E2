#include "NoteModel.h"
#include <QDateTime>
#include <QIcon>
#include "../ui/IconHelper.h"
#include <QFileInfo>
#include <QBuffer>
#include <QPixmap>
#include <QByteArray>

static QString getIconHtml(const QString& name, const QString& color) {
    QIcon icon = IconHelper::getIcon(name, color, 16);
    QPixmap pixmap = icon.pixmap(16, 16);
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString("<img src='data:image/png;base64,%1' width='16' height='16' style='vertical-align:middle;'>")
           .arg(QString(ba.toBase64()));
}

NoteModel::NoteModel(QObject* parent) : QAbstractListModel(parent) {}

int NoteModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_notes.count();
}

QVariant NoteModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_notes.count()) return QVariant();

    const QVariantMap& note = m_notes.at(index.row());
    switch (role) {
        case Qt::DecorationRole: {
            QString type = note.value("item_type").toString();
            QString content = note.value("content").toString().trimmed();
            QString iconName = "text"; // Default
            QString iconColor = "#95a5a6";

            if (type == "image") {
                iconName = "image";
                iconColor = "#9b59b6";
            } else if (type == "file" || type == "files") {
                iconName = "file";
                iconColor = "#f1c40f";
            } else if (type == "folder") {
                iconName = "folder";
                iconColor = "#e67e22";
            } else {
                // Smart recognition for text
                if (content.startsWith("http://") || content.startsWith("https://") || content.startsWith("www.")) {
                    iconName = "link";
                    iconColor = "#3498db";
                } else if (content.startsWith("#") || content.startsWith("import ") || content.startsWith("class ") || content.startsWith("def ")) {
                    iconName = "code";
                    iconColor = "#2ecc71";
                } else if (content.length() < 260 && (content.contains(":/") || content.startsWith("/") || content.startsWith("\\\\"))) {
                    QFileInfo info(content.remove('\"'));
                    if (info.exists()) {
                        if (info.isDir()) {
                            iconName = "folder";
                            iconColor = "#e67e22";
                        } else {
                            iconName = "file";
                            iconColor = "#f1c40f";
                        }
                    }
                }
            }
            return IconHelper::getIcon(iconName, iconColor, 32);
        }
        case Qt::ToolTipRole: {
            QString title = note.value("title").toString();
            QString content = note.value("content").toString();
            QString updatedAt = note.value("updated_at").toString();
            bool pinned = note.value("is_pinned").toBool();
            bool locked = note.value("is_locked").toBool();
            bool favorite = note.value("is_favorite").toBool();
            int rating = note.value("rating").toInt();

            QString statusStr;
            if (pinned) statusStr += getIconHtml("pin", "#e74c3c") + " 置顶 ";
            if (locked) statusStr += getIconHtml("lock", "#2ecc71") + " 锁定 ";
            if (favorite) statusStr += getIconHtml("bookmark", "#ff6b81") + " 书签 ";
            if (statusStr.isEmpty()) statusStr = "无";

            QString ratingStr;
            for(int i=0; i<rating; ++i) ratingStr += getIconHtml("star", "#f39c12");
            if (ratingStr.isEmpty()) ratingStr = "无";

            QString preview = content.left(400).replace("\n", "<br>").trimmed();
            if (content.length() > 400) preview += "...";
            if (preview.isEmpty()) preview = title;

            return QString("<html><body style='color: #ddd;'>"
                           "<table border='0' cellpadding='2' cellspacing='0'>"
                           "<tr><td>%1</td><td><b>更新于:</b> %2</td></tr>"
                           "<tr><td>%3</td><td><b>评分:</b> %4</td></tr>"
                           "<tr><td>%5</td><td><b>状态:</b> %6</td></tr>"
                           "</table>"
                           "<hr style='border: 0; border-top: 1px solid #555; margin: 5px 0;'>"
                           "<div style='color: #ccc; font-size: 12px; line-height: 1.4;'>%7</div>"
                           "</body></html>")
                .arg(getIconHtml("clock", "#aaa"), updatedAt,
                     getIconHtml("star", "#f39c12"), ratingStr,
                     getIconHtml("eye", "#aaa"), statusStr,
                     preview);
        }
        case Qt::DisplayRole: {
            QString type = note.value("item_type").toString();
            QString title = note.value("title").toString();
            QString content = note.value("content").toString();
            if (type == "text" || type.isEmpty()) {
                return content.replace('\n', ' ').replace('\r', ' ').trimmed().left(150);
            }
            return title;
        }
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