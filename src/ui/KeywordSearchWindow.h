#ifndef KEYWORDSEARCHWINDOW_H
#define KEYWORDSEARCHWINDOW_H

#include "FramelessDialog.h"
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

private:
    void initUI();
    bool isTextFile(const QString& filePath);
    void log(const QString& msg, const QString& type = "info");
    void highlightResult(const QString& keyword);

    QLineEdit* m_pathEdit;
    QLineEdit* m_filterEdit;
    QLineEdit* m_searchEdit;
    QLineEdit* m_replaceEdit;
    QCheckBox* m_caseCheck;
    QTextBrowser* m_logDisplay;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    QString m_lastBackupPath;
    QStringList m_ignoreDirs;
};

#endif // KEYWORDSEARCHWINDOW_H
