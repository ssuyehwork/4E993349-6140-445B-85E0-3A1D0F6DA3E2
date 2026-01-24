#ifndef FRAMELESSDIALOG_H
#define FRAMELESSDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QFrame>

/**
 * @brief 无边框对话框基类，自带标题栏、关闭按钮、阴影、置顶
 */
class FramelessDialog : public QDialog {
    Q_OBJECT
public:
    explicit FramelessDialog(const QString& title, QWidget* parent = nullptr);
    virtual ~FramelessDialog() = default;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

    QWidget* m_contentArea;
    QVBoxLayout* m_mainLayout;
    QLabel* m_titleLabel;

private:
    QPoint m_dragPos;
};

/**
 * @brief 无边框文本输入对话框
 */
class FramelessInputDialog : public FramelessDialog {
    Q_OBJECT
public:
    explicit FramelessInputDialog(const QString& title, const QString& label, 
                                  const QString& initial = "", QWidget* parent = nullptr);
    QString text() const { return m_edit->text().trimmed(); }

private:
    QLineEdit* m_edit;
};

/**
 * @brief 无边框确认提示框
 */
class FramelessMessageBox : public FramelessDialog {
    Q_OBJECT
public:
    explicit FramelessMessageBox(const QString& title, const QString& text, QWidget* parent = nullptr);

signals:
    void confirmed();
    void cancelled();
};

#endif // FRAMELESSDIALOG_H
