#include "ColorPickerWindow.h"
#include "IconHelper.h"
#include "StringUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QMouseEvent>
#include <QClipboard>
#include <QMimeData>
#include <QSettings>
#include <QPainter>
#include <QBuffer>
#include <QImageReader>
#include <QToolTip>
#include <QSet>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QPainterPath>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QColorDialog>
#include <QGridLayout>
#include <QStackedWidget>
#include <QSlider>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ----------------------------------------------------------------------------
// ScreenColorPickerOverlay: 屏幕取色器 (复刻 Python 逻辑)
// ----------------------------------------------------------------------------
class ScreenColorPickerOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ScreenColorPickerOverlay(std::function<void(QString)> callback, QWidget* parent = nullptr)
        : QWidget(parent), m_callback(callback)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setCursor(Qt::CrossCursor);
        setMouseTracking(true);

        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) setGeometry(screen->geometry());

        m_infoWin = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        m_infoWin->setFixedSize(220, 380);
        m_infoWin->setAttribute(Qt::WA_TranslucentBackground);
        m_infoWin->setStyleSheet("QWidget { background: #1a1a1a; border-radius: 15px; border: 2px solid white; }");

        auto* l = new QVBoxLayout(m_infoWin);
        l->setContentsMargins(10, 10, 10, 10);

        m_magnifier = new QLabel();
        m_magnifier->setFixedSize(160, 160);
        m_magnifier->setStyleSheet("background: #2b2b2b; border: none; background-color: #2b2b2b;");
        m_magnifier->setAlignment(Qt::AlignCenter);
        l->addWidget(m_magnifier, 0, Qt::AlignCenter);

        m_preview = new QFrame();
        m_preview->setFixedSize(100, 40);
        m_preview->setStyleSheet("border-radius: 8px; background: white;");
        l->addWidget(m_preview, 0, Qt::AlignCenter);

        m_hexLabel = new QLabel("#FFFFFF");
        m_hexLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        m_hexLabel->setAlignment(Qt::AlignCenter);
        l->addWidget(m_hexLabel);

        m_rgbLabel = new QLabel("RGB: 255, 255, 255");
        m_rgbLabel->setStyleSheet("color: white; font-family: Consolas; font-size: 12px; border: none; background: transparent;");
        m_rgbLabel->setAlignment(Qt::AlignCenter);
        l->addWidget(m_rgbLabel);

        m_posLabel = new QLabel("X: 0  Y: 0");
        m_posLabel->setStyleSheet("color: #888; font-family: Consolas; font-size: 12px; border: none; background: transparent;");
        m_posLabel->setAlignment(Qt::AlignCenter);
        l->addWidget(m_posLabel);

        auto* hint = new QLabel("左键确认 | 右键/ESC取消");
        hint->setStyleSheet("color: #666; font-size: 11px; border: none; background: transparent;");
        hint->setAlignment(Qt::AlignCenter);
        l->addWidget(hint);

        if (screen) m_infoWin->move(screen->geometry().right() - 240, screen->geometry().bottom() - 420);
        m_infoWin->show();
    }

    ~ScreenColorPickerOverlay() {
        if (m_infoWin) {
            m_infoWin->hide();
            m_infoWin->deleteLater();
        }
    }

protected:
    void hideEvent(QHideEvent* event) override {
        if (m_infoWin) m_infoWin->hide();
        QWidget::hideEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        updatePreview(event->globalPosition().toPoint());
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_callback(m_selectedHex);
            m_infoWin->hide();
            close();
        } else if (event->button() == Qt::RightButton) {
            m_infoWin->hide();
            close();
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            m_infoWin->hide();
            close();
        }
    }

private:
    void updatePreview(QPoint pos) {
        QScreen *screen = QGuiApplication::screenAt(pos);
        if (!screen) return;

        int size = 21;
        QPixmap screenshot = screen->grabWindow(0, pos.x() - size/2, pos.y() - size/2, size, size);
        if (screenshot.isNull()) return;

        QImage img = screenshot.toImage();
        QColor centerColor = img.pixelColor(size/2, size/2);
        m_selectedHex = centerColor.name().toUpper();

        int magSize = 168;
        QPixmap magnified = screenshot.scaled(magSize, magSize, Qt::KeepAspectRatio, Qt::FastTransformation);

        QPainter p(&magnified);
        p.setPen(QPen(Qt::gray, 1));
        int gridSize = 8;
        for (int i = 0; i <= magSize; i += gridSize) {
            p.drawLine(i, 0, i, magSize);
            p.drawLine(0, i, magSize, i);
        }
        p.setPen(QPen(Qt::white, 2));
        p.drawRect(magSize/2 - gridSize, magSize/2 - gridSize, gridSize, gridSize);
        p.end();

        m_magnifier->setPixmap(magnified);
        m_preview->setStyleSheet(QString("border-radius: 8px; background: %1;").arg(m_selectedHex));
        m_hexLabel->setText(m_selectedHex);
        m_rgbLabel->setText(QString("RGB: %1, %2, %3").arg(centerColor.red()).arg(centerColor.green()).arg(centerColor.blue()));
        m_posLabel->setText(QString("X: %1  Y: %2").arg(pos.x()).arg(pos.y()));
    }

    std::function<void(QString)> m_callback;
    QWidget* m_infoWin = nullptr;
    QLabel* m_magnifier;
    QFrame* m_preview;
    QLabel* m_hexLabel;
    QLabel* m_rgbLabel;
    QLabel* m_posLabel;
    QString m_selectedHex;
};

