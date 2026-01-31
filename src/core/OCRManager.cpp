#include "OCRManager.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QStringList>
#include <QTemporaryFile>
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QLocale>
#include <QCoreApplication>
#include <utility>

OCRManager& OCRManager::instance() {
    static OCRManager inst;
    return inst;
}

OCRManager::OCRManager(QObject* parent) : QObject(parent) {}

void OCRManager::setLanguage(const QString& lang) {
    QMutexLocker locker(&m_mutex);
    m_language = lang;
}

QString OCRManager::getLanguage() const {
    QMutexLocker locker(&m_mutex);
    return m_language;
}

void OCRManager::recognizeAsync(const QImage& image, int contextId) {
    (void)QtConcurrent::run(QThreadPool::globalInstance(), [this, image, contextId]() {
        this->recognizeSync(image, contextId);
    });
}

void OCRManager::recognizeAsync(const QString& imagePath, int contextId) {
    (void)QtConcurrent::run(QThreadPool::globalInstance(), [this, imagePath, contextId]() {
        QImage image(imagePath);
        if (image.isNull()) {
            emit recognitionFinished("无法加载图像文件: " + imagePath, contextId);
            return;
        }
        this->recognizeSync(image, contextId);
    });
}

// 图像预处理函数：提高 OCR 识别准确度
QImage OCRManager::preprocessImage(const QImage& original) {
    if (original.isNull()) return original;
    
    QImage processed = original;

    // 0. 尺寸上限保护：防止超大图导致内存溢出
    if (processed.width() > 4000 || processed.height() > 4000) {
        processed = processed.scaled(4000, 4000, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    // 1. 转换为灰度图 (保留 8 位深度的灰度细节)
    processed = processed.convertToFormat(QImage::Format_Grayscale8);
    
    // 2. 动态缩放策略 (优化为 FastTransformation 提高速度)
    int scale = 1;
    if (processed.width() < 1000 && processed.height() < 1000) {
        scale = 3;
    } else if (processed.width() < 2000 && processed.height() < 2000) {
        scale = 2;
    }
    
    if (scale > 1) {
        processed = processed.scaled(
            processed.width() * scale, 
            processed.height() * scale, 
            Qt::KeepAspectRatio, 
            Qt::FastTransformation
        );
    }
    
    // 3. 自动反色处理：Tesseract 在白底黑字下表现最好
    // 简单判断：如果四个角的像素平均值较暗，则认为可能是深色背景
    int cornerSum = 0;
    cornerSum += qGray(processed.pixel(0, 0));
    cornerSum += qGray(processed.pixel(processed.width()-1, 0));
    cornerSum += qGray(processed.pixel(0, processed.height()-1));
    cornerSum += qGray(processed.pixel(processed.width()-1, processed.height()-1));
    if (cornerSum / 4 < 128) {
        processed.invertPixels();
    }

    // 4. 增强对比度（线性拉伸）
    // 泰语等细笔画文字对二值化和过度的对比度拉伸很敏感，因此我们收窄忽略范围（从 1% 降至 0.5%）
    int histogram[256] = {0};
    for (int y = 0; y < processed.height(); ++y) {
        const uchar* line = processed.constScanLine(y);
        for (int x = 0; x < processed.width(); ++x) {
            histogram[line[x]]++;
        }
    }
    
    int totalPixels = processed.width() * processed.height();
    int minGray = 0, maxGray = 255;
    int count = 0;
    
    for (int i = 0; i < 256; ++i) {
        count += histogram[i];
        if (count > totalPixels * 0.005) {
            minGray = i;
            break;
        }
    }
    
    count = 0;
    for (int i = 255; i >= 0; --i) {
        count += histogram[i];
        if (count > totalPixels * 0.005) {
            maxGray = i;
            break;
        }
    }
    
    if (maxGray > minGray) {
        for (int y = 0; y < processed.height(); ++y) {
            uchar* line = processed.scanLine(y);
            for (int x = 0; x < processed.width(); ++x) {
                int val = line[x];
                val = (val - minGray) * 255 / (maxGray - minGray);
                val = qBound(0, val, 255);
                line[x] = static_cast<uchar>(val);
            }
        }
    }

    // 5. 简单锐化处理 (卷积) - 仅对较小图像执行，防止大图 CPU 暴涨
    if (processed.width() < 3000 && processed.height() < 3000) {
        QImage sharpened = processed;
        int kernel[3][3] = {
            {0, -1, 0},
            {-1, 5, -1},
            {0, -1, 0}
        };
        for (int y = 1; y < processed.height() - 1; ++y) {
            const uchar* prevLine = processed.constScanLine(y - 1);
            const uchar* currLine = processed.constScanLine(y);
            const uchar* nextLine = processed.constScanLine(y + 1);
            uchar* destLine = sharpened.scanLine(y);
            for (int x = 1; x < processed.width() - 1; ++x) {
                int sum = currLine[x] * kernel[1][1]
                        + prevLine[x] * kernel[0][1] + nextLine[x] * kernel[2][1]
                        + currLine[x-1] * kernel[1][0] + currLine[x+1] * kernel[1][2];
                destLine[x] = static_cast<uchar>(qBound(0, sum, 255));
            }
        }
        processed = sharpened;
    }
    
    // 注意：不再手动调用 Otsu 二值化。
    // Tesseract 4.0+ 内部的二值化器（基于 Leptonica）在处理具有抗锯齿边缘的灰度图像时表现更好。
    
    return processed;
}

void OCRManager::recognizeSync(const QImage& image, int contextId) {
    QString result;
    QString langToUse;
    QString tessDataPath;
    QString tesseractPath;
    QString currentLang;

    {
        QMutexLocker locker(&m_mutex);
        langToUse = m_language;

        if (m_isPathCached) {
            tessDataPath = m_cachedTessDataPath;
            tesseractPath = m_cachedTesseractPath;
            currentLang = m_cachedLangs;
        }
    }

#ifdef Q_OS_WIN
    // 预处理图像以提高识别准确度
    QImage processedImage = preprocessImage(image);
    
    if (processedImage.isNull()) {
        result = "图像无效";
        emit recognitionFinished(result, contextId);
        return;
    }
    
    // 使用 BMP 格式存储临时文件，因为其写入速度最快且不涉及复杂的压缩计算，能缩短毫秒级开销
    QTemporaryFile tempFile(QDir::tempPath() + "/ocr_XXXXXX.bmp");
    tempFile.setAutoRemove(true);
    
    if (!tempFile.open()) {
        result = "无法创建临时图像文件";
        emit recognitionFinished(result, contextId);
        return;
    }
    
    QString filePath = QDir::toNativeSeparators(tempFile.fileName());
    
    // 保存预处理后的灰度图
    if (!processedImage.save(filePath, "BMP")) {
        result = "无法保存临时图像文件";
        emit recognitionFinished(result, contextId);
        return;
    }
    
    tempFile.close();

    // 如果没有缓存，则执行探测逻辑（仅首次）
    if (tesseractPath.isEmpty()) {
        QString appPath = QCoreApplication::applicationDirPath();
        QStringList basePaths;
        basePaths << appPath << QDir(appPath).absolutePath() + "/.." << QDir(appPath).absolutePath() + "/../..";

        for (const QString& base : std::as_const(basePaths)) {
            if (tessDataPath.isEmpty()) {
                QStringList dataPotentials;
                dataPotentials << base + "/resources/Tesseract-OCR/tessdata"
                               << base + "/Tesseract-OCR/tessdata"
                               << base + "/tessdata"
                               << "C:/Program Files/Tesseract-OCR/tessdata";
                for (const QString& p : std::as_const(dataPotentials)) {
                    if (QDir(p).exists()) {
                        tessDataPath = QDir(p).absolutePath();
                        break;
                    }
                }
            }
            if (tesseractPath.isEmpty()) {
                QStringList exePotentials;
                exePotentials << base + "/resources/Tesseract-OCR/tesseract.exe"
                               << base + "/Tesseract-OCR/tesseract.exe"
                               << base + "/resources/tesseract.exe"
                               << base + "/tesseract.exe"
                               << "C:/Program Files/Tesseract-OCR/tesseract.exe";
                for (const QString& p : std::as_const(exePotentials)) {
                    if (QFile::exists(p)) {
                        tesseractPath = QDir::toNativeSeparators(p);
                        break;
                    }
                }
            }
            if (!tessDataPath.isEmpty() && !tesseractPath.isEmpty()) break;
        }

        if (tesseractPath.isEmpty()) tesseractPath = "tesseract";

        // 智能语言探测
        QStringList foundLangs;
        if (!tessDataPath.isEmpty()) {
            QDir dir(tessDataPath);
            if (dir.exists()) {
                QStringList filters; filters << "*.traineddata";
                QStringList files = dir.entryList(filters, QDir::Files);
                QStringList priority;
                if (QLocale::system().language() == QLocale::Chinese &&
                    QLocale::system().script() == QLocale::TraditionalChineseScript) {
                    priority = {"chi_tra", "tha", "eng", "chi_sim", "jpn", "kor"};
                } else {
                    priority = {"chi_sim", "tha", "eng", "chi_tra", "jpn", "kor"};
                }
                for (const QString& pLang : std::as_const(priority)) {
                    if (files.contains(pLang + ".traineddata")) {
                        foundLangs << pLang;
                        files.removeAll(pLang + ".traineddata");
                    }
                }
                for (const QString& file : std::as_const(files)) {
                    if (foundLangs.size() >= 3) break;
                    QString name = file.left(file.lastIndexOf('.'));
                    if (name != "osd" && !foundLangs.contains(name)) foundLangs << name;
                }
            }
        }
        currentLang = foundLangs.isEmpty() ? langToUse : foundLangs.join('+');

        // 存入缓存
        {
            QMutexLocker locker(&m_mutex);
            m_cachedTesseractPath = tesseractPath;
            m_cachedTessDataPath = tessDataPath;
            m_cachedLangs = currentLang;
            m_isPathCached = true;
        }
    }

    if (!tesseractPath.isEmpty()) {
        QProcess tesseract;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!tessDataPath.isEmpty() && QDir(tessDataPath).exists()) {
            QDir prefixDir(tessDataPath);
            prefixDir.cdUp();
            env.insert("TESSDATA_PREFIX", QDir::toNativeSeparators(prefixDir.absolutePath()));
        }
        tesseract.setProcessEnvironment(env);
        
        qDebug() << "OCR: Used tessdata path:" << tessDataPath;
        qDebug() << "OCR: Using language:" << currentLang;

        QStringList args;
        if (QFile::exists(tessDataPath)) {
            args << "--tessdata-dir" << QDir::toNativeSeparators(tessDataPath);
        }
        
        args << filePath << "stdout" << "-l" << currentLang << "--oem" << "1" << "--psm" << "3";
        tesseract.start(tesseractPath, args);
        
        if (!tesseract.waitForStarted()) {
            result = "无法启动 Tesseract 引擎。路径: " + tesseractPath;
        } else if (tesseract.waitForFinished(20000)) {
            QByteArray output = tesseract.readAllStandardOutput();
            QByteArray errorOutput = tesseract.readAllStandardError();
            result = QString::fromUtf8(output).trimmed();
            
            if (!result.isEmpty()) {
                qDebug() << "Tesseract OCR succeeded using:" << tesseractPath << "with lang:" << currentLang;
                emit recognitionFinished(result, contextId);
                return;
            }
            
            if (!errorOutput.isEmpty()) {
                result = "Tesseract 错误: " + QString::fromUtf8(errorOutput).left(100);
            } else {
                result = "未识别到任何内容。请检查数据包及语言。";
            }
        } else {
            tesseract.kill();
            result = "OCR 识别超时 (20s)。语言包过量或图片过大。";
        }
    } else {
        result = "未找到 Tesseract 引擎组件。搜索路径包括 resources/Tesseract-OCR。";
    }
#else
    result = "当前平台不支持 OCR 功能";
#endif

    if (result.isEmpty()) {
        result = "未能从图片中识别出任何文字";
    }
    
    emit recognitionFinished(result, contextId);
}