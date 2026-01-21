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

        // 背景 (强制深色，消除白色色块)
        painter->fillRect(rect, QColor("#1E1E1E"));

        if (isSelected) {
            painter->fillRect(rect, QColor("#37373D"));
        } else if (isHovered) {
            painter->fillRect(rect, QColor("#2A2D2E"));
        }

        // 分隔线
        painter->setPen(QColor("#252526"));
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

        // 时间 (极简展示)
        QString timeStr = index.data(NoteModel::TimeRole).toDateTime().toString("MM-dd HH:mm");
        painter->setPen(QColor("#666666"));
        painter->setFont(QFont("Segoe UI", 8));
        painter->drawText(rect.adjusted(0, 0, -10, 0), Qt::AlignRight | Qt::AlignVCenter, timeStr);

        painter->restore();
    }
};

#endif // QUICKNOTEDELEGATE_H
