#ifndef NOTEDELEGATE_H
#define NOTEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>

class NoteDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QRect rect = option.rect.adjusted(8, 4, -8, -4);
        bool isSelected = option.state & QStyle::State_Selected;
        bool isHovered = option.state & QStyle::State_MouseOver;

        // 1. 背景色绘制
        QColor bgColor = QColor("#1E1E1E");
        if (isSelected) bgColor = QColor("#37373D");
        else if (isHovered) bgColor = QColor("#2A2D2E");

        painter->setBrush(bgColor);
        painter->setPen(isSelected ? QColor("#007ACC") : QColor("#333333"));
        painter->drawRoundedRect(rect, 6, 6);

        // 2. 绘制标题 (加粗)
        QString title = index.data(Qt::UserRole + 2).toString(); // TitleRole
        painter->setPen(isSelected ? Qt::white : QColor("#D4D4D4"));
        QFont titleFont = painter->font();
        titleFont.setBold(true);
        titleFont.setPointSize(10);
        painter->setFont(titleFont);
        painter->drawText(rect.adjusted(12, 10, -30, 0), Qt::AlignTop | Qt::AlignLeft, title);

        // 3. 绘制摘要预览 (3行)
        QString content = index.data(Qt::UserRole + 3).toString(); // ContentRole
        painter->setPen(QColor("#969696"));
        QFont contentFont = painter->font();
        contentFont.setBold(false);
        contentFont.setPointSize(9);
        painter->setFont(contentFont);

        QRect contentRect = rect.adjusted(12, 32, -12, -28);
        painter->drawText(contentRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, content);

        // 4. 底部信息：时间戳与标签气泡
        QString timeStr = index.data(Qt::UserRole + 5).toString();
        painter->setPen(QColor("#666666"));
        painter->drawText(rect.adjusted(12, 0, -12, -8), Qt::AlignBottom | Qt::AlignLeft, timeStr);

        // 5. 右上角图标：置顶/锁定 (简易绘制)
        if (index.data(Qt::UserRole + 6).toBool()) { // PinnedRole
            painter->setBrush(QColor("#007ACC"));
            painter->drawEllipse(rect.right() - 15, rect.top() + 10, 6, 6);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(200, 110);
    }
};

#endif // NOTEDELEGATE_H
