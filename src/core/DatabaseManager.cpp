#include "DatabaseManager.h"
#include <QDebug>
#include <QSqlRecord>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QRandomGenerator>

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

void DatabaseManager::close() {
    QMutexLocker locker(&m_mutex);
    if (m_db.isOpen()) {
        m_db.close();
    }
    // 移除数据库连接，防止残留文件句柄
    QString connectionName = m_db.connectionName();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
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
            preset_tags TEXT,
            password TEXT,
            password_hint TEXT
        )
    )";
    query.exec(createCategoriesTable);

    // 补全缺失列
    query.exec("ALTER TABLE categories ADD COLUMN password TEXT");
    query.exec("ALTER TABLE categories ADD COLUMN password_hint TEXT");

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
        QString finalColor = color.isEmpty() ? "#2d2d2d" : color;
        QStringList finalTags = tags;

        // 获取分类预设标签和颜色 (如果指定了分类且未指定颜色)
        if (categoryId != -1) {
            QSqlQuery catQuery(m_db);
            catQuery.prepare("SELECT color, preset_tags FROM categories WHERE id = :id");
            catQuery.bindValue(":id", categoryId);
            if (catQuery.exec() && catQuery.next()) {
                if (color.isEmpty()) finalColor = catQuery.value(0).toString();
                QString preset = catQuery.value(1).toString();
                if (!preset.isEmpty()) {
                    QStringList pTags = preset.split(",", Qt::SkipEmptyParts);
                    for (const QString& t : pTags) {
                        QString trimmed = t.trimmed();
                        if (!finalTags.contains(trimmed)) finalTags << trimmed;
                    }
                }
            }
        }

        QSqlQuery query(m_db);
        query.prepare("INSERT INTO notes (title, content, tags, color, category_id, item_type, data_blob, "
                      "content_hash, created_at, updated_at, source_app, source_title) "
                      "VALUES (:title, :content, :tags, :color, :category_id, :item_type, :data_blob, "
                      ":hash, :created_at, :updated_at, :source_app, :source_title)");
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", finalTags.join(","));
        query.bindValue(":color", finalColor);
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
        if (!color.isEmpty()) {
            sql += ", color=:color";
        } else if (categoryId != -1) {
            // 如果只修改分类未指定颜色，自动继承分类颜色
            QSqlQuery catQuery(m_db);
            catQuery.prepare("SELECT color FROM categories WHERE id = :id");
            catQuery.bindValue(":id", categoryId);
            if (catQuery.exec() && catQuery.next()) {
                sql += ", color=:color";
            }
        }

        if (categoryId != -1) sql += ", category_id=:category_id";
        sql += " WHERE id=:id";

        query.prepare(sql);
        query.bindValue(":title", title);
        query.bindValue(":content", content);
        query.bindValue(":tags", tags.join(","));
        query.bindValue(":updated_at", currentTime);
        
        if (!color.isEmpty()) {
            query.bindValue(":color", color);
        } else if (categoryId != -1) {
            QSqlQuery catQuery(m_db);
            catQuery.prepare("SELECT color FROM categories WHERE id = :id");
            catQuery.bindValue(":id", categoryId);
            if (catQuery.exec() && catQuery.next()) {
                query.bindValue(":color", catQuery.value(0).toString());
            }
        }
        if (categoryId != -1) query.bindValue(":category_id", categoryId);
        query.bindValue(":id", id);
        
        success = query.exec();
    }

    if (success) emit noteUpdated();
    return success;
}

bool DatabaseManager::setCategoryPassword(int id, const QString& password, const QString& hint) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;

    QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());

    QSqlQuery query(m_db);
    query.prepare("UPDATE categories SET password=:password, password_hint=:hint WHERE id=:id");
    query.bindValue(":password", hashedPassword);
    query.bindValue(":hint", hint);
    query.bindValue(":id", id);
    bool success = query.exec();
    if (success) {
        emit categoriesChanged();
    }
    return success;
}

bool DatabaseManager::removeCategoryPassword(int id) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    query.prepare("UPDATE categories SET password=NULL, password_hint=NULL WHERE id=:id");
    query.bindValue(":id", id);
    bool success = query.exec();
    if (success) {
        m_unlockedCategories.remove(id);
        emit categoriesChanged();
    }
    return success;
}

