#include "OCRManager.h"
#include <QtConcurrent>
#include <QTemporaryFile>
#include <QProcess>
#include <QDir>
#include <QDebug>

OCRManager& OCRManager::instance() {
    static OCRManager inst;
    return inst;
}

OCRManager::OCRManager(QObject* parent) : QObject(parent) {}

void OCRManager::recognizeAsync(const QImage& image, int contextId) {
    (void)QtConcurrent::run([this, image, contextId]() {
        QString result;

#ifdef Q_OS_WIN
        QTemporaryFile tempFile(QDir::tempPath() + "/ocr_XXXXXX.png");
        tempFile.setAutoRemove(true);
        if (tempFile.open()) {
            QString filePath = QDir::toNativeSeparators(tempFile.fileName());
            image.save(filePath, "PNG");
            tempFile.close();

            // PowerShell 脚本，利用 Windows.Media.Ocr
            // 显式设置输出编码为 UTF8 以兼容中文
            QString psCommand = QString(
                "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8;"
                "$ErrorActionPreference = 'Stop';"
                "try {"
                "  [Windows.Media.Ocr.OcrEngine, Windows.Media, ContentType = WindowsRuntime] | Out-Null;"
                "  [Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime] | Out-Null;"
                "  [Windows.Graphics.Imaging.BitmapDecoder, Windows.Graphics, ContentType = WindowsRuntime] | Out-Null;"
                "  $file = [Windows.Storage.StorageFile]::GetFileFromPathAsync('%1').GetAwaiter().GetResult();"
                "  $stream = $file.OpenAsync([Windows.Storage.FileAccessMode]::Read).GetAwaiter().GetResult();"
                "  $decoder = [Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream).GetAwaiter().GetResult();"
                "  $bitmap = $decoder.GetSoftwareBitmapAsync().GetAwaiter().GetResult();"
                "  $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages();"
                "  if ($engine) {"
                "    $res = $engine.RecognizeAsync($bitmap).GetAwaiter().GetResult();"
                "    Write-Output $res.Text;"
                "  } else {"
                "    Write-Output 'Error: OCR Engine not available';"
                "  }"
                "} catch {"
                "  Write-Output ('Error: ' + $_.Exception.Message);"
                "}"
            ).arg(filePath);

            QProcess process;
            process.start("powershell", QStringList() << "-NoProfile" << "-ExecutionPolicy" << "Bypass" << "-Command" << psCommand);
            if (process.waitForFinished(15000)) {
                QByteArray output = process.readAllStandardOutput();
                result = QString::fromUtf8(output).trimmed();
                if (result.startsWith("Error:")) {
                    qDebug() << "OCR PowerShell Error:" << result;
                    result = "识别失败: " + result.mid(6).trimmed();
                }
            } else {
                process.kill();
                result = "识别超时 (15s)";
            }
        } else {
            result = "无法创建临时图像文件";
        }
#else
        result = "当前平台不支持基于 Windows Media 的 OCR";
#endif

        if (result.isEmpty()) result = "未能从图片中识别出任何文字";
        emit recognitionFinished(result, contextId);
    });
}
