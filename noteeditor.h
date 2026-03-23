#pragma once
#include "Note.h"
#include "multipagenoteview.h"
#include <QWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <functional>

class NoteEditor : public QWidget {
    Q_OBJECT
public:
    explicit NoteEditor(QWidget *parent = nullptr);

    void setNote(Note *note);
    Note *note() const { return note_; }

    // Zugriff auf die View für MainWindow
    MultiPageNoteView* view() const { return canvas_; }

    std::function<void(Note *)> onSaveRequested;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Note *note_{nullptr};

    // Nur noch der Canvas, keine eigene Toolbar
    MultiPageNoteView *canvas_{nullptr};
    QPushButton *m_menuBtn{nullptr};

    void setupUi();
    void setupShortcuts();
};
