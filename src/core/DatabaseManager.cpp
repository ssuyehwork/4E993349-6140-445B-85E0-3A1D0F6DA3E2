#include "DatabaseManager.h"
#include <QDebug>
#include <QSqlRecord>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::init(const QString& dbPath) {
    QMutexLocker locker(&m_mutex);
    m_dbPath = dbPath;
    
    // 自动备份
    if (QFile::exists(dbPath)) {
        QString backupDir = QCoreApplication::applicationDirPath() + "/backups";
        QDir().mkpath(backupDir);
        QString backupPath = backupDir + "/backup_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".db";
        if (QFile::copy(dbPath, backupPath)) {
            // 清理旧备份 (保留 20 份)
            QDir dir(backupDir);
            QFileInfoList list = dir.entryInfoList(QStringList() << "*.db", QDir::Files, QDir::Time);
            while (list.size() > 20) {
                QFile::remove(list.takeLast().absoluteFilePath());
            }
        }
    }

    if (m_db.isOpen()) m_db.close();

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    if (!createTables()) return false;

    return true;
}

bool DatabaseManager::createTables() {
    QSqlQuery query(m_db);
    
    // 1. 创建 notes 表
    QString createNotesTable = R"(
        CREATE TABLE IF NOT EXISTS notes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT,
            content TEXT,
            tags TEXT,
            color TEXT DEFAULT '#2d2d2d',
            category_id INTEGER,
            item_type TEXT DEFAULT 'text',
            data_blob BLOB,
            content_hash TEXT,
            rating INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            is_pinned INTEGER DEFAULT 0,
            is_locked INTEGER DEFAULT 0,
            is_favorite INTEGER DEFAULT 0,
            is_deleted INTEGER DEFAULT 0,
            source_app TEXT,
            source_title TEXT
        )
    )";
    
    if (!query.exec(createNotesTable)) return false;

    // 检查并补全缺失列 (针对旧数据库)
    QStringList columnsToAdd = {
        "ALTER TABLE notes ADD COLUMN color TEXT DEFAULT '#2d2d2d'",
        "ALTER TABLE notes ADD COLUMN category_id INTEGER",
        "ALTER TABLE notes ADD COLUMN item_type TEXT DEFAULT 'text'",
        "ALTER TABLE notes ADD COLUMN data_blob BLOB",
        "ALTER TABLE notes ADD COLUMN content_hash TEXT",
        "ALTER TABLE notes ADD COLUMN rating INTEGER DEFAULT 0",
        "ALTER TABLE notes ADD COLUMN source_app TEXT",
        "ALTER TABLE notes ADD COLUMN source_title TEXT"
    };
    for (const QString& sql : columnsToAdd) {
        query.exec(sql); // 忽略已存在的错误
    }

    // 2. 创建 categories 表
    QString createCategoriesTable = R"(
        CREATE TABLE IF NOT EXISTS categories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            parent_id INTEGER,
            color TEXT DEFAULT '#808080',
            sort_order INTEGER DEFAULT 0,
            preset_tags TEXT
        )
    )";
    query.exec(createCategoriesTable);

    // 3. 创建 tags 和关联表
    query.exec("CREATE TABLE IF NOT EXISTS tags (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL)");
    query.exec("CREATE TABLE IF NOT EXISTS note_tags (note_id INTEGER, tag_id INTEGER, PRIMARY KEY (note_id, tag_id))");
    
    // 索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_notes_content_hash ON notes(content_hash)");

    // 4. FTS5 全文搜索
    QString createFtsTable = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(
            title, content, content='notes', content_rowid='id'
        )
    )";
    query.exec(createFtsTable);

    // 触发器同步 FTS
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_ai AFTER INSERT ON notes BEGIN "
               "INSERT INTO notes_fts(rowid, title, content) VALUES (new.id, new.title, new.content); END;");
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_ad AFTER DELETE ON notes BEGIN "
               "INSERT INTO notes_fts(notes_fts, rowid, title, content) VALUES('delete', old.id, old.title, old.content); END;");
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_au AFTER UPDATE ON notes BEGIN "
               "INSERT INTO notes_fts(notes_fts, rowid, title, content) VALUES('delete', old.id, old.title, old.content); "
               "INSERT INTO notes_fts(rowid, title, content) VALUES (new.id, new.title, new.content); END;");

    return true;
}

