#include "DatabaseManager.h"
#include <QDebug>
#include <QSqlRecord>
#include <QtConcurrent>

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
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    if (!createTables()) return false;

    // 如果数据库为空，添加一条欢迎信息
    QSqlQuery query("SELECT COUNT(*) FROM notes");
    if (query.next() && query.value(0).toInt() == 0) {
        addNote("欢迎使用极速灵感", "这是一条自动生成的欢迎笔记。你可以点击悬浮球或按 Alt+Space 开始记录！", {"入门"});
    }

    return true;
}

bool DatabaseManager::createTables() {
    QSqlQuery query;

    // 普通笔记表
    QString createNotesTable = R"(
        CREATE TABLE IF NOT EXISTS notes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT,
            content TEXT,
            tags TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            is_pinned INTEGER DEFAULT 0,
            is_locked INTEGER DEFAULT 0,
            is_favorite INTEGER DEFAULT 0
        )
    )";

    if (!query.exec(createNotesTable)) {
        qCritical() << "创建 notes 表失败:" << query.lastError().text();
        return false;
    }

    // FTS5 全文搜索虚拟表
    QString createFtsTable = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(
            title, content, content='notes', content_rowid='id'
        )
    )";

    if (!query.exec(createFtsTable)) {
        qWarning() << "创建 FTS5 表失败 (可能不支持 FTS5):" << query.lastError().text();
    }

    // 触发器：同步普通表和 FTS 表
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_ai AFTER INSERT ON notes BEGIN "
               "INSERT INTO notes_fts(rowid, title, content) VALUES (new.id, new.title, new.content); END;");
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_ad AFTER DELETE ON notes BEGIN "
               "INSERT INTO notes_fts(notes_fts, rowid, title, content) VALUES('delete', old.id, old.title, old.content); END;");
    query.exec("CREATE TRIGGER IF NOT EXISTS notes_au AFTER UPDATE ON notes BEGIN "
               "INSERT INTO notes_fts(notes_fts, rowid, title, content) VALUES('delete', old.id, old.title, old.content); "
               "INSERT INTO notes_fts(rowid, title, content) VALUES (new.id, new.title, new.content); END;");

    return true;
}

bool DatabaseManager::addNote(const QString& title, const QString& content, const QStringList& tags) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query;
    query.prepare("INSERT INTO notes (title, content, tags) VALUES (:title, :content, :tags)");
    query.bindValue(":title", title);
    query.bindValue(":content", content);
    query.bindValue(":tags", tags.join(","));

    if (!query.exec()) {
        qCritical() << "添加笔记失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateNote(int id, const QString& title, const QString& content, const QStringList& tags) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query;
    query.prepare("UPDATE notes SET title=:title, content=:content, tags=:tags, updated_at=CURRENT_TIMESTAMP WHERE id=:id");
    query.bindValue(":title", title);
    query.bindValue(":content", content);
    query.bindValue(":tags", tags.join(","));
    query.bindValue(":id", id);
    return query.exec();
}

bool DatabaseManager::deleteNote(int id) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query;
    query.prepare("DELETE FROM notes WHERE id=:id");
    query.bindValue(":id", id);
    return query.exec();
}

void DatabaseManager::addNoteAsync(const QString& title, const QString& content, const QStringList& tags) {
    (void)QtConcurrent::run([this, title, content, tags]() {
        if (addNote(title, content, tags)) {
            emit noteAdded();
        }
    });
}

QList<QVariantMap> DatabaseManager::searchNotes(const QString& keyword) {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    QSqlQuery query;

    // 使用 FTS5 进行高效搜索
    query.prepare("SELECT notes.* FROM notes JOIN notes_fts ON notes.id = notes_fts.rowid WHERE notes_fts MATCH :keyword ORDER BY rank");
    query.bindValue(":keyword", keyword);

    if (!query.exec()) {
        // 退而求其次使用 LIKE
        query.prepare("SELECT * FROM notes WHERE title LIKE :keyword OR content LIKE :keyword");
        query.bindValue(":keyword", "%" + keyword + "%");
        query.exec();
    }

    while (query.next()) {
        QVariantMap map;
        QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i) {
            map[rec.fieldName(i)] = query.value(i);
        }
        results.append(map);
    }
    return results;
}

QList<QVariantMap> DatabaseManager::getAllNotes() {
    QMutexLocker locker(&m_mutex);
    QList<QVariantMap> results;
    QSqlQuery query("SELECT * FROM notes ORDER BY updated_at DESC");

    while (query.next()) {
        QVariantMap map;
        QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i) {
            map[rec.fieldName(i)] = query.value(i);
        }
        results.append(map);
    }
    return results;
}
