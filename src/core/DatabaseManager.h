#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariantList>
#include <QRecursiveMutex>
#include <QStringList>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool init(const QString& dbPath = "rapid_notes.db");
    
    // 核心 CRUD 操作
    bool addNote(const QString& title, const QString& content, const QStringList& tags = QStringList(), 
                 const QString& color = "", int categoryId = -1, 
                 const QString& itemType = "text", const QByteArray& dataBlob = QByteArray(),
                 const QString& sourceApp = "", const QString& sourceTitle = "");
    bool updateNote(int id, const QString& title, const QString& content, const QStringList& tags, 
                    const QString& color = "", int categoryId = -1);
    bool deleteNote(int id);
    bool deleteNotesBatch(const QList<int>& ids);
    bool updateNoteState(int id, const QString& column, const QVariant& value);
    bool updateNoteStateBatch(const QList<int>& ids, const QString& column, const QVariant& value);
    bool toggleNoteState(int id, const QString& column);
    bool moveNoteToCategory(int noteId, int catId);
    bool moveNotesToCategory(const QList<int>& noteIds, int catId);

    // 分类管理
    int addCategory(const QString& name, int parentId = -1, const QString& color = "");
    bool renameCategory(int id, const QString& name);
    bool setCategoryColor(int id, const QString& color);
    bool deleteCategory(int id);
    QList<QVariantMap> getAllCategories();
    bool emptyTrash();
    bool restoreAllFromTrash();

    // 预设标签
    bool setCategoryPresetTags(int catId, const QString& tags);
    QString getCategoryPresetTags(int catId);

    // 标签管理
    bool addTagsToNote(int noteId, const QStringList& tags);
    bool removeTagFromNote(int noteId, const QString& tag);

    // 搜索与查询
    QList<QVariantMap> searchNotes(const QString& keyword, const QString& filterType = "all", const QVariant& filterValue = -1, int page = -1, int pageSize = 20);
    int getNotesCount(const QString& keyword, const QString& filterType = "all", const QVariant& filterValue = -1);
    QList<QVariantMap> getAllNotes();
    QStringList getAllTags();
    QVariantMap getNoteById(int id);

    // 统计
    QVariantMap getCounts();
    QVariantMap getFilterStats(const QString& keyword = "", const QString& filterType = "all", const QVariant& filterValue = -1);

    // 异步操作
    void addNoteAsync(const QString& title, const QString& content, const QStringList& tags = QStringList(),
                      const QString& color = "", int categoryId = -1,
                      const QString& itemType = "text", const QByteArray& dataBlob = QByteArray(),
                      const QString& sourceApp = "", const QString& sourceTitle = "");

signals:
    // 【修改】现在信号携带具体数据，实现增量更新
    void noteAdded(const QVariantMap& note);
    void noteUpdated(); // 用于普通刷新
    void categoriesChanged();

private:
    DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createTables();
    
    QSqlDatabase m_db;
    QString m_dbPath; 
    QRecursiveMutex m_mutex;
};

#endif // DATABASEMANAGER_H