// 【重构核心】对标 Ditto 的“查重并置顶”创建逻辑
bool DatabaseManager::addNote(const QString& title, const QString& content, const QStringList& tags,
                             const QString& color, int categoryId,
                             const QString& itemType, const QByteArray& dataBlob,
                             const QString& sourceApp, const QString& sourceTitle) {
    QVariantMap newNoteMap;
    bool success = false;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // 计算 SHA256 哈希值
    QByteArray hashData = dataBlob.isEmpty() ? content.toUtf8() : dataBlob;
    QString contentHash = QCryptographicHash::hash(hashData, QCryptographicHash::Sha256).toHex();

    {   // === 锁的作用域开始 ===
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QSqlQuery checkQuery(m_db);
        // 首先检查哈希是否存在（且未删除）
        checkQuery.prepare("SELECT id FROM notes WHERE content_hash = :hash AND is_deleted = 0 LIMIT 1");
        checkQuery.bindValue(":hash", contentHash);
        
        if (checkQuery.exec() && checkQuery.next()) {
            // --- 命中重复：更新时间戳和来源（即置顶逻辑） ---
            int existingId = checkQuery.value(0).toInt();
            QSqlQuery updateQuery(m_db);
            updateQuery.prepare("UPDATE notes SET updated_at = :now, source_app = :app, source_title = :stitle "
                                "WHERE id = :id");
            updateQuery.bindValue(":now", currentTime);
            updateQuery.bindValue(":app", sourceApp);
            updateQuery.bindValue(":stitle", sourceTitle);
            updateQuery.bindValue(":id", existingId);
            
            if (updateQuery.exec()) {
                // 触发更新信号而非添加信号，通知 UI 刷新列表顺序
                emit noteUpdated();
                return true;
            }
        }

        // --- 未命中：插入新记录 ---
        QSqlQuery query(m_db);
        query.prepare("INSERT INTO notes (title, content, tags, color, category_id, item_type, data_blob, "
                      "content_hash, created_at, updated_at, source_app, source_title) "
                      "VALUES (:title, :content, :tags, :color, :category_id, :item_type, :data_blob, "
                      ":hash, :created_at, :updated_at, :source_app, :source_title)");
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", tags.join(","));
        query.bindValue(":color", color.isEmpty() ? "#2d2d2d" : color);
        query.bindValue(":category_id", categoryId == -1 ? QVariant(QMetaType::fromType<int>()) : categoryId);
        query.bindValue(":item_type", itemType);
        query.bindValue(":data_blob", dataBlob);
        query.bindValue(":hash", contentHash);
        query.bindValue(":created_at", currentTime);
        query.bindValue(":updated_at", currentTime);
        query.bindValue(":source_app", sourceApp);
        query.bindValue(":source_title", sourceTitle);
        
        if (query.exec()) {
            success = true;
            QVariant lastId = query.lastInsertId();
            QSqlQuery fetch(m_db);
            fetch.prepare("SELECT * FROM notes WHERE id = :id");
            fetch.bindValue(":id", lastId);
            if (fetch.exec() && fetch.next()) {
                QSqlRecord rec = fetch.record();
                for (int i = 0; i < rec.count(); ++i) {
                    newNoteMap[rec.fieldName(i)] = fetch.value(i);
                }
            }
        } else {
            qCritical() << "添加笔记失败:" << query.lastError().text();
        }
    }

    if (success && !newNoteMap.isEmpty()) {
        emit noteAdded(newNoteMap);
    }
    
    return success;
}

bool DatabaseManager::updateNote(int id, const QString& title, const QString& content, const QStringList& tags,
                                const QString& color, int categoryId) {
    bool success = false;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QSqlQuery query(m_db);
        QString sql = "UPDATE notes SET title=:title, content=:content, tags=:tags, updated_at=:updated_at";
        if (!color.isEmpty()) sql += ", color=:color";
        if (categoryId != -1) sql += ", category_id=:category_id";
        sql += " WHERE id=:id";

        query.prepare(sql);
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", tags.join(","));
        query.bindValue(":updated_at", currentTime);
        if (!color.isEmpty()) query.bindValue(":color", color);
        if (categoryId != -1) query.bindValue(":category_id", categoryId);
        query.bindValue(":id", id);
        
        success = query.exec();
    }

    if (success) emit noteUpdated();
    return success;
}