// ----------------------------------------------------------------------------
// PixelRulerOverlay: 像素测量尺 (复刻 Python 逻辑)
// ----------------------------------------------------------------------------
class PixelRulerOverlay : public QWidget {
    Q_OBJECT
public:
    explicit PixelRulerOverlay(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setCursor(Qt::CrossCursor);

        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) setGeometry(screen->geometry());

        m_infoWin = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        m_infoWin->setAttribute(Qt::WA_TranslucentBackground);
        m_infoWin->setStyleSheet("background: #1a1a1a; border-radius: 20px;");
        m_infoWin->setFixedSize(300, 150);

        auto* l = new QVBoxLayout(m_infoWin);
        m_infoLabel = new QLabel("点击起点，拖动到终点测量距离\nESC 或 右键退出");
        m_infoLabel->setStyleSheet("color: #00ffff; font-size: 14px; font-weight: bold; border: none; background: transparent;");
        m_infoLabel->setAlignment(Qt::AlignCenter);
        m_infoLabel->setWordWrap(true);
        l->addWidget(m_infoLabel);

        if (screen) m_infoWin->move(screen->geometry().center().x() - 150, 30);
        m_infoWin->show();
    }

    ~PixelRulerOverlay() {
        if (m_infoWin) {
            m_infoWin->hide();
            m_infoWin->deleteLater();
        }
    }

protected:
    void hideEvent(QHideEvent* event) override {
        if (m_infoWin) m_infoWin->hide();
        QWidget::hideEvent(event);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 80));

        if (m_startPoint.isNull() || m_endPoint.isNull()) return;

        int x1 = m_startPoint.x(), y1 = m_startPoint.y();
        int x2 = m_endPoint.x(), y2 = m_endPoint.y();
        int dx = std::abs(x2 - x1);
        int dy = std::abs(y2 - y1);
        double distance = std::sqrt(std::pow((double)dx, 2) + std::pow((double)dy, 2));

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::cyan, 3));
        painter.drawLine(m_startPoint, m_endPoint);

        painter.setBrush(Qt::green);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(m_startPoint, 6, 6);
        painter.setBrush(Qt::red);
        painter.drawEllipse(m_endPoint, 6, 6);

        QPen dashPen(Qt::yellow, 1, Qt::DashLine);
        if (dx > 5) {
            painter.setPen(dashPen);
            painter.drawLine(x1, y1, x2, y1);
            painter.drawText(QRect((x1+x2)/2 - 40, y1 - 25, 80, 20), Qt::AlignCenter, QString("%1 px").arg(dx));
        }
        if (dy > 5) {
            painter.setPen(QPen(QColor(255, 165, 0), 1, Qt::DashLine));
            painter.drawLine(x2, y1, x2, y2);
            painter.drawText(QRect(x2 + 10, (y1+y2)/2 - 10, 80, 20), Qt::AlignLeft | Qt::AlignVCenter, QString("%1 px").arg(dy));
        }

        QPoint mid = (m_startPoint + m_endPoint) / 2;
        painter.setPen(QPen(Qt::cyan, 2));
        painter.setBrush(QColor(26, 26, 26));
        painter.drawRect(mid.x() - 70, mid.y() - 20, 140, 40);
        painter.setPen(Qt::cyan);
        QFont f = font();
        f.setBold(true);
        f.setPointSize(12);
        painter.setFont(f);
        painter.drawText(QRect(mid.x() - 70, mid.y() - 20, 140, 40), Qt::AlignCenter, QString("%1 px").arg(distance, 0, 'f', 1));
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_startPoint = event->pos();
            m_endPoint = m_startPoint;
            update();
        } else if (event->button() == Qt::RightButton) {
            m_infoWin->hide();
            close();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            m_endPoint = event->pos();
            int dx = std::abs(m_endPoint.x() - m_startPoint.x());
            int dy = std::abs(m_endPoint.y() - m_startPoint.y());
            double dist = std::sqrt(std::pow((double)dx, 2) + std::pow((double)dy, 2));
            m_infoLabel->setText(QString("起点: (%1, %2)\n终点: (%3, %4)\n\n水平: %5 px | 垂直: %6 px\n对角线: %7 px")
                .arg(m_startPoint.x()).arg(m_startPoint.y())
                .arg(m_endPoint.x()).arg(m_endPoint.y())
                .arg(dx).arg(dy).arg(dist, 0, 'f', 1));
            update();
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            m_infoWin->hide();
            close();
        }
    }

private:
    QPoint m_startPoint;
    QPoint m_endPoint;
    QWidget* m_infoWin;
    QLabel* m_infoLabel;
};

// ----------------------------------------------------------------------------
// ColorWheel: 基于 HSV 的圆形色轮
// ----------------------------------------------------------------------------
class ColorWheel : public QWidget {
    Q_OBJECT
public:
    explicit ColorWheel(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(400, 400);
        m_wheelImg = QImage(400, 400, QImage::Format_RGB32);
        m_wheelImg.fill(QColor("#1a1a1a"));

        int center = 200;
        int radius = 190;
        for (int y = 0; y < 400; ++y) {
            for (int x = 0; x < 400; ++x) {
                int dx = x - center;
                int dy = y - center;
                double dist = std::sqrt((double)dx*dx + (double)dy*dy);
                if (dist <= radius) {
                    double angle = std::atan2((double)dy, (double)dx);
                    double hue = (angle + M_PI) / (2 * M_PI);
                    double sat = dist / radius;
                    QColor c = QColor::fromHsvF(hue, sat, 1.0);
                    m_wheelImg.setPixelColor(x, y, c);
                }
            }
        }
    }

signals:
    void colorChanged(double hue, double sat);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawImage(0, 0, m_wheelImg);
    }

    void mousePressEvent(QMouseEvent* event) override { handleMouse(event); }
    void mouseMoveEvent(QMouseEvent* event) override { if (event->buttons() & Qt::LeftButton) handleMouse(event); }

