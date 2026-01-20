#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MarkdownHighlighter(QTextDocument* parent = nullptr);
protected:
    void highlightBlock(const QString& text) override;
private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightingRule> m_highlightingRules;
};

class Editor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit Editor(QWidget* parent = nullptr);
signals:
    void linkTriggered(const QPoint& pos);
protected:
    void insertFromMimeData(const QMimeData* source) override;
    void keyPressEvent(QKeyEvent* event) override;
private:
    MarkdownHighlighter* m_highlighter;
};

#endif // EDITOR_H
