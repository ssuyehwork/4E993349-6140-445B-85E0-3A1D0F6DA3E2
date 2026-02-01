#include "KeywordSearchWindow.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDirIterator>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QtConcurrent>
#include <QScrollBar>
#include <QMessageBox>

KeywordSearchWindow::KeywordSearchWindow(QWidget* parent) : FramelessDialog("查找关键字", parent) {
    resize(900, 700);
    m_ignoreDirs = {".git", ".svn", ".idea", ".vscode", "__pycache__", "node_modules", "dist", "build", "venv"};
    initUI();
}

KeywordSearchWindow::~KeywordSearchWindow() {
}

void KeywordSearchWindow::initUI() {
    auto* mainLayout = new QVBoxLayout(m_contentArea);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // --- 配置区域 ---
    auto* configGroup = new QWidget();
    auto* configLayout = new QVBoxLayout(configGroup);
    configLayout->setContentsMargins(0, 0, 0, 0);
    configLayout->setSpacing(10);

    // 目录选择
    auto* pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText("选择搜索根目录...");
    m_pathEdit->setStyleSheet("QLineEdit { background: #252526; border: 1px solid #333; border-radius: 4px; padding: 6px; color: #EEE; }");
    auto* browseBtn = new QPushButton("浏览...");
    browseBtn->setStyleSheet("QPushButton { background: #3E3E42; border: none; border-radius: 4px; padding: 6px 15px; color: #EEE; } QPushButton:hover { background: #4E4E52; }");
    connect(browseBtn, &QPushButton::clicked, this, &KeywordSearchWindow::onBrowseFolder);
    pathLayout->addWidget(new QLabel("搜索目录:"));
    pathLayout->addWidget(m_pathEdit, 1);
    pathLayout->addWidget(browseBtn);
    configLayout->addLayout(pathLayout);

    // 文件过滤
    auto* filterLayout = new QHBoxLayout();
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText("例如: *.py, *.txt (留空则扫描所有文本文件)");
    m_filterEdit->setStyleSheet("QLineEdit { background: #252526; border: 1px solid #333; border-radius: 4px; padding: 6px; color: #EEE; }");
    filterLayout->addWidget(new QLabel("文件过滤:"));
    filterLayout->addWidget(m_filterEdit);
    configLayout->addLayout(filterLayout);

    // 查找内容
    auto* searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("输入要查找的内容...");
    m_searchEdit->setStyleSheet("QLineEdit { background: #252526; border: 1px solid #333; border-radius: 4px; padding: 6px; color: #EEE; }");
    searchLayout->addWidget(new QLabel("查找内容:"));
    searchLayout->addWidget(m_searchEdit);
    configLayout->addLayout(searchLayout);

    // 替换内容
    auto* replaceLayout = new QHBoxLayout();
    m_replaceEdit = new QLineEdit();
    m_replaceEdit->setPlaceholderText("替换为 (可选)...");
    m_replaceEdit->setStyleSheet("QLineEdit { background: #252526; border: 1px solid #333; border-radius: 4px; padding: 6px; color: #EEE; }");
    replaceLayout->addWidget(new QLabel("替换内容:"));
    replaceLayout->addWidget(m_replaceEdit);
    configLayout->addLayout(replaceLayout);

    // 选项
    auto* optionLayout = new QHBoxLayout();
    m_caseCheck = new QCheckBox("区分大小写");
    m_caseCheck->setStyleSheet("QCheckBox { color: #AAA; }");
    optionLayout->addWidget(m_caseCheck);
    optionLayout->addStretch();
    configLayout->addLayout(optionLayout);

    mainLayout->addWidget(configGroup);

    // --- 按钮区域 ---
    auto* btnLayout = new QHBoxLayout();
    auto* searchBtn = new QPushButton(" 智能搜索");
    searchBtn->setIcon(IconHelper::getIcon("find_keyword", "#FFF", 16));
    searchBtn->setStyleSheet("QPushButton { background: #007ACC; border: none; border-radius: 4px; padding: 8px 20px; color: #FFF; font-weight: bold; } QPushButton:hover { background: #0098FF; }");
    connect(searchBtn, &QPushButton::clicked, this, &KeywordSearchWindow::onSearch);

    auto* replaceBtn = new QPushButton(" 执行替换");
    replaceBtn->setIcon(IconHelper::getIcon("edit", "#FFF", 16));
    replaceBtn->setStyleSheet("QPushButton { background: #D32F2F; border: none; border-radius: 4px; padding: 8px 20px; color: #FFF; font-weight: bold; } QPushButton:hover { background: #F44336; }");
    connect(replaceBtn, &QPushButton::clicked, this, &KeywordSearchWindow::onReplace);

    auto* undoBtn = new QPushButton(" 撤销替换");
    undoBtn->setIcon(IconHelper::getIcon("undo", "#EEE", 16));
    undoBtn->setStyleSheet("QPushButton { background: #3E3E42; border: none; border-radius: 4px; padding: 8px 20px; color: #EEE; } QPushButton:hover { background: #4E4E52; }");
    connect(undoBtn, &QPushButton::clicked, this, &KeywordSearchWindow::onUndo);

    auto* clearBtn = new QPushButton(" 清空日志");
    clearBtn->setIcon(IconHelper::getIcon("trash", "#EEE", 16));
    clearBtn->setStyleSheet("QPushButton { background: #3E3E42; border: none; border-radius: 4px; padding: 8px 20px; color: #EEE; } QPushButton:hover { background: #4E4E52; }");
    connect(clearBtn, &QPushButton::clicked, this, &KeywordSearchWindow::onClearLog);

    btnLayout->addWidget(searchBtn);
    btnLayout->addWidget(replaceBtn);
    btnLayout->addWidget(undoBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // --- 日志展示区域 ---
    m_logDisplay = new QTextBrowser();
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setUndoRedoEnabled(false);
    m_logDisplay->setOpenLinks(false);
    m_logDisplay->setOpenExternalLinks(false);
    m_logDisplay->setStyleSheet(
        "QTextBrowser { background: #1E1E1E; border: 1px solid #333; border-radius: 4px; color: #D4D4D4; font-family: 'Consolas', monospace; font-size: 12px; }"
    );
    connect(m_logDisplay, &QTextBrowser::anchorClicked, this, [](const QUrl& url) {
        if (url.scheme() == "file") {
            QString path = url.toLocalFile();
            QString nativePath = QDir::toNativeSeparators(path);
            QProcess::startDetached("explorer.exe", { "/select," + nativePath });
        }
    });
    mainLayout->addWidget(m_logDisplay, 1);

    // --- 状态栏 ---
    auto* statusLayout = new QVBoxLayout();
    m_progressBar = new QProgressBar();
    m_progressBar->setFixedHeight(4);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet("QProgressBar { background: #252526; border: none; } QProgressBar::chunk { background: #007ACC; }");
    m_progressBar->hide();

    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("color: #888; font-size: 11px;");

    statusLayout->addWidget(m_progressBar);
    statusLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(statusLayout);

}

void KeywordSearchWindow::onBrowseFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, "选择搜索目录");
    if (!folder.isEmpty()) {
        m_pathEdit->setText(folder);
    }
}

