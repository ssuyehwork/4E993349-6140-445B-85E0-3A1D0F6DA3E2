#include "OCRManager.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QStringList>
#include <QTemporaryFile>
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

OCRManager& OCRManager::instance() {
    static OCRManager inst;
    return inst;
}

OCRManager::OCRManager(QObject* parent) : QObject(parent) {}

void OCRManager::setLanguage(const QString& lang) {
    m_language = lang;
    qDebug() << "OCR language set to:" << m_language;
}

QString OCRManager::getLanguage() const {
    return m_language;
}

void OCRManager::recognizeAsync(const QImage& image, int contextId) {
    (void)QtConcurrent::run(QThreadPool::globalInstance(), [this, image, contextId]() {
        this->recognizeSync(image, contextId);
    });
}

// 图像预处理函数：提高 OCR 识别准确度
QImage OCRManager::preprocessImage(const QImage& original) {
    if (original.isNull()) {
        return original;
    }
    
    // 1. 转换为灰度图 (保留 8 位深度的灰度细节，这对 Tesseract 4+ 至关重要)
    QImage processed = original.convertToFormat(QImage::Format_Grayscale8);

    // 2. 动态缩放策略
    // 目标：使文字像素高度达到 Tesseract 偏好的 30-35 像素。
    // 如果原图已经很大（宽度 > 2000），则不需要放大 3 倍，否则会导致内存占用过高且识别变慢
    int scale = 3;
    if (processed.width() > 2000 || processed.height() > 2000) {
        scale = 1;
    } else if (processed.width() > 1000 || processed.height() > 1000) {
        scale = 2;
    }

    if (scale > 1) {
        processed = processed.scaled(
            processed.width() * scale,
            processed.height() * scale,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
    }

    // 3. 增强对比度（线性拉伸）
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
    
    // 注意：不再手动调用 Otsu 二值化。
    // Tesseract 4.0+ 内部的二值化器（基于 Leptonica）在处理具有抗锯齿边缘的灰度图像时表现更好。
    
    return processed;
}

void OCRManager::recognizeSync(const QImage& image, int contextId) {
    QString result;

#ifdef Q_OS_WIN
    // 预处理图像以提高识别准确度
    QImage processedImage = preprocessImage(image);
    
    if (processedImage.isNull()) {
        result = "图像无效";
        emit recognitionFinished(result, contextId);
        return;
    }
    
    QTemporaryFile tempFile(QDir::tempPath() + "/ocr_XXXXXX.png");
    tempFile.setAutoRemove(true);
    
    if (!tempFile.open()) {
        result = "无法创建临时图像文件";
        emit recognitionFinished(result, contextId);
        return;
    }
    
    QString filePath = QDir::toNativeSeparators(tempFile.fileName());
    
    // 保存预处理后的灰度图
    if (!processedImage.save(filePath, "PNG", 100)) {
        result = "无法保存临时图像文件";
        emit recognitionFinished(result, contextId);
        return;
    }
    
    tempFile.close();

    // 路径探测逻辑：增强鲁棒性，支持从 bin 或 build 目录运行
    QString appPath = QCoreApplication::applicationDirPath();
    QString tessDataPath;
    QString tesseractPath;

    // 尝试在多个层级寻找 resources 目录
    QStringList basePaths;
    basePaths << appPath;
    basePaths << QDir(appPath).absolutePath() + "/..";
    basePaths << QDir(appPath).absolutePath() + "/../..";

    for (const QString& base : basePaths) {
        // 搜索数据目录 (支持资源目录、根目录及标准安装路径)
        if (tessDataPath.isEmpty()) {
            QStringList dataPotentials;
            dataPotentials << base + "/resources/Tesseract-OCR/tessdata"
                           << base + "/Tesseract-OCR/tessdata"
                           << base + "/tessdata"
                           << "C:/Program Files/Tesseract-OCR/tessdata";
            for (const QString& p : dataPotentials) {
                if (QDir(p).exists()) {
                    tessDataPath = QDir(p).absolutePath();
                    break;
                }
            }
        }

        // 搜索执行文件
        if (tesseractPath.isEmpty()) {
            QStringList exePotentials;
            exePotentials << base + "/resources/Tesseract-OCR/tesseract.exe"
                           << base + "/Tesseract-OCR/tesseract.exe"
                           << base + "/resources/tesseract.exe"
                           << base + "/tesseract.exe"
                           << "C:/Program Files/Tesseract-OCR/tesseract.exe";
            for (const QString& p : exePotentials) {
                if (QFile::exists(p)) {
                    tesseractPath = QDir::toNativeSeparators(p);
                    break;
                }
            }
        }

        if (!tessDataPath.isEmpty() && !tesseractPath.isEmpty()) break;
    }

    // 系统 PATH 兜底
    if (tesseractPath.isEmpty()) tesseractPath = "tesseract";

    if (!tesseractPath.isEmpty()) {
        QProcess tesseract;
        
        // 设置 TESSDATA_PREFIX 环境变量（Tesseract 主程序所在的父目录或 tessdata 所在目录）
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!tessDataPath.isEmpty() && QDir(tessDataPath).exists()) {
            // TESSDATA_PREFIX 通常应该是包含 tessdata 文件夹的目录
            QDir prefixDir(tessDataPath);
            prefixDir.cdUp();
            env.insert("TESSDATA_PREFIX", QDir::toNativeSeparators(prefixDir.absolutePath()));
        }
        tesseract.setProcessEnvironment(env);

        // 智能语言探测：自动扫描 tessdata 目录下所有可用的训练数据
        QStringList foundLangs;
        if (!tessDataPath.isEmpty()) {
            QDir dir(tessDataPath);
            if (dir.exists()) {
            QStringList filters; filters << "*.traineddata";
            QStringList files = dir.entryList(filters, QDir::Files);

            // 定义优先级：中文和英文最优先，泰语次之
            QStringList priority = {"chi_sim", "eng", "chi_tra", "tha", "jpn", "kor"};
            for (const QString& pLang : priority) {
                if (files.contains(pLang + ".traineddata")) {
                    foundLangs << pLang;
                    files.removeAll(pLang + ".traineddata");
                }
            }
            // 其余语言按字母顺序追加
            for (const QString& file : files) {
                QString name = file.left(file.lastIndexOf('.'));
                if (name != "osd") foundLangs << name;
            }
            }
        }

        QString currentLang = foundLangs.isEmpty() ? m_language : foundLangs.join('+');
        qDebug() << "OCR: Used tessdata path:" << tessDataPath;
        qDebug() << "OCR: Detected languages:" << foundLangs.size() << ":" << currentLang;

        QStringList args;
        // 明确指定数据目录
        if (QFile::exists(tessDataPath)) {
            args << "--tessdata-dir" << QDir::toNativeSeparators(tessDataPath);
        }

        args << filePath << "stdout" << "-l" << currentLang << "--oem" << "1" << "--psm" << "3";
        tesseract.start(tesseractPath, args);
        
        if (tesseract.waitForFinished(10000)) {
            QByteArray output = tesseract.readAllStandardOutput();
            result = QString::fromUtf8(output).trimmed();
            
            if (!result.isEmpty()) {
                qDebug() << "Tesseract OCR succeeded using:" << tesseractPath << "with lang:" << currentLang;
                emit recognitionFinished(result, contextId);
                return;
            }
            result = "未识别到任何内容。请检查 Tesseract 数据包是否完整。";
        } else {
            if (tesseractPath == "tesseract") {
                 result = "未找到 Tesseract 引擎组件。请检查资源文件或安装 Tesseract 并添加至系统 PATH。";
            } else {
                 result = "OCR 识别超时。";
            }
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