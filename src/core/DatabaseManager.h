#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariantList>
#include <QMutex>
#include <QStringList>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool init(const QString& dbPath = "rapid_notes.db");

    // 核心 CRUD 操作
    bool addNote(const QString& title, const QString& content, const QStringList& tags,
                 const QString& color = "", int categoryId = -1,
                 const QString& itemType = "text", const QByteArray& dataBlob = QByteArray());
    bool updateNote(int id, const QString& title, const QString& content, const QStringList& tags,
                    const QString& color = "", int categoryId = -1);
    bool deleteNote(int id);
    bool updateNoteState(int id, const QString& column, const QVariant& value);

    // 分类管理
    int addCategory(const QString& name, int parentId = -1, const QString& color = "");
    bool updateCategory(int id, const QString& name, const QString& color);
    bool deleteCategory(int id);
    QList<QVariantMap> getAllCategories();

    // 标签管理
    bool addTagsToNote(int noteId, const QStringList& tags);
    bool removeTagFromNote(int noteId, const QString& tag);

    // 搜索与查询
    QList<QVariantMap> searchNotes(const QString& keyword, const QString& filterType = "all", int filterValue = -1, int page = -1, int pageSize = 20);
    int getNotesCount(const QString& keyword, const QString& filterType = "all", int filterValue = -1);
    QList<QVariantMap> getAllNotes();
    QStringList getAllTags();
    QVariantMap getNoteById(int id);

    // 统计
    QVariantMap getCounts();

    // 异步操作
    void addNoteAsync(const QString& title, const QString& content, const QStringList& tags,
                      const QString& color = "", int categoryId = -1,
                      const QString& itemType = "text", const QByteArray& dataBlob = QByteArray());

signals:
    // 【修改】现在信号携带具体数据，实现增量更新
    void noteAdded(const QVariantMap& note);
    void noteUpdated(); // 用于普通刷新

private:
    DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createTables();

    QSqlDatabase m_db;
    QString m_dbPath;
    QMutex m_mutex;
};

#endif // DATABASEMANAGER_H