bool KeywordSearchWindow::isTextFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray chunk = file.read(1024);
    file.close();

    if (chunk.isEmpty()) return true;
    if (chunk.contains('\0')) return false;

    return true;
}

void KeywordSearchWindow::log(const QString& msg, const QString& type) {
    QString color = "#D4D4D4";
    if (type == "success") color = "#6A9955";
    else if (type == "error") color = "#F44747";
    else if (type == "header") color = "#007ACC";
    else if (type == "file") color = "#E1523D";

    QString html = QString("<span style='color:%1;'>%2</span>").arg(color, msg.toHtmlEscaped());
    // 如果是文件，添加自定义属性以便识别
    if (type == "file") {
        html = QString("<a href=\"%1\" style=\"color:%2; text-decoration: underline;\">📄 文件: %3</a>")
                .arg(QUrl::fromLocalFile(msg).toString(), color, msg.toHtmlEscaped());
    }

    m_logDisplay->append(html);
}

void KeywordSearchWindow::onSearch() {
    QString rootDir = m_pathEdit->text();
    QString keyword = m_searchEdit->text();
    if (rootDir.isEmpty() || keyword.isEmpty()) {
        QMessageBox::warning(this, "提示", "目录和查找内容不能为空!");
        return;
    }

    m_logDisplay->clear();
    m_progressBar->show();
    m_progressBar->setRange(0, 0);
    m_statusLabel->setText("正在搜索...");

    QString filter = m_filterEdit->text();
    bool caseSensitive = m_caseCheck->isChecked();

    (void)QtConcurrent::run([this, rootDir, keyword, filter, caseSensitive]() {
        int foundFiles = 0;
        int scannedFiles = 0;

        QStringList filters;
        if (!filter.isEmpty()) {
            filters = filter.split(QRegularExpression("[,\\s;]+"), Qt::SkipEmptyParts);
        }

        QDirIterator it(rootDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();

            // 过滤目录
            bool skip = false;
            for (const QString& ignore : m_ignoreDirs) {
                if (filePath.contains("/" + ignore + "/") || filePath.contains("\\" + ignore + "\\")) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            // 过滤文件名
            if (!filters.isEmpty()) {
                bool matchFilter = false;
                QString fileName = QFileInfo(filePath).fileName();
                for (const QString& f : filters) {
                    QRegularExpression re(QRegularExpression::wildcardToRegularExpression(f));
                    if (re.match(fileName).hasMatch()) {
                        matchFilter = true;
                        break;
                    }
                }
                if (!matchFilter) continue;
            }

            if (!isTextFile(filePath)) continue;

            scannedFiles++;
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString content = in.readAll();
                file.close();

                Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                if (content.contains(keyword, cs)) {
                    foundFiles++;
                    int count = content.count(keyword, cs);
                    QMetaObject::invokeMethod(this, [this, filePath, count]() {
                        log(filePath, "file");
                        log(QString("   匹配次数: %1\n").arg(count));
                    });
                }
            }
        }

        QMetaObject::invokeMethod(this, [this, scannedFiles, foundFiles, keyword, caseSensitive]() {
            log(QString("\n搜索完成! 扫描 %1 个文件，找到 %2 个匹配\n").arg(scannedFiles).arg(foundFiles), "success");
            m_statusLabel->setText(QString("完成: 找到 %1 个文件").arg(foundFiles));
            m_progressBar->hide();
            highlightResult(keyword);
        });
    });
}

