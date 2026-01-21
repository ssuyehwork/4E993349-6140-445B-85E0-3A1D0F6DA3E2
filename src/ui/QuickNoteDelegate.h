#ifndef QUICKNOTEDELEGATE_H
#define QUICKNOTEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>
#include "../models/NoteModel.h"
#include "IconHelper.h"

class QuickNoteDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit QuickNoteDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        return QSize(option.rect.width(), 45); // 紧凑型高度
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (!index.isValid()) return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QRect rect = option.rect;
        bool isSelected = (option.state & QStyle::State_Selected);
        bool isHovered = (option.state & QStyle::State_MouseOver);

        // 1. 绘制背景 (支持斑马纹和动态配色)
        QColor bgColor;
        if (isSelected) {
            // 优先使用 QSS 设置的选中色
            bgColor = option.palette.color(QPalette::Highlight);
            // 兜底：如果调色板未正确加载显示为白色，强制使用蓝色
            if (bgColor.lightness() > 200) bgColor = QColor("#4a90e2");
        } else if (isHovered) {
            bgColor = QColor(255, 255, 255, 25);
        } else {
            // 【核心修复】强制指定颜色以对齐 Python 版，解决启动发白问题
            bgColor = (index.row() % 2 == 0) ? QColor("#1E1E1E") : QColor("#151515");
        }
        painter->fillRect(rect, bgColor);

        // 2. 分隔线 (对齐 Python 版，使用极浅的黑色半透明)
        painter->setPen(QColor(0, 0, 0, 25));
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());

        // 图标 (DecorationRole)
        QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull()) {
            icon.paint(painter, rect.left() + 10, rect.top() + (rect.height() - 20) / 2, 20, 20);
        }

        // 标题文本
        QString text = index.data(Qt::DisplayRole).toString();
        painter->setPen(isSelected ? Qt::white : QColor("#CCCCCC"));
        painter->setFont(QFont("Microsoft YaHei", 9));
        
        QRect textRect = rect.adjusted(40, 0, -50, 0);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, 
                         painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));

        // 时间 (极简展示) - 显示在右上方
        QString timeStr = index.data(NoteModel::TimeRole).toDateTime().toString("MM-dd HH:mm");
        painter->setPen(QColor("#666666"));
        painter->setFont(QFont("Segoe UI", 7));
        painter->drawText(rect.adjusted(0, 3, -10, 0), Qt::AlignRight | Qt::AlignTop, timeStr);

        painter->restore();
    }
};

#endif // QUICKNOTEDELEGATE_H
