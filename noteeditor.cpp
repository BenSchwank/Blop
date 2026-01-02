#include "noteeditor.h"
#include <QKeySequence>
#include <QShortcut>
#include <QVBoxLayout>

NoteEditor::NoteEditor(QWidget *parent) : QWidget(parent) {
    setupUi();
    setupShortcuts();
}

void NoteEditor::setupUi() {
    // Layout erstellen, damit der Canvas das Widget füllt
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Canvas erstellen
    canvas_ = new MultiPageNoteView(this);

    // Canvas direkt ins Layout
    layout->addWidget(canvas_);

    // Speichern-Callback weiterleiten
    canvas_->onSaveRequested = [this](Note *n) {
        if (onSaveRequested)
            onSaveRequested(n);
    };
}

void NoteEditor::setupShortcuts() {
    // Speichern Shortcut
    new QShortcut(QKeySequence::Save, this, [this]() {
        if (onSaveRequested && note_)
            onSaveRequested(note_);
    });

    // Delete Shortcut für Seiten (Optional)
    new QShortcut(QKeySequence(Qt::Key_Delete), this, [this]() {
        if (!note_) return;
    });

    // Werkzeug Shortcuts (Direkt auf den Canvas wirken)
    // Hinweis: Das MainWindow bekommt diese Änderungen aktuell nicht visuell mit,
    // aber die Funktion ist gegeben.
    new QShortcut(QKeySequence(Qt::Key_P), this,
                  [this]() { canvas_->setToolMode(ToolMode::Pen); });
    new QShortcut(QKeySequence(Qt::Key_E), this,
                  [this]() { canvas_->setToolMode(ToolMode::Eraser); });
    new QShortcut(QKeySequence(Qt::Key_L), this,
                  [this]() { canvas_->setToolMode(ToolMode::Lasso); });
}

void NoteEditor::setNote(Note *note) {
    note_ = note;
    canvas_->setNote(note);
}
