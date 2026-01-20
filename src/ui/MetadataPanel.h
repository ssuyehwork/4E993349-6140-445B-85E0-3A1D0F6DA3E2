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
    int m_currentNoteId = -1;
    QLineEdit* m_titleEdit;
    QLineEdit* m_tagEdit;
    QLabel* m_createdLabel;
    QLabel* m_updatedLabel;
    QLabel* m_ratingLabel;
};

#endif // METADATAPANEL_H
