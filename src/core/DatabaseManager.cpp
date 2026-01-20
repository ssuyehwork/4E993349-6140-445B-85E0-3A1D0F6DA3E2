#include "DatabaseManager.h"
#include <QDebug>
#include <QSqlRecord>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QFile>
#include <QDir>

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
        QFile::copy(dbPath, backupPath);
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
            is_deleted INTEGER DEFAULT 0
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
        "ALTER TABLE notes ADD COLUMN rating INTEGER DEFAULT 0"
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

// 【修复核心】防止死锁的 addNote
bool DatabaseManager::addNote(const QString& title, const QString& content, const QStringList& tags,
                             const QString& color, int categoryId,
                             const QString& itemType, const QByteArray& dataBlob) {
    QVariantMap newNoteMap;
    bool success = false;

    {   // === 锁的作用域开始 ===
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QSqlQuery query(m_db);
        query.prepare("INSERT INTO notes (title, content, tags, color, category_id, item_type, data_blob) "
                      "VALUES (:title, :content, :tags, :color, :category_id, :item_type, :data_blob)");
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", tags.join(","));
        query.bindValue(":color", color.isEmpty() ? "#2d2d2d" : color);
        query.bindValue(":category_id", categoryId == -1 ? QVariant(QMetaType(QMetaType::Int)) : categoryId);
        query.bindValue(":item_type", itemType);
        query.bindValue(":data_blob", dataBlob);

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
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QSqlQuery query(m_db);
        QString sql = "UPDATE notes SET title=:title, content=:content, tags=:tags, updated_at=CURRENT_TIMESTAMP";
        if (!color.isEmpty()) sql += ", color=:color";
        if (categoryId != -1) sql += ", category_id=:category_id";
        sql += " WHERE id=:id";

        query.prepare(sql);
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", tags.join(","));
        if (!color.isEmpty()) query.bindValue(":color", color);
        if (categoryId != -1) query.bindValue(":category_id", categoryId);
        query.bindValue(":id", id);

        success = query.exec();
    }

    if (success) emit noteUpdated();
    return success;
}

// 【修复核心】防止死锁的 updateNoteState
bool DatabaseManager::updateNoteState(int id, const QString& column, const QVariant& value) {
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        QStringList allowedColumns = {"is_pinned", "is_locked", "is_favorite", "is_deleted"};
        if (!allowedColumns.contains(column)) return false;

        QSqlQuery query(m_db);
        QString sql = QString("UPDATE notes SET %1 = :val WHERE id = :id").arg(column);
        query.prepare(sql);
        query.bindValue(":val", value);
        query.bindValue(":id", id);

        success = query.exec();
    } // 自动解锁

    if (success) emit noteUpdated();
    return success;
}

// 【修复核心】防止死锁的 deleteNote
bool DatabaseManager::deleteNote(int id) {
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

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
                                 const QString& itemType, const QByteArray& dataBlob) {
    QMetaObject::invokeMethod(this, [this, title, content, tags, color, categoryId, itemType, dataBlob]() {
        addNote(title, content, tags, color, categoryId, itemType, dataBlob);
    }, Qt::QueuedConnection);
}

QList<QVariantMap> DatabaseManager::searchNotes(const QString& keyword, const QString& filterType, int filterValue) {
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
        whereClause += "AND date(updated_at, 'localtime') = date('now', 'localtime') ";
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

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO categories (name, parent_id, color) VALUES (:name, :parent_id, :color)");
    query.bindValue(":name", name);
    query.bindValue(":parent_id", parentId == -1 ? QVariant(QMetaType(QMetaType::Int)) : parentId);
    query.bindValue(":color", color.isEmpty() ? "#808080" : color);

    if (query.exec()) return query.lastInsertId().toInt();
    return -1;
}

bool DatabaseManager::updateCategory(int id, const QString& name, const QString& color) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("UPDATE categories SET name=:name, color=:color WHERE id=:id");
    query.bindValue(":name", name);
    query.bindValue(":color", color);
    query.bindValue(":id", id);
    return query.exec();
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
    counts["today"] = getCount("is_deleted = 0 AND date(updated_at, 'localtime') = date('now', 'localtime')");
    counts["uncategorized"] = getCount("is_deleted = 0 AND category_id IS NULL");
    counts["untagged"] = getCount("is_deleted = 0 AND (tags IS NULL OR tags = '')");
    counts["bookmark"] = getCount("is_deleted = 0 AND is_favorite = 1");
    counts["trash"] = getCount("is_deleted = 1");

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
