#ifndef NOTEDELEGATE_H
#define NOTEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include "../models/NoteModel.h"
#include "IconHelper.h"

class NoteDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit NoteDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    // 定义卡片高度
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(index);
        return QSize(option.rect.width(), 110); // 每个卡片高度 110px
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // 1. 获取数据
        QString title = index.data(NoteModel::TitleRole).toString();
        QString content = index.data(NoteModel::ContentRole).toString();
        QString timeStr = index.data(NoteModel::TimeRole).toDateTime().toString("yyyy-MM-dd HH:mm:ss");
        bool isPinned = index.data(NoteModel::PinnedRole).toBool();

        // 2. 处理选中状态和背景 (更精致的配色与阴影感)
        QRect rect = option.rect.adjusted(8, 4, -8, -4);
        bool isSelected = (option.state & QStyle::State_Selected);

        QColor bgColor = isSelected ? QColor("#323233") : QColor("#252526");
        QColor borderColor = isSelected ? QColor("#4a90e2") : QColor("#333333");

        // 绘制卡片背景
        QPainterPath path;
        path.addRoundedRect(rect, 8, 8);

        // 模拟阴影
        if (!isSelected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 40));
            painter->drawRoundedRect(rect.translated(0, 2), 8, 8);
        }

        painter->setPen(QPen(borderColor, isSelected ? 2 : 1));
        painter->setBrush(bgColor);
        painter->drawPath(path);

        // 3. 绘制标题 (加粗，主文本色)
        painter->setPen(QColor("#cccccc"));
        QFont titleFont("Microsoft YaHei", 10, QFont::Bold);
        painter->setFont(titleFont);
        QRect titleRect = rect.adjusted(12, 10, -35, -70);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

        // 4. 绘制置顶/星级标识
        if (isPinned) {
            QPixmap pin = IconHelper::getIcon("pin", "#f1c40f", 14).pixmap(14, 14);
            painter->drawPixmap(rect.right() - 25, rect.top() + 12, pin);
        }

        // 5. 绘制内容预览 (副文本色，最多2行)
        painter->setPen(QColor("#858585"));
        painter->setFont(QFont("Microsoft YaHei", 9));
        QRect contentRect = rect.adjusted(12, 34, -12, -32);
        QString cleanContent = content.simplified();
        QString elidedContent = painter->fontMetrics().elidedText(cleanContent, Qt::ElideRight, contentRect.width() * 2);
        painter->drawText(contentRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, elidedContent);

        // 6. 绘制底部元数据栏 (时间图标 + 时间 + 类型标签)
        QRect bottomRect = rect.adjusted(12, 78, -12, -8);

        // 时间
        painter->setPen(QColor("#666666"));
        painter->setFont(QFont("Segoe UI", 8));
        QPixmap clock = IconHelper::getIcon("clock", "#666666", 12).pixmap(12, 12);
        painter->drawPixmap(bottomRect.left(), bottomRect.top() + (bottomRect.height() - 12) / 2, clock);
        painter->drawText(bottomRect.adjusted(16, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, timeStr);

        // 模拟 Python 版的“剪贴板”或“类型”标签
        QString itemType = index.data(NoteModel::TypeRole).toString();
        if (itemType.isEmpty()) itemType = "笔记";
        else if (itemType == "text") itemType = "剪贴板";

        QString tagText = itemType.toUpper();
        int tagWidth = painter->fontMetrics().horizontalAdvance(tagText) + 16;
        QRect tagRect(bottomRect.right() - tagWidth, bottomRect.top() + 2, tagWidth, 18);

        painter->setBrush(QColor("#1e1e1e"));
        painter->setPen(QPen(QColor("#444"), 1));
        painter->drawRoundedRect(tagRect, 4, 4);

        painter->setPen(QColor("#888888"));
        painter->setFont(QFont("Microsoft YaHei", 7, QFont::Bold));
        painter->drawText(tagRect, Qt::AlignCenter, tagText);

        painter->restore();
    }
};

#endif // NOTEDELEGATE_H