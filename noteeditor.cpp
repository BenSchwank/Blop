#include "noteeditor.h"
#include <QFileDialog>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

NoteEditor::NoteEditor(QWidget *parent) : QWidget(parent) {
    setupUi();
    setupShortcuts();
}

void NoteEditor::setupUi() {
    // Use a stacked/absolute layout so overlays can float on top
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Canvas (MultiPageNoteView)
    canvas_ = new MultiPageNoteView(this);
    layout->addWidget(canvas_);

    // Floating 3-dot action button (top-right corner)
    m_menuBtn = new QPushButton("⋯", this);
    m_menuBtn->setFixedSize(36, 36);
    m_menuBtn->setCursor(Qt::PointingHandCursor);
    m_menuBtn->setToolTip("Notiz-Aktionen");
    m_menuBtn->setAttribute(Qt::WA_TranslucentBackground);
    m_menuBtn->setStyleSheet(
        "QPushButton {"
        "  background: rgba(30,30,50,0.75);"
        "  border: 1px solid rgba(108,92,231,0.5);"
        "  border-radius: 8px;"
        "  color: #D8D5FF;"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(108,92,231,0.85);"
        "  color: white;"
        "}");

    connect(m_menuBtn, &QPushButton::clicked, this, [this]() {
        QMenu *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->setStyleSheet(
            "QMenu { background: #1E1E2E; border: 1px solid #6C5CE7;"
            "  border-radius: 8px; padding: 6px; }"
            "QMenu::item { color: #D8D5FF; padding: 10px 20px;"
            "  border-radius: 5px; font-size: 13px; }"
            "QMenu::item:selected { background: #6C5CE7; color: white; }");

        QAction *actExportPdf = menu->addAction("📄  Als PDF exportieren");
        QAction *actExportImg = menu->addAction("🖼️  Als Bild exportieren");
        menu->addSeparator();
        QAction *actImportPdf = menu->addAction("📥  PDF importieren");

        QAction *chosen = menu->exec(m_menuBtn->mapToGlobal(QPoint(0, m_menuBtn->height() + 4)));

        if (chosen == actExportPdf) {
            QString path = QFileDialog::getSaveFileName(
                this, "Als PDF exportieren",
                note_ ? note_->title + ".pdf" : "Notiz.pdf",
                "PDF Dokument (*.pdf)");
            if (!path.isEmpty()) {
                bool ok = canvas_->exportPageToPdf(0, path);
                if (ok)
                    QMessageBox::information(this, "Exportiert", "PDF wurde gespeichert!");
                else
                    QMessageBox::warning(this, "Fehler", "PDF konnte nicht gespeichert werden.");
            }
        } else if (chosen == actExportImg) {
            QString path = QFileDialog::getSaveFileName(
                this, "Als Bild exportieren",
                note_ ? note_->title + ".png" : "Notiz.png",
                "Bilder (*.png *.jpg)");
            if (!path.isEmpty()) {
                bool ok = canvas_->exportPageToPng(0, path);
                if (ok)
                    QMessageBox::information(this, "Exportiert", "Bild wurde gespeichert!");
                else
                    QMessageBox::warning(this, "Fehler", "Bild konnte nicht gespeichert werden.");
            }
        } else if (chosen == actImportPdf) {
            QString path = QFileDialog::getOpenFileName(
                this, "PDF importieren", QString(),
                "PDF Dokument (*.pdf)");
            if (!path.isEmpty()) {
                bool ok = canvas_->importPdfPages(path);
                if (ok)
                    QMessageBox::information(this, "Importiert",
                        "PDF wurde seitenweise importiert.\nJede PDF-Seite erscheint als eigene Notizseite.");
                else
                    QMessageBox::warning(this, "Fehler",
                        "PDF konnte nicht importiert werden.\n"
                        "Bitte stelle sicher, dass Qt mit PDF-Unterstützung kompiliert ist.");
            }
        }
    });

    // Speichern-Callback weiterleiten
    canvas_->onSaveRequested = [this](Note *n) {
        if (onSaveRequested)
            onSaveRequested(n);
    };
}

void NoteEditor::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Keep the 3-dot button pinned to top-right with 12px margin
    if (m_menuBtn) {
        m_menuBtn->move(width() - m_menuBtn->width() - 12, 12);
        m_menuBtn->raise();
    }
}

void NoteEditor::setupShortcuts() {
    new QShortcut(QKeySequence::Save, this, [this]() {
        if (onSaveRequested && note_)
            onSaveRequested(note_);
    });

    new QShortcut(QKeySequence(Qt::Key_Delete), this, [this]() {
        if (!note_) return;
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
