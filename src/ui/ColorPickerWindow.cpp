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
#include <QScrollArea>
#include <QFrame>

// ----------------------------------------------------------------------------
// ScreenPickerOverlay: 全屏取色遮罩层
// ----------------------------------------------------------------------------
class ScreenPickerOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ScreenPickerOverlay() {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setCursor(Qt::CrossCursor);

        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) screen = QGuiApplication::primaryScreen();

        m_screenshot = screen->grabWindow(0, screen->geometry().x(), screen->geometry().y(),
                                         screen->geometry().width(), screen->geometry().height());
        setGeometry(screen->geometry());
        setMouseTracking(true);
    }

signals:
    void colorPicked(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_screenshot);

        QPoint pos = mapFromGlobal(QCursor::pos());
        int size = 120;
        QRect magRect(pos.x() + 20, pos.y() + 20, size, size);

        if (magRect.right() > width()) magRect.moveLeft(pos.x() - size - 20);
        if (magRect.bottom() > height()) magRect.moveTop(pos.y() - size - 20);

        painter.setBrush(Qt::black);
        painter.drawRect(magRect.adjusted(-2, -2, 2, 2));

        QPixmap mag = m_screenshot.copy(pos.x() - 5, pos.y() - 5, 11, 11).scaled(size, size, Qt::KeepAspectRatio);
        painter.drawPixmap(magRect.topLeft(), mag);

        painter.setPen(QPen(Qt::white, 1));
        painter.drawRect(magRect.center().x() - 5, magRect.center().y() - 5, 10, 10);

        QColor c = m_screenshot.toImage().pixelColor(pos);
        painter.fillRect(magRect.left(), magRect.bottom() + 5, size, 20, Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(QRect(magRect.left(), magRect.bottom() + 5, size, 20), Qt::AlignCenter, c.name().toUpper());
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            QImage img = m_screenshot.toImage();
            QPoint pos = event->pos();
            if (pos.x() >= 0 && pos.x() < img.width() && pos.y() >= 0 && pos.y() < img.height()) {
                emit colorPicked(img.pixelColor(pos));
            }
        }
        close();
    }

    void mouseMoveEvent(QMouseEvent* event) override { update(); }
    void keyPressEvent(QKeyEvent* event) override { if(event->key() == Qt::Key_Escape) close(); }

private:
    QPixmap m_screenshot;
};

// ----------------------------------------------------------------------------
// ColorPickerWindow 实现
// ----------------------------------------------------------------------------
ColorPickerWindow::ColorPickerWindow(QWidget* parent)
    : FramelessDialog("吸取颜色", parent)
{
    resize(520, 800);
    setAcceptDrops(true);
    initUI();
    loadFavorites();
    loadHistory();
    updateColorDisplay(QColor("#4A90E2"), false);
}