private:
    void handleMouse(QMouseEvent* event) {
        int dx = event->pos().x() - 200;
        int dy = event->pos().y() - 200;
        double dist = std::sqrt((double)dx*dx + (double)dy*dy);
        if (dist <= 190) {
            double angle = std::atan2((double)dy, (double)dx);
            double hue = (angle + M_PI) / (2 * M_PI);
            double sat = std::min(dist / 190.0, 1.0);
            emit colorChanged(hue, sat);
        }
    }
    QImage m_wheelImg;
};

// ----------------------------------------------------------------------------
// ColorPickerDialog: 独立选色弹窗
// ----------------------------------------------------------------------------
class ColorPickerDialog : public QDialog {
    Q_OBJECT
public:
    ColorPickerDialog(QWidget* parent, std::function<void(QString)> callback)
        : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool), m_callback(callback)
    {
        setFixedSize(500, 550);
        setAttribute(Qt::WA_TranslucentBackground);

        auto* container = new QWidget(this);
        container->setObjectName("container");
        container->setStyleSheet("QWidget#container { background: #1a1a1a; border-radius: 15px; border: 1px solid #333; }");
        auto* mainL = new QVBoxLayout(this);
        mainL->setContentsMargins(0,0,0,0);
        mainL->addWidget(container);

        auto* l = new QVBoxLayout(container);
        l->setContentsMargins(20, 20, 20, 20);
        l->setSpacing(10);

        auto* title = new QLabel("颜色选择器");
        title->setStyleSheet("color: white; font-weight: bold; font-size: 16px; border: none; background: transparent;");
        l->addWidget(title);

        m_wheel = new ColorWheel();
        connect(m_wheel, &ColorWheel::colorChanged, this, &ColorPickerDialog::onWheelChanged);
        l->addWidget(m_wheel, 0, Qt::AlignCenter);

        auto* bRow = new QHBoxLayout();
        auto* blbl = new QLabel("亮度:");
        blbl->setStyleSheet("color: white; border: none; background: transparent;");
        bRow->addWidget(blbl);
        m_brightSlider = new QSlider(Qt::Horizontal);
        m_brightSlider->setRange(0, 100);
        m_brightSlider->setValue(100);
        connect(m_brightSlider, &QSlider::valueChanged, this, &ColorPickerDialog::updatePreview);
        bRow->addWidget(m_brightSlider);
        l->addLayout(bRow);

        m_preview = new QFrame();
        m_preview->setFixedHeight(60);
        m_preview->setStyleSheet("border-radius: 10px; background: white;");
        auto* pl = new QVBoxLayout(m_preview);
        m_hexLabel = new QLabel("#FFFFFF");
        m_hexLabel->setStyleSheet("color: black; font-weight: bold; font-size: 18px; border: none; background: transparent;");
        m_hexLabel->setAlignment(Qt::AlignCenter);
        pl->addWidget(m_hexLabel);
        l->addWidget(m_preview);

        auto* btnConfirm = new QPushButton("确认选择");
        btnConfirm->setFixedHeight(40);
        btnConfirm->setStyleSheet("QPushButton { background: #3b8ed0; color: white; border-radius: 6px; font-weight: bold; } QPushButton:hover { background: #1f6aa5; }");
        connect(btnConfirm, &QPushButton::clicked, this, &ColorPickerDialog::onConfirm);
        l->addWidget(btnConfirm);

        auto* btnClose = new QPushButton("取消");
        btnClose->setStyleSheet("color: #888; border: none; background: transparent;");
        connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
        l->addWidget(btnClose);
    }

private slots:
    void onWheelChanged(double h, double s) { m_hue = h; m_sat = s; updatePreview(); }
    void updatePreview() {
        QColor c = QColor::fromHsvF(m_hue, m_sat, m_brightSlider->value() / 100.0);
        m_selectedHex = c.name().toUpper();
        m_preview->setStyleSheet(QString("border-radius: 10px; background: %1;").arg(m_selectedHex));
        m_hexLabel->setText(m_selectedHex);
        m_hexLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 18px; border: none; background: transparent;")
            .arg(c.lightness() > 128 ? "black" : "white"));
    }
    void onConfirm() { m_callback(m_selectedHex); accept(); }

private:
    std::function<void(QString)> m_callback;
    ColorWheel* m_wheel;
    QSlider* m_brightSlider;
    QFrame* m_preview;
    QLabel* m_hexLabel;
    double m_hue = 0, m_sat = 0;
    QString m_selectedHex = "#FFFFFF";
};

// ----------------------------------------------------------------------------
// ColorPickerWindow 实现
// ----------------------------------------------------------------------------

ColorPickerWindow::ColorPickerWindow(QWidget* parent)
    : FramelessDialog("专业颜色管理器 Pro", parent)
{
    setFixedSize(1400, 900);
    setAcceptDrops(true);
    m_favorites = loadFavorites();
    initUI();
    useColor("#D64260");
}

ColorPickerWindow::~ColorPickerWindow() {
    saveFavorites();
}

