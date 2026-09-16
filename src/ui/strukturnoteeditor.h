#pragma once

#include "strukturdocument.h"

#include <QWidget>
#include <functional>

class QScrollArea;
class QVBoxLayout;
class QTimer;
class QLabel;
class QPlainTextEdit;

/// Text-first Struktur document with A4 note-page embeds.
class StrukturNoteEditor : public QWidget {
  Q_OBJECT
public:
  explicit StrukturNoteEditor(QWidget *parent = nullptr);

  void loadDocument(const QString &path);
  QString documentPath() const { return m_path; }
  void refreshAllEmbeds();

  /// Called by MainWindow after a linked note was saved/closed.
  std::function<void(const QString &notePath, int pageIndex)> onOpenEmbed;
  /// Request creating a new A4 note next to this struktur file; returns path.
  std::function<QString(const QString &title)> onCreateLinkedNote;
  /// Append a page to an existing .bnote; returns new page index or -1.
  std::function<int(const QString &notePath)> onAppendPage;

signals:
  void documentModified();

public slots:
  void saveNow();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void rebuildUiFromDoc();
  void scheduleSave();
  void harvestIntoDoc();
  void insertParagraphAfter(int blockIndex, const QString &initialText = QString());
  void insertEmbed(const StrukturEmbedBlock &embed, int afterIndex = -1);
  void showInsertMenu();
  void showInsertMenuAt(int afterBlockIndex, const QPoint &globalPos);
  int blockIndexOfWidget(QWidget *w) const;
  QWidget *makeParagraphRow(const QString &text);
  void wireParagraphRow(QWidget *row, QPlainTextEdit *edit);

  QString m_path;
  StrukturDocument m_doc;
  QScrollArea *m_scroll{nullptr};
  QWidget *m_host{nullptr};
  QVBoxLayout *m_blocksLay{nullptr};
  QLabel *m_titleLabel{nullptr};
  QTimer *m_saveTimer{nullptr};
  bool m_loading{false};
};
