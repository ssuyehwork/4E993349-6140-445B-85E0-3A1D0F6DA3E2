#include "FileCryptoHelper.h"
#include "AES.h"
#include <QDebug>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QRandomGenerator>

#define SALT_SIZE 16
#define IV_SIZE 16
#define KEY_SIZE 32
#define PBKDF2_ITERATIONS 5000 // 减少迭代次数以适应内置实现性能

// 简单的 PBKDF2 替代方案：利用 Qt 自带哈希链式派生密钥
static QByteArray deriveKeyInternal(const QString& password, const QByteArray& salt) {
    QByteArray key = password.toUtf8();
    for (int i = 0; i < PBKDF2_ITERATIONS; ++i) {
        key = QCryptographicHash::hash(key + salt, QCryptographicHash::Sha256);
    }
    return key;
}

bool FileCryptoHelper::encryptFile(const QString& sourcePath, const QString& destPath, const QString& password) {
    QFile src(sourcePath);
    if (!src.open(QIODevice::ReadOnly)) return false;
    QByteArray srcData = src.readAll();
    src.close();

    // 1. 生成随机盐和 IV
    QByteArray salt(SALT_SIZE, 0);
    QByteArray iv(IV_SIZE, 0);
    for(int i=0; i<SALT_SIZE; ++i) salt[i] = (char)QRandomGenerator::global()->bounded(256);
    for(int i=0; i<IV_SIZE; ++i) iv[i] = (char)QRandomGenerator::global()->bounded(256);

    // 2. 派生密钥
    QByteArray key = deriveKeyInternal(password, salt);

    // 3. 执行加密
    AES aes(AES::AES_256);
    std::vector<uint8_t> input(srcData.begin(), srcData.end());
    std::vector<uint8_t> keyVec(key.begin(), key.end());
    std::vector<uint8_t> ivVec(iv.begin(), iv.end());
    
    std::vector<uint8_t> encrypted = aes.encryptCBC(input, keyVec, ivVec);

    // 4. 写入文件：Salt + IV + EncryptedData
    QFile dest(destPath);
    if (!dest.open(QIODevice::WriteOnly)) return false;
    dest.write(salt);
    dest.write(iv);
    dest.write((const char*)encrypted.data(), encrypted.size());
    dest.close();

    return true;
}

bool FileCryptoHelper::decryptFile(const QString& sourcePath, const QString& destPath, const QString& password) {
    QFile src(sourcePath);
    if (!src.open(QIODevice::ReadOnly)) return false;

    QByteArray salt = src.read(SALT_SIZE);
    QByteArray iv = src.read(IV_SIZE);
    QByteArray encryptedData = src.readAll();
    src.close();

    if (salt.size() != SALT_SIZE || iv.size() != IV_SIZE) return false;

    // 1. 派生密钥
    QByteArray key = deriveKeyInternal(password, salt);

    // 2. 执行解密
    AES aes(AES::AES_256);
    std::vector<uint8_t> input(encryptedData.begin(), encryptedData.end());
    std::vector<uint8_t> keyVec(key.begin(), key.end());
    std::vector<uint8_t> ivVec(iv.begin(), iv.end());

    std::vector<uint8_t> decrypted = aes.decryptCBC(input, keyVec, ivVec);
    if (decrypted.empty()) return false;

    // 3. 验证并保存
    QFile dest(destPath);
    if (!dest.open(QIODevice::WriteOnly)) return false;
    dest.write((const char*)decrypted.data(), decrypted.size());
    dest.close();

    return true;
}

bool FileCryptoHelper::secureDelete(const QString& filePath) {
    QFile file(filePath);
    if (!file.exists()) return true;
    
    qint64 size = QFileInfo(filePath).size();
    if (file.open(QIODevice::WriteOnly)) {
        QByteArray junk(4096, 0);
        for (qint64 i = 0; i < size; i += junk.size()) {
            for(int j=0; j<junk.size(); ++j) junk[j] = (char)QRandomGenerator::global()->bounded(256);
            file.write(junk);
        }
        file.flush();
        file.close();
    }
    return QFile::remove(filePath);
}