void ColorPickerWindow::initUI() {
    setStyleSheet(R"(
        QWidget { font-family: "Microsoft YaHei", "Segoe UI", sans-serif; color: #E0E0E0; }
        QLineEdit { background-color: #2A2A2A; border: 1px solid #444; color: #FFFFFF; border-radius: 6px; padding: 4px; font-weight: bold; }
        QPushButton { background-color: #333; border: 1px solid #444; border-radius: 4px; padding: 6px; outline: none; }
        QPushButton:hover { background-color: #444; }
        QScrollArea { background: transparent; border: none; }
        QScrollBar:vertical { background: transparent; width: 8px; }
        QScrollBar::handle:vertical { background: #444; border-radius: 4px; }
    )");

    auto* mainHLayout = new QHBoxLayout(m_contentArea);
    mainHLayout->setContentsMargins(20, 20, 20, 20);
    mainHLayout->setSpacing(15);

    auto* leftContainer = new QWidget();
    leftContainer->setObjectName("leftPanelContainer");
    createLeftPanel(leftContainer);

    createRightPanel(m_contentArea);

    mainHLayout->addWidget(leftContainer);
    mainHLayout->addWidget(m_stack, 1);
}

void ColorPickerWindow::createLeftPanel(QWidget* parent) {
    auto* leftPanel = new QFrame(parent);
    leftPanel->setObjectName("leftPanel");
    leftPanel->setFixedWidth(400);
    leftPanel->setStyleSheet("QFrame#leftPanel { background: #252526; border-radius: 15px; }");

    auto* rootL = new QVBoxLayout(parent);
    rootL->setContentsMargins(0,0,0,0);
    rootL->addWidget(leftPanel);

    auto* l = new QVBoxLayout(leftPanel);
    l->setContentsMargins(15, 20, 15, 20);
    l->setSpacing(12);

    auto* topRow = new QHBoxLayout();
    auto* previewContainer = new QFrame();
    previewContainer->setFixedSize(130, 130);
    previewContainer->setStyleSheet("background: #1e1e1e; border-radius: 12px;");
    auto* pl = new QVBoxLayout(previewContainer);
    m_colorDisplay = new QWidget();
    m_colorDisplay->setObjectName("mainPreview");
    m_colorDisplay->setStyleSheet("border-radius: 10px; background: #D64260;");
    auto* cl = new QVBoxLayout(m_colorDisplay);
    m_colorLabel = new QLabel("#D64260");
    m_colorLabel->setCursor(Qt::PointingHandCursor);
    m_colorLabel->setAlignment(Qt::AlignCenter);
    m_colorLabel->setStyleSheet("font-family: Consolas; font-size: 20px; font-weight: bold; border: none; background: transparent;");
    m_colorLabel->installEventFilter(this);
    cl->addWidget(m_colorLabel);
    pl->addWidget(m_colorDisplay);
    topRow->addWidget(previewContainer);

    auto* inputCol = new QFrame();
    inputCol->setStyleSheet("background: #333; border-radius: 12px;");
    auto* icl = new QVBoxLayout(inputCol);
    icl->setContentsMargins(10, 10, 10, 10);

    auto createInputRow = [&](const QString& label, QLineEdit*& entry, std::function<void()> onReturn, std::function<void()> onCopy) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label);
        lbl->setFixedWidth(35);
        lbl->setStyleSheet("font-weight: bold; font-size: 12px; border: none; background: transparent;");
        row->addWidget(lbl);
        entry = new QLineEdit();
        entry->setFixedHeight(32);
        connect(entry, &QLineEdit::returnPressed, this, onReturn);
        row->addWidget(entry);
        auto* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon("copy", "#FFFFFF"));
        btn->setFixedSize(32, 32);
        connect(btn, &QPushButton::clicked, this, onCopy);
        row->addWidget(btn);
        icl->addLayout(row);
    };

    createInputRow("HEX", m_hexEntry, [this](){ applyHexColor(); }, [this](){ copyHexValue(); });

    auto* rgbRow = new QHBoxLayout();
    auto* rlbl = new QLabel("RGB");
    rlbl->setFixedWidth(35); rlbl->setStyleSheet("font-weight: bold; font-size: 12px; border: none; background: transparent;");
    rgbRow->addWidget(rlbl);
    m_rEntry = new QLineEdit(); m_rEntry->setFixedWidth(42);
    m_gEntry = new QLineEdit(); m_gEntry->setFixedWidth(42);
    m_bEntry = new QLineEdit(); m_bEntry->setFixedWidth(42);
    rgbRow->addWidget(m_rEntry);
    rgbRow->addWidget(m_gEntry);
    rgbRow->addWidget(m_bEntry);
    auto* btnCopyRgb = new QPushButton();
    btnCopyRgb->setIcon(IconHelper::getIcon("copy", "#FFFFFF"));
    btnCopyRgb->setFixedSize(32, 32);
    connect(btnCopyRgb, &QPushButton::clicked, this, &ColorPickerWindow::copyRgbValue);
    rgbRow->addWidget(btnCopyRgb);
    icl->addLayout(rgbRow);
    topRow->addWidget(inputCol);
    l->addLayout(topRow);

    auto* toolsFrame = new QHBoxLayout();
    auto createToolBtn = [&](const QString& iconName, std::function<void()> cmd, QString color, QString tip) {
        auto* btn = new QPushButton();
        btn->setIcon(IconHelper::getIcon(iconName, "#FFFFFF"));
        btn->setIconSize(QSize(28, 28));
        btn->setFixedSize(52, 52);
        btn->setStyleSheet(QString("QPushButton { background: %1; border: none; border-radius: 4px; } QPushButton:hover { opacity: 0.8; }").arg(color));
        btn->setToolTip(tip);
        connect(btn, &QPushButton::clicked, cmd);
        toolsFrame->addWidget(btn);
    };
    createToolBtn("palette", [this](){ openColorPicker(); }, "#3b8ed0", "打开专业色盘");
    createToolBtn("screen_picker", [this](){ startScreenPicker(); }, "#3b8ed0", "吸取屏幕颜色");
    createToolBtn("pixel_ruler", [this](){ openPixelRuler(); }, "#9b59b6", "屏幕像素测量");
    createToolBtn("image", [this](){ extractFromImage(); }, "#2ecc71", "从图片提取颜色");
    createToolBtn("star", [this](){ addToFavorites(); }, "#f39c12", "收藏当前颜色");
    l->addLayout(toolsFrame);

    auto* gradBox = new QFrame();
    gradBox->setStyleSheet("background: #333; border-radius: 12px;");
    auto* gl = new QVBoxLayout(gradBox);
    auto* gt = new QLabel("渐变生成器");
    gt->setStyleSheet("font-weight: bold; font-size: 15px; border: none; background: transparent;");
    gt->setAlignment(Qt::AlignCenter);
    gl->addWidget(gt);
    auto addGradInput = [&](const QString& label, QLineEdit*& entry) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label);
        lbl->setFixedWidth(55);
        lbl->setStyleSheet("font-size: 12px; border: none; background: transparent;");
        row->addWidget(lbl);
        entry = new QLineEdit();
        entry->setFixedHeight(32);
        row->addWidget(entry);
        gl->addLayout(row);
    };
    addGradInput("起始色", m_gradStart);
    addGradInput("结束色", m_gradEnd);
    auto* stepsRow = new QHBoxLayout();
    stepsRow->addStretch();
    auto* stepslbl = new QLabel("步数");
    stepslbl->setStyleSheet("color: white; border: none; background: transparent;");
    stepsRow->addWidget(stepslbl);
    m_gradSteps = new QLineEdit("7");
    m_gradSteps->setFixedWidth(40);
    m_gradSteps->setAlignment(Qt::AlignCenter);
    stepsRow->addWidget(m_gradSteps);
    stepsRow->addSpacing(20);
    auto* btnMode = new QPushButton("变暗");
    connect(btnMode, &QPushButton::clicked, [this, btnMode](){
        if (m_gradMode == "变暗") m_gradMode = "变亮";
        else if (m_gradMode == "变亮") m_gradMode = "饱和";
        else m_gradMode = "变暗";
        btnMode->setText(m_gradMode);
    });
    stepsRow->addWidget(btnMode);
    stepsRow->addStretch();
    gl->addLayout(stepsRow);
    auto* btnGrad = new QPushButton("生成渐变");
    btnGrad->setFixedHeight(36);
    btnGrad->setStyleSheet("background: #007ACC; font-weight: bold;");
    connect(btnGrad, &QPushButton::clicked, this, &ColorPickerWindow::generateGradient);
    gl->addWidget(btnGrad);
    l->addWidget(gradBox);

    m_imagePreviewFrame = new QFrame();
    m_imagePreviewFrame->setStyleSheet("background: #1e1e1e; border: 2px solid #444; border-radius: 12px;");
    m_imagePreviewFrame->setFixedSize(370, 220);
    auto* ipl = new QVBoxLayout(m_imagePreviewFrame);
    m_imagePreviewLabel = new QLabel("暂无图片");
    m_imagePreviewLabel->setAlignment(Qt::AlignCenter);
    m_imagePreviewLabel->setStyleSheet("color: #666; border: none; background: transparent;");
    ipl->addWidget(m_imagePreviewLabel);
    auto* btnClearImg = new QPushButton("清除图片 / 重置");
    btnClearImg->setStyleSheet("color: #888; border: 1px solid #444; background: transparent; font-size: 11px;");
    connect(btnClearImg, &QPushButton::clicked, [this](){
        m_imagePreviewFrame->hide();
        m_imagePreviewLabel->setPixmap(QPixmap());
        m_imagePreviewLabel->setText("暂无图片");
        qDeleteAll(m_extractGridContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
        m_dropHintContainer->show();
        showNotification("已重置图片提取");
    });
    ipl->addWidget(btnClearImg);
    l->addWidget(m_imagePreviewFrame);
    m_imagePreviewFrame->hide();
    l->addStretch();

    auto* navBar = new QHBoxLayout();
    auto createNavBtn = [&](const QString& text) {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(40);
        btn->setStyleSheet("QPushButton { background: #333; border-radius: 6px; font-weight: bold; } QPushButton:hover { background: #444; }");
        connect(btn, &QPushButton::clicked, [this, text](){ switchView(text); });
        return btn;
    };
    navBar->addWidget(createNavBtn("我的收藏"));
    navBar->addWidget(createNavBtn("渐变预览"));
    navBar->addWidget(createNavBtn("图片提取"));
    l->addLayout(navBar);
}

void ColorPickerWindow::createRightPanel(QWidget* parent) {
    m_stack = new QStackedWidget();
    auto createScroll = [&](QWidget*& content) {
        auto* scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        content = new QWidget();
        scroll->setWidget(content);
        return scroll;
    };
    m_favScroll = createScroll(m_favContent);
    m_gradScroll = createScroll(m_gradContent);
    m_extractScroll = createScroll(m_extractContent);

    // 收藏容器
    auto* fl = new QVBoxLayout(m_favContent);
    m_favGridContainer = new QWidget();
    fl->addWidget(m_favGridContainer);
    fl->addStretch();

    // 渐变容器
    auto* gl = new QVBoxLayout(m_gradContent);
    m_gradGridContainer = new QWidget();
    gl->addWidget(m_gradGridContainer);
    gl->addStretch();

    // 提取容器
    auto* el = new QVBoxLayout(m_extractContent);
    el->setContentsMargins(10, 10, 10, 10);

    m_dropHintContainer = new QFrame();
    m_dropHintContainer->setStyleSheet("background: transparent; border: 3px dashed #555; border-radius: 15px;");
    m_dropHintContainer->setFixedHeight(250);
    auto* hl = new QVBoxLayout(m_dropHintContainer);

    auto* iconHint = new QLabel();
    iconHint->setPixmap(IconHelper::getIcon("image", "#444444").pixmap(48, 48));
    iconHint->setAlignment(Qt::AlignCenter);
    iconHint->setStyleSheet("border: none; background: transparent;");
    hl->addWidget(iconHint);

    auto* hint = new QLabel("拖放图片到软件任意位置\n\n或\n\nCtrl+V 粘贴\n点击左侧相机图标");
    hint->setStyleSheet("color: #666; font-size: 18px; border: none; background: transparent;");
    hint->setAlignment(Qt::AlignCenter);
    hl->addWidget(hint);
    el->addWidget(m_dropHintContainer);

    m_extractGridContainer = new QWidget();
    el->addWidget(m_extractGridContainer);
    el->addStretch();

    m_stack->addWidget(m_favScroll);
    m_stack->addWidget(m_gradScroll);
    m_stack->addWidget(m_extractScroll);
}

void ColorPickerWindow::switchView(const QString& value) {
    if (value == "我的收藏") { m_stack->setCurrentWidget(m_favScroll); updateFavoritesDisplay(); }
    else if (value == "渐变预览") { m_stack->setCurrentWidget(m_gradScroll); }
    else if (value == "图片提取") { m_stack->setCurrentWidget(m_extractScroll); }
}

void ColorPickerWindow::updateColorDisplay() {
    m_colorDisplay->setStyleSheet(QString("border-radius: 10px; background: %1;").arg(m_currentColor));
    m_colorLabel->setText(m_currentColor);
    QColor c = hexToColor(m_currentColor);
    m_colorLabel->setStyleSheet(QString("font-family: Consolas; font-size: 20px; font-weight: bold; border: none; background: transparent; color: %1;")
        .arg(c.lightness() > 128 ? "black" : "white"));
    m_hexEntry->setText(m_currentColor);
    m_rEntry->setText(QString::number(c.red()));
    m_gEntry->setText(QString::number(c.green()));
    m_bEntry->setText(QString::number(c.blue()));
}

void ColorPickerWindow::useColor(const QString& hex) {
    m_currentColor = hex.toUpper();
    updateColorDisplay();
}

void ColorPickerWindow::applyHexColor() {
    QString h = m_hexEntry->text().trimmed();
    if (!h.startsWith("#")) h = "#" + h;
    QColor c(h);
    if (c.isValid()) useColor(c.name().toUpper());
    else showNotification("无效的 HEX 颜色代码", true);
}

void ColorPickerWindow::applyRgbColor() {
    int r = m_rEntry->text().toInt();
    int g = m_gEntry->text().toInt();
    int b = m_bEntry->text().toInt();
    QColor c(r, g, b);
    if (c.isValid()) useColor(c.name().toUpper());
    else showNotification("RGB 值必须在 0-255 之间", true);
}

void ColorPickerWindow::copyHexValue() {
    QApplication::clipboard()->setText(m_currentColor);
    showNotification("已复制 " + m_currentColor);
}

void ColorPickerWindow::copyRgbValue() {
    QColor c = hexToColor(m_currentColor);
    QString rgb = QString("rgb(%1, %2, %3)").arg(c.red()).arg(c.green()).arg(c.blue());
    QApplication::clipboard()->setText(rgb);
    showNotification("已复制 " + rgb);
}

void ColorPickerWindow::startScreenPicker() {
    auto* picker = new ScreenColorPickerOverlay([this](QString hex){ useColor(hex); }, this);
    picker->setAttribute(Qt::WA_DeleteOnClose);
    picker->show();
}

void ColorPickerWindow::openPixelRuler() {
    auto* ruler = new PixelRulerOverlay(this);
    ruler->setAttribute(Qt::WA_DeleteOnClose);
    ruler->show();
}

void ColorPickerWindow::openColorPicker() {
    auto* dlg = new ColorPickerDialog(this, [this](QString hex){ useColor(hex); });
    dlg->show();
}

void ColorPickerWindow::addToFavorites() {
    addSpecificColorToFavorites(m_currentColor);
}

void ColorPickerWindow::addSpecificColorToFavorites(const QString& color) {
    if (!m_favorites.contains(color)) {
        m_favorites.prepend(color);
        saveFavorites();
        updateFavoritesDisplay();
        QApplication::clipboard()->setText(color);
        showNotification("已收藏并复制 " + color);
    } else {
        showNotification(color + " 已在收藏中", true);
    }
}

void ColorPickerWindow::removeFavorite(const QString& color) {
    m_favorites.removeAll(color);
    saveFavorites();
    updateFavoritesDisplay();
}

void ColorPickerWindow::updateFavoritesDisplay() {
    if (m_favGridContainer->layout()) delete m_favGridContainer->layout();
    qDeleteAll(m_favGridContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));

    auto* layout = new QVBoxLayout(m_favGridContainer);
    layout->setContentsMargins(0, 0, 0, 0);

    if (m_favorites.isEmpty()) {
        auto* lbl = new QLabel("暂无收藏\n右键点击任何颜色块即可收藏");
        lbl->setStyleSheet("color: #666; font-size: 16px; border: none; background: transparent;");
        lbl->setAlignment(Qt::AlignCenter);
        layout->addWidget(lbl, 0, Qt::AlignCenter);
        return;
    }

    auto* grid = new QGridLayout();
    grid->setSpacing(15);
    for (int i = 0; i < 4; ++i) grid->setColumnStretch(i, 1);
    for (int i = 0; i < m_favorites.size(); ++i) {
        QWidget* tile = createFavoriteTile(m_favGridContainer, m_favorites[i]);
        grid->addWidget(tile, i / 4, i % 4);
    }
    layout->addLayout(grid);
}

