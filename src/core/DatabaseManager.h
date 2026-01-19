#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariantList>
#include <QMutex>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool init(const QString& dbPath = "rapid_notes.db");

    // 核心 CRUD 操作
    bool addNote(const QString& title, const QString& content, const QStringList& tags);
    bool updateNote(int id, const QString& title, const QString& content, const QStringList& tags);
    bool deleteNote(int id);

    // 搜索
    QList<QVariantMap> searchNotes(const QString& keyword);
    QList<QVariantMap> getAllNotes();

    // 异步操作
    void addNoteAsync(const QString& title, const QString& content, const QStringList& tags);

signals:
    void noteAdded();
    void notesLoaded(const QList<QVariantMap>& notes);

private:
    DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createTables();

    QSqlDatabase m_db;
    QMutex m_mutex;
};

#endif // DATABASEMANAGER_H
