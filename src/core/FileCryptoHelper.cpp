#include "FileCryptoHelper.h"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <QDebug>
#include <QFileInfo>

#define SALT_SIZE 16
#define IV_SIZE 16
#define KEY_SIZE 32
#define PBKDF2_ITERATIONS 10000

bool FileCryptoHelper::encryptFile(const QString& sourcePath, const QString& destPath, const QString& password) {
    QFile src(sourcePath);
    if (!src.open(QIODevice::ReadOnly)) return false;

    QFile dest(destPath);
    if (!dest.open(QIODevice::WriteOnly)) return false;

    // 1. 生成盐值
    unsigned char salt[SALT_SIZE];
    RAND_bytes(salt, SALT_SIZE);
    dest.write((const char*)salt, SALT_SIZE);

    // 2. 生成 IV
    unsigned char iv[IV_SIZE];
    RAND_bytes(iv, IV_SIZE);
    dest.write((const char*)iv, IV_SIZE);

    // 3. 派生密钥
    unsigned char key[KEY_SIZE];
    PKCS5_PBKDF2_HMAC(password.toStdString().c_str(), password.length(),
                      salt, SALT_SIZE, PBKDF2_ITERATIONS, EVP_sha256(), KEY_SIZE, key);

    // 4. 初始化加密环境
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    // 5. 分块加密
    unsigned char inBuf[4096];
    unsigned char outBuf[4096 + AES_BLOCK_SIZE];
    int inLen, outLen;

    while ((inLen = src.read((char*)inBuf, sizeof(inBuf))) > 0) {
        EVP_EncryptUpdate(ctx, outBuf, &outLen, inBuf, inLen);
        dest.write((const char*)outBuf, outLen);
    }

    EVP_EncryptFinal_ex(ctx, outBuf, &outLen);
    dest.write((const char*)outBuf, outLen);

    EVP_CIPHER_CTX_free(ctx);
    src.close();
    dest.close();

    return true;
}

bool FileCryptoHelper::decryptFile(const QString& sourcePath, const QString& destPath, const QString& password) {
    QFile src(sourcePath);
    if (!src.open(QIODevice::ReadOnly)) return false;

    // 1. 读取盐值
    unsigned char salt[SALT_SIZE];
    if (src.read((char*)salt, SALT_SIZE) != SALT_SIZE) return false;

    // 2. 读取 IV
    unsigned char iv[IV_SIZE];
    if (src.read((char*)iv, IV_SIZE) != IV_SIZE) return false;

    // 3. 派生密钥
    unsigned char key[KEY_SIZE];
    PKCS5_PBKDF2_HMAC(password.toStdString().c_str(), password.length(),
                      salt, SALT_SIZE, PBKDF2_ITERATIONS, EVP_sha256(), KEY_SIZE, key);

    QFile dest(destPath);
    if (!dest.open(QIODevice::WriteOnly)) return false;

    // 4. 初始化解密环境
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);

    // 5. 分块解密
    unsigned char inBuf[4096 + AES_BLOCK_SIZE];
    unsigned char outBuf[4096 + AES_BLOCK_SIZE * 2];
    int inLen, outLen;

    while ((inLen = src.read((char*)inBuf, sizeof(inBuf))) > 0) {
        if (!EVP_DecryptUpdate(ctx, outBuf, &outLen, inBuf, inLen)) {
            EVP_CIPHER_CTX_free(ctx);
            dest.close();
            QFile::remove(destPath);
            return false; // 密码错误或数据损坏
        }
        dest.write((const char*)outBuf, outLen);
    }

    if (!EVP_DecryptFinal_ex(ctx, outBuf, &outLen)) {
        EVP_CIPHER_CTX_free(ctx);
        dest.close();
        QFile::remove(destPath);
        return false; // 密码错误
    }
    dest.write((const char*)outBuf, outLen);

    EVP_CIPHER_CTX_free(ctx);
    src.close();
    dest.close();

    return true;
}

bool FileCryptoHelper::secureDelete(const QString& filePath) {
    QFile file(filePath);
    if (!file.exists()) return true;

    qint64 size = QFileInfo(filePath).size();
    if (file.open(QIODevice::WriteOnly)) {
        // 简单覆盖一次随机数据
        QByteArray junk(4096, 0);
        for (qint64 i = 0; i < size; i += junk.size()) {
            RAND_bytes((unsigned char*)junk.data(), junk.size());
            file.write(junk);
        }
        file.flush();
        file.close();
    }
    return QFile::remove(filePath);
}
