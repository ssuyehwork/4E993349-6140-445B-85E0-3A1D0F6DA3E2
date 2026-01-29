#include "OCRManager.h"
#include <QtConcurrent>
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

void OCRManager::recognizeAsync(const QImage& image, int contextId) {
    (void)QtConcurrent::run([this, image, contextId]() {
        QString result;

#ifdef Q_OS_WIN
        // 优先检查是否存在用户放置的 Tesseract OCR
        QString appDir = QCoreApplication::applicationDirPath();
        QString tesseractPath = appDir + "/third_party/tesseract/bin/tesseract.exe";

        QTemporaryFile tempFile(QDir::tempPath() + "/ocr_XXXXXX.png");
        tempFile.setAutoRemove(true);
        if (tempFile.open()) {
            QString filePath = QDir::toNativeSeparators(tempFile.fileName());
            image.save(filePath, "PNG");
            tempFile.close();

            if (QFile::exists(tesseractPath)) {
                // --- 方案 A: 调用 Tesseract OCR (Google 开源) ---
                QString outputBase = QDir::tempPath() + "/tess_out";
                QProcess process;
                process.start(tesseractPath, QStringList() << filePath << outputBase << "-l" << "chi_sim+eng");
                if (process.waitForFinished(15000)) {
                    QFile outFile(outputBase + ".txt");
                    if (outFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        result = QString::fromUtf8(outFile.readAll()).trimmed();
                        outFile.close();
                        QFile::remove(outputBase + ".txt");
                    }
                }
            }

            if (result.isEmpty()) {
                // --- 方案 B: 调用 Windows 10/11 原生 OCR 引擎 ---
                // PowerShell 脚本，利用 Windows.Media.Ocr
            // 显式设置输出编码为 UTF8 以兼容中文
            QString psCommand = QString(
                "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8;"
                "$ErrorActionPreference = 'Stop';"
                "function Wait-WinRT($task) {"
                "  while ($task.Status -eq 'Started') { Start-Sleep -m 20 };"
                "  return $task.GetResults();"
                "}"
                "try {"
                "  [Windows.Media.Ocr.OcrEngine, Windows.Media, ContentType = WindowsRuntime] | Out-Null;"
                "  [Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime] | Out-Null;"
                "  [Windows.Graphics.Imaging.BitmapDecoder, Windows.Graphics, ContentType = WindowsRuntime] | Out-Null;"
                "  $file = Wait-WinRT([Windows.Storage.StorageFile]::GetFileFromPathAsync('%1'));"
                "  $stream = Wait-WinRT($file.OpenAsync([Windows.Storage.FileAccessMode]::Read));"
                "  $decoder = Wait-WinRT([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream));"
                "  $bitmap = Wait-WinRT($decoder.GetSoftwareBitmapAsync());"
                "  $engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages();"
                "  if ($engine) {"
                "    $res = Wait-WinRT($engine.RecognizeAsync($bitmap));"
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
