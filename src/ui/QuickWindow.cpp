#include "QuickWindow.h"
#include "../core/DatabaseManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>

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
    m_searchEdit->setPlaceholderText("搜索笔记或记录灵感...");
    connect(m_searchEdit, &QLineEdit::textChanged, [this](const QString& text){
        if (text.isEmpty()) {
            m_model->setNotes(DatabaseManager::instance().getAllNotes());
        } else {
            m_model->setNotes(DatabaseManager::instance().searchNotes(text));
        }
    });

    m_listView = new QListView();
    m_model = new NoteModel(this);
    m_listView->setModel(m_model);
    m_listView->setFixedHeight(300);

    layout->addWidget(m_searchEdit);
    layout->addWidget(m_listView);

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
    return QWidget::event(event);
}
