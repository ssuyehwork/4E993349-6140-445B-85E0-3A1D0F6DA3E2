#ifndef KEYWORDSEARCHWINDOW_H
#define KEYWORDSEARCHWINDOW_H

#include "FramelessDialog.h"
#include "ClickableLineEdit.h"
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTextBrowser>
#include <QProgressBar>
#include <QLabel>
#include <QListWidget>

class KeywordSearchWindow : public FramelessDialog {
    Q_OBJECT
public:
    explicit KeywordSearchWindow(QWidget* parent = nullptr);
    ~KeywordSearchWindow();

protected:
    void hideEvent(QHideEvent* event) override;

private slots:
    void onBrowseFolder();
    void onSearch();
    void onReplace();
    void onUndo();
    void onClearLog();
    void onResultDoubleClicked(const QModelIndex& index);
    void onShowHistory();

private:
    void initUI();

    // 历史记录管理
    enum HistoryType { Path, Keyword };
    void addHistoryEntry(HistoryType type, const QString& text);
    bool isTextFile(const QString& filePath);
    void log(const QString& msg, const QString& type = "info");
    void highlightResult(const QString& keyword);

    ClickableLineEdit* m_pathEdit;
    QLineEdit* m_filterEdit;
    ClickableLineEdit* m_searchEdit;
    QLineEdit* m_replaceEdit;
    QCheckBox* m_caseCheck;
    QTextBrowser* m_logDisplay;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    QString m_lastBackupPath;
    QStringList m_ignoreDirs;
};

#endif // KEYWORDSEARCHWINDOW_H