bool DatabaseManager::deleteNotesBatch(const QList<int>& ids) {
    if (ids.isEmpty()) return true;
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        m_db.transaction();

        QSqlQuery check(m_db);
        check.prepare("SELECT tags, is_favorite, is_locked FROM notes WHERE id = :id");

        QSqlQuery query(m_db);
        query.prepare("DELETE FROM notes WHERE id = :id");

        for (int id : ids) {
            // 凡是数据被绑定标签或绑定书签或保护(锁定)时, 不可被删除
            check.bindValue(":id", id);
            if (check.exec() && check.next()) {
                if (!check.value(0).toString().isEmpty() || check.value(1).toBool() || check.value(2).toBool()) {
                    continue;
                }
            }
            query.bindValue(":id", id);
            query.exec();
        }
        success = m_db.commit();
    }
    if (success) emit noteUpdated();
    return success;
}

// 【修复核心】防止死锁的 updateNoteState
bool DatabaseManager::updateNoteState(int id, const QString& column, const QVariant& value) {
    bool success = false;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;
        
        QStringList allowedColumns = {"is_pinned", "is_locked", "is_favorite", "is_deleted", "tags", "rating", "category_id", "color"};
        if (!allowedColumns.contains(column)) return false;

        QSqlQuery query(m_db);
        
        // 增强逻辑：同步更新颜色 (对标 Python)
        if (column == "is_favorite") {
            bool fav = value.toBool();
            QString color = fav ? "#ff6b81" : ""; 
            if (!fav) {
                // 恢复分类颜色
                QSqlQuery catQuery(m_db);
                catQuery.prepare("SELECT c.color FROM categories c JOIN notes n ON n.category_id = c.id WHERE n.id = :id");
                catQuery.bindValue(":id", id);
                if (catQuery.exec() && catQuery.next()) color = catQuery.value(0).toString();
                else color = "#0A362F"; // 默认未分类色
            }
            query.prepare("UPDATE notes SET is_favorite = :val, color = :color, updated_at = :now WHERE id = :id");
            query.bindValue(":color", color);
        } else if (column == "is_deleted") {
            bool del = value.toBool();
            QString color = del ? "#2d2d2d" : "#0A362F";
            query.prepare("UPDATE notes SET is_deleted = :val, color = :color, category_id = NULL, updated_at = :now WHERE id = :id");
            query.bindValue(":color", color);
        } else {
            query.prepare(QString("UPDATE notes SET %1 = :val, updated_at = :now WHERE id = :id").arg(column));
        }
        
        query.bindValue(":val", value);
        query.bindValue(":now", currentTime);
        query.bindValue(":id", id);
        
        success = query.exec();
    } 

    if (success) emit noteUpdated();
    return success;
}

bool DatabaseManager::updateNoteStateBatch(const QList<int>& ids, const QString& column, const QVariant& value) {
    if (ids.isEmpty()) return true;
    bool success = false;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;
        
        QStringList allowedColumns = {"is_pinned", "is_locked", "is_favorite", "is_deleted", "tags", "rating", "category_id"};
        if (!allowedColumns.contains(column)) return false;

        m_db.transaction();

        QSqlQuery query(m_db);

        QString sql;
        if (column == "is_deleted") {
            bool del = value.toBool();
            QString color = del ? "#2d2d2d" : "#0A362F";
            sql = QString("UPDATE notes SET is_deleted = :val, color = '%1', category_id = NULL, updated_at = :updated_at WHERE id = :id").arg(color);
        } else if (column == "is_favorite") {
            // 注意：Batch 收藏暂不处理复杂的颜色恢复逻辑，简单设置颜色
            bool fav = value.toBool();
            QString color = fav ? "#ff6b81" : "#0A362F";
            sql = QString("UPDATE notes SET is_favorite = :val, color = '%1', updated_at = :updated_at WHERE id = :id").arg(color);
        } else {
            sql = QString("UPDATE notes SET %1 = :val, updated_at = :updated_at WHERE id = :id").arg(column);
        }

        query.prepare(sql);
        for (int id : ids) {
            query.bindValue(":val", value);
            query.bindValue(":updated_at", currentTime);
            query.bindValue(":id", id);
            query.exec();
        }
        success = m_db.commit();
    }
    if (success) emit noteUpdated();
    return success;
}

