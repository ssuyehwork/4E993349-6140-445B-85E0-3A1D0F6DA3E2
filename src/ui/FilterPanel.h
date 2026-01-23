#ifndef FILTERPANEL_H
#define FILTERPANEL_H

#include <QWidget>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QScrollArea>
#include <QLabel>
#include "FlowLayout.h"

class FilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit FilterPanel(QWidget* parent = nullptr);
    void updateStats(const QString& keyword, const QString& type, const QVariant& value);
    QVariantMap getCheckedCriteria() const;

signals:
    void criteriaChanged();

protected:
    void leaveEvent(QEvent* event) override { hide(); }

private:
    void createSection(const QString& title, const QString& key, const QVariantMap& data);

    QVBoxLayout* m_mainLayout;
    QMap<QString, QList<QCheckBox*>> m_checkboxes;
};

#endif // FILTERPANEL_H
