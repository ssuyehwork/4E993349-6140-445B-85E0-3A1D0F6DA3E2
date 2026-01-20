#include "QuickWindow.h"
#include "../core/DatabaseManager.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QScreen>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QListWidget>

QuickWindow::QuickWindow(QWidget* parent) : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    initUI();
}

void QuickWindow::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    auto* container = new QWidget();
    container->setObjectName("container");
    container->setStyleSheet(
        "QWidget#container { background: #1E1E1E; border-radius: 10px; border: 1px solid #333; }"
        "QLineEdit { background: transparent; border: none; color: white; font-size: 18px; padding: 10px; }"
        "QListView { background: transparent; border: none; color: #BBB; }"
    );

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 5);
    container->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(container);

    m_searchEdit = new QLineEdit();
    m_searchEdit->installEventFilter(this); // 用于拦截上下键
    m_searchEdit->setPlaceholderText("搜索笔记或记录灵感...");
    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text){
        if (text.isEmpty()) {
            m_model->setNotes(DatabaseManager::instance().getAllNotes());
        } else {
            m_model->setNotes(DatabaseManager::instance().searchNotes(text));
        }
    });

    auto* contentLayout = new QHBoxLayout();

    m_listView = new QListView();
    m_model = new NoteModel(this);
    m_listView->setModel(m_model);

    auto* sideBar = new QListWidget();
    sideBar->setFixedWidth(120);
    sideBar->addItems({"全部", "今日", "已收藏", "回收站"});
    sideBar->setStyleSheet("background: #252526; border: none; font-size: 12px; color: #888;");

    contentLayout->addWidget(sideBar);
    contentLayout->addWidget(m_listView);

    layout->addWidget(m_searchEdit);
    layout->addLayout(contentLayout);

    mainLayout->addWidget(container);
    setFixedSize(600, 400);
}

void QuickWindow::showCentered() {
    QRect screenGeom = screen()->geometry();
    move(screenGeom.center() - rect().center());
    show();
    activateWindow();
    m_searchEdit->setFocus();
}

bool QuickWindow::event(QEvent* event) {
    if (event->type() == QEvent::WindowDeactivate) {
        hide();
    }
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            QModelIndex idx = m_listView->currentIndex();
            if (idx.isValid()) {
                QGuiApplication::clipboard()->setText(idx.data(Qt::UserRole + 3).toString());
                hide();
            }
        }
    }
    return QWidget::event(event);
}

bool QuickWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Up) {
            int row = m_listView->currentIndex().row();
            m_listView->setCurrentIndex(m_model->index(qMax(0, row - 1), 0));
            return true;
        } else if (ke->key() == Qt::Key_Down) {
            int row = m_listView->currentIndex().row();
            m_listView->setCurrentIndex(m_model->index(qMin(m_model->rowCount() - 1, row + 1), 0));
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