bool DatabaseManager::toggleNoteState(int id, const QString& column) {
    QVariant currentVal;
    {
        QMutexLocker locker(&m_mutex);
        QSqlQuery query(m_db);
        query.prepare(QString("SELECT %1 FROM notes WHERE id = :id").arg(column));
        query.bindValue(":id", id);
        if (query.exec() && query.next()) currentVal = query.value(0);
    }
    if (currentVal.isValid()) {
        return updateNoteState(id, column, !currentVal.toBool());
    }
    return false;
}

bool DatabaseManager::moveNoteToCategory(int noteId, int catId) {
    return moveNotesToCategory({noteId}, catId);
}

bool DatabaseManager::moveNotesToCategory(const QList<int>& noteIds, int catId) {
    if (noteIds.isEmpty()) return true;
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;

    m_db.transaction();
    QString catColor = "#0A362F"; 
    QString presetTags;
    if (catId != -1) {
        QSqlQuery catQuery(m_db);
        catQuery.prepare("SELECT color, preset_tags FROM categories WHERE id = :id");
        catQuery.bindValue(":id", catId);
        if (catQuery.exec() && catQuery.next()) {
            catColor = catQuery.value(0).toString();
            presetTags = catQuery.value(1).toString();
        }
    }

    QSqlQuery query(m_db);
    query.prepare("UPDATE notes SET category_id = :cat_id, color = :color, is_deleted = 0, updated_at = :now WHERE id = :id");
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    
    for (int id : noteIds) {
        query.bindValue(":cat_id", catId == -1 ? QVariant() : catId);
        query.bindValue(":color", catColor);
        query.bindValue(":now", now);
        query.bindValue(":id", id);
        query.exec();
        
        if (!presetTags.isEmpty()) {
            // Apply preset tags (internal without lock)
            QSqlQuery fetchTags(m_db);
            fetchTags.prepare("SELECT tags FROM notes WHERE id = :id");
            fetchTags.bindValue(":id", id);
            if (fetchTags.exec() && fetchTags.next()) {
                QString existing = fetchTags.value(0).toString();
                QStringList tagList = existing.split(",", Qt::SkipEmptyParts);
                QStringList newTags = presetTags.split(",", Qt::SkipEmptyParts);
                bool changed = false;
                for (const QString& t : newTags) {
                    if (!tagList.contains(t.trimmed())) {
                        tagList.append(t.trimmed());
                        changed = true;
                    }
                }
                if (changed) {
                    QSqlQuery updateTags(m_db);
                    updateTags.prepare("UPDATE notes SET tags = :tags WHERE id = :id");
                    updateTags.bindValue(":tags", tagList.join(","));
                    updateTags.bindValue(":id", id);
                    updateTags.exec();
                }
            }
        }
    }
    bool success = m_db.commit();
    if (success) emit noteUpdated();
    return success;
}

// 【修复核心】防止死锁的 deleteNote
bool DatabaseManager::deleteNote(int id) {
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        // 保护检查
        QSqlQuery check(m_db);
        check.prepare("SELECT tags, is_favorite, is_locked FROM notes WHERE id = :id");
        check.bindValue(":id", id);
        if (check.exec() && check.next()) {
            if (!check.value(0).toString().isEmpty() || check.value(1).toBool() || check.value(2).toBool()) {
                return false;
            }
        }

        QSqlQuery query(m_db);
        query.prepare("DELETE FROM notes WHERE id=:id");
        query.bindValue(":id", id);
        success = query.exec();
    } // 自动解锁

    if (success) emit noteUpdated();
    return success;
}

