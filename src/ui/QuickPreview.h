#ifndef QUICKPREVIEW_H
#define QUICKPREVIEW_H

#include <QDialog>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QPushButton>
#include <QPixmap>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QShortcut>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QPalette>
#include "IconHelper.h"
#include "Editor.h"

class ScalableImageLabel : public QLabel {
    Q_OBJECT
public:
    explicit ScalableImageLabel(QWidget* parent = nullptr) : QLabel(parent) {
        setAlignment(Qt::AlignCenter);
        setStyleSheet("background: transparent; border: none;");
    }

    void setPixmapData(const QPixmap& pix) {
        m_originalPixmap = pix;
        updateDisplay();
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        updateDisplay();
    }

private:
    void updateDisplay() {
        if (m_originalPixmap.isNull()) return;
        setPixmap(m_originalPixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    QPixmap m_originalPixmap;
};

class QuickPreview : public QDialog {
    Q_OBJECT
public:
    explicit QuickPreview(QWidget* parent = nullptr) : QDialog(parent, Qt::FramelessWindowHint | Qt::Window) {
        setAttribute(Qt::WA_TranslucentBackground);
        setupUI();
    }

    void showPreview(const QString& title, const QString& content, const QString& type, const QByteArray& dataBlob, const QPoint& pos) {
        m_currentTitle = title;
        m_mode = "text";
        m_dataList.clear();
        m_currentIndex = 0;

        if (type == "image" && !dataBlob.isEmpty()) {
            m_mode = "gallery";
            m_dataList.append(dataBlob);
        } else {
            QStringList paths = content.split(';', Qt::SkipEmptyParts);
            QStringList validImages;
            static const QStringList imgExts = {"png", "jpg", "jpeg", "bmp", "gif", "webp", "ico", "svg", "tif"};

            for (const QString& p : paths) {
                QString cleanP = p.trimmed().remove('\"');
                QFileInfo info(cleanP);
                if (info.exists() && imgExts.contains(info.suffix().toLower())) {
                    validImages.append(cleanP);
                }
            }

            if (!validImages.isEmpty()) {
                m_mode = "gallery";
                for (const QString& p : validImages) m_dataList.append(p);
            } else {
                m_mode = "text";
                m_dataList.append(content);
            }
        }

        loadCurrentContent();

        if (!isVisible()) {
            centerOnScreen();
            resize(1130, 740);
        }

        show();
        raise();
        activateWindow();
    }

private:
    void setupUI() {
        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(10, 10, 10, 10);

        m_container = new QFrame();
        m_container->setObjectName("PreviewContainer");
        m_container->setStyleSheet(
            "QFrame#PreviewContainer { background-color: #1e1e1e; border: 1px solid #333; border-radius: 8px; }"
        );
        
        auto* containerLayout = new QVBoxLayout(m_container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        auto* titleBar = new QWidget();
        titleBar->setFixedHeight(36);
        titleBar->setStyleSheet("background-color: #252526; border-top-left-radius: 8px; border-top-right-radius: 8px; border-bottom: 1px solid #333;");
        auto* titleLayout = new QHBoxLayout(titleBar);
        titleLayout->setContentsMargins(12, 0, 4, 0);
        titleLayout->setSpacing(0);

        m_titleLabel = new QLabel("预览");
        m_titleLabel->setStyleSheet("font-weight: bold; color: #ddd; background: transparent;");
        titleLayout->addWidget(m_titleLabel);
        titleLayout->addStretch();

        // 【修复】改用 SVG 图标，确保按钮 100% 可见
        auto createTitleBtn = [this](const QString& iconName, const QString& hoverColor = "") {
            QPushButton* btn = new QPushButton();
            btn->setFixedSize(32, 32);
            btn->setIcon(IconHelper::getIcon(iconName, "#aaaaaa"));
            btn->setIconSize(QSize(16, 16));
            QString style = "QPushButton { background: transparent; border: none; border-radius: 4px; } "
                            "QPushButton:hover { background-color: " + (hoverColor.isEmpty() ? "rgba(255, 255, 255, 0.1)" : hoverColor) + "; }";
            btn->setStyleSheet(style);
            return btn;
        };

        auto* btnMin = createTitleBtn("minimize");
        connect(btnMin, &QPushButton::clicked, this, &QuickPreview::showMinimized);
        titleLayout->addWidget(btnMin);

        m_btnMax = createTitleBtn("maximize");
        connect(m_btnMax, &QPushButton::clicked, this, &QuickPreview::toggleMaximize);
        titleLayout->addWidget(m_btnMax);

        auto* btnClose = createTitleBtn("close", "#e74c3c");
        connect(btnClose, &QPushButton::clicked, this, &QuickPreview::hide);
        titleLayout->addWidget(btnClose);

        containerLayout->addWidget(titleBar);

        auto* contentArea = new QWidget();
        auto* contentLayout = new QVBoxLayout(contentArea);
        contentLayout->setContentsMargins(15, 5, 15, 5);

        m_textEdit = new QTextEdit();
        m_textEdit->setReadOnly(true);
        m_textEdit->setStyleSheet(
            "QTextEdit { background-color: #1e1e1e; border: none; color: #eee; padding: 10px; "
            "font-family: 'Microsoft YaHei', Consolas, monospace; font-size: 14px; line-height: 1.6; }"
        );
        new MarkdownHighlighter(m_textEdit->document());
        contentLayout->addWidget(m_textEdit);

        m_imageLabel = new ScalableImageLabel();
        contentLayout->addWidget(m_imageLabel);

        containerLayout->addWidget(contentArea, 1);

        m_controlBar = new QWidget();
        auto* ctrlLayout = new QHBoxLayout(m_controlBar);
        ctrlLayout->setContentsMargins(20, 5, 20, 10);

        QString navBtnStyle = "QPushButton { background-color: #252526; border: 1px solid #333; color: #ddd; padding: 6px 15px; border-radius: 4px; } "
                              "QPushButton:hover { background-color: #3A90FF; border-color: #3A90FF; color: white; }";

        m_btnPrev = new QPushButton("◀ 上一张");
        m_btnPrev->setStyleSheet(navBtnStyle);
        connect(m_btnPrev, &QPushButton::clicked, this, &QuickPreview::prevImage);

        m_btnNext = new QPushButton("下一张 ▶");
        m_btnNext->setStyleSheet(navBtnStyle);
        connect(m_btnNext, &QPushButton::clicked, this, &QuickPreview::nextImage);

        auto* hintLabel = new QLabel("按 [Space] 关闭 | [←/→] 切换");
        hintLabel->setStyleSheet("color: #888; font-size: 11px;");

        ctrlLayout->addWidget(m_btnPrev);
        ctrlLayout->addStretch();
        ctrlLayout->addWidget(hintLabel);
        ctrlLayout->addStretch();
        ctrlLayout->addWidget(m_btnNext);

        containerLayout->addWidget(m_controlBar);
        rootLayout->addWidget(m_container);

        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(15);
        shadow->setColor(QColor(0, 0, 0, 150));
        shadow->setOffset(0, 5);
        m_container->setGraphicsEffect(shadow);

        // 【修复】增加 Space 快捷键绑定，并设置 WindowShortcut 优先级，确保关闭逻辑
        auto* spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
        spaceShortcut->setContext(Qt::WindowShortcut);
        connect(spaceShortcut, &QShortcut::activated, this, &QuickPreview::hide);

        auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        escShortcut->setContext(Qt::WindowShortcut);
        connect(escShortcut, &QShortcut::activated, this, &QuickPreview::hide);

        new QShortcut(QKeySequence(Qt::Key_Left), this, [this](){ if(m_mode=="gallery") prevImage(); });
        new QShortcut(QKeySequence(Qt::Key_Right), this, [this](){ if(m_mode=="gallery") nextImage(); });
    }

    void loadCurrentContent() {
        if (m_dataList.isEmpty()) return;
        QVariant currentData = m_dataList.at(m_currentIndex);
        int total = m_dataList.size();

        if (m_mode == "text") {
            m_titleLabel->setText("📝 文本预览");
            m_textEdit->setPlainText(currentData.toString());
            m_textEdit->show();
            m_imageLabel->hide();
            m_controlBar->hide();
        } else {
            m_titleLabel->setText(QString("🖼️ 图片预览 [%1/%2]").arg(m_currentIndex + 1).arg(total));
            QPixmap pix;
            if (currentData.typeId() == QMetaType::QByteArray) {
                pix.loadFromData(currentData.toByteArray());
            } else {
                pix.load(currentData.toString());
            }
            m_imageLabel->setPixmapData(pix);
            m_textEdit->hide();
            m_imageLabel->show();
            m_controlBar->setVisible(total > 1);
        }
    }

    void prevImage() {
        if (m_currentIndex > 0) {
            m_currentIndex--;
            loadCurrentContent();
        }
    }

    void nextImage() {
        if (m_currentIndex < m_dataList.size() - 1) {
            m_currentIndex++;
            loadCurrentContent();
        }
    }

    void toggleMaximize() {
        if (isMaximized()) {
            showNormal();
            m_btnMax->setIcon(IconHelper::getIcon("maximize", "#aaaaaa"));
            layout()->setContentsMargins(10, 10, 10, 10);
        } else {
            showMaximized();
            m_btnMax->setIcon(IconHelper::getIcon("restore", "#aaaaaa"));
            layout()->setContentsMargins(0, 0, 0, 0);
        }
    }

    void centerOnScreen() {
        QScreen *screen = QApplication::screenAt(QCursor::pos());
        if (!screen) screen = QApplication::primaryScreen();
        if (screen) {
            QRect geom = screen->availableGeometry();
            move(geom.center() - rect().center());
        }
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && event->pos().y() < 50) {
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton && !m_dragPos.isNull()) {
            move(event->globalPosition().toPoint() - m_dragPos);
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        m_dragPos = QPoint();
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->pos().y() < 50) {
            toggleMaximize();
        } else {
            QDialog::mouseDoubleClickEvent(event);
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
        } else {
            QDialog::keyPressEvent(event);
        }
    }

private:
    QFrame* m_container;
    QLabel* m_titleLabel;
    QTextEdit* m_textEdit;
    ScalableImageLabel* m_imageLabel;
    QWidget* m_controlBar;
    QPushButton* m_btnPrev;
    QPushButton* m_btnNext;
    QPushButton* m_btnMax;

    QPoint m_dragPos;
    QString m_currentTitle;
    QString m_mode;
    QList<QVariant> m_dataList;
    int m_currentIndex = 0;
};

#endif // QUICKPREVIEW_H