QWidget* ColorPickerWindow::createFavoriteTile(QWidget* parent, const QString& color) {
    auto* tile = new QFrame(parent);
    tile->setStyleSheet("background: #333; border-radius: 10px;");
    auto* l = new QVBoxLayout(tile);
    l->setContentsMargins(6, 6, 6, 6);
    auto* btn = new QPushButton("");
    btn->setFixedSize(100, 30);
    btn->setStyleSheet(QString("background: %1; border-radius: 6px; border: none;").arg(color));
    btn->setCursor(Qt::PointingHandCursor);
    connect(btn, &QPushButton::clicked, [this, color](){ useColor(color); copyHexValue(); });
    l->addWidget(btn, 0, Qt::AlignCenter);
    auto* info = new QHBoxLayout();
    auto* lbl = new QLabel(color);
    lbl->setStyleSheet("font-weight: bold; font-size: 12px; border: none; background: transparent;");
    info->addWidget(lbl);
    auto* del = new QPushButton();
    del->setIcon(IconHelper::getIcon("close", "#888888"));
    del->setFixedSize(25, 25);
    del->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background: #c0392b; }");
    connect(del, &QPushButton::clicked, [this, color](){ removeFavorite(color); });
    info->addWidget(del);
    l->addLayout(info);
    return tile;
}

