#include "ColorPickerWindow.h"
#include "IconHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QClipboard>
#include <QMimeData>
#include <QColorDialog>
#include <QSettings>
#include <QPainter>
#include <QBuffer>
#include <QImageReader>
#include <QToolTip>
#include <QSet>

// 简单的全屏取色器
class ScreenPickerOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ScreenPickerOverlay() {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setCursor(Qt::CrossCursor);

        // 捕获鼠标当前所在屏幕内容
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) screen = QGuiApplication::primaryScreen();

        // Windows 下 grabWindow(0) 捕获整个桌面，但我们最好限定在当前屏幕避免坐标偏移混乱
        m_screenshot = screen->grabWindow(0, screen->geometry().x(), screen->geometry().y(),
                                         screen->geometry().width(), screen->geometry().height());

        setGeometry(screen->geometry());
    }

signals:
    void colorPicked(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_screenshot);

        // 绘制放大镜效果 (可选)
        QPoint pos = mapFromGlobal(QCursor::pos());
        QRect magRect(pos.x() + 20, pos.y() + 20, 100, 100);
        painter.setPen(Qt::white);
        painter.drawRect(magRect);

        QPixmap mag = m_screenshot.copy(pos.x() - 5, pos.y() - 5, 10, 10).scaled(100, 100);
        painter.drawPixmap(magRect.topLeft(), mag);
        painter.drawRect(pos.x() + 20 + 45, pos.y() + 20 + 45, 10, 10);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            QImage img = m_screenshot.toImage();
            // 确保坐标在图像范围内
            QPoint pos = event->pos();
            if (pos.x() >= 0 && pos.x() < img.width() && pos.y() >= 0 && pos.y() < img.height()) {
                QColor color = img.pixelColor(pos);
                emit colorPicked(color);
            }
            close();
        } else {
            close();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        update();
    }

private:
    QPixmap m_screenshot;
};


ColorPickerWindow::ColorPickerWindow(QWidget* parent)
    : FramelessDialog("吸取颜色", parent)
{
    resize(400, 580);
    setAcceptDrops(true);
    initUI();
    loadFavorites();
    updateColorDisplay(QColor("#4A90E2"));
}

void ColorPickerWindow::initUI() {
    // 基础 QSS 应用
    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            color: #E0E0E0;
            outline: none;
        }
        QLineEdit {
            background-color: #333333;
            border: 1px solid #444444;
            color: #FFFFFF;
            border-radius: 6px;
            padding: 8px;
        }
        QPushButton {
            background-color: #333;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 5px;
        }
        QPushButton:hover {
            background-color: #444;
        }
        QPushButton#PickBtn {
            background-color: #007ACC;
            color: white;
            border: none;
            font-weight: bold;
        }
        QPushButton#PickBtn:hover {
            background-color: #0062A3;
        }
        QListWidget {
            background-color: #181818;
            border: 1px solid #333;
            border-radius: 6px;
        }
        QLabel {
            font-size: 12px;
            color: #888;
        }
    )");

    auto* layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(24, 15, 24, 24);
    layout->setSpacing(16);

    // 1. 颜色显示与 Hex 输入
    auto* topLayout = new QHBoxLayout();

    m_colorPreview = new QLabel();
    m_colorPreview->setFixedSize(80, 80);
    m_colorPreview->setStyleSheet("border-radius: 40px; border: 4px solid #333;");

    auto* infoLayout = new QVBoxLayout();
    m_hexEdit = new QLineEdit();
    m_hexEdit->setReadOnly(true);
    m_hexEdit->setAlignment(Qt::AlignCenter);
    m_hexEdit->setStyleSheet("font-size: 18px; font-weight: bold; color: #007ACC;");

    auto* pickBtn = new QPushButton(" 屏幕吸色");
    pickBtn->setObjectName("PickBtn");
    pickBtn->setIcon(IconHelper::getIcon("zap", "#FFFFFF"));
    pickBtn->setFixedHeight(36);
    connect(pickBtn, &QPushButton::clicked, this, &ColorPickerWindow::startPickColor);

    infoLayout->addWidget(m_hexEdit);
    infoLayout->addWidget(pickBtn);

    topLayout->addWidget(m_colorPreview);
    topLayout->addLayout(infoLayout);
    layout->addLayout(topLayout);

    // 2. 操作按钮
    auto* btnLayout = new QHBoxLayout();

    auto* wheelBtn = new QPushButton(" 色轮");
    wheelBtn->setIcon(IconHelper::getIcon("palette", "#AAAAAA"));
    wheelBtn->setStyleSheet("QPushButton { background: #333; color: #EEE; border: 1px solid #444; border-radius: 4px; height: 32px; } QPushButton:hover { background: #444; }");
    connect(wheelBtn, &QPushButton::clicked, this, &ColorPickerWindow::openColorWheel);

    auto* favBtn = new QPushButton(" 收藏");
    favBtn->setIcon(IconHelper::getIcon("star", "#AAAAAA"));
    favBtn->setStyleSheet("QPushButton { background: #333; color: #EEE; border: 1px solid #444; border-radius: 4px; height: 32px; } QPushButton:hover { background: #444; }");
    connect(favBtn, &QPushButton::clicked, this, &ColorPickerWindow::addCurrentToFavorites);

    btnLayout->addWidget(wheelBtn);
    btnLayout->addWidget(favBtn);
    layout->addLayout(btnLayout);

    // 3. 收藏列表
    layout->addWidget(new QLabel("收藏的颜色:"));
    m_favoriteList = new QListWidget();
    m_favoriteList->setFixedHeight(120);
    m_favoriteList->setFlow(QListWidget::LeftToRight);
    m_favoriteList->setWrapping(true);
    m_favoriteList->setSpacing(5);
    m_favoriteList->setStyleSheet("QListWidget { background: #181818; border: 1px solid #333; border-radius: 4px; padding: 5px; outline: none; }");
    connect(m_favoriteList, &QListWidget::itemClicked, this, &ColorPickerWindow::onFavoriteSelected);
    layout->addWidget(m_favoriteList);

    // 4. 图片解析颜色
    layout->addWidget(new QLabel("图片解析颜色 (拖拽图片至此):"));
    m_extractedList = new QListWidget();
    m_extractedList->setFixedHeight(120);
    m_extractedList->setFlow(QListWidget::LeftToRight);
    m_extractedList->setWrapping(true);
    m_extractedList->setSpacing(5);
    m_extractedList->setStyleSheet("QListWidget { background: #181818; border: 1px solid #333; border-radius: 4px; padding: 5px; outline: none; }");
    connect(m_extractedList, &QListWidget::itemClicked, this, &ColorPickerWindow::onFavoriteSelected);
    layout->addWidget(m_extractedList);

    auto* clearBtn = new QPushButton("清空收藏");
    clearBtn->setStyleSheet("QPushButton { color: #888; background: transparent; border: none; font-size: 11px; } QPushButton:hover { color: #e74c3c; text-decoration: underline; }");
    connect(clearBtn, &QPushButton::clicked, this, &ColorPickerWindow::clearFavorites);
    layout->addWidget(clearBtn, 0, Qt::AlignRight);
}

