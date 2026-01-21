#include "SearchLineEdit.h"
#include <QSettings>
#include <QMenu>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QLayout>
#include <QStyle>
#include <QGraphicsDropShadowEffect>

// --- Simple FlowLayout ---
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing) {
        setContentsMargins(margin, margin, margin, margin);
    }
    ~FlowLayout() {
        QLayoutItem *item;
        while ((item = takeAt(0))) delete item;
    }
    void addItem(QLayoutItem *item) override { m_itemList.append(item); }
    int horizontalSpacing() const { return m_hSpace >= 0 ? m_hSpace : spacing(); }
    int verticalSpacing() const { return m_vSpace >= 0 ? m_vSpace : spacing(); }
    Qt::Orientations expandingDirections() const override { return Qt::Orientations(); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
    int count() const override { return m_itemList.size(); }
    QLayoutItem *itemAt(int index) const override { return m_itemList.value(index); }
    QLayoutItem *takeAt(int index) override { return (index >= 0 && index < m_itemList.size()) ? m_itemList.takeAt(index) : nullptr; }
    QSize minimumSize() const override {
        QSize size;
        for (auto* item : m_itemList) size = size.expandedTo(item->minimumSize());
        return size + QSize(contentsMargins().left() + contentsMargins().right(), contentsMargins().top() + contentsMargins().bottom());
    }
    void setGeometry(const QRect &rect) override { QLayout::setGeometry(rect); doLayout(rect, false); }
    QSize sizeHint() const override { return minimumSize(); }

private:
    int doLayout(const QRect &rect, bool testOnly) const {
        int left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);
        QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;
        int spacingX = horizontalSpacing();
        int spacingY = verticalSpacing();

        for (auto* item : m_itemList) {
            QWidget *wid = item->widget();
            int spaceX = (spacingX == -1) ? wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal) : spacingX;
            int spaceY = (spacingY == -1) ? wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical) : spacingY;
            
            int nextX = x + item->sizeHint().width() + spaceX;
            if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
                x = effectiveRect.x();
                y = y + lineHeight + spaceY;
                nextX = x + item->sizeHint().width() + spaceX;
                lineHeight = 0;
            }
            if (!testOnly) item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
            x = nextX;
            lineHeight = qMax(lineHeight, item->sizeHint().height());
        }
        return y + lineHeight - rect.y() + bottom;
    }
    QList<QLayoutItem *> m_itemList;
    int m_hSpace, m_vSpace;
};

// --- History Chip ---
class HistoryChip : public QFrame {
    Q_OBJECT
public:
    HistoryChip(const QString& text, QWidget* parent = nullptr) : QFrame(parent), m_text(text) {
        setCursor(Qt::PointingHandCursor);
        setStyleSheet("QFrame { background: #3A3A3E; border: 1px solid #555; border-radius: 12px; } QFrame:hover { background: #454549; border-color: #4a90e2; }");
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 4, 4, 4);
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet("color: #DDD; font-size: 11px; border: none; background: transparent;");
        layout->addWidget(lbl);
        auto* btn = new QPushButton("×");
        btn->setFixedSize(16, 16);
        btn->setStyleSheet("QPushButton { color: #666; border: none; background: transparent; font-weight: bold; } QPushButton:hover { color: #E74C3C; }");
        connect(btn, &QPushButton::clicked, this, [this](){ emit deleted(m_text); });
        layout->addWidget(btn);
    }
    void mousePressEvent(QMouseEvent* e) override { if(e->button() == Qt::LeftButton) emit clicked(m_text); }
signals:
    void clicked(const QString& text);
    void deleted(const QString& text);
private:
    QString m_text;
};

