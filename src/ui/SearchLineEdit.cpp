#include "SearchLineEdit.h"
#include "IconHelper.h"
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
#include <QPropertyAnimation>
#include "FlowLayout.h"

// --- History Chip ---
class HistoryChip : public QFrame {
    Q_OBJECT
public:
    HistoryChip(const QString& text, QWidget* parent = nullptr) : QFrame(parent), m_text(text) {
        setAttribute(Qt::WA_StyledBackground);
        setCursor(Qt::PointingHandCursor);
        setObjectName("HistoryChip");
        
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 4, 4, 4);
        layout->setSpacing(6);
        
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet("border: none; background: transparent; color: #DDD; font-size: 12px;");
        layout->addWidget(lbl);
        
        m_btnDel = new QPushButton();
        m_btnDel->setIcon(IconHelper::getIcon("close", "#666", 16));
        m_btnDel->setIconSize(QSize(10, 10));
        m_btnDel->setFixedSize(16, 16);
        m_btnDel->setCursor(Qt::PointingHandCursor);
        m_btnDel->setStyleSheet(
            "QPushButton {"
            "  background-color: transparent;"
            "  border-radius: 8px;"
            "  padding: 0px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #E74C3C;"
            "}"
        );
        
        connect(m_btnDel, &QPushButton::clicked, this, [this](){ emit deleted(m_text); });
        layout->addWidget(m_btnDel);

        setStyleSheet(
            "#HistoryChip {"
            "  background-color: #3A3A3E;"
            "  border: 1px solid #555;"
            "  border-radius: 12px;"
            "}"
            "#HistoryChip:hover {"
            "  background-color: #454549;"
            "  border-color: #4a90e2;"
            "}"
        );
    }
    
    void mousePressEvent(QMouseEvent* e) override { 
        if(e->button() == Qt::LeftButton && !m_btnDel->underMouse()) {
            emit clicked(m_text); 
        }
        QFrame::mousePressEvent(e);
    }

signals:
    void clicked(const QString& text);
    void deleted(const QString& text);
private:
    QString m_text;
    QPushButton* m_btnDel;
};

// --- SearchHistoryPopup ---
class SearchHistoryPopup : public QWidget {
    Q_OBJECT
public:
    explicit SearchHistoryPopup(SearchLineEdit* edit) 
        : QWidget(edit->window(), Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) 
    {
        m_edit = edit;
        setAttribute(Qt::WA_TranslucentBackground);
        
        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(m_shadowMargin, m_shadowMargin, m_shadowMargin, m_shadowMargin);
        
        m_container = new QFrame();
        m_container->setObjectName("PopupContainer");
        m_container->setStyleSheet(
            "#PopupContainer {"
            "  background-color: #252526;"
            "  border: 1px solid #444;"
            "  border-radius: 10px;"
            "}"
        );
        rootLayout->addWidget(m_container);

        auto* shadow = new QGraphicsDropShadowEffect(m_container);
        shadow->setBlurRadius(20); shadow->setXOffset(0); shadow->setYOffset(5);
        shadow->setColor(QColor(0, 0, 0, 120));
        m_container->setGraphicsEffect(shadow);

        auto* layout = new QVBoxLayout(m_container);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(10);

        auto* top = new QHBoxLayout();
        auto* title = new QLabel("🕒 搜索历史");
        title->setStyleSheet("color: #888; font-weight: bold; font-size: 11px; background: transparent; border: none;");
        top->addWidget(title);
        top->addStretch();
        auto* clearBtn = new QPushButton("清空");
        clearBtn->setCursor(Qt::PointingHandCursor);
        clearBtn->setStyleSheet("QPushButton { background: transparent; color: #666; border: none; font-size: 11px; } QPushButton:hover { color: #E74C3C; }");
        connect(clearBtn, &QPushButton::clicked, [this](){
            m_edit->clearHistory();
            refreshUI();
        });
        top->addWidget(clearBtn);
        layout->addLayout(top);

        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet(
            "QScrollArea { background-color: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background-color: transparent; }"
        );
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        m_chipsWidget = new QWidget();
        m_chipsWidget->setStyleSheet("background-color: transparent;");
        m_flow = new FlowLayout(m_chipsWidget, 0, 8, 8);
        scroll->setWidget(m_chipsWidget);
        layout->addWidget(scroll);

        m_opacityAnim = new QPropertyAnimation(this, "windowOpacity");
        m_opacityAnim->setDuration(200);
        m_opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

        refreshUI();
    }

    void refreshUI() {
        QLayoutItem* item;
        while ((item = m_flow->takeAt(0))) {
            if(item->widget()) item->widget()->deleteLater();
            delete item;
        }
        
        QStringList history = m_edit->getHistory();
        int targetContentWidth = m_edit->width();
        int contentHeight = 0;

        if(history.isEmpty()) {
            auto* lbl = new QLabel("暂无历史记录");
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet("color: #555; font-style: italic; margin: 20px; background: transparent; border: none;");
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
                    refreshUI(); 
                });
                m_flow->addWidget(chip);
            }
            
            int effectiveWidth = targetContentWidth - 30;
            int flowHeight = m_flow->heightForWidth(effectiveWidth);
            contentHeight = qMin(400, qMax(120, flowHeight + 50));
        }
        
        this->resize(targetContentWidth + (m_shadowMargin * 2), contentHeight + (m_shadowMargin * 2));
    }

    void showAnimated() {
        refreshUI();
        
        // 坐标对齐逻辑
        QPoint pos = m_edit->mapToGlobal(QPoint(0, m_edit->height()));
        int xPos = pos.x() - m_shadowMargin;
        int yPos = pos.y() + 5 - m_shadowMargin; // 5px 间距
        move(xPos, yPos);
        
        setWindowOpacity(0);
        show();
        
        m_opacityAnim->setStartValue(0);
        m_opacityAnim->setEndValue(1);
        m_opacityAnim->start();
    }

private:
    SearchLineEdit* m_edit;
    QFrame* m_container;
    QWidget* m_chipsWidget;
    FlowLayout* m_flow;
    QPropertyAnimation* m_opacityAnim;
    int m_shadowMargin = 12;
};

// --- SearchLineEdit Implementation ---
SearchLineEdit::SearchLineEdit(QWidget* parent) : QLineEdit(parent) {
    setClearButtonEnabled(true);
    setStyleSheet(
        "QLineEdit { "
        "  background-color: #252526; "
        "  border: 1px solid #333; "
        "  border-radius: 6px; "
        "  padding: 8px 15px; "
        "  color: #eee; "
        "  font-size: 14px; "
        "} "
        "QLineEdit:focus { border: 1px solid #4a90e2; } "
    );
}

void SearchLineEdit::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) showPopup();
    QLineEdit::mouseDoubleClickEvent(e);
}

void SearchLineEdit::showPopup() {
    if(!m_popup) m_popup = new SearchHistoryPopup(this);
    m_popup->showAnimated();
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
