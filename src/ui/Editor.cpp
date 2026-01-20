#include "Editor.h"

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    HighlightingRule rule;

    // 标题 (#)
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor("#569CD6"));
    headerFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("^#+.*");
    rule.format = headerFormat;
    m_highlightingRules.append(rule);

    // 加粗 (**)
    QTextCharFormat boldFormat;
    boldFormat.setFontWeight(QFont::Bold);
    boldFormat.setForeground(QColor("#CE9178"));
    rule.pattern = QRegularExpression("\\*\\*.*\\*\\*");
    rule.format = boldFormat;
    m_highlightingRules.append(rule);

    // 双向链接 ([[]])
    QTextCharFormat linkFormat;
    linkFormat.setForeground(QColor("#4EC9B0"));
    linkFormat.setFontUnderline(true);
    rule.pattern = QRegularExpression("\\[\\[.*\\]\\]");
    rule.format = linkFormat;
    m_highlightingRules.append(rule);
}

void MarkdownHighlighter::highlightBlock(const QString& text) {
    for (const HighlightingRule& rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

#include <QMimeData>
#include <QFileInfo>
#include <QKeyEvent>

Editor::Editor(QWidget* parent) : QPlainTextEdit(parent) {
    m_highlighter = new MarkdownHighlighter(document());
    setStyleSheet("background: #1E1E1E; color: #D4D4D4; font-family: 'Consolas', 'Courier New'; font-size: 13pt;");
}

void Editor::insertFromMimeData(const QMimeData* source) {
    if (source->hasImage()) {
        // 处理图片粘贴逻辑 (示例：保存到数据库)
        appendPlainText("[图片已粘贴并保存]");
        return;
    }
    if (source->hasUrls()) {
        for (const QUrl& url : source->urls()) {
            appendPlainText(QString("参考文件: %1").arg(url.toLocalFile()));
        }
        return;
    }
    QPlainTextEdit::insertFromMimeData(source);
}

void Editor::keyPressEvent(QKeyEvent* event) {
    QPlainTextEdit::keyPressEvent(event);

    // 检测 [[ 输入
    if (event->text() == "[") {
        QString fullText = toPlainText();
        int cursor = textCursor().position();
        if (cursor >= 2 && fullText.mid(cursor - 2, 2) == "[[") {
            emit linkTriggered(cursorRect().bottomRight());
        }
    }
}
