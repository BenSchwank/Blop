#pragma once
#include "Note.h"
#include "multipagenoteview.h"
#include <QWidget>
#include <QResizeEvent>
#include <functional>

class NoteEditor : public QWidget {
    Q_OBJECT
public:
    explicit NoteEditor(QWidget *parent = nullptr);

    void setNote(Note *note);
    Note *note() const { return note_; }

    // Zugriff auf die View für MainWindow
    MultiPageNoteView *view() const { return canvas_; }

    /// Menü von der Titelleiste / Android-Topbar (Anchor = Button unten am Menü)
    void showOverflowMenuFromAnchor(QWidget *anchor);

    std::function<void(Note *)> onSaveRequested;
    /// Rechte Notiz-Leiste (Tags, Optionen) — nicht die globalen App-Einstellungen
    std::function<void()> onOpenNoteOptionsRequested;
    /// Full page manager (reorder / batch) — everyday nav stays on the page rail
    std::function<void()> onOpenPageManagerRequested;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    Note *note_{nullptr};

    MultiPageNoteView *canvas_{nullptr};

    void setupUi();
    void setupShortcuts();
};