void ColorPickerWindow::updateColorDisplay(const QColor& color) {
    m_currentColor = color;
    QString hex = color.name().toUpper();
    m_hexEdit->setText(hex);
    m_colorPreview->setStyleSheet(QString("background-color: %1; border-radius: 40px; border: 4px solid #333;").arg(hex));

    // 复制到剪贴板
    QApplication::clipboard()->setText(hex);
    QToolTip::showText(QCursor::pos(), "已复制颜色: " + hex);
}

void ColorPickerWindow::startPickColor() {
    auto* picker = new ScreenPickerOverlay();
    connect(picker, &ScreenPickerOverlay::colorPicked, this, &ColorPickerWindow::updateColorDisplay);
    picker->show();
}

void ColorPickerWindow::openColorWheel() {
    QColor color = QColorDialog::getColor(m_currentColor, this, "选择颜色", QColorDialog::ShowAlphaChannel);
    if (color.isValid()) {
        updateColorDisplay(color);
    }
}

void ColorPickerWindow::addCurrentToFavorites() {
    QString hex = m_currentColor.name().toUpper();
    saveFavorite(hex);
    loadFavorites();
}

void ColorPickerWindow::onFavoriteSelected(QListWidgetItem* item) {
    if (!item) return;
    updateColorDisplay(QColor(item->toolTip()));
}

void ColorPickerWindow::saveFavorite(const QString& hex) {
    QSettings settings("RapidNotes", "ColorPicker");
    QStringList favs = settings.value("favorites").toStringList();
    if (!favs.contains(hex)) {
        favs.prepend(hex);
        if (favs.size() > 50) favs.removeLast();
        settings.setValue("favorites", favs);
    }
}

void ColorPickerWindow::loadFavorites() {
    m_favoriteList->clear();
    QSettings settings("RapidNotes", "ColorPicker");
    QStringList favs = settings.value("favorites").toStringList();
    for (const QString& hex : favs) {
        auto* item = new QListWidgetItem();
        item->setToolTip(hex);
        item->setSizeHint(QSize(30, 30));

        QPixmap pix(30, 30);
        pix.fill(QColor(hex));
        item->setIcon(QIcon(pix));

        m_favoriteList->addItem(item);
    }
}

void ColorPickerWindow::clearFavorites() {
    QSettings settings("RapidNotes", "ColorPicker");
    settings.remove("favorites");
    loadFavorites();
}

void ColorPickerWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
        event->acceptProposedAction();
    }
}

void ColorPickerWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    if (mime->hasImage()) {
        extractColorsFromImage(qvariant_cast<QImage>(mime->imageData()));
    } else if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (url.isLocalFile()) {
                QImage img(url.toLocalFile());
                if (!img.isNull()) {
                    extractColorsFromImage(img);
                    break;
                }
            }
        }
    }
}

void ColorPickerWindow::extractColorsFromImage(const QImage& img) {
    m_extractedList->clear();
    if (img.isNull()) return;

    // 简单采样一些颜色 (9个点)
    int w = img.width();
    int h = img.height();
    QSet<QString> colors;

    for (int i = 1; i <= 3; ++i) {
        for (int j = 1; j <= 3; ++j) {
            QColor c = img.pixelColor(w * i / 4, h * j / 4);
            colors.insert(c.name().toUpper());
        }
    }

    for (const QString& hex : colors) {
        auto* item = new QListWidgetItem();
        item->setToolTip(hex);
        item->setSizeHint(QSize(30, 30));
        QPixmap pix(30, 30);
        pix.fill(QColor(hex));
        item->setIcon(QIcon(pix));
        m_extractedList->addItem(item);
    }
}

#include "ColorPickerWindow.moc"
