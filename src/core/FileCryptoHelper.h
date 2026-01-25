#ifndef FILECRYPTOHELPER_H
#define FILECRYPTOHELPER_H

#include <QString>
#include <QByteArray>
#include <QFile>

class FileCryptoHelper {
public:
    static bool encryptFile(const QString& sourcePath, const QString& destPath, const QString& password);
    static bool decryptFile(const QString& sourcePath, const QString& destPath, const QString& password);
    
    // 安全删除文件（覆盖后再删除）
    static bool secureDelete(const QString& filePath);

private:
    static QByteArray deriveKey(const QString& password, const QByteArray& salt);
};

#endif // FILECRYPTOHELPER_H
