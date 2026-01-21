#include "NoteModel.h"
#include <QDateTime>
#include <QIcon>
#include "../ui/IconHelper.h"
#include "../core/DatabaseManager.h"
#include <QFileInfo>
#include <QBuffer>
#include <QPixmap>
#include <QByteArray>
#include <QUrl>

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

NoteModel::NoteModel(QObject* parent) : QAbstractListModel(parent) {
    updateCategoryMap();
}

int NoteModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_notes.count();
}

QVariant NoteModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_notes.count()) return QVariant();

    const QVariantMap& note = m_notes.at(index.row());
    switch (role) {
        case Qt::BackgroundRole:
            return QVariant(); // 强制不返回任何背景色，由 Delegate 控制
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
                // 【核心修复】智能检测文本内容，对齐 Python 版逻辑
                QString stripped = content.trimmed();
                QString cleanPath = stripped;
                if ((cleanPath.startsWith("\"") && cleanPath.endsWith("\"")) || 
                    (cleanPath.startsWith("'") && cleanPath.endsWith("'"))) {
                    cleanPath = cleanPath.mid(1, cleanPath.length() - 2);
                }

                if (stripped.startsWith("http://") || stripped.startsWith("https://") || stripped.startsWith("www.")) {
                    iconName = "link";
                    iconColor = "#3498db";
                } else if (stripped.startsWith("#") || stripped.startsWith("import ") || stripped.startsWith("class ") || 
                           stripped.startsWith("def ") || stripped.startsWith("<") || stripped.startsWith("{") ||
                           stripped.startsWith("function") || stripped.startsWith("var ") || stripped.startsWith("const ")) {
                    iconName = "code";
                    iconColor = "#2ecc71";
                } else if (cleanPath.length() < 260 && (
                           (cleanPath.length() > 2 && cleanPath[1] == ':') || 
                           cleanPath.startsWith("\\\\") || cleanPath.startsWith("/") || 
                           cleanPath.startsWith("./") || cleanPath.startsWith("../"))) {
                    QFileInfo info(cleanPath);
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
            int catId = note.value("category_id").toInt();
            QString tags = note.value("tags").toString();
            bool pinned = note.value("is_pinned").toBool();
            bool locked = note.value("is_locked").toBool();
            bool favorite = note.value("is_favorite").toBool();
            int rating = note.value("rating").toInt();

            QString catName = m_categoryMap.value(catId, "未分类");
            if (tags.isEmpty()) tags = "无";

            QString statusStr;
            if (pinned) statusStr += getIconHtml("pin_vertical", "#e74c3c") + " 置顶 ";
            if (locked) statusStr += getIconHtml("lock", "#2ecc71") + " 锁定 ";
            if (favorite) statusStr += getIconHtml("bookmark_filled", "#ff6b81") + " 书签 ";
            if (statusStr.isEmpty()) statusStr = "无";

            QString ratingStr;
            for(int i=0; i<rating; ++i) ratingStr += getIconHtml("star_filled", "#f39c12") + " ";
            if (ratingStr.isEmpty()) ratingStr = "无";

            QString preview = content.left(400).toHtmlEscaped().replace("\n", "<br>").trimmed();
            if (content.length() > 400) preview += "...";
            if (preview.isEmpty()) preview = title.toHtmlEscaped();

            return QString("<html><body style='color: #ddd;'>"
                           "<table border='0' cellpadding='2' cellspacing='0'>"
                           "<tr><td width='22'>%1</td><td><b>分区:</b> %2</td></tr>"
                           "<tr><td width='22'>%3</td><td><b>标签:</b> %4</td></tr>"
                           "<tr><td width='22'>%5</td><td><b>评级:</b> %6</td></tr>"
                           "<tr><td width='22'>%7</td><td><b>状态:</b> %8</td></tr>"
                           "</table>"
                           "<hr style='border: 0; border-top: 1px solid #555; margin: 5px 0;'>"
                           "<div style='color: #ccc; font-size: 12px; line-height: 1.4;'>%9</div>"
                           "</body></html>")
                .arg(getIconHtml("branch", "#4a90e2"), catName,
                     getIconHtml("tag", "#FFAB91"), tags,
                     getIconHtml("star", "#f39c12"), ratingStr,
                     getIconHtml("pin_tilted", "#aaa"), statusStr,
                     preview);
        }
        case Qt::DisplayRole: {
            QString type = note.value("item_type").toString();
            QString title = note.value("title").toString();
            QString content = note.value("content").toString();
            if (type == "text" || type.isEmpty()) {
                QString display = content.replace('\n', ' ').replace('\r', ' ').trimmed().left(150);
                return display.isEmpty() ? title : display;
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
        case RatingRole:
            return note.value("rating");
        case CategoryIdRole:
            return note.value("category_id");
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
    QStringList texts;
    QList<QUrl> urls;

    for (const QModelIndex& index : indexes) {
        if (index.isValid()) {
            ids << QString::number(data(index, IdRole).toInt());
            
            QString content = data(index, ContentRole).toString();
            QString type = data(index, TypeRole).toString();
            
            if (type == "text" || type.isEmpty()) {
                texts << content;
            } else if (type == "file" || type == "folder" || type == "files") {
                QStringList rawPaths = content.split(';', Qt::SkipEmptyParts);
                for (const QString& p : rawPaths) {
                    QString path = p.trimmed().remove('\"');
                    if (QFileInfo::exists(path)) {
                        urls << QUrl::fromLocalFile(path);
                    }
                }
                texts << content; // 同时也作为文本
            }
        }
    }
    
    mimeData->setData("application/x-note-ids", ids.join(",").toUtf8());
    if (!texts.isEmpty()) {
        mimeData->setText(texts.join("\n---\n"));
    }
    if (!urls.isEmpty()) {
        mimeData->setUrls(urls);
    }
    
    return mimeData;
}

void NoteModel::setNotes(const QList<QVariantMap>& notes) {
    updateCategoryMap();
    beginResetModel();
    m_notes = notes;
    endResetModel();
}

void NoteModel::updateCategoryMap() {
    auto categories = DatabaseManager::instance().getAllCategories();
    m_categoryMap.clear();
    for (const auto& cat : categories) {
        m_categoryMap[cat["id"].toInt()] = cat["name"].toString();
    }
}

// 【新增】函数的具体实现
void NoteModel::prependNote(const QVariantMap& note) {
    // 通知视图：我要在第0行插入1条数据
    beginInsertRows(QModelIndex(), 0, 0);
    m_notes.prepend(note);
    endInsertRows();
}