bool DatabaseManager::verifyCategoryPassword(int id, const QString& password) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;

    QString hashedPassword = QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());

    QSqlQuery query(m_db);
    query.prepare("SELECT password FROM categories WHERE id=:id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        QString actualPwd = query.value(0).toString();
        if (actualPwd == hashedPassword) {
            unlockCategory(id); // 使用统一解锁方法，确保发出 categoriesChanged 信号
            return true;
        }
    }
    return false;
}

bool DatabaseManager::isCategoryLocked(int id) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;

    // 如果已经在已解锁列表中，则未锁定
    if (m_unlockedCategories.contains(id)) return false;

    // 检查数据库中是否有密码
    QSqlQuery query(m_db);
    query.prepare("SELECT password FROM categories WHERE id=:id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return !query.value(0).toString().isEmpty();
    }
    return false;
}

void DatabaseManager::lockCategory(int id) {
    QMutexLocker locker(&m_mutex);
    m_unlockedCategories.remove(id);
    emit categoriesChanged();
}

void DatabaseManager::unlockCategory(int id) {
    QMutexLocker locker(&m_mutex);
    m_unlockedCategories.insert(id);
    emit categoriesChanged();
}

bool DatabaseManager::restoreAllFromTrash() {
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;
        QSqlQuery query(m_db);
        // 恢复所有回收站数据到未分类 (category_id = NULL, is_deleted = 0)
        // 颜色恢复为默认未分类色 #0A362F
        success = query.exec("UPDATE notes SET is_deleted = 0, category_id = NULL, color = '#0A362F' WHERE is_deleted = 1");
    }
    if (success) {
        emit noteUpdated();
        emit categoriesChanged();
    }
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
        } else if (column == "category_id") {
            // 处理分类变更时的颜色同步
            int catId = value.isNull() ? -1 : value.toInt();
            QString color = "#0A362F"; // 默认未分类色
            if (catId != -1) {
                QSqlQuery catQuery(m_db);
                catQuery.prepare("SELECT color FROM categories WHERE id = :id");
                catQuery.bindValue(":id", catId);
                if (catQuery.exec() && catQuery.next()) color = catQuery.value(0).toString();
            }
            query.prepare("UPDATE notes SET category_id = :val, color = :color, is_deleted = 0, updated_at = :now WHERE id = :id");
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
        
        if (column == "category_id") {
            int catId = value.isNull() ? -1 : value.toInt();
            QString color = "#0A362F";
            if (catId != -1) {
                QSqlQuery catQuery(m_db);
                catQuery.prepare("SELECT color FROM categories WHERE id = :id");
                catQuery.bindValue(":id", catId);
                if (catQuery.exec() && catQuery.next()) color = catQuery.value(0).toString();
            }
            query.prepare("UPDATE notes SET category_id = :val, color = :color, is_deleted = 0, updated_at = :updated_at WHERE id = :id");
            
            for (int id : ids) {
                query.bindValue(":val", value);
                query.bindValue(":color", color);
                query.bindValue(":updated_at", currentTime);
                query.bindValue(":id", id);
                query.exec();
            }
        } else {
            QString sql = QString("UPDATE notes SET %1 = :val, updated_at = :updated_at WHERE id = :id").arg(column);
            query.prepare(sql);
            for (int id : ids) {
                query.bindValue(":val", value);
                query.bindValue(":updated_at", currentTime);
                query.bindValue(":id", id);
                query.exec();
            }
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

        QSqlQuery query(m_db);
        query.prepare("DELETE FROM notes WHERE id=:id");
        query.bindValue(":id", id);
        success = query.exec();
    } // 自动解锁

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
        QSqlQuery query(m_db);
        query.prepare("DELETE FROM notes WHERE id=:id");
        for (int id : ids) {
            query.bindValue(":id", id);
            query.exec();
        }
        success = m_db.commit();
    }
    if (success) emit noteUpdated();
    return success;
}