// --- SearchHistoryPopup ---
class SearchHistoryPopup : public QWidget {
    Q_OBJECT
public:
    explicit SearchHistoryPopup(SearchLineEdit* edit) : QWidget(edit->window(), Qt::Popup | Qt::FramelessWindowHint) {
        m_edit = edit;
        setAttribute(Qt::WA_TranslucentBackground);
        
        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(10, 10, 10, 10);
        
        m_container = new QFrame();
        m_container->setStyleSheet("background: #252526; border: 1px solid #444; border-radius: 10px;");
        rootLayout->addWidget(m_container);

        auto* shadow = new QGraphicsDropShadowEffect(m_container);
        shadow->setBlurRadius(20); shadow->setXOffset(0); shadow->setYOffset(5);
        shadow->setColor(QColor(0,0,0,120));
        m_container->setGraphicsEffect(shadow);

        auto* layout = new QVBoxLayout(m_container);
        auto* top = new QHBoxLayout();
        auto* title = new QLabel("🕒 搜索历史");
        title->setStyleSheet("color: #888; font-weight: bold; font-size: 11px;");
        top->addWidget(title);
        top->addStretch();
        auto* clearBtn = new QPushButton("清空");
        clearBtn->setStyleSheet("QPushButton { color: #666; border: none; font-size: 11px; } QPushButton:hover { color: #E74C3C; }");
        connect(clearBtn, &QPushButton::clicked, m_edit, &SearchLineEdit::clearHistory);
        connect(clearBtn, &QPushButton::clicked, this, &SearchHistoryPopup::refresh);
        top->addWidget(clearBtn);
        layout->addLayout(top);

        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("background: transparent; border: none;");
        m_chipsWidget = new QWidget();
        m_flow = new FlowLayout(m_chipsWidget, 0, 8, 8);
        scroll->setWidget(m_chipsWidget);
        layout->addWidget(scroll);
    }

    void refresh() {
        QLayoutItem* item;
        while ((item = m_flow->takeAt(0))) {
            if(item->widget()) item->widget()->deleteLater();
            delete item;
        }
        QStringList history = m_edit->getHistory();
        int contentHeight = 0;
        int targetContentWidth = m_edit->width();

        if(history.isEmpty()) {
            auto* lbl = new QLabel("暂无历史记录");
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet("color: #555; font-style: italic; margin: 20px; border: none; background: transparent;");
            m_flow->addWidget(lbl);
            contentHeight = 100;
        } else {
            for(const QString& text : history) {
                auto* chip = new HistoryChip(text);
                connect(chip, &HistoryChip::clicked, this, [this](const QString& t){ 
                    m_edit->setText(t); 
                    emit m_edit->returnPressed(); 
                    close(); 
                });
                connect(chip, &HistoryChip::deleted, this, [this](const QString& t){ 
                    m_edit->removeHistoryEntry(t); 
                    refresh(); 
                });
                m_flow->addWidget(chip);
            }
            
            int effectiveWidth = targetContentWidth - 30;
            int flowHeight = m_flow->heightForWidth(effectiveWidth);
            contentHeight = qMin(400, qMax(120, flowHeight + 50));
        }
        
        setFixedWidth(targetContentWidth + 20); // 20 is shadow_margin * 2
        setFixedHeight(contentHeight + 20);
        
        QPoint pos = m_edit->mapToGlobal(QPoint(0, m_edit->height()));
        move(pos.x() - 10, pos.y() + 5 - 10); // align container with edit, 5px gap, -10 shadow margin
    }

private:
    SearchLineEdit* m_edit;
    QFrame* m_container;
    QWidget* m_chipsWidget;
    FlowLayout* m_flow;
};

// --- SearchLineEdit Implementation ---
SearchLineEdit::SearchLineEdit(QWidget* parent) : QLineEdit(parent) {
    setStyleSheet(
        "QLineEdit { "
        "  background-color: #252526; "
        "  border: 1px solid #333333; "
        "  border-radius: 16px; "
        "  padding: 6px 12px; "
        "  color: #eee; "
        "  font-size: 14px; "
        "} "
        "QLineEdit:focus { border: 1px solid #4a90e2; }"
    );
}

void SearchLineEdit::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) showPopup();
    QLineEdit::mouseDoubleClickEvent(e);
}

void SearchLineEdit::showPopup() {
    if(!m_popup) m_popup = new SearchHistoryPopup(this);
    m_popup->refresh();
    m_popup->show();
}

void SearchLineEdit::addHistoryEntry(const QString& text) {
    if(text.isEmpty()) return;
    QSettings settings("RapidNotes", "SearchHistory");
    QStringList history = settings.value("list").toStringList();
    history.removeAll(text);
    history.prepend(text);
    while(history.size() > 20) history.removeLast();
    settings.setValue("list", history);
}

QStringList SearchLineEdit::getHistory() const {
    QSettings settings("RapidNotes", "SearchHistory");
    return settings.value("list").toStringList();
}

void SearchLineEdit::clearHistory() {
    QSettings settings("RapidNotes", "SearchHistory");
    settings.setValue("list", QStringList());
}

void SearchLineEdit::removeHistoryEntry(const QString& text) {
    QSettings settings("RapidNotes", "SearchHistory");
    QStringList history = settings.value("list").toStringList();
    history.removeAll(text);
    settings.setValue("list", history);
}

#include "SearchLineEdit.moc"
