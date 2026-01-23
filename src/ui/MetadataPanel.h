#ifndef METADATAPANEL_H
#define METADATAPANEL_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QFrame>

class ClickableLineEdit : public QLineEdit {
    Q_OBJECT
public:
    using QLineEdit::QLineEdit;
signals:
    void doubleClicked();
protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) emit doubleClicked();
        QLineEdit::mouseDoubleClickEvent(event);
    }
};
#include <QMap>

class MetadataPanel : public QWidget {
    Q_OBJECT
public:
    explicit MetadataPanel(QWidget* parent = nullptr);
    void setNote(const QVariantMap& note);
    void setMultipleNotes(int count);
    void clearSelection();

signals:
    void noteUpdated();
    void tagAdded(const QStringList& tags);

private:
    void initUI();
    QWidget* createInfoWidget(const QString& icon, const QString& title, const QString& subtitle);
    QWidget* createMetadataDisplay();
    QWidget* createCapsule(const QString& label, const QString& key);
    void openExpandedTitleEditor();
    void handleTagInput();
    void openTagSelector();

    QStackedWidget* m_stack;
    QWidget* m_metadataDisplayWidget;
    
    // Metadata Display widgets
    ClickableLineEdit* m_titleEdit;
    ClickableLineEdit* m_tagEdit;
    QFrame* m_separatorLine;
    QMap<QString, QLabel*> m_capsules;

    int m_currentNoteId = -1;
};

#endif // METADATAPANEL_H