void ColorPickerWindow::generateGradient() {
    QString startHex = m_gradStart->text().trimmed();
    if (!startHex.startsWith("#")) startHex = "#" + startHex;
    QColor start = QColor(startHex);
    if (!start.isValid()) { showNotification("起始色无效", true); return; }
    QString endHex = m_gradEnd->text().trimmed();
    int steps = m_gradSteps->text().toInt();
    if (steps < 2) steps = 2;
    QStringList colors;
    if (endHex.isEmpty()) {
        float h, s, v;
        start.getHsvF(&h, &s, &v);
        for (int i = 0; i < steps; ++i) {
            double ratio = (double)i / (steps - 1);
            QColor c;
            if (m_gradMode == "变暗") c = QColor::fromHsvF(h, s, v * (1 - ratio * 0.7));
            else if (m_gradMode == "变亮") c = QColor::fromHsvF(h, s, v + (1 - v) * ratio);
            else if (m_gradMode == "饱和") c = QColor::fromHsvF(h, std::min(1.0f, std::max(0.0f, s + (1 - s) * (float)ratio * (s < 0.5f ? 1.0f : -1.0f))), v);
            colors << c.name().toUpper();
        }
    } else {
        if (!endHex.startsWith("#")) endHex = "#" + endHex;
        QColor end = QColor(endHex);
        if (!end.isValid()) { showNotification("结束色无效", true); return; }
        for (int i = 0; i < steps; ++i) {
            double r = (double)i / (steps - 1);
            int red = start.red() + (end.red() - start.red()) * r;
            int green = start.green() + (end.green() - start.green()) * r;
            int blue = start.blue() + (end.blue() - start.blue()) * r;
            colors << QColor(red, green, blue).name().toUpper();
        }
    }

    if (m_gradGridContainer->layout()) delete m_gradGridContainer->layout();
    qDeleteAll(m_gradGridContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));

    auto* layout = new QVBoxLayout(m_gradGridContainer);
    auto* title = new QLabel("生成结果 (左键应用 / 右键收藏)");
    title->setStyleSheet("font-weight: bold; font-size: 16px; border: none; background: transparent;");
    layout->addWidget(title, 0, Qt::AlignCenter);
    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    for (int i = 0; i < 5; ++i) grid->setColumnStretch(i, 1);
    for (int i = 0; i < colors.size(); ++i) {
        QWidget* tile = createColorTile(m_gradGridContainer, colors[i]);
        grid->addWidget(tile, i / 5, i % 5);
    }
    layout->addLayout(grid);
    switchView("渐变预览");
}

