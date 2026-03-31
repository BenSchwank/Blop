#include "noteeditor.h"
#include "SelectionMenuIcons.h"
#include <QApplication>
#include <QFileDialog>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QVBoxLayout>
#include "UIStyles.h"

NoteEditor::NoteEditor(QWidget *parent) : QWidget(parent) {
    setupUi();
    setupShortcuts();
}

void NoteEditor::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    canvas_ = new MultiPageNoteView(this);
    layout->addWidget(canvas_);

    canvas_->onSaveRequested = [this](Note *n) {
        if (onSaveRequested)
            onSaveRequested(n);
    };
}

void NoteEditor::showOverflowMenuFromAnchor(QWidget *anchor) {
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QString accent = UIStyles::Accent.name();
    menu->setStyleSheet(QString(
                             "QMenu { background: #1E1E2E; "
                             "border: 1px solid %1; "
                             "border-radius: 8px; padding: 6px; }"
                             "QMenu::item { color: #D8D5FF; "
                             "padding: 10px 20px; "
                             "border-radius: 5px; font-size: 13px; }"
                             "QMenu::item:selected { background: %1; "
                             "color: white; }")
                             .arg(accent));

    QAction *actLayout =
        menu->addAction(QStringLiteral("Seitenlayout…"));
    actLayout->setIcon(SelectionMenuIcons::pageLayoutIcon());
    QAction *actNoteOptions =
        menu->addAction(QStringLiteral("Optionen & Tags…"));
    actNoteOptions->setIcon(SelectionMenuIcons::gearIcon());
    menu->addSeparator();
    QAction *actExportPdf =
        menu->addAction(QStringLiteral("Als PDF exportieren"));
    actExportPdf->setIcon(SelectionMenuIcons::pdfDocIcon());
    QAction *actExportImg =
        menu->addAction(QStringLiteral("Als Bild exportieren"));
    actExportImg->setIcon(SelectionMenuIcons::screenshotIcon());
    menu->addSeparator();
    QAction *actImportPdf =
        menu->addAction(QStringLiteral("PDF importieren"));
    actImportPdf->setIcon(SelectionMenuIcons::importIcon());

    QPoint pos;
    if (anchor && anchor->isVisible())
        pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 4));
    else
        pos = QCursor::pos();

    QAction *chosen = menu->exec(pos);

    if (chosen == actLayout) {
        if (canvas_)
            canvas_->openPageLayoutForVisiblePage();
    } else if (chosen == actNoteOptions) {
        if (onOpenNoteOptionsRequested)
            onOpenNoteOptionsRequested();
    } else if (chosen == actExportPdf) {
        QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Als PDF exportieren"),
            note_ ? note_->title + ".pdf" : "Notiz.pdf",
            QStringLiteral("PDF Dokument (*.pdf)"));
        if (!path.isEmpty()) {
            bool ok = canvas_->exportPageToPdf(0, path);
            if (ok)
                QMessageBox::information(this, QStringLiteral("Exportiert"),
                                         QStringLiteral("PDF wurde gespeichert!"));
            else
                QMessageBox::warning(this, QStringLiteral("Fehler"),
                                     QStringLiteral("PDF konnte nicht gespeichert werden."));
        }
    } else if (chosen == actExportImg) {
        QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Als Bild exportieren"),
            note_ ? note_->title + ".png" : "Notiz.png",
            QStringLiteral("Bilder (*.png *.jpg)"));
        if (!path.isEmpty()) {
            bool ok = canvas_->exportPageToPng(0, path);
            if (ok)
                QMessageBox::information(this, QStringLiteral("Exportiert"),
                                         QStringLiteral("Bild wurde gespeichert!"));
            else
                QMessageBox::warning(this, QStringLiteral("Fehler"),
                                     QStringLiteral("Bild konnte nicht gespeichert werden."));
        }
    } else if (chosen == actImportPdf) {
        QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("PDF importieren"), QString(),
            QStringLiteral("PDF Dokument (*.pdf)"));
        if (!path.isEmpty()) {
            bool ok = canvas_->importPdfPages(path);
            if (ok)
                QMessageBox::information(
                    this, QStringLiteral("Importiert"),
                    QStringLiteral(
                        "PDF wurde seitenweise importiert.\nJede PDF-Seite erscheint als "
                        "eigene Notizseite."));
            else
                QMessageBox::warning(
                    this, QStringLiteral("Fehler"),
                    QStringLiteral(
                        "PDF konnte nicht importiert werden.\n"
                        "Bitte stelle sicher, dass Qt mit PDF-Unterstützung kompiliert ist."));
        }
    }
}

void NoteEditor::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

void NoteEditor::setupShortcuts() {
    new QShortcut(QKeySequence::Save, this, [this]() {
        if (onSaveRequested && note_)
            onSaveRequested(note_);
    });

    new QShortcut(QKeySequence(Qt::Key_Delete), this, [this]() {
        if (!note_)
            return;
    });

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
