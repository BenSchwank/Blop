#include "markdowneditor.h"
#include <QHBoxLayout>
#include <QKeySequence>
#include <QShortcut>
#include <QVBoxLayout>
#include <utility>


MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  HighlightingRule rule;

  // Headers
  headerFormat.setForeground(QColor("#5E5CE6"));
  headerFormat.setFontWeight(QFont::Bold);
  rule.pattern = QRegularExpression("^#{1,6}\\s.*");
  rule.format = headerFormat;
  highlightingRules.append(rule);

  // Bold
  boldFormat.setFontWeight(QFont::Bold);
  rule.pattern = QRegularExpression("\\*\\*[^\\*]+\\*\\*");
  rule.format = boldFormat;
  highlightingRules.append(rule);

  // Italic
  italicFormat.setFontItalic(true);
  rule.pattern = QRegularExpression("\\*[^\\*]+\\*");
  rule.format = italicFormat;
  highlightingRules.append(rule);

  // Lists
  listFormat.setForeground(QColor("#7D7AFF"));
  rule.pattern = QRegularExpression("^\\s*[\\*\\-\\+]\\s+.*");
  rule.format = listFormat;
  highlightingRules.append(rule);

  // Code
  codeFormat.setForeground(QColor("#A6E22E"));
  codeFormat.setFontFamilies({QStringLiteral("Consolas")});
  rule.pattern = QRegularExpression("`[^`]+`");
  rule.format = codeFormat;
  highlightingRules.append(rule);

  // Multi-line code blocks
  rule.pattern = QRegularExpression("```[\\s\\S]*?```");
  rule.format = codeFormat;
  highlightingRules.append(rule);

  // Quotes
  quoteFormat.setForeground(Qt::darkGray);
  quoteFormat.setFontItalic(true);
  rule.pattern = QRegularExpression("^>\\s.*");
  rule.format = quoteFormat;
  highlightingRules.append(rule);
}

void MarkdownHighlighter::highlightBlock(const QString &text) {
  for (const HighlightingRule &rule : std::as_const(highlightingRules)) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}

MarkdownEditor::MarkdownEditor(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // Toolbar (optional)
  QWidget *toolbar = new QWidget(this);
  toolbar->setFixedHeight(50);
  toolbar->setStyleSheet(
      "background-color: #252526; border-bottom: 1px solid #333;");
  QHBoxLayout *tbLayout = new QHBoxLayout(toolbar);

  QPushButton *btnSave = new QPushButton(QStringLiteral("Speichern"));
  btnSave->setCursor(Qt::PointingHandCursor);
  btnSave->setStyleSheet(
      "QPushButton { background: #5E5CE6; color: white; border: none; padding: "
      "8px 16px; border-radius: 6px; font-weight: bold; } QPushButton:hover { "
      "background: #7D7AFF; }");
  connect(btnSave, &QPushButton::clicked, this, &MarkdownEditor::onSaveClicked);
  tbLayout->addWidget(btnSave);
  tbLayout->addStretch();
  mainLayout->addWidget(toolbar);

  // Splitter
  m_splitter = new QSplitter(Qt::Horizontal, this);
  m_splitter->setStyleSheet("QSplitter::handle { background: #333; }");

  m_editor = new QPlainTextEdit(m_splitter);
  m_editor->setStyleSheet(
      "QPlainTextEdit { background: #1E1E1E; color: #D4D4D4; border: none; "
      "font-family: Consolas, monospace; font-size: 14px; padding: 10px; }");
  m_highlighter = new MarkdownHighlighter(m_editor->document());

  m_preview = new QTextBrowser(m_splitter);
  m_preview->setOpenExternalLinks(true);
  m_preview->setStyleSheet(
      "QTextBrowser { background: #1E1E1E; color: #D4D4D4; border: none; "
      "font-size: 14px; padding: 10px; }");

  m_splitter->addWidget(m_editor);
  m_splitter->addWidget(m_preview);
  m_splitter->setSizes({500, 500});

  mainLayout->addWidget(m_splitter);

  // Shortcuts
  new QShortcut(QKeySequence::Save, this, [this]() { onSaveClicked(); });

  connect(m_editor, &QPlainTextEdit::textChanged, this,
          &MarkdownEditor::onTextChanged);
}

void MarkdownEditor::setText(const QString &text) {
  m_editor->setPlainText(text);
  onTextChanged();
}

QString MarkdownEditor::text() const { return m_editor->toPlainText(); }

void MarkdownEditor::onTextChanged() {
  QString mdContent = m_editor->toPlainText();
  m_preview->setMarkdown(mdContent);
}

void MarkdownEditor::onSaveClicked() {
  if (onSaveRequested) {
    onSaveRequested(text());
  }
}
