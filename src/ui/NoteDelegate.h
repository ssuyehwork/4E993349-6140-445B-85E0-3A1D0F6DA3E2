#ifndef NOTEDELEGATE_H
#define NOTEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>
#include "../models/NoteModel.h"

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
        QString timeStr = index.data(NoteModel::TimeRole).toDateTime().toString("yyyy-MM-dd HH:mm");
        bool isPinned = index.data(NoteModel::PinnedRole).toBool();

        // 2. 处理选中状态和背景
        QRect rect = option.rect.adjusted(5, 5, -5, -5); // 留出间距
        QColor bgColor = (option.state & QStyle::State_Selected) ? QColor("#37373D") : QColor("#2D2D2D");

        // 绘制圆角卡片背景
        QPainterPath path;
        path.addRoundedRect(rect, 6, 6);
        painter->fillPath(path, bgColor);

        // 如果选中，画个边框
        if (option.state & QStyle::State_Selected) {
            painter->setPen(QPen(QColor("#007ACC"), 2));
            painter->drawPath(path);
        }

        // 3. 绘制标题 (加粗，白色)
        painter->setPen(QColor("#E0E0E0"));
        painter->setFont(QFont("Microsoft YaHei", 11, QFont::Bold));
        QRect titleRect = rect.adjusted(10, 10, -40, -60);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, title);

        // 4. 绘制置顶图标 (如果有)
        if (isPinned) {
            painter->setPen(QColor("#FFD700")); // 金色
            painter->setFont(QFont("Segoe UI Emoji", 10)); // 使用 Emoji 字体或其他图标字体
            painter->drawText(rect.right() - 30, rect.top() + 25, "📌");
        }

        // 5. 绘制内容预览 (灰色，最多2行)
        painter->setPen(QColor("#AAAAAA"));
        painter->setFont(QFont("Microsoft YaHei", 9));
        QRect contentRect = rect.adjusted(10, 35, -10, -30);
        // 去除换行符，只显示纯文本预览
        QString cleanContent = content.simplified();
        painter->drawText(contentRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          painter->fontMetrics().elidedText(cleanContent, Qt::ElideRight, contentRect.width() * 2));

        // 6. 绘制底部信息栏 (时间 + 标签)
        painter->setPen(QColor("#666666"));
        painter->setFont(QFont("Consolas", 8));
        QRect bottomRect = rect.adjusted(10, 80, -10, -5);
        painter->drawText(bottomRect, Qt::AlignLeft | Qt::AlignVCenter, "🕒 " + timeStr);

        // 绘制一个假的标签气泡演示
        QString tags = index.data(NoteModel::TagsRole).toString();
        if (!tags.isEmpty()) {
            QRect tagRect(rect.right() - 100, rect.bottom() - 25, 90, 20);
            painter->setBrush(QColor("#1E1E1E"));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(tagRect, 4, 4);

            painter->setPen(QColor("#888888"));
            painter->drawText(tagRect, Qt::AlignCenter, tags.left(10));
        }

        painter->restore();
    }
};

#endif // NOTEDELEGATE_H