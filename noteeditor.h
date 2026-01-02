#pragma once
#include "Note.h"
#include "multipagenoteview.h"
#include <QWidget>
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

private:
    Note *note_{nullptr};

    // Nur noch der Canvas, keine eigene Toolbar
    MultiPageNoteView *canvas_{nullptr};

    void setupUi();
    void setupShortcuts();
};
