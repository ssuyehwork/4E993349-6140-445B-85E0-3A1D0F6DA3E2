#ifndef NOTEEDITWINDOW_H
#define NOTEEDITWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>
#include "Editor.h" 

class NoteEditWindow : public QWidget {
    Q_OBJECT
public:
    explicit NoteEditWindow(int noteId = 0, QWidget* parent = nullptr);
    void setDefaultCategory(int catId);

signals:
    void noteSaved();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void initUI();
    void setupShortcuts();
    void loadNoteData(int id);
    void setupLeftPanel(QVBoxLayout* layout);
    void setupRightPanel(QVBoxLayout* layout);
    QPushButton* createColorBtn(const QString& color, int id);

    void toggleMaximize();
    void saveNote();
    void toggleSearchBar();

    int m_noteId;
    int m_catId = -1;

    // 窗口控制
    bool m_isMaximized = false;
    QRect m_normalGeometry;
    QPoint m_dragPos;

    // UI 控件引用
    QWidget* m_titleBar;
    QLabel* m_winTitleLabel;
    QPushButton* m_maxBtn;
    QSplitter* m_splitter;
    QLineEdit* m_titleEdit;
    QLineEdit* m_tagEdit;
    QButtonGroup* m_colorGroup;
    QCheckBox* m_defaultColorCheck;
    Editor* m_contentEdit;

    // 搜索栏
    QWidget* m_searchBar;
    QLineEdit* m_searchEdit;
};

#endif // NOTEEDITWINDOW_H