#include "Editor.h"
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    HighlightingRule rule;

    // 标题 (Headers) - 蓝色
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor("#569CD6"));
    headerFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("^#{1,6}\\s.*");
    rule.format = headerFormat;
    m_highlightingRules.append(rule);

    // 粗体 (**bold**) - 红色
    QTextCharFormat boldFormat;
    boldFormat.setFontWeight(QFont::Bold);
    boldFormat.setForeground(QColor("#E06C75"));
    rule.pattern = QRegularExpression("\\*\\*.*?\\*\\*");
    rule.format = boldFormat;
    m_highlightingRules.append(rule);

    // 待办事项 ([ ] [x]) - 黄色/绿色
    QTextCharFormat uncheckedFormat;
    uncheckedFormat.setForeground(QColor("#E5C07B"));
    rule.pattern = QRegularExpression("-\\s\\[\\s\\]");
    rule.format = uncheckedFormat;
    m_highlightingRules.append(rule);

    QTextCharFormat checkedFormat;
    checkedFormat.setForeground(QColor("#6A9955"));
    rule.pattern = QRegularExpression("-\\s\\[x\\]");
    rule.format = checkedFormat;
    m_highlightingRules.append(rule);

    // 代码 (Code) - 绿色
    QTextCharFormat codeFormat;
    codeFormat.setForeground(QColor("#98C379"));
    codeFormat.setFontFamilies({"Consolas", "Monaco", "monospace"});
    rule.pattern = QRegularExpression("`[^`]+`|```.*");
    rule.format = codeFormat;
    m_highlightingRules.append(rule);

    // 引用 (> Quote) - 灰色
    QTextCharFormat quoteFormat;
    quoteFormat.setForeground(QColor("#808080"));
    quoteFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("^\\s*>.*");
    rule.format = quoteFormat;
    m_highlightingRules.append(rule);

    // 列表 (Lists) - 紫色
    QTextCharFormat listFormat;
    listFormat.setForeground(QColor("#C678DD"));
    rule.pattern = QRegularExpression("^\\s*[\\-\\*]\\s");
    rule.format = listFormat;
    m_highlightingRules.append(rule);

    // 链接 (Links) - 浅蓝
    QTextCharFormat linkFormat;
    linkFormat.setForeground(QColor("#61AFEF"));
    linkFormat.setFontUnderline(true);
    rule.pattern = QRegularExpression("\\[.*?\\]\\(.*?\\)|https?://\\S+");
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

#include <QVBoxLayout>
#include <QMimeData>
#include <QUrl>

InternalEditor::InternalEditor(QWidget* parent) : QPlainTextEdit(parent) {
    setStyleSheet("background: #1E1E1E; color: #D4D4D4; font-family: 'Consolas', 'Courier New'; font-size: 13pt; border: none; padding: 10px;");
}

void InternalEditor::insertFromMimeData(const QMimeData* source) {
    if (source->hasImage()) {
        appendPlainText("\n[图片已粘贴 - 待实现存储逻辑]\n");
        return;
    }
    if (source->hasUrls()) {
        for (const QUrl& url : source->urls()) {
            if (url.isLocalFile()) appendPlainText(QString("\n[文件引用: %1]\n").arg(url.toLocalFile()));
            else appendPlainText(QString("\n[链接: %1]\n").arg(url.toString()));
        }
        return;
    }
    QPlainTextEdit::insertFromMimeData(source);
}

Editor::Editor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);

    m_edit = new InternalEditor(this);
    m_highlighter = new MarkdownHighlighter(m_edit->document());

    m_preview = new QTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setStyleSheet("background: #1E1E1E; color: #D4D4D4; padding: 10px; border: none;");

    m_stack->addWidget(m_edit);
    m_stack->addWidget(m_preview);

    layout->addWidget(m_stack);
}

void Editor::setPlainText(const QString& text) {
    m_edit->setPlainText(text);
}

QString Editor::toPlainText() const {
    return m_edit->toPlainText();
}

void Editor::setPlaceholderText(const QString& text) {
    m_edit->setPlaceholderText(text);
}

void Editor::togglePreview(bool preview) {
    if (preview) {
        QString html = "<html><body style='font-family: sans-serif;'>" + m_edit->toPlainText().replace("\n", "<br>") + "</body></html>";
        m_preview->setHtml(html);
        m_stack->setCurrentWidget(m_preview);
    } else {
        m_stack->setCurrentWidget(m_edit);
    }
}