void ColorPickerWindow::initUI() {
    setStyleSheet(R"(
        QWidget { font-family: "Microsoft YaHei", "Segoe UI", sans-serif; color: #E0E0E0; }
        QLineEdit { background-color: #2A2A2A; border: 1px solid #444; color: #FFFFFF; border-radius: 6px; padding: 8px; font-weight: bold; }
        QPushButton { background-color: #333; border: 1px solid #444; border-radius: 4px; padding: 6px; }
        QPushButton:hover { background-color: #444; }
        QPushButton#PickBtn { background-color: #007ACC; color: white; border: none; font-weight: bold; }
        QPushButton#PickBtn:hover { background-color: #0062A3; }
        QListWidget { background-color: #181818; border: 1px solid #333; border-radius: 6px; outline: none; }
        QLabel#FormatLabel { color: #CCC; font-size: 13px; font-family: Consolas, monospace; }
        QLabel#SectionTitle { color: #AAA; font-size: 12px; font-weight: bold; border-left: 3px solid #4A90E2; padding-left: 8px; margin-top: 12px; }
    )");

    auto* scroll = new QScrollArea(m_contentArea);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");

    auto* mainContainer = new QWidget();
    auto* layout = new QVBoxLayout(mainContainer);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(12);

    auto* topLayout = new QHBoxLayout();
    m_colorPreview = new QLabel();
    m_colorPreview->setFixedSize(100, 100);
    m_colorPreview->setStyleSheet("border-radius: 12px; border: 4px solid #333;");

    auto* mainInfoLayout = new QVBoxLayout();
    m_hexEdit = new QLineEdit();
    m_hexEdit->setAlignment(Qt::AlignCenter);
    m_hexEdit->setStyleSheet("font-size: 24px; color: #007ACC; height: 40px;");

    auto* pickBtn = new QPushButton(" 屏幕取色 (ESC 取消)");
    pickBtn->setObjectName("PickBtn");
    pickBtn->setAutoDefault(false);
    pickBtn->setIcon(IconHelper::getIcon("zap", "#FFFFFF"));
    pickBtn->setFixedHeight(40);
    connect(pickBtn, &QPushButton::clicked, this, &ColorPickerWindow::startPickColor);

    mainInfoLayout->addWidget(m_hexEdit);
    mainInfoLayout->addWidget(pickBtn);
    topLayout->addWidget(m_colorPreview);
    topLayout->addLayout(mainInfoLayout);
    layout->addLayout(topLayout);

    auto* formatsFrame = new QFrame();
    formatsFrame->setStyleSheet("background: #252526; border-radius: 8px; padding: 10px;");
    auto* formatsLayout = new QGridLayout(formatsFrame);

    auto addFormatRow = [&](const QString& name, QLabel*& label, int row) {
        auto* btn = new QPushButton(name);
        btn->setAutoDefault(false);
        btn->setFixedSize(60, 26);
        btn->setStyleSheet("font-size: 12px; font-weight: bold; color: #AAA; background: #333; border: 1px solid #444;");
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, &ColorPickerWindow::copySpecificFormat);

        label = new QLabel("---");
        label->setObjectName("FormatLabel");
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);

        formatsLayout->addWidget(btn, row, 0);
        formatsLayout->addWidget(label, row, 1);
    };

    addFormatRow("RGB", m_rgbLabel, 0);
    addFormatRow("HSV", m_hsvLabel, 1);
    addFormatRow("CMYK", m_cmykLabel, 2);
    layout->addWidget(formatsFrame);

    auto* variationsTitle = new QLabel("色彩变体 (明暗变化)");
    variationsTitle->setObjectName("SectionTitle");
    layout->addWidget(variationsTitle);

    m_shadesContainer = new QWidget();
    new QHBoxLayout(m_shadesContainer);
    m_shadesContainer->layout()->setContentsMargins(0, 0, 0, 0);
    m_shadesContainer->layout()->setSpacing(4);
    layout->addWidget(m_shadesContainer);

    auto* harmonyTitle = new QLabel("色彩和谐 (配色理论)");
    harmonyTitle->setObjectName("SectionTitle");
    layout->addWidget(harmonyTitle);

    m_harmonyContainer = new QWidget();
    new QHBoxLayout(m_harmonyContainer);
    m_harmonyContainer->layout()->setContentsMargins(0, 0, 0, 0);
    m_harmonyContainer->layout()->setSpacing(8);
    layout->addWidget(m_harmonyContainer);

    auto* accessTitle = new QLabel("无障碍检查 (WCAG 2.1)");
    accessTitle->setObjectName("SectionTitle");
    layout->addWidget(accessTitle);

    auto* accessFrame = new QFrame();
    accessFrame->setStyleSheet("background: #252526; border-radius: 8px; padding: 12px;");
    auto* accessLayout = new QHBoxLayout(accessFrame);
    m_contrastLabel = new QLabel("对比度: ---");
    m_contrastLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_aaLabel = new QLabel("AA");
    m_aaaLabel = new QLabel("AAA");
    QString badgeStyle = "padding: 3px 10px; border-radius: 4px; font-weight: bold; font-size: 11px; color: #AAA; background: #383838;";
    m_aaLabel->setStyleSheet(badgeStyle);
    m_aaaLabel->setStyleSheet(badgeStyle);
    accessLayout->addWidget(m_contrastLabel);
    accessLayout->addStretch();
    accessLayout->addWidget(m_aaLabel);
    accessLayout->addWidget(m_aaaLabel);
    layout->addWidget(accessFrame);

    auto* blindTitle = new QLabel("视觉障碍模拟");
    blindTitle->setObjectName("SectionTitle");
    layout->addWidget(blindTitle);
    m_blindnessContainer = new QWidget();
    auto* bLayout = new QVBoxLayout(m_blindnessContainer);
    bLayout->setContentsMargins(0, 0, 0, 0);
    bLayout->setSpacing(4);
    layout->addWidget(m_blindnessContainer);

    auto* listsLayout = new QHBoxLayout();

    auto* favCol = new QVBoxLayout();
    favCol->addWidget(new QLabel("我的收藏:"));
    m_favoriteList = new QListWidget();
    m_favoriteList->setFixedHeight(120);
    m_favoriteList->setFlow(QListWidget::LeftToRight);
    m_favoriteList->setWrapping(true);
    m_favoriteList->setSpacing(4);
    connect(m_favoriteList, &QListWidget::itemClicked, this, &ColorPickerWindow::onColorSelected);
    favCol->addWidget(m_favoriteList);
    listsLayout->addLayout(favCol);

    auto* histCol = new QVBoxLayout();
    histCol->addWidget(new QLabel("最近拾取:"));
    m_historyList = new QListWidget();
    m_historyList->setFixedHeight(120);
    m_historyList->setFlow(QListWidget::LeftToRight);
    m_historyList->setWrapping(true);
    m_historyList->setSpacing(4);
    connect(m_historyList, &QListWidget::itemClicked, this, &ColorPickerWindow::onColorSelected);
    histCol->addWidget(m_historyList);
    listsLayout->addLayout(histCol);

    layout->addLayout(listsLayout);

    auto* bottomBtnLayout = new QHBoxLayout();
    auto* wheelBtn = new QPushButton(" 色轮");
    wheelBtn->setAutoDefault(false);
    wheelBtn->setIcon(IconHelper::getIcon("palette", "#AAAAAA"));
    connect(wheelBtn, &QPushButton::clicked, this, &ColorPickerWindow::openColorWheel);

    auto* favBtn = new QPushButton(" 收藏");
    favBtn->setAutoDefault(false);
    favBtn->setIcon(IconHelper::getIcon("star", "#AAAAAA"));
    connect(favBtn, &QPushButton::clicked, this, &ColorPickerWindow::addCurrentToFavorites);

    auto* exportBtn = new QPushButton(" 导出方案");
    exportBtn->setAutoDefault(false);
    exportBtn->setIcon(IconHelper::getIcon("save", "#AAAAAA"));
    connect(exportBtn, &QPushButton::clicked, this, &ColorPickerWindow::exportPaletteAsCSS);

    auto* clearBtn = new QPushButton("清空收藏");
    clearBtn->setAutoDefault(false);
    clearBtn->setStyleSheet("color: #888; border: none; background: transparent; font-size: 11px;");
    connect(clearBtn, &QPushButton::clicked, this, &ColorPickerWindow::clearFavorites);

    bottomBtnLayout->addWidget(wheelBtn);
    bottomBtnLayout->addWidget(favBtn);
    bottomBtnLayout->addWidget(exportBtn);
    bottomBtnLayout->addStretch();
    bottomBtnLayout->addWidget(clearBtn);
    layout->addLayout(bottomBtnLayout);

    auto* imageTitle = new QLabel("图片解析 (拖拽图片至此)");
    imageTitle->setObjectName("SectionTitle");
    layout->addWidget(imageTitle);
    m_extractedList = new QListWidget();
    m_extractedList->setFixedHeight(100);
    m_extractedList->setFlow(QListWidget::LeftToRight);
    m_extractedList->setWrapping(true);
    m_extractedList->setSpacing(4);
    connect(m_extractedList, &QListWidget::itemClicked, this, &ColorPickerWindow::onColorSelected);
    layout->addWidget(m_extractedList);

    scroll->setWidget(mainContainer);
    auto* finalLayout = new QVBoxLayout(m_contentArea);
    finalLayout->setContentsMargins(0, 0, 0, 0);
    finalLayout->addWidget(scroll);
}

void ColorPickerWindow::updateColorDisplay(const QColor& color, bool addToHist) {
    if (!color.isValid()) return;
    m_currentColor = color;
    QString hex = color.name().toUpper();
    m_hexEdit->setText(hex);
    m_colorPreview->setStyleSheet(QString("background-color: %1; border-radius: 12px; border: 4px solid #333;").arg(hex));

    updateFormats(color);
    updateVariations(color);
    updateHarmony(color);
    updateAccessibility(color);
    updateBlindnessSimulation(color);

    if (addToHist) addToHistory(hex);

    QApplication::clipboard()->setText(hex);
    QToolTip::showText(QCursor::pos(), "已拾取并复制 HEX: " + hex, this, QRect(), 1500);
}

void ColorPickerWindow::updateFormats(const QColor& color) {
    m_rgbLabel->setText(QString("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue()));
    m_hsvLabel->setText(QString("hsv(%1, %2%, %3%)").arg(color.hue() < 0 ? 0 : color.hue()).arg(int(color.saturationF()*100)).arg(int(color.valueF()*100)));
    m_cmykLabel->setText(QString("cmyk(%1%, %2%, %3%, %4%)").arg(int(color.cyanF()*100)).arg(int(color.magentaF()*100)).arg(int(color.yellowF()*100)).arg(int(color.blackF()*100)));
}

double ColorPickerWindow::getLuminance(const QColor& c) {
    auto fix = [](double v) { return (v <= 0.03928) ? (v / 12.92) : pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * fix(c.redF()) + 0.7152 * fix(c.greenF()) + 0.0722 * fix(c.blueF());
}

double ColorPickerWindow::getContrastRatio(const QColor& c1, const QColor& c2) {
    double l1 = getLuminance(c1), l2 = getLuminance(c2);
    return (qMax(l1, l2) + 0.05) / (qMin(l1, l2) + 0.05);
}

void ColorPickerWindow::updateAccessibility(const QColor& color) {
    double contrastWhite = getContrastRatio(color, Qt::white);
    double contrastBlack = getContrastRatio(color, Qt::black);
    double bestContrast = qMax(contrastWhite, contrastBlack);
    m_contrastLabel->setText(QString("最高对比度: %1:1").arg(bestContrast, 0, 'f', 2));

    auto updateBadge = [](QLabel* lbl, bool pass) {
        if (pass) lbl->setStyleSheet("background: #27ae60; color: white; padding: 3px 10px; border-radius: 4px; font-weight: bold; font-size: 11px;");
        else lbl->setStyleSheet("background: #c0392b; color: white; padding: 3px 10px; border-radius: 4px; font-weight: bold; font-size: 11px;");
    };
    updateBadge(m_aaLabel, bestContrast >= 4.5);
    updateBadge(m_aaaLabel, bestContrast >= 7.0);
}

QColor ColorPickerWindow::simulateBlindness(const QColor& color, const double m[3][3]) {
    double r = color.redF(), g = color.greenF(), b = color.blueF();
    double nr = qBound(0.0, r * m[0][0] + g * m[0][1] + b * m[0][2], 1.0);
    double ng = qBound(0.0, r * m[1][0] + g * m[1][1] + b * m[1][2], 1.0);
    double nb = qBound(0.0, r * m[2][0] + g * m[2][1] + b * m[2][2], 1.0);
    return QColor::fromRgbF(nr, ng, nb);
}

void ColorPickerWindow::updateBlindnessSimulation(const QColor& color) {
    QLayoutItem* child;
    while ((child = m_blindnessContainer->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    static const struct { QString name; double m[3][3]; } types[] = {
        {"红色盲 (Protanopia)", {{0.567, 0.433, 0.0}, {0.558, 0.442, 0.0}, {0.0, 0.242, 0.758}}},
        {"绿色盲 (Deuteranopia)", {{0.625, 0.375, 0.0}, {0.70, 0.30, 0.0}, {0.0, 0.30, 0.70}}},
        {"蓝色盲 (Tritanopia)", {{0.95, 0.05, 0.0}, {0.0, 0.433, 0.567}, {0.0, 0.475, 0.525}}}
    };

    for (const auto& t : types) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(t.name);
        lbl->setStyleSheet("font-size: 11px; color: #888;");
        auto* bar = new QFrame();
        QColor simulated = simulateBlindness(color, t.m);
        bar->setFixedHeight(14);
        bar->setStyleSheet(QString("background: %1; border-radius: 3px; border: 1px solid #333;").arg(simulated.name()));
        row->addWidget(lbl, 1);
        row->addWidget(bar, 2);
        static_cast<QVBoxLayout*>(m_blindnessContainer->layout())->addLayout(row);
    }
}

void ColorPickerWindow::exportPaletteAsCSS() {
    QStringList css;
    css << "/* RapidNotes Generated Palette */";
    css << ":root {";
    css << QString("  --primary-color: %1;").arg(m_currentColor.name().toUpper());
    for (int i = 0; i < m_shadesContainer->layout()->count(); ++i) {
        if (auto* btn = qobject_cast<QPushButton*>(m_shadesContainer->layout()->itemAt(i)->widget())) {
            css << QString("  --color-variant-%1: %2;").arg(i+1).arg(btn->styleSheet().section(":", 1).section(";", 0, 0).trimmed().toUpper());
        }
    }
    css << "}";
    QApplication::clipboard()->setText(css.join("\n"));
    QToolTip::showText(QCursor::pos(), "配色方案已导出并存入剪贴板", this);
}

void ColorPickerWindow::copySpecificFormat() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString fmt = btn->text();
    QString val;
    if (fmt == "RGB") val = m_rgbLabel->text();
    else if (fmt == "HSV") val = m_hsvLabel->text();
    else if (fmt == "CMYK") val = m_cmykLabel->text();
    if (!val.isEmpty()) {
        QApplication::clipboard()->setText(val);
        QToolTip::showText(QCursor::pos(), "已复制 " + fmt + " 格式", this);
    }
}

void ColorPickerWindow::updateVariations(const QColor& color) {
    QLayoutItem* child;
    while ((child = m_shadesContainer->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    for (int i = 1; i <= 7; ++i) {
        QColor v = color;
        if (i < 4) v = v.darker(100 + (4-i)*30);
        else if (i > 4) v = v.lighter(100 + (i-4)*30);

        auto* btn = new QPushButton();
        btn->setAutoDefault(false);
        btn->setFixedSize(60, 30);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("background-color: %1; border: 1px solid #333;").arg(v.name()));
        connect(btn, &QPushButton::clicked, this, [this, v](){ updateColorDisplay(v); });
        m_shadesContainer->layout()->addWidget(btn);
    }
}

void ColorPickerWindow::updateHarmony(const QColor& color) {
    QLayoutItem* child;
    while ((child = m_harmonyContainer->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    auto addHarmonyBtn = [&](const QString& label, const QColor& c) {
        auto* vCol = new QVBoxLayout();
        auto* btn = new QPushButton();
        btn->setAutoDefault(false);
        btn->setFixedSize(50, 50);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("background-color: %1; border-radius: 25px; border: 2px solid #333;").arg(c.name()));
        connect(btn, &QPushButton::clicked, this, [this, c](){ updateColorDisplay(c); });
        auto* lbl = new QLabel(label);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-size: 9px; color: #888;");
        vCol->addWidget(btn);
        vCol->addWidget(lbl);
        m_harmonyContainer->layout()->addItem(vCol);
    };

    QColor comp = QColor::fromHsv((color.hue() + 180) % 360, color.saturation(), color.value());
    addHarmonyBtn("互补色", comp);
    QColor ana1 = QColor::fromHsv((color.hue() + 330) % 360, color.saturation(), color.value());
    QColor ana2 = QColor::fromHsv((color.hue() + 30) % 360, color.saturation(), color.value());
    addHarmonyBtn("类比左", ana1);
    addHarmonyBtn("类比右", ana2);
    QColor tri1 = QColor::fromHsv((color.hue() + 120) % 360, color.saturation(), color.value());
    QColor tri2 = QColor::fromHsv((color.hue() + 240) % 360, color.saturation(), color.value());
    addHarmonyBtn("三分 A", tri1);
    addHarmonyBtn("三分 B", tri2);
    QColor split1 = QColor::fromHsv((color.hue() + 150) % 360, color.saturation(), color.value());
    QColor split2 = QColor::fromHsv((color.hue() + 210) % 360, color.saturation(), color.value());
    addHarmonyBtn("分裂左", split1);
    addHarmonyBtn("分裂右", split2);
    QColor sq = QColor::fromHsv((color.hue() + 90) % 360, color.saturation(), color.value());
    addHarmonyBtn("方形", sq);
}

void ColorPickerWindow::startPickColor() {
    auto* picker = new ScreenPickerOverlay();
    connect(picker, &ScreenPickerOverlay::colorPicked, this, [this](const QColor& c){ updateColorDisplay(c); });
    picker->show();
}

void ColorPickerWindow::openColorWheel() {
    QColor color = QColorDialog::getColor(m_currentColor, this, "选择颜色", QColorDialog::ShowAlphaChannel);
    if (color.isValid()) updateColorDisplay(color);
}

void ColorPickerWindow::onColorSelected(QListWidgetItem* item) {
    if (item) updateColorDisplay(QColor(item->toolTip()), false);
}

void ColorPickerWindow::addCurrentToFavorites() {
    saveFavorite(m_currentColor.name().toUpper());
    loadFavorites();
}

void ColorPickerWindow::addToHistory(const QString& hex) {
    QSettings settings("RapidNotes", "ColorPicker");
    QStringList hist = settings.value("history").toStringList();
    hist.removeAll(hex);
    hist.prepend(hex);
    if (hist.size() > 14) hist.removeLast();
    settings.setValue("history", hist);
    loadHistory();
}

void ColorPickerWindow::loadHistory() {
    m_historyList->clear();
    QSettings settings("RapidNotes", "ColorPicker");
    QStringList hist = settings.value("history").toStringList();
    for (const QString& hex : hist) {
        auto* item = new QListWidgetItem();
        item->setToolTip(hex);
        item->setSizeHint(QSize(32, 32));
        QPixmap pix(32, 32);
        pix.fill(QColor(hex));
        item->setIcon(QIcon(pix));
        m_historyList->addItem(item);
    }
}

void ColorPickerWindow::saveFavorite(const QString& hex) {
    QSettings settings("RapidNotes", "ColorPicker");
    QStringList favs = settings.value("favorites").toStringList();
    if (!favs.contains(hex)) {
        favs.prepend(hex);
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
        item->setSizeHint(QSize(32, 32));
        QPixmap pix(32, 32);
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
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) event->acceptProposedAction();
}

void ColorPickerWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    if (mime->hasImage()) extractColorsFromImage(qvariant_cast<QImage>(mime->imageData()));
    else if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (url.isLocalFile()) {
                QImage img(url.toLocalFile());
                if (!img.isNull()) { extractColorsFromImage(img); break; }
            }
        }
    }
}

void ColorPickerWindow::extractColorsFromImage(const QImage& img) {
    m_extractedList->clear();
    if (img.isNull()) return;

    QSet<QString> colors;
    for (int i = 1; i <= 5; ++i) {
        for (int j = 1; j <= 5; ++j) {
            colors.insert(img.pixelColor(img.width() * i / 6, img.height() * j / 6).name().toUpper());
        }
    }

    for (const QString& hex : colors) {
        auto* item = new QListWidgetItem();
        item->setToolTip(hex);
        item->setSizeHint(QSize(32, 32));
        QPixmap pix(32, 32);
        pix.fill(QColor(hex));
        item->setIcon(QIcon(pix));
        m_extractedList->addItem(item);
    }
}

#include "ColorPickerWindow.moc"
