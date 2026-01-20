#ifndef METADATAPANEL_H
#define METADATAPANEL_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>

class MetadataPanel : public QWidget {
    Q_OBJECT
public:
    explicit MetadataPanel(QWidget* parent = nullptr);
    void setNote(const QVariantMap& note);

signals:
    void noteUpdated();

private:
    void updateCapsule(const QString& key, const QString& value);
    QWidget* createCapsule(const QString& label, const QString& key);

    int m_currentNoteId = -1;
    QLineEdit* m_titleEdit;
    QLineEdit* m_tagEdit;

    QMap<QString, QLabel*> m_capsules;
};

#endif // METADATAPANEL_H
