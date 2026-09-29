#pragma once

#include "strukturdocument.h"

#include <QWidget>
#include <functional>

class QLineEdit;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;
class QTimer;
class QPlainTextEdit;

namespace struktur_detail {
class StrukturEmbedWidget;
}

/// Notion-like Struktur document: full-width text blocks plus 4-column A4
/// embed grids.
class StrukturNoteEditor : public QWidget {
  Q_OBJECT
public:
  explicit StrukturNoteEditor(QWidget *parent = nullptr);

  void loadDocument(const QString &path);
  QString documentPath() const { return m_path; }
  QString displayTitle() const;
  void refreshAllEmbeds();

  /// Scrolls the block into view and flashes an accent frame on it.
  void revealBlock(int blockIndex);
  /// Reveals the embed that was last opened via onOpenEmbed (if any).
  void revealLastOpenedEmbed();

  /// Called when the user opens an embedded A4 page.
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
  void applyThemeStyles();
  void rebuildUiFromDoc();
  void scheduleSave();
  void harvestIntoDoc();
  void insertParagraphAfter(int blockIndex, const QString &initialText = QString(),
                            const QString &kind = QString());
  void insertEmbed(const StrukturEmbedBlock &embed, int afterIndex = -1,
                 bool forceNewGrid = false);
  void showInsertMenu();
  void showInsertMenuAt(int afterBlockIndex, const QPoint &globalPos);
  int blockIndexOfWidget(QWidget *w) const;
  QWidget *makeParagraphRow(const QString &text, const QString &kind = QString());
  struktur_detail::StrukturEmbedWidget *
  makeEmbedWidget(const StrukturEmbedBlock &embed, QWidget *parent = nullptr);
  QWidget *makeEmbedGrid(const QVector<StrukturGridItem> &items);
  void wireEmbed(struktur_detail::StrukturEmbedWidget *emb);
  void wireParagraphRow(QWidget *row, QPlainTextEdit *edit);
  void applyRowKind(QWidget *row, const QString &kind);
  void removeEmptyParagraph(QWidget *row);
  void focusSiblingParagraph(QWidget *row, int direction);
  void focusFirstParagraph();
  void promptLinkedNoteTitle(int afterBlockIndex, bool forceNewGrid = false);
  void pickExistingNote(int afterBlockIndex, bool appendPage);
  int insertIndex(int desired) const;
  QWidget *blockWidgetAt(int index) const;

  QString m_path;
  StrukturDocument m_doc;
  QScrollArea *m_scroll{nullptr};
  QWidget *m_host{nullptr};
  QVBoxLayout *m_blocksLay{nullptr};
  QLineEdit *m_titleEdit{nullptr};
  QTimer *m_saveTimer{nullptr};
  bool m_loading{false};
  int m_lastOpenedEmbed{-1};
  int m_lastOpenedGridItem{-1};
};
