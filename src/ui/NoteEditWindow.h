#ifndef NOTEEDITWINDOW_H
#define NOTEEDITWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QVBoxLayout> // 【修复点】必须包含这个，否则编译器不认识 QVBoxLayout
#include "Editor.h" 

class NoteEditWindow : public QWidget {
    Q_OBJECT
public:
    // mode: 0=新建, >0=编辑(传入笔记ID)
    explicit NoteEditWindow(int noteId = 0, QWidget* parent = nullptr);

signals:
    void noteSaved(); // 保存成功后通知主界面刷新

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void initUI();
    void loadNoteData(int id);
    // 这里使用了 QVBoxLayout 指针，所以上面必须 include 它
    void setupLeftPanel(QVBoxLayout* layout);
    void setupRightPanel(QVBoxLayout* layout);
    QPushButton* createColorBtn(const QString& color, int id);

    int m_noteId;
    
    // UI 控件引用
    QComboBox* m_categoryCombo;
    QLineEdit* m_titleEdit;
    QLineEdit* m_tagEdit;
    QButtonGroup* m_colorGroup;
    QCheckBox* m_defaultColorCheck;
    Editor* m_contentEdit;
};

#endif // NOTEEDITWINDOW_H