QWidget* ColorPickerWindow::createColorTile(QWidget* parent, const QString& color) {
    auto* tile = new QFrame(parent);
    tile->setStyleSheet("background: #333; border-radius: 12px;");
    auto* l = new QVBoxLayout(tile);
    l->setContentsMargins(6, 6, 6, 6);
    auto* cf = new QFrame();
    cf->setFixedSize(100, 30);
    cf->setStyleSheet(QString("border-radius: 6px; background: %1;").arg(color));
    auto* cfl = new QVBoxLayout(cf);
    cfl->setContentsMargins(0, 0, 0, 0);
    auto* clbl = new QLabel(color);
    clbl->setAlignment(Qt::AlignCenter);
    clbl->setCursor(Qt::PointingHandCursor);
    QColor c(color);
    clbl->setStyleSheet(QString("font-weight: bold; font-size: 13px; border: none; background: transparent; color: %1;")
        .arg(c.lightness() > 128 ? "black" : "white"));
    cfl->addWidget(clbl);
    l->addWidget(cf, 0, Qt::AlignCenter);
    cf->installEventFilter(this);
    clbl->installEventFilter(this);
    cf->setProperty("color", color);
    clbl->setProperty("color", color);
    return tile;
}

void ColorPickerWindow::extractFromImage() {
    QString path = QFileDialog::getOpenFileName(this, "选择图片", "", "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (!path.isEmpty()) processImage(path);
}

void ColorPickerWindow::processImage(const QString& filePath, const QImage& image) {
    QImage img = image;
    if (img.isNull() && !filePath.isEmpty()) {
        img.load(filePath);
    }
    if (img.isNull()) return;

    m_imagePreviewFrame->show();
    m_imagePreviewLabel->setPixmap(QPixmap::fromImage(img).scaled(360, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imagePreviewLabel->setText("");

    m_dropHintContainer->hide();
    m_extractGridContainer->show();
    if (m_extractGridContainer->layout()) delete m_extractGridContainer->layout();
    qDeleteAll(m_extractGridContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));

    auto* layout = new QVBoxLayout(m_extractGridContainer);
    auto* title = new QLabel("提取的调色板 (点击应用)");
    title->setStyleSheet("font-weight: bold; font-size: 18px; border: none; background: transparent;");
    layout->addWidget(title, 0, Qt::AlignCenter);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    for (int i = 0; i < 5; ++i) grid->setColumnStretch(i, 1);

    QStringList colors = extractDominantColors(img, 20);
    for (int i = 0; i < colors.size(); ++i) {
        QWidget* tile = createColorTile(m_extractGridContainer, colors[i]);
        grid->addWidget(tile, i / 5, i % 5);
    }
    layout->addLayout(grid);

    switchView("图片提取");
    showNotification("图片已加载，调色板生成完毕");
}

void ColorPickerWindow::pasteImage() {
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (mime && mime->hasImage()) {
        QImage img = qvariant_cast<QImage>(mime->imageData());
        processImage("", img);
    } else {
        showNotification("剪贴板中没有图片", true);
    }
}

QStringList ColorPickerWindow::extractDominantColors(const QImage& img, int num) {
    QImage small = img.scaled(150, 150, Qt::IgnoreAspectRatio, Qt::FastTransformation).convertToFormat(QImage::Format_RGB32);
    QMap<QRgb, int> counts;
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) { counts[small.pixel(x, y)]++; }
    }
    QList<QRgb> sorted = counts.keys();
    std::sort(sorted.begin(), sorted.end(), [&](QRgb a, QRgb b){ return counts[a] > counts[b]; });
    QStringList result;
    for (QRgb rgb : sorted) {
        QString hex = QColor(rgb).name().toUpper();
        if (!result.contains(hex)) { result << hex; if (result.size() >= num) break; }
    }
    return result;
}