void KeywordSearchWindow::highlightResult(const QString& keyword) {
    if (keyword.isEmpty()) return;
    // Qt 的 QTextEdit 在 HTML 模式下不好直接对普通文本进行高亮，但我们可以重新构建 HTML 或者使用 ExtraSelections
    // 简单起见，这里我们就不在 QTextEdit 内部高亮了，因为已经有链接样式。
}

void KeywordSearchWindow::onReplace() {
    QString rootDir = m_pathEdit->text();
    QString keyword = m_searchEdit->text();
    QString replaceText = m_replaceEdit->text();
    if (rootDir.isEmpty() || keyword.isEmpty()) {
        QMessageBox::warning(this, "提示", "目录和查找内容不能为空!");
        return;
    }

    if (QMessageBox::question(this, "确认", "确定执行批量替换？操作不可逆（虽然会有备份）。") != QMessageBox::Yes) {
        return;
    }

    m_progressBar->show();
    m_progressBar->setRange(0, 0);
    m_statusLabel->setText("正在替换...");

    QString filter = m_filterEdit->text();
    bool caseSensitive = m_caseCheck->isChecked();

    (void)QtConcurrent::run([this, rootDir, keyword, replaceText, filter, caseSensitive]() {
        int modifiedFiles = 0;
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString backupDirName = "_backup_" + timestamp;
        QDir root(rootDir);
        root.mkdir(backupDirName);
        m_lastBackupPath = root.absoluteFilePath(backupDirName);

        QStringList filters;
        if (!filter.isEmpty()) {
            filters = filter.split(QRegularExpression("[,\\s;]+"), Qt::SkipEmptyParts);
        }

        QDirIterator it(rootDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            if (filePath.contains(backupDirName)) continue;

            // 过滤目录和文件名（逻辑同搜索）
            bool skip = false;
            for (const QString& ignore : m_ignoreDirs) {
                if (filePath.contains("/" + ignore + "/") || filePath.contains("\\" + ignore + "\\")) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            if (!filters.isEmpty()) {
                bool matchFilter = false;
                QString fileName = QFileInfo(filePath).fileName();
                for (const QString& f : filters) {
                    QRegularExpression re(QRegularExpression::wildcardToRegularExpression(f));
                    if (re.match(fileName).hasMatch()) {
                        matchFilter = true;
                        break;
                    }
                }
                if (!matchFilter) continue;
            }

            if (!isTextFile(filePath)) continue;

            QFile file(filePath);
            if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
                QTextStream in(&file);
                QString content = in.readAll();

                Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                if (content.contains(keyword, cs)) {
                    // 备份
                    QString fileName = QFileInfo(filePath).fileName();
                    QFile::copy(filePath, m_lastBackupPath + "/" + fileName + ".bak");

                    // 替换
                    QString newContent;
                    if (caseSensitive) {
                        newContent = content.replace(keyword, replaceText);
                    } else {
                        newContent = content.replace(QRegularExpression(QRegularExpression::escape(keyword), QRegularExpression::CaseInsensitiveOption), replaceText);
                    }

                    file.resize(0);
                    in << newContent;
                    modifiedFiles++;
                    QMetaObject::invokeMethod(this, [this, fileName]() {
                        log("已修改: " + fileName, "success");
                    });
                }
                file.close();
            }
        }

        QMetaObject::invokeMethod(this, [this, modifiedFiles]() {
            log(QString("\n替换完成! 修改了 %1 个文件").arg(modifiedFiles), "success");
            m_statusLabel->setText(QString("完成: 修改了 %1 个文件").arg(modifiedFiles));
            m_progressBar->hide();
            QMessageBox::information(this, "完成", QString("已修改 %1 个文件\n备份于: %2").arg(modifiedFiles).arg(QFileInfo(m_lastBackupPath).fileName()));
        });
    });
}