void DatabaseManager::addNoteAsync(const QString& title, const QString& content, const QStringList& tags,
                                 const QString& color, int categoryId,
                                 const QString& itemType, const QByteArray& dataBlob,
                                 const QString& sourceApp, const QString& sourceTitle) {
    QMetaObject::invokeMethod(this, [this, title, content, tags, color, categoryId, itemType, dataBlob, sourceApp, sourceTitle]() {
        addNote(title, content, tags, color, categoryId, itemType, dataBlob, sourceApp, sourceTitle);
    }, Qt::QueuedConnection);
}

QList<QVariantMap> DatabaseManager::searchNotes(const QString& keyword, const QString& filterType, int filterValue, int page, int pageSize) {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    if (!m_db.isOpen()) return results;

    QString baseSql = "SELECT notes.* FROM notes ";
    QString whereClause = "WHERE is_deleted = 0 ";
    QVariantList params;

    if (filterType == "category") {
        if (filterValue == -1) whereClause += "AND category_id IS NULL ";
        else {
            whereClause += "AND category_id = ? ";
            params << filterValue;
        }
    } else if (filterType == "today") {
        whereClause += "AND date(updated_at) = date('now', 'localtime') ";
    } else if (filterType == "bookmark") {
        whereClause += "AND is_favorite = 1 ";
    } else if (filterType == "trash") {
        whereClause = "WHERE is_deleted = 1 ";
    } else if (filterType == "untagged") {
        whereClause += "AND (tags IS NULL OR tags = '') ";
    }

    if (!keyword.isEmpty()) {
        baseSql += "JOIN notes_fts ON notes.id = notes_fts.rowid ";
        whereClause += "AND notes_fts MATCH ? ";
        params << keyword;
    }

    QString finalSql = baseSql + whereClause + "ORDER BY is_pinned DESC, updated_at DESC";
    
    if (page > 0) {
        finalSql += QString(" LIMIT %1 OFFSET %2").arg(pageSize).arg((page - 1) * pageSize);
    }

    QSqlQuery query(m_db);
    query.prepare(finalSql);
    for (int i = 0; i < params.size(); ++i) query.bindValue(i, params[i]);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap map;
            QSqlRecord rec = query.record();
            for (int i = 0; i < rec.count(); ++i) map[rec.fieldName(i)] = query.value(i);
            results.append(map);
        }
    }
    return results;
}

int DatabaseManager::getNotesCount(const QString& keyword, const QString& filterType, int filterValue) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return 0;

    QString baseSql = "SELECT COUNT(*) FROM notes ";
    QString whereClause = "WHERE is_deleted = 0 ";
    QVariantList params;

    if (filterType == "category") {
        if (filterValue == -1) whereClause += "AND category_id IS NULL ";
        else {
            whereClause += "AND category_id = ? ";
            params << filterValue;
        }
    } else if (filterType == "today") {
        whereClause += "AND date(updated_at) = date('now', 'localtime') ";
    } else if (filterType == "bookmark") {
        whereClause += "AND is_favorite = 1 ";
    } else if (filterType == "trash") {
        whereClause = "WHERE is_deleted = 1 ";
    } else if (filterType == "untagged") {
        whereClause += "AND (tags IS NULL OR tags = '') ";
    }

    if (!keyword.isEmpty()) {
        baseSql += "JOIN notes_fts ON notes.id = notes_fts.rowid ";
        whereClause += "AND notes_fts MATCH ? ";
        params << keyword;
    }

    QSqlQuery query(m_db);
    query.prepare(baseSql + whereClause);
    for (int i = 0; i < params.size(); ++i) query.bindValue(i, params[i]);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QList<QVariantMap> DatabaseManager::getAllNotes() {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    if (!m_db.isOpen()) return results;

    QSqlQuery query(m_db);
    if (query.exec("SELECT * FROM notes WHERE is_deleted = 0 ORDER BY is_pinned DESC, updated_at DESC")) {
        while (query.next()) {
            QVariantMap map;
            QSqlRecord rec = query.record();
            for (int i = 0; i < rec.count(); ++i) {
                map[rec.fieldName(i)] = query.value(i);
            }
            results.append(map);
        }
    }
    return results;
}