// 批量软删除：放入回收站
bool DatabaseManager::softDeleteNotes(const QList<int>& ids) {
    if (ids.isEmpty()) return true;
    bool success = false;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;

        m_db.transaction();
        QSqlQuery query(m_db);
        query.prepare("UPDATE notes SET is_deleted = 1, category_id = NULL, color = '#2d2d2d', "
                      "is_pinned = 0, is_favorite = 0, updated_at = :now WHERE id = :id");
        
        for (int id : ids) {
            query.bindValue(":now", currentTime);
            query.bindValue(":id", id);
            query.exec();
        }
        success = m_db.commit();
    }
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

QList<QVariantMap> DatabaseManager::searchNotes(const QString& keyword, const QString& filterType, const QVariant& filterValue, int page, int pageSize) {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    if (!m_db.isOpen()) return results;

    QString baseSql = "SELECT notes.* FROM notes ";
    QString whereClause = "WHERE is_deleted = 0 ";
    QVariantList params;

    // 安全过滤：排除所有锁定分类的数据（除非正在查看该特定分类且已通过 UI 层的锁定检查）
    if (filterType != "category" && filterType != "trash") {
        QSqlQuery catQuery(m_db);
        catQuery.exec("SELECT id FROM categories WHERE password IS NOT NULL AND password != ''");
        QList<int> lockedIds;
        while (catQuery.next()) {
            int cid = catQuery.value(0).toInt();
            if (!m_unlockedCategories.contains(cid)) lockedIds.append(cid);
        }
        if (!lockedIds.isEmpty()) {
            QStringList placeholders;
            for (int i = 0; i < lockedIds.size(); ++i) placeholders << "?";
            whereClause += QString("AND (category_id IS NULL OR category_id NOT IN (%1)) ").arg(placeholders.join(","));
            for (int id : lockedIds) params << id;
        }
    }

    if (filterType == "category") {
        if (filterValue.toInt() == -1) whereClause += "AND category_id IS NULL ";
        else {
            whereClause += "AND category_id = ? ";
            params << filterValue.toInt();
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
    
    // 回收站不分页，显示所有
    if (page > 0 && filterType != "trash") {
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

int DatabaseManager::getNotesCount(const QString& keyword, const QString& filterType, const QVariant& filterValue) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return 0;

    QString baseSql = "SELECT COUNT(*) FROM notes ";
    QString whereClause = "WHERE is_deleted = 0 ";
    QVariantList params;

    // 安全过滤：排除所有锁定分类的数据
    if (filterType != "category" && filterType != "trash") {
        QSqlQuery catQuery(m_db);
        catQuery.exec("SELECT id FROM categories WHERE password IS NOT NULL AND password != ''");
        QList<int> lockedIds;
        while (catQuery.next()) {
            int cid = catQuery.value(0).toInt();
            if (!m_unlockedCategories.contains(cid)) lockedIds.append(cid);
        }
        if (!lockedIds.isEmpty()) {
            QStringList placeholders;
            for (int i = 0; i < lockedIds.size(); ++i) placeholders << "?";
            whereClause += QString("AND (category_id IS NULL OR category_id NOT IN (%1)) ").arg(placeholders.join(","));
            for (int id : lockedIds) params << id;
        }
    }

    if (filterType == "category") {
        if (filterValue.toInt() == -1) whereClause += "AND category_id IS NULL ";
        else {
            whereClause += "AND category_id = ? ";
            params << filterValue.toInt();
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

    // 获取锁定分类 ID
    QSqlQuery catQuery(m_db);
    catQuery.exec("SELECT id FROM categories WHERE password IS NOT NULL AND password != ''");
    QList<int> lockedIds;
    while (catQuery.next()) {
        int cid = catQuery.value(0).toInt();
        if (!m_unlockedCategories.contains(cid)) lockedIds.append(cid);
    }

    QString sql = "SELECT * FROM notes WHERE is_deleted = 0 ";
    if (!lockedIds.isEmpty()) {
        QStringList ids;
        for (int id : lockedIds) ids << QString::number(id);
        sql += QString("AND (category_id IS NULL OR category_id NOT IN (%1)) ").arg(ids.join(","));
    }
    sql += "ORDER BY is_pinned DESC, updated_at DESC";

    QSqlQuery query(m_db);
    if (query.exec(sql)) {
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

QList<QVariantMap> DatabaseManager::getRecentTagsWithCounts(int limit) {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    if (!m_db.isOpen()) return results;

    struct TagData {
        QString name;
        int count = 0;
        QDateTime lastUsed;
    };
    QMap<QString, TagData> tagMap;

    QSqlQuery query(m_db);
    // 获取所有非删除笔记的标签和更新时间
    if (query.exec("SELECT tags, updated_at FROM notes WHERE tags != '' AND is_deleted = 0")) {
        while (query.next()) {
            QString tagsStr = query.value(0).toString();
            QDateTime updatedAt = query.value(1).toDateTime();
            QStringList parts = tagsStr.split(",", Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                QString name = part.trimmed();
                if (name.isEmpty()) continue;
                
                if (!tagMap.contains(name)) {
                    tagMap[name] = {name, 1, updatedAt};
                } else {
                    tagMap[name].count++;
                    if (updatedAt > tagMap[name].lastUsed) {
                        tagMap[name].lastUsed = updatedAt;
                    }
                }
            }
        }
    }

    // 转换为列表并排序
    QList<TagData> sortedList = tagMap.values();
    std::sort(sortedList.begin(), sortedList.end(), [](const TagData& a, const TagData& b) {
        if (a.lastUsed != b.lastUsed) return a.lastUsed > b.lastUsed; // 最近使用优先
        return a.count > b.count; // 其次按计数
    });

    // 取前 N 个
    int actualLimit = qMin(limit, (int)sortedList.size());
    for (int i = 0; i < actualLimit; ++i) {
        QVariantMap m;
        m["name"] = sortedList[i].name;
        m["count"] = sortedList[i].count;
        results.append(m);
    }

    return results;
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
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_db.isOpen()) return false;
        QSqlQuery query(m_db);
        // 修正逻辑：只要 is_deleted=1 就彻底删除，不再判断锁/收藏/标签
        // 用户既然显式清空回收站，就应该尊重其意愿
        success = query.exec("DELETE FROM notes WHERE is_deleted = 1");
    }
    if (success) emit noteUpdated();
    return success;
}

bool DatabaseManager::setCategoryPresetTags(int catId, const QString& tags) {
    QMutexLocker locker(&m_mutex);
    if (!m_db.isOpen()) return false;
    
    m_db.transaction();
    
    QSqlQuery query(m_db);
    query.prepare("UPDATE categories SET preset_tags=:tags WHERE id=:id");
    query.bindValue(":tags", tags);
    query.bindValue(":id", catId);
    
    if (!query.exec()) {
        m_db.rollback();
        return false;
    }

    // 只要设定了预设标签，旧数据也必须绑定
    if (!tags.isEmpty()) {
        QStringList newTagsList = tags.split(",", Qt::SkipEmptyParts);
        
        QSqlQuery fetchNotes(m_db);
        fetchNotes.prepare("SELECT id, tags FROM notes WHERE category_id = :catId AND is_deleted = 0");
        fetchNotes.bindValue(":catId", catId);
        
        if (fetchNotes.exec()) {
            while (fetchNotes.next()) {
                int noteId = fetchNotes.value(0).toInt();
                QString existingTagsStr = fetchNotes.value(1).toString();
                QStringList existingTags = existingTagsStr.split(",", Qt::SkipEmptyParts);
                
                bool changed = false;
                for (const QString& t : newTagsList) {
                    QString trimmed = t.trimmed();
                    if (!trimmed.isEmpty() && !existingTags.contains(trimmed)) {
                        existingTags.append(trimmed);
                        changed = true;
                    }
                }
                
                if (changed) {
                    QSqlQuery updateNote(m_db);
                    updateNote.prepare("UPDATE notes SET tags = :tags WHERE id = :id");
                    updateNote.bindValue(":tags", existingTags.join(","));
                    updateNote.bindValue(":id", noteId);
                    updateNote.exec();
                }
            }
        }
    }
    
    bool ok = m_db.commit();
    if (ok) {
        emit categoriesChanged();
        emit noteUpdated();
    }
    return ok;
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

QVariantMap DatabaseManager::getFilterStats(const QString& keyword, const QString& filterType, const QVariant& filterValue) {
    QMutexLocker locker(&m_mutex);
    QVariantMap stats;
    if (!m_db.isOpen()) return stats;

    QString whereClause = "WHERE 1=1 ";
    QVariantList params;

    if (filterType == "trash") {
        whereClause += "AND is_deleted = 1 ";
    } else {
        whereClause += "AND is_deleted = 0 ";
        if (filterType == "category") {
            if (filterValue.toInt() == -1) whereClause += "AND category_id IS NULL ";
            else {
                whereClause += "AND category_id = ? ";
                params << filterValue.toInt();
            }
        } else if (filterType == "today") {
            whereClause += "AND date(updated_at) = date('now', 'localtime') ";
        } else if (filterType == "bookmark") {
            whereClause += "AND is_favorite = 1 ";
        } else if (filterType == "untagged") {
            whereClause += "AND (tags IS NULL OR tags = '') ";
        }
    }

    if (!keyword.isEmpty()) {
        whereClause += "AND id IN (SELECT rowid FROM notes_fts WHERE notes_fts MATCH ?) ";
        params << keyword;
    }

    QSqlQuery query(m_db);

    // 1. 星级统计
    QMap<int, int> stars;
    query.prepare("SELECT rating, COUNT(*) FROM notes " + whereClause + " GROUP BY rating");
    for (int i = 0; i < params.size(); ++i) query.bindValue(i, params[i]);
    if (query.exec()) {
        while (query.next()) stars[query.value(0).toInt()] = query.value(1).toInt();
    }
    QVariantMap starsMap;
    for (auto it = stars.begin(); it != stars.end(); ++it) starsMap[QString::number(it.key())] = it.value();
    stats["stars"] = starsMap;

    // 2. 颜色统计
    QMap<QString, int> colors;
    query.prepare("SELECT color, COUNT(*) FROM notes " + whereClause + " GROUP BY color");
    for (int i = 0; i < params.size(); ++i) query.bindValue(i, params[i]);
    if (query.exec()) {
        while (query.next()) colors[query.value(0).toString()] = query.value(1).toInt();
    }
    QVariantMap colorsMap;
    for (auto it = colors.begin(); it != colors.end(); ++it) colorsMap[it.key()] = it.value();
    stats["colors"] = colorsMap;

    // 3. 类型统计
    QMap<QString, int> types;
    query.prepare("SELECT item_type, COUNT(*) FROM notes " + whereClause + " GROUP BY item_type");
    for (int i = 0; i < params.size(); ++i) query.bindValue(i, params[i]);
    if (query.exec()) {
        while (query.next()) types[query.value(0).toString()] = query.value(1).toInt();
    }
    QVariantMap typesMap;
    for (auto it = types.begin(); it != types.end(); ++it) typesMap[it.key()] = it.value();
    stats["types"] = typesMap;

    // 4. 标签统计
    QMap<QString, int> tags;
    query.prepare("SELECT tags FROM notes " + whereClause);
    for (int i = 0; i < params.size(); ++i) query.bindValue(i, params[i]);
    if (query.exec()) {
        while (query.next()) {
            QStringList parts = query.value(0).toString().split(",", Qt::SkipEmptyParts);
            for (const QString& t : parts) tags[t.trimmed()]++;
        }
    }
    QVariantMap tagsMap;
    for (auto it = tags.begin(); it != tags.end(); ++it) tagsMap[it.key()] = it.value();
    stats["tags"] = tagsMap;

    // 5. 日期统计 (创建时间)
    QVariantMap dateStats;
    auto getCount = [&](const QString& dateCond) {
        QSqlQuery q(m_db);
        q.prepare("SELECT COUNT(*) FROM notes " + whereClause + " AND " + dateCond);
        for (int i = 0; i < params.size(); ++i) q.bindValue(i, params[i]);
        if (q.exec() && q.next()) return q.value(0).toInt();
        return 0;
    };

    dateStats["today"] = getCount("date(created_at) = date('now', 'localtime')");
    dateStats["yesterday"] = getCount("date(created_at) = date('now', '-1 day', 'localtime')");
    dateStats["week"] = getCount("date(created_at) >= date('now', '-6 days', 'localtime')");
    dateStats["month"] = getCount("strftime('%Y-%m', created_at) = strftime('%Y-%m', 'now', 'localtime')");
    stats["date_create"] = dateStats;

    return stats;
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