void KeywordSearchWindow::onUndo() {
    if (m_lastBackupPath.isEmpty() || !QDir(m_lastBackupPath).exists()) {
        QMessageBox::warning(this, "提示", "未找到有效的备份目录！");
        return;
    }

    int restored = 0;
    QDir backupDir(m_lastBackupPath);
    QStringList baks = backupDir.entryList({"*.bak"});

    QString rootDir = m_pathEdit->text();

    for (const QString& bak : baks) {
        QString origName = bak.left(bak.length() - 4);

        // 在根目录下寻找原始文件（简化策略：找同名文件）
        QDirIterator it(rootDir, {origName}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            QString targetPath = it.next();
            if (QFile::remove(targetPath)) {
                if (QFile::copy(backupDir.absoluteFilePath(bak), targetPath)) {
                    restored++;
                }
            }
        }
    }

    log(QString("↶ 撤销完成，已恢复 %1 个文件\n").arg(restored), "success");
    QMessageBox::information(this, "成功", QString("已恢复 %1 个文件").arg(restored));
}

void KeywordSearchWindow::onClearLog() {
    m_logDisplay->clear();
    m_statusLabel->setText("就绪");
}

void KeywordSearchWindow::hideEvent(QHideEvent* event) {
    FramelessDialog::hideEvent(event);
}

void KeywordSearchWindow::onResultDoubleClicked(const QModelIndex& index) {
}
