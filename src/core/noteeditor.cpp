#include "noteeditor.h"
#include "SelectionMenuIcons.h"
#include "blop_diag.h"
#include "blop_inwindow_menu.h"
#include "blop_theme.h"
#include <QApplication>
#include <QFileDialog>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
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
    BlopDiag::recordUiAction(QStringLiteral("note_overflow"));

#ifdef Q_OS_ANDROID
    {
        // QMenu spawns a top-level QWindow whose backing store allocation runs
        // QOpenGLContext::makeCurrent. Combined with QtAndroidAccessibility
        // holding the EGL surface on Android 16 this triggers the deadlock
        // protector and aborts the process (verified by tombstone v3.16.7).
        // Route to the in-window QFrame menu replacement on Android.
        QPointer<NoteEditor> safeNote(this);
        QList<BlopInWindowMenu::Item> items;
        items.append({QStringLiteral("Seitenlayout…"),
                      SelectionMenuIcons::pageLayoutIcon(),
                      [safeNote]() {
                          if (safeNote && safeNote->canvas_)
                              safeNote->canvas_->openPageLayoutForVisiblePage();
                      },
                      false, false});
        items.append({QStringLiteral("Optionen & Tags…"),
                      SelectionMenuIcons::gearIcon(),
                      [safeNote]() {
                          if (safeNote && safeNote->onOpenNoteOptionsRequested)
                              safeNote->onOpenNoteOptionsRequested();
                      },
                      false, false});

        QPoint pos;
        if (anchor && anchor->isVisible())
            pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 4));
        else
            pos = QCursor::pos();
        BlopInWindowMenu::show(this, pos, items);
        return;
    }
#endif

    // Heap-allocated menu so popup() (used on Android to avoid a nested event
    // loop on the single-window surface) can outlive this function. Actions
    // are wired via QAction::triggered lambdas, NOT a single exec() return
    // value, so the same code path works for both popup() and exec().
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QString accent = UIStyles::Accent.name();
    menu->setStyleSheet(BlopTheme::themed(
        QString("QMenu { background: #1E1E2E; "
                "border: 1px solid %1; "
                "border-radius: 8px; padding: 6px; }"
                "QMenu::item { color: #D8D5FF; "
                "padding: 10px 20px; "
                "border-radius: 5px; font-size: 13px; }"
                "QMenu::item:selected { background: %1; "
                "color: white; }")
            .arg(accent)));

    QPointer<NoteEditor> safe(this);

    QAction *actLayout =
        menu->addAction(QStringLiteral("Seitenlayout…"));
    actLayout->setIcon(SelectionMenuIcons::pageLayoutIcon());
    QObject::connect(actLayout, &QAction::triggered, this, [safe]() {
        if (safe && safe->canvas_)
            safe->canvas_->openPageLayoutForVisiblePage();
    });

    QAction *actNoteOptions =
        menu->addAction(QStringLiteral("Optionen & Tags…"));
    actNoteOptions->setIcon(SelectionMenuIcons::gearIcon());
    QObject::connect(actNoteOptions, &QAction::triggered, this, [safe]() {
        if (safe && safe->onOpenNoteOptionsRequested)
            safe->onOpenNoteOptionsRequested();
    });

#ifndef Q_OS_ANDROID
    // QFileDialog and QMessageBox are top-level QDialog subclasses; on Android
    // they crash the single-window surface (same crash family as QMenu::exec).
    // Until we have in-window equivalents, hide these items on Android.
    menu->addSeparator();
    QAction *actExportPdf =
        menu->addAction(QStringLiteral("Als PDF exportieren"));
    actExportPdf->setIcon(SelectionMenuIcons::pdfDocIcon());
    QObject::connect(actExportPdf, &QAction::triggered, this, [safe]() {
        if (!safe)
            return;
        QString path = QFileDialog::getSaveFileName(
            safe, QStringLiteral("Als PDF exportieren"),
            safe->note_ ? safe->note_->title + ".pdf" : "Notiz.pdf",
            QStringLiteral("PDF Dokument (*.pdf)"));
        if (path.isEmpty() || !safe || !safe->canvas_)
            return;
        bool ok = safe->canvas_->exportNoteToPdf(path);
        if (ok)
            QMessageBox::information(safe, QStringLiteral("Exportiert"),
                                     QStringLiteral("PDF wurde gespeichert!"));
        else
            QMessageBox::warning(safe, QStringLiteral("Fehler"),
                                 QStringLiteral("PDF konnte nicht gespeichert werden."));
    });

    QAction *actExportImg =
        menu->addAction(QStringLiteral("Als Bild exportieren"));
    actExportImg->setIcon(SelectionMenuIcons::screenshotIcon());
    QObject::connect(actExportImg, &QAction::triggered, this, [safe]() {
        if (!safe)
            return;
        QString path = QFileDialog::getSaveFileName(
            safe, QStringLiteral("Als Bild exportieren"),
            safe->note_ ? safe->note_->title + ".png" : "Notiz.png",
            QStringLiteral("Bilder (*.png *.jpg)"));
        if (path.isEmpty() || !safe || !safe->canvas_)
            return;
        bool ok = safe->canvas_->exportNoteToPng(path);
        if (ok)
            QMessageBox::information(safe, QStringLiteral("Exportiert"),
                                     QStringLiteral("Bild wurde gespeichert!"));
        else
            QMessageBox::warning(safe, QStringLiteral("Fehler"),
                                 QStringLiteral("Bild konnte nicht gespeichert werden."));
    });

    menu->addSeparator();
    QAction *actImportPdf =
        menu->addAction(QStringLiteral("PDF importieren"));
    actImportPdf->setIcon(SelectionMenuIcons::importIcon());
    QObject::connect(actImportPdf, &QAction::triggered, this, [safe]() {
        if (!safe)
            return;
        QString path = QFileDialog::getOpenFileName(
            safe, QStringLiteral("PDF importieren"), QString(),
            QStringLiteral("PDF Dokument (*.pdf)"));
        if (path.isEmpty() || !safe || !safe->canvas_)
            return;
        bool ok = safe->canvas_->importPdfPages(path);
        if (ok)
            QMessageBox::information(
                safe, QStringLiteral("Importiert"),
                QStringLiteral(
                    "PDF wurde seitenweise importiert.\nJede PDF-Seite erscheint als "
                    "eigene Notizseite."));
        else
            QMessageBox::warning(
                safe, QStringLiteral("Fehler"),
                QStringLiteral(
                    "PDF konnte nicht importiert werden.\n"
                    "Bitte stelle sicher, dass Qt mit PDF-Unterstützung kompiliert ist."));
    });
#endif // !Q_OS_ANDROID

    QPoint pos;
    if (anchor && anchor->isVisible())
        pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 4));
    else
        pos = QCursor::pos();

    // Desktop only -- the Android path returned early above through the
    // BlopInWindowMenu helper to dodge the EGL/QWindow deadlock.
    menu->exec(pos);
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
#ifdef Q_OS_ANDROID
    // Open every note at "A4 fits viewport" on phones. requestAutoFit() is a
    // no-op once the user has manually pinch/wheel-zoomed in this session.
    if (canvas_)
        canvas_->requestAutoFit();
#endif
}
