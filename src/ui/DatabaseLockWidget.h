#ifndef DATABASELOCKWIDGET_H
#define DATABASELOCKWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>

class DatabaseLockWidget : public QWidget {
    Q_OBJECT
public:
    explicit DatabaseLockWidget(QWidget* parent = nullptr);
    void showError(const QString& msg);

signals:
    void unlocked(const QString& password);
    void quitRequested();

private slots:
    void onVerify();

private:
    QLineEdit* m_pwdEdit;
    QLabel* m_errorLabel;
};

#endif // DATABASELOCKWIDGET_H