QStringList DatabaseManager::getAllTags() {
    QMutexLocker locker(&m_mutex);
    QStringList allTags;
    if (!m_db.isOpen()) return allTags;

    QSqlQuery query(m_db);
    if (query.exec("SELECT tags FROM notes WHERE tags != '' AND is_deleted = 0")) {
        while (query.next()) {
            QString tagsStr = query.value(0).toString();
            QStringList parts = tagsStr.split(",", Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                QString trimmed = part.trimmed();
                if (!allTags.contains(trimmed)) {
                    allTags.append(trimmed);
                }
            }
        }
    }
    allTags.sort();
    return allTags;
}
int DatabaseManager::addCategory(const QString& name, int parentId, const QString& color) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return -1;

    QString chosenColor = color;
    if (chosenColor.isEmpty()) {
        // 使用 Python 版的调色盘
        static const QStringList palette = {
            "#FF6B6B", "#4ECDC4", "#45B7D1", "#96CEB4", "#FFEEAD",
            "#D4A5A5", "#9B59B6", "#3498DB", "#E67E22", "#2ECC71",
            "#E74C3C", "#F1C40F", "#1ABC9C", "#34495E", "#95A5A6"
        };
        chosenColor = palette.at(QRandomGenerator::global()->bounded(palette.size()));
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO categories (name, parent_id, color) VALUES (:name, :parent_id, :color)");
    query.bindValue(":name", name);
    query.bindValue(":parent_id", parentId == -1 ? QVariant(QMetaType::fromType<int>()) : parentId);
    query.bindValue(":color", chosenColor);

    if (query.exec()) {
        emit categoriesChanged();
        return query.lastInsertId().toInt();
    }
    return -1;
}

bool DatabaseManager::renameCategory(int id, const QString& name) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("UPDATE categories SET name=:name WHERE id=:id");
    query.bindValue(":name", name);
    query.bindValue(":id", id);
    bool success = query.exec();
    if (success) emit categoriesChanged();
    return success;
}

bool DatabaseManager::setCategoryColor(int id, const QString& color) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    
    m_db.transaction();
    QSqlQuery treeQuery(m_db);
    treeQuery.prepare(R"(
        WITH RECURSIVE category_tree(id) AS (
            SELECT :id
            UNION ALL
            SELECT c.id FROM categories c JOIN category_tree ct ON c.parent_id = ct.id
        )
        SELECT id FROM category_tree
    )");
    treeQuery.bindValue(":id", id);
    
    QList<int> allIds;
    if (treeQuery.exec()) {
        while (treeQuery.next()) allIds << treeQuery.value(0).toInt();
    }
    
    if (!allIds.isEmpty()) {
        QString placeholders;
        for(int i=0; i<allIds.size(); ++i) placeholders += (i==0 ? "?" : ",?");
        
        QSqlQuery updateNotes(m_db);
        updateNotes.prepare(QString("UPDATE notes SET color = ? WHERE category_id IN (%1)").arg(placeholders));
        updateNotes.addBindValue(color);
        for(int cid : allIds) updateNotes.addBindValue(cid);
        updateNotes.exec();
        
        QSqlQuery updateCats(m_db);
        updateCats.prepare(QString("UPDATE categories SET color = ? WHERE id IN (%1)").arg(placeholders));
        updateCats.addBindValue(color);
        for(int cid : allIds) updateCats.addBindValue(cid);
        updateCats.exec();
    }
    
    bool success = m_db.commit();
    if (success) {
        emit categoriesChanged();
        emit noteUpdated();
    }
    return success;
}

bool DatabaseManager::deleteCategory(int id) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM categories WHERE id=:id");
    query.bindValue(":id", id);
    if (query.exec()) {
        QSqlQuery updateNotes(m_db);
        updateNotes.prepare("UPDATE notes SET category_id = NULL WHERE category_id = :id");
        updateNotes.bindValue(":id", id);
        updateNotes.exec();
        emit categoriesChanged();
        return true;
    }
    return false;
}

