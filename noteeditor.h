#pragma once
#include "Note.h"
#include "moderntoolbar.h"
#include "multipagenoteview.h"
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QWidget>
#include <functional>

class NoteEditor : public QWidget {
    Q_OBJECT
public:
    explicit NoteEditor(QWidget *parent = nullptr);

    void setNote(Note *note);
    Note *note() const { return note_; }

    std::function<void(Note *)> onSaveRequested;

protected:
    bool eventFilter(QObject *, QEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    Note *note_{nullptr}; // <--- Hier hinzugefügt
    QTabWidget *tabs_{nullptr};
    QTextEdit *text_{nullptr};
    MultiPageNoteView *canvas_{nullptr};
    ModernToolbar *toolbar_{nullptr};
    QTimer textSaveDebounce_;
    void setupUi();
    void setupShortcuts();
};
