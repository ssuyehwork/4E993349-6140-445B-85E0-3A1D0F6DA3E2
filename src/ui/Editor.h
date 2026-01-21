#ifndef EDITOR_H
#define EDITOR_H

#include <QTextEdit>
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

#include <QStackedWidget>

class InternalEditor : public QTextEdit {
    Q_OBJECT
public:
    explicit InternalEditor(QWidget* parent = nullptr);
protected:
    void insertFromMimeData(const QMimeData* source) override;
};

class Editor : public QWidget {
    Q_OBJECT
public:
    explicit Editor(QWidget* parent = nullptr);
    void setPlainText(const QString& text);
    QString toPlainText() const;
    void setPlaceholderText(const QString& text);
    void togglePreview(bool preview);

private:
    QStackedWidget* m_stack;
    InternalEditor* m_edit;
    QTextEdit* m_preview;
    MarkdownHighlighter* m_highlighter;
};

#endif // EDITOR_H