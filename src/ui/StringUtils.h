#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <QString>
#include <QTextDocument>
#include <QMimeData>
#include <QClipboard>
#include <QApplication>

class StringUtils {
public:
    static bool isHtml(const QString& content) {
        QString trimmed = content.trimmed();
        return trimmed.startsWith("<!DOCTYPE", Qt::CaseInsensitive) || 
               trimmed.startsWith("<html", Qt::CaseInsensitive) || 
               trimmed.contains("<style", Qt::CaseInsensitive) ||
               Qt::mightBeRichText(content);
    }

    static QString htmlToPlainText(const QString& html) {
        if (!isHtml(html)) return html;
        QTextDocument doc;
        doc.setHtml(html);
        return doc.toPlainText();
    }

    static void copyNoteToClipboard(const QString& content) {
        if (isHtml(content)) {
            QMimeData* mimeData = new QMimeData();
            // 同时设置 HTML 和 纯文本，这样粘贴到支持富文本的地方保留格式，粘贴到纯文本的地方则是文字
            mimeData->setHtml(content);
            mimeData->setText(htmlToPlainText(content));
            QApplication::clipboard()->setMimeData(mimeData);
        } else {
            QApplication::clipboard()->setText(content);
        }
    }
};

#endif // STRINGUTILS_H
