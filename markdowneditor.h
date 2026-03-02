#pragma once

#include <QRegularExpression>
#include <QtGui>
#include <QtWidgets>
#include <functional>

class MarkdownHighlighter : public QSyntaxHighlighter {
  Q_OBJECT
public:
  explicit MarkdownHighlighter(QTextDocument *parent = nullptr);

protected:
  void highlightBlock(const QString &text) override;

private:
  struct HighlightingRule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };
  QVector<HighlightingRule> highlightingRules;

  QTextCharFormat headerFormat;
  QTextCharFormat boldFormat;
  QTextCharFormat italicFormat;
  QTextCharFormat listFormat;
  QTextCharFormat codeFormat;
  QTextCharFormat quoteFormat;
};

class MarkdownEditor : public QWidget {
  Q_OBJECT
public:
  explicit MarkdownEditor(QWidget *parent = nullptr);
  void setText(const QString &text);
  QString text() const;

  std::function<void(const QString &)> onSaveRequested;

private slots:
  void onTextChanged();
  void onSaveClicked();

private:
  QPlainTextEdit *m_editor;
  QTextBrowser *m_preview;
  QSplitter *m_splitter;
  MarkdownHighlighter *m_highlighter;
};