void ColorPickerWindow::showNotification(const QString& message, bool isError) {
    if (m_notification) {
        m_notification->hide();
        m_notification->deleteLater();
    }

    m_notification = new QFrame(this);
    m_notification->setObjectName("notification");
    m_notification->setStyleSheet(QString("QFrame#notification { background: %1; border-radius: 20px; }")
        .arg(isError ? "#e74c3c" : "#2ecc71"));

    auto* l = new QHBoxLayout(m_notification);
    l->setContentsMargins(15, 8, 15, 8);
    l->setSpacing(10);

    auto* icon = new QLabel();
    icon->setPixmap(IconHelper::getIcon(isError ? "close" : "select", "#FFFFFF").pixmap(20, 20));
    icon->setStyleSheet("border: none; background: transparent;");
    l->addWidget(icon);

    auto* lbl = new QLabel(message);
    lbl->setStyleSheet("color: white; font-weight: bold; font-size: 14px; border: none; background: transparent;");
    l->addWidget(lbl);

    m_notification->adjustSize();
    m_notification->move(width()/2 - m_notification->width()/2, height() - 100);
    m_notification->show();
    m_notification->raise();

    QTimer::singleShot(2000, m_notification, &QWidget::hide);
}

void ColorPickerWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) event->acceptProposedAction();
}

void ColorPickerWindow::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasImage()) {
        QImage img = qvariant_cast<QImage>(event->mimeData()->imageData());
        if (!img.isNull()) processImage("", img);
    } else if (event->mimeData()->hasUrls()) {
        processImage(event->mimeData()->urls().first().toLocalFile());
    }
}

void ColorPickerWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier)) pasteImage();
    else FramelessDialog::keyPressEvent(event);
}

void ColorPickerWindow::hideEvent(QHideEvent* event) {
    // 关闭主界面时，确保所有活动的 Overlay 窗口也关闭
    QList<QWidget*> overlays = findChildren<QWidget*>();
    for (auto* w : overlays) {
        if (qobject_cast<ScreenColorPickerOverlay*>(w) || qobject_cast<PixelRulerOverlay*>(w)) {
            w->close();
        }
    }
    FramelessDialog::hideEvent(event);
}

bool ColorPickerWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        QString color = watched->property("color").toString();
        if (!color.isEmpty()) {
            if (me->button() == Qt::LeftButton) {
                useColor(color);
                QApplication::clipboard()->setText(color);
                showNotification("已应用并复制 " + color);
            } else if (me->button() == Qt::RightButton) {
                addSpecificColorToFavorites(color);
            }
            return true;
        } else if (watched == m_colorLabel) {
            if (me->button() == Qt::LeftButton) copyHexValue();
            else if (me->button() == Qt::RightButton) addToFavorites();
            return true;
        }
    }
    return FramelessDialog::eventFilter(watched, event);
}

QString ColorPickerWindow::rgbToHex(int r, int g, int b) { return QColor(r, g, b).name().toUpper(); }
QColor ColorPickerWindow::hexToColor(const QString& hex) { return QColor(hex); }
QString ColorPickerWindow::colorToHex(const QColor& c) { return c.name().toUpper(); }

QStringList ColorPickerWindow::loadFavorites() {
    QSettings s("RapidNotes", "ColorPicker");
    return s.value("favorites").toStringList();
}

void ColorPickerWindow::saveFavorites() {
    QSettings s("RapidNotes", "ColorPicker");
    s.setValue("favorites", m_favorites);
}

#include "ColorPickerWindow.moc"
