#include "FileSearchWindow.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QSet>
#include <QtConcurrent>

FileSearchWindow::FileSearchWindow(QWidget* parent)
    : FramelessDialog("查找文件", parent)
{
    resize(600, 500);
    initUI();
}

void FileSearchWindow::initUI() {
    auto* layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(20, 10, 20, 20);
    layout->setSpacing(15);

    // 微软雅黑字体设置
    QFont yahei("Microsoft YaHei", 10);
    this->setFont(yahei);

    // 第一行：文件夹路径选择
    auto* pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText("选择或输入文件夹路径...");
    m_pathEdit->setStyleSheet("QLineEdit { background: #2A2A2A; border: 1px solid #444; color: #EEE; padding: 5px; border-radius: 4px; }");

    auto* browseBtn = new QPushButton();
    browseBtn->setIcon(IconHelper::getIcon("folder", "#AAAAAA"));
    browseBtn->setFixedSize(32, 32);
    browseBtn->setStyleSheet("QPushButton { background: #333; border: 1px solid #444; border-radius: 4px; } QPushButton:hover { background: #444; }");
    connect(browseBtn, &QPushButton::clicked, this, &FileSearchWindow::browseFolder);

    m_searchBtn = new QPushButton("开始遍历");
    m_searchBtn->setFixedWidth(100);
    m_searchBtn->setStyleSheet("QPushButton { background: #0078d4; color: white; border: none; border-radius: 4px; height: 32px; font-weight: bold; } QPushButton:hover { background: #1086e0; }");
    connect(m_searchBtn, &QPushButton::clicked, this, &FileSearchWindow::startSearch);
    connect(&m_watcher, &QFutureWatcher<QStringList>::finished, this, &FileSearchWindow::onSearchFinished);

    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseBtn);
    pathLayout->addWidget(m_searchBtn);
    layout->addLayout(pathLayout);

    // 第二行：过滤与搜索
    auto* filterLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索文件名...");
    m_searchEdit->setStyleSheet("QLineEdit { background: #2A2A2A; border: 1px solid #444; color: #EEE; padding: 5px; border-radius: 4px; }");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FileSearchWindow::filterResults);

    m_extFilter = new QComboBox();
    m_extFilter->addItem("所有扩展名", "*");
    m_extFilter->setFixedWidth(120);
    m_extFilter->setStyleSheet("QComboBox { background: #2A2A2A; border: 1px solid #444; color: #EEE; padding: 5px; border-radius: 4px; } "
                               "QComboBox QAbstractItemView { background: #2A2A2A; color: #EEE; selection-background-color: #0078d4; }");
    connect(m_extFilter, &QComboBox::currentTextChanged, this, &FileSearchWindow::filterResults);

    filterLayout->addWidget(new QLabel("搜索:"));
    filterLayout->addWidget(m_searchEdit);
    filterLayout->addWidget(new QLabel("过滤:"));
    filterLayout->addWidget(m_extFilter);
    layout->addLayout(filterLayout);

    // 文件列表
    m_fileList = new QListWidget();
    m_fileList->setStyleSheet("QListWidget { background: #181818; border: 1px solid #333; color: #CCC; border-radius: 4px; padding: 5px; outline: none; } "
                              "QListWidget::item { padding: 5px; border-bottom: 1px solid #222; } "
                              "QListWidget::item:selected { background: #0078d4; color: white; }");
    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &FileSearchWindow::locateFile);
    layout->addWidget(m_fileList);

    layout->addWidget(new QLabel("提示：双击列表项可定位文件位置", this));
}

void FileSearchWindow::browseFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择文件夹", m_pathEdit->text());
    if (!dir.isEmpty()) {
        m_pathEdit->setText(dir);
    }
}

void FileSearchWindow::startSearch() {
    QString path = m_pathEdit->text();
    if (path.isEmpty() || !QDir(path).exists()) return;

    m_searchBtn->setEnabled(false);
    m_searchBtn->setText("遍历中...");
    m_allFiles.clear();
    m_fileList->clear();

    // 在后台线程执行遍历
    QFuture<QStringList> future = QtConcurrent::run([path]() {
        QStringList result;
        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            result << it.next();
        }
        return result;
    });
    m_watcher.setFuture(future);
}

void FileSearchWindow::onSearchFinished() {
    m_allFiles = m_watcher.result();

    m_extFilter->clear();
    m_extFilter->addItem("所有扩展名", "*");

    QSet<QString> extensions;
    for (const QString& path : m_allFiles) {
        QFileInfo info(path);
        QString ext = info.suffix().toLower();
        if (!ext.isEmpty()) {
            extensions.insert("." + ext);
        }
    }

    QStringList sortedExts = extensions.values();
    sortedExts.sort();
    m_extFilter->addItems(sortedExts);

    filterResults();

    m_searchBtn->setEnabled(true);
    m_searchBtn->setText("开始遍历");
}

void FileSearchWindow::filterResults() {
    m_fileList->clear();
    QString keyword = m_searchEdit->text().toLower();
    QString ext = m_extFilter->currentText();
    if (ext == "所有扩展名") ext = "";

    for (const QString& path : m_allFiles) {
        QFileInfo info(path);
        QString fileName = info.fileName();

        bool matchKeyword = keyword.isEmpty() || fileName.toLower().contains(keyword);
        bool matchExt = ext.isEmpty() || path.toLower().endsWith(ext.toLower());

        if (matchKeyword && matchExt) {
            auto* item = new QListWidgetItem(fileName);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            m_fileList->addItem(item);
        }
    }
}

void FileSearchWindow::locateFile(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    QFileInfo info(path);
    if (info.exists()) {
        // 定位并选中文件
        QStringList args;
        args << "/select," << QDir::toNativeSeparators(path);
        QProcess::startDetached("explorer.exe", args);
    }
}
