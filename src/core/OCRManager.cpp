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

    // 2. 优化缩放策略
    // 目标：使文字像素高度达到 Tesseract 偏好的 30-35 像素，通常对应 300 DPI
    // 假设屏幕截图约为 96 DPI，则需要放大 3 倍左右
    int scale = 3;
    processed = processed.scaled(
        processed.width() * scale,
        processed.height() * scale,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    // 3. 增强对比度（对比度拉伸）
    int histogram[256] = {0};
    for (int y = 0; y < processed.height(); ++y) {
        const uchar* line = processed.constScanLine(y);
        for (int x = 0; x < processed.width(); ++x) {
            histogram[line[x]]++;
        }
    }
    
    // 找到有效的灰度范围（忽略边缘 1% 的噪点）
    int totalPixels = processed.width() * processed.height();
    int minGray = 0, maxGray = 255;
    int count = 0;
    
    for (int i = 0; i < 256; ++i) {
        count += histogram[i];
        if (count > totalPixels * 0.01) {
            minGray = i;
            break;
        }
    }
    
    count = 0;
    for (int i = 255; i >= 0; --i) {
        count += histogram[i];
        if (count > totalPixels * 0.01) {
            maxGray = i;
            break;
        }
    }
    
    // 应用对比度拉伸，使背景更白，文字更黑
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

    // 仅使用 Tesseract OCR，移除 Windows.Media.Ocr
    QString appPath = QCoreApplication::applicationDirPath();
    QString tesseractPath = appPath + "/resources/Tesseract-OCR/tesseract.exe";
    QString tessDataPath = appPath + "/resources/Tesseract-OCR";
    
    if (QFile::exists(tesseractPath)) {
        QProcess tesseract;
        
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("TESSDATA_PREFIX", QDir::toNativeSeparators(tessDataPath));
        tesseract.setProcessEnvironment(env);
        
        QStringList args;
        // --oem 1 使用 LSTM 引擎（通常更准确）
        // --psm 3 自动页面分割
        args << filePath << "stdout" << "-l" << m_language << "--oem" << "1" << "--psm" << "3";
        tesseract.start(tesseractPath, args);
        
        if (tesseract.waitForFinished(10000)) {
            QByteArray output = tesseract.readAllStandardOutput();
            result = QString::fromUtf8(output).trimmed();
            
            if (!result.isEmpty()) {
                qDebug() << "Tesseract OCR succeeded";
                emit recognitionFinished(result, contextId);
                return;
            }
            result = "未识别到任何内容 (Tesseract)";
        } else {
            result = "OCR 识别超时";
        }
    } else {
        result = "未找到 Tesseract 引擎组件";
        qDebug() << "Tesseract not found at:" << tesseractPath;
    }
#else
    result = "当前平台不支持 OCR 功能";
#endif

    if (result.isEmpty()) {
        result = "未能从图片中识别出任何文字";
    }
    
    emit recognitionFinished(result, contextId);
}