QList<QVariantMap> DatabaseManager::getAllCategories() {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    if (!m_db.isOpen()) return results;
    QSqlQuery query(m_db);
    if (query.exec("SELECT * FROM categories ORDER BY sort_order, name")) {
        while (query.next()) {
            QVariantMap map;
            QSqlRecord rec = query.record();
            for (int i = 0; i < rec.count(); ++i) map[rec.fieldName(i)] = query.value(i);
            results.append(map);
        }
    }
    return results;
}

bool DatabaseManager::emptyTrash() {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    // 依然遵循保护原则，含有标签/书签/锁定的内容即使在回收站中也不允许物理删除
    return query.exec("DELETE FROM notes WHERE is_deleted = 1 AND (tags IS NULL OR tags = '') AND is_favorite = 0 AND is_locked = 0");
}

bool DatabaseManager::restoreAllFromTrash() {
    bool success = false;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;
        QSqlQuery query(m_db);
        // 恢复所有已删除项：设为未分类(NULL)，恢复未分类色(#0A362F)，is_deleted设为0
        query.prepare("UPDATE notes SET is_deleted = 0, category_id = NULL, color = '#0A362F', updated_at = :now WHERE is_deleted = 1");
        query.bindValue(":now", currentTime);
        success = query.exec();
    }
    if (success) emit noteUpdated();
    return success;
}

bool DatabaseManager::setCategoryPresetTags(int catId, const QString& tags) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("UPDATE categories SET preset_tags=:tags WHERE id=:id");
    query.bindValue(":tags", tags);
    query.bindValue(":id", catId);
    return query.exec();
}

QString DatabaseManager::getCategoryPresetTags(int catId) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return "";
    QSqlQuery query(m_db);
    query.prepare("SELECT preset_tags FROM categories WHERE id=:id");
    query.bindValue(":id", catId);
    if (query.exec() && query.next()) return query.value(0).toString();
    return "";
}

QVariantMap DatabaseManager::getNoteById(int id) {
    QMutexLocker locker(&m_mutex);
    QVariantMap map;
    if (!m_db.isOpen()) return map;
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM notes WHERE id = :id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i) map[rec.fieldName(i)] = query.value(i);
    }
    return map;
}

QVariantMap DatabaseManager::getCounts() {
    QMutexLocker locker(&m_mutex);
    QVariantMap counts;
    if (!m_db.isOpen()) return counts;
    QSqlQuery query(m_db);
    
    auto getCount = [&](const QString& where) {
        if (query.exec("SELECT COUNT(*) FROM notes WHERE " + where)) {
            if (query.next()) return query.value(0).toInt();
        }
        return 0;
    };

    counts["all"] = getCount("is_deleted = 0");
    counts["today"] = getCount("is_deleted = 0 AND date(updated_at) = date('now', 'localtime')");
    counts["uncategorized"] = getCount("is_deleted = 0 AND category_id IS NULL");
    counts["untagged"] = getCount("is_deleted = 0 AND (tags IS NULL OR tags = '')");
    counts["bookmark"] = getCount("is_deleted = 0 AND is_favorite = 1");
    counts["trash"] = getCount("is_deleted = 1");

    // 获取分类统计
    if (query.exec("SELECT category_id, COUNT(*) FROM notes WHERE is_deleted = 0 AND category_id IS NOT NULL GROUP BY category_id")) {
        while (query.next()) {
            counts["cat_" + query.value(0).toString()] = query.value(1).toInt();
        }
    }

    return counts;
}

bool DatabaseManager::addTagsToNote(int noteId, const QStringList& tags) {
    // 简写，直接更新 notes 表的 tags 字段
    QVariantMap note = getNoteById(noteId);
    if (note.isEmpty()) return false;
    QStringList existing = note["tags"].toString().split(",", Qt::SkipEmptyParts);
    for (const QString& t : tags) {
        if (!existing.contains(t.trimmed())) existing.append(t.trimmed());
    }
    return updateNoteState(noteId, "tags", existing.join(","));
}

bool DatabaseManager::removeTagFromNote(int noteId, const QString& tag) {
    QVariantMap note = getNoteById(noteId);
    if (note.isEmpty()) return false;
    QStringList existing = note["tags"].toString().split(",", Qt::SkipEmptyParts);
    existing.removeAll(tag.trimmed());
    return updateNoteState(noteId, "tags", existing.join(","));
}
