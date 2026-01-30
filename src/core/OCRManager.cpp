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
    
    QImage processed = original;
    
    // 1. 放大图像以提高分辨率（如果图像太小）
    if (processed.width() < 1000 || processed.height() < 1000) {
        double scaleW = 1000.0 / processed.width();
        double scaleH = 1000.0 / processed.height();
        int scale = static_cast<int>(qMax(scaleW, scaleH) + 0.5);
        scale = qMin(scale, 4); // 最多放大 4 倍
        
        if (scale > 1) {
            processed = processed.scaled(
                processed.width() * scale, 
                processed.height() * scale, 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
            );
        }
    }
    
    // 2. 转换为灰度图
    if (processed.format() != QImage::Format_Grayscale8) {
        processed = processed.convertToFormat(QImage::Format_Grayscale8);
    }
    
    // 3. 增强对比度（直方图均衡化）
    int histogram[256] = {0};
    for (int y = 0; y < processed.height(); ++y) {
        const uchar* line = processed.constScanLine(y);
        for (int x = 0; x < processed.width(); ++x) {
            histogram[line[x]]++;
        }
    }
    
    // 找到有效的灰度范围（忽略边缘 1%）
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
    
    // 应用对比度拉伸
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
    
    // 4. Otsu 自动阈值二值化
    // 重新计算直方图（对比度拉伸后）
    for (int i = 0; i < 256; ++i) {
        histogram[i] = 0;
    }
    for (int y = 0; y < processed.height(); ++y) {
        const uchar* line = processed.constScanLine(y);
        for (int x = 0; x < processed.width(); ++x) {
            histogram[line[x]]++;
        }
    }
    
    // 计算 Otsu 阈值
    int threshold = 128;
    double maxVariance = 0;
    
    for (int t = 0; t < 256; ++t) {
        double w0 = 0, w1 = 0;
        double sum0 = 0, sum1 = 0;
        
        for (int i = 0; i <= t; ++i) {
            w0 += histogram[i];
            sum0 += i * histogram[i];
        }
        
        for (int i = t + 1; i < 256; ++i) {
            w1 += histogram[i];
            sum1 += i * histogram[i];
        }
        
        if (w0 > 0 && w1 > 0) {
            double mean0 = sum0 / w0;
            double mean1 = sum1 / w1;
            double variance = w0 * w1 * (mean0 - mean1) * (mean0 - mean1);
            
            if (variance > maxVariance) {
                maxVariance = variance;
                threshold = t;
            }
        }
    }
    
    // 应用二值化
    for (int y = 0; y < processed.height(); ++y) {
        uchar* line = processed.scanLine(y);
        for (int x = 0; x < processed.width(); ++x) {
            line[x] = (line[x] > threshold) ? 255 : 0;
        }
    }
    
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
    
    // 保存预处理后的图像，使用高质量 PNG
    if (!processedImage.save(filePath, "PNG", 100)) {
        result = "无法保存临时图像文件";
        emit recognitionFinished(result, contextId);
        return;
    }
    
    tempFile.close();

    // 1. 默认使用 Tesseract OCR
    QString appPath = QCoreApplication::applicationDirPath();
    QString tesseractPath = appPath + "/resources/Tesseract-OCR/tesseract.exe";
    QString tessDataPath = appPath + "/resources/Tesseract-OCR";
    
    if (QFile::exists(tesseractPath)) {
        QProcess tesseract;
        
        // 设置 TESSDATA_PREFIX 环境变量，让 Tesseract 找到语言数据文件
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("TESSDATA_PREFIX", QDir::toNativeSeparators(tessDataPath));
        tesseract.setProcessEnvironment(env);
        
        QStringList args;
        args << filePath << "stdout" << "-l" << m_language;
        tesseract.start(tesseractPath, args);
        
        if (tesseract.waitForFinished(10000)) {
            QByteArray output = tesseract.readAllStandardOutput();
            QByteArray errorOutput = tesseract.readAllStandardError();
            
            // 输出错误信息以便调试
            if (!errorOutput.isEmpty()) {
                qDebug() << "Tesseract stderr:" << QString::fromUtf8(errorOutput);
            }
            
            result = QString::fromUtf8(output).trimmed();
            
            if (!result.isEmpty()) {
                qDebug() << "Tesseract OCR succeeded";
                emit recognitionFinished(result, contextId);
                return;
            }
            
            qDebug() << "Tesseract returned empty result, trying Windows.Media.Ocr as fallback";
        } else {
            qDebug() << "Tesseract timeout, trying Windows.Media.Ocr as fallback";
        }
    } else {
        qDebug() << "Tesseract not found at:" << tesseractPath << ", trying Windows.Media.Ocr as fallback";
    }

    // 2. Tesseract 失败或不存在，使用 Windows.Media.Ocr 作为备用
    QString psScript = R"(
        [Console]::OutputEncoding = [System.Text.Encoding]::UTF8;
        $ErrorActionPreference = 'Stop';
        Add-Type -AssemblyName System.Runtime.WindowsRuntime;
        $asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0];
        function Await($WinRtTask, $ResultType) {
            $asTask = $asTaskGeneric.MakeGenericMethod($ResultType);
            $netTask = $asTask.Invoke($null, @($WinRtTask));
            $netTask.Wait(-1) | Out-Null;
            $netTask.Result;
        }
        try {
            [Windows.Media.Ocr.OcrEngine, Windows.Media, ContentType = WindowsRuntime] | Out-Null;
            [Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime] | Out-Null;
            [Windows.Graphics.Imaging.BitmapDecoder, Windows.Graphics, ContentType = WindowsRuntime] | Out-Null;
            [Windows.Graphics.Imaging.SoftwareBitmap, Windows.Graphics, ContentType = WindowsRuntime] | Out-Null;
            $file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync('%1')) ([Windows.Storage.StorageFile]);
            $stream = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream]);
            $decoder = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder]);
            $bitmap = Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap]);
            $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages();
            if ($engine) {
                $ocrResult = Await ($engine.RecognizeAsync($bitmap)) ([Windows.Media.Ocr.OcrResult]);
                Write-Output $ocrResult.Text;
            } else {
                Write-Output 'Error: OCR Engine not available';
            }
        } catch {
            Write-Output ('Error: ' + $_.Exception.Message);
        }
    )";
    
    QString psCommand = psScript.arg(filePath);

    QProcess process;
    process.start("powershell", QStringList() << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-Command" << psCommand);
    
    if (process.waitForFinished(15000)) {
        QByteArray output = process.readAllStandardOutput();
        result = QString::fromUtf8(output).trimmed();
        
        if (result.startsWith("Error:")) {
            qDebug() << "Windows.Media.Ocr failed:" << result;
            result = "识别失败: " + result.mid(6).trimmed();
        } else if (!result.isEmpty()) {
            qDebug() << "Windows.Media.Ocr succeeded";
        }
    } else {
        process.kill();
        result = "识别超时 (15s)";
        qDebug() << "Windows.Media.Ocr process timeout";
    }
    
#else
    result = "当前平台不支持 OCR 功能";
#endif

    if (result.isEmpty()) {
        result = "未能从图片中识别出任何文字";
    }
    
    emit recognitionFinished(result, contextId);
}