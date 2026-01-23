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

InternalEditor::InternalEditor(QWidget* parent) : QTextEdit(parent) {
    setStyleSheet("background: #1E1E1E; color: #D4D4D4; font-family: 'Consolas', 'Courier New'; font-size: 13pt; border: none; outline: none; padding: 10px;");
    setAcceptRichText(false); // 强制纯文本编辑，除非是插入图片
}

void InternalEditor::insertFromMimeData(const QMimeData* source) {
    if (source->hasImage()) {
        QImage image = qvariant_cast<QImage>(source->imageData());
        if (!image.isNull()) {
            // 自动缩放宽图
            if (image.width() > 600) {
                image = image.scaledToWidth(600, Qt::SmoothTransformation);
            }
            textCursor().insertImage(image);
            return;
        }
    }
    if (source->hasUrls()) {
        for (const QUrl& url : source->urls()) {
            if (url.isLocalFile()) insertPlainText(QString("\n[文件引用: %1]\n").arg(url.toLocalFile()));
            else insertPlainText(QString("\n[链接: %1]\n").arg(url.toString()));
        }
        return;
    }
    QTextEdit::insertFromMimeData(source);
}

Editor::Editor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    
    m_edit = new InternalEditor(this);
    m_highlighter = new MarkdownHighlighter(m_edit->document());

    m_preview = new QTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setStyleSheet("background: #1E1E1E; color: #D4D4D4; padding: 10px; border: none; outline: none;");

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
        QString text = m_edit->toPlainText();
        QString html = "<html><head><style>"
                       "body { font-family: 'Microsoft YaHei'; color: #ddd; background-color: #1e1e1e; line-height: 1.6; padding: 20px; }"
                       "h1 { color: #569CD6; border-bottom: 1px solid #333; padding-bottom: 5px; }"
                       "h2 { color: #569CD6; border-bottom: 1px solid #222; }"
                       "code { background-color: #333; padding: 2px 4px; border-radius: 3px; font-family: Consolas; color: #98C379; }"
                       "pre { background-color: #252526; padding: 10px; border-radius: 5px; border: 1px solid #444; }"
                       "blockquote { border-left: 4px solid #569CD6; padding-left: 15px; color: #888; font-style: italic; background: #252526; margin: 10px 0; }"
                       "p { margin: 10px 0; }"
                       "</style></head><body>";
        
        QStringList lines = text.split("\n");
        bool inCodeBlock = false;
        
        for (QString line : lines) {
            if (line.startsWith("```")) {
                if (!inCodeBlock) { html += "<pre><code>"; inCodeBlock = true; }
                else { html += "</code></pre>"; inCodeBlock = false; }
                continue;
            }
            
            if (inCodeBlock) {
                html += line.toHtmlEscaped() + "<br>";
                continue;
            }

            if (line.startsWith("###### ")) html += "<h6>" + line.mid(7).toHtmlEscaped() + "</h6>";
            else if (line.startsWith("##### ")) html += "<h5>" + line.mid(6).toHtmlEscaped() + "</h5>";
            else if (line.startsWith("#### ")) html += "<h4>" + line.mid(5).toHtmlEscaped() + "</h4>";
            else if (line.startsWith("### ")) html += "<h3>" + line.mid(4).toHtmlEscaped() + "</h3>";
            else if (line.startsWith("## ")) html += "<h2>" + line.mid(3).toHtmlEscaped() + "</h2>";
            else if (line.startsWith("# ")) html += "<h1>" + line.mid(2).toHtmlEscaped() + "</h1>";
            else if (line.startsWith("> ")) html += "<blockquote>" + line.mid(2).toHtmlEscaped() + "</blockquote>";
            else if (line.startsWith("- [ ] ")) html += "<p><span style='color:#E5C07B;'>☐</span> " + line.mid(6).toHtmlEscaped() + "</p>";
            else if (line.startsWith("- [x] ")) html += "<p><span style='color:#6A9955;'>☑</span> " + line.mid(6).toHtmlEscaped() + "</p>";
            else if (line.isEmpty()) html += "<br>";
            else {
                // 处理行内代码 `code`
                QString processedLine = line.toHtmlEscaped();
                QRegularExpression inlineCode("`(.*?)`");
                processedLine.replace(inlineCode, "<code>\\1</code>");
                html += "<p>" + processedLine + "</p>";
            }
        }
        
        html += "</body></html>";
        m_preview->setHtml(html);
        m_stack->setCurrentWidget(m_preview);
    } else {
        m_stack->setCurrentWidget(m_edit);
    }
}