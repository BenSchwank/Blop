#include "noteeditor.h"
#include <QKeySequence>
#include <QShortcut>
#include <QVBoxLayout>


NoteEditor::NoteEditor(QWidget *parent) : QWidget(parent) {
  setupUi();
  setupShortcuts();

  textSaveDebounce_.setInterval(300);
  textSaveDebounce_.setSingleShot(true);
  connect(&textSaveDebounce_, &QTimer::timeout, [this]() {
    if (onSaveRequested && note_)
      onSaveRequested(note_);
  });
}

void NoteEditor::setupUi() {
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  tabs_ = new QTabWidget(this);
  text_ = new QTextEdit(this);
  text_->setAcceptRichText(false);

  canvas_ = new MultiPageNoteView(this);
  toolbar_ = new ModernToolbar(canvas_);
  toolbar_->move(canvas_->width() - toolbar_->width() - 20, 20);

  tabs_->addTab(text_, "Text");
  tabs_->addTab(canvas_, "Notiz");

  layout->addWidget(tabs_);

  canvas_->onSaveRequested = [this](Note *n) {
    if (onSaveRequested)
      onSaveRequested(n);
  };

  connect(toolbar_, &ModernToolbar::toolChanged, this,
          [this](ToolMode m) { canvas_->setToolMode(m); });

  text_->installEventFilter(this);
}

void NoteEditor::setupShortcuts() {
  new QShortcut(QKeySequence::Save, this, [this]() {
    if (onSaveRequested && note_)
      onSaveRequested(note_);
  });
  new QShortcut(QKeySequence::Undo, text_, [this]() { text_->undo(); });
  new QShortcut(QKeySequence::Redo, text_, [this]() { text_->redo(); });
  new QShortcut(QKeySequence(Qt::Key_Delete), this, [this]() {
    if (!note_)
      return;
    int idx = note_->pages.size() - 1;
    if (idx >= 0) {
      note_->pages.remove(idx);
      canvas_->setNote(note_);
      if (onSaveRequested)
        onSaveRequested(note_);
    }
  });
  new QShortcut(QKeySequence(Qt::Key_P), this,
                [this]() { toolbar_->setToolMode(ToolMode::Pen); });
  new QShortcut(QKeySequence(Qt::Key_E), this,
                [this]() { toolbar_->setToolMode(ToolMode::Eraser); });
  new QShortcut(QKeySequence(Qt::Key_L), this,
                [this]() { toolbar_->setToolMode(ToolMode::Lasso); });
}

void NoteEditor::setNote(Note *note) {
  note_ = note;
  canvas_->setNote(note);
}

bool NoteEditor::eventFilter(QObject *obj, QEvent *ev) {
  if (obj == text_) {
    if (ev->type() == QEvent::KeyPress || ev->type() == QEvent::InputMethod) {
      textSaveDebounce_.start();
    }
  }
  return QWidget::eventFilter(obj, ev);
}

void NoteEditor::resizeEvent(QResizeEvent *) {
  // Keep toolbar top-right with 20px margin
  if (toolbar_)
    toolbar_->move(canvas_->width() - toolbar_->width() - 20, 20);
}
