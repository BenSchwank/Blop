#include "strukturnoteeditor.h"

#include "blopstyle.h"
#include "blop_theme.h"
#include "notemanager.h"
#include "notepagerenderer.h"
#include "uiscale.h"

#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace struktur_detail {

static QString notionMenuQss() {
  return QStringLiteral(
      "QMenu {"
      "  background: %1; border: 1px solid %2; border-radius: %3px;"
      "  padding: 4px;"
      "}"
      "QMenu::item {"
      "  padding: 8px 14px; border-radius: 6px; color: %4;"
      "  background: transparent;"
      "}"
      "QMenu::item:selected { background: #F1F5F9; }"
      "QMenu::separator { height: 1px; background: %2; margin: 4px 8px; }")
      .arg(BlopStyle::paperSurface().name(QColor::HexRgb),
           BlopStyle::paperBorder().name(QColor::HexRgb),
           QString::number(UiScale::dp(BlopStyle::radiusMdDp())),
           BlopStyle::paperInk().name(QColor::HexRgb));
}

/// Notion-like text block: clean white, no ruled lines, soft placeholder.
class NotionTextEdit : public QPlainTextEdit {
  Q_OBJECT
public:
  explicit NotionTextEdit(QWidget *parent = nullptr) : QPlainTextEdit(parent) {
    setObjectName(QStringLiteral("StrukturParagraph"));
    setFrameShape(QFrame::NoFrame);
    setTabChangesFocus(false);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setPlaceholderText(QStringLiteral("Schreib etwas, oder füge eine Notiz ein…"));
    document()->setDocumentMargin(UiScale::dp(2));
    QFont f = font();
    f.setPixelSize(UiScale::dp(16));
    setFont(f);
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit#StrukturParagraph {"
        "  background: transparent; color: %1; border: none;"
        "  padding: 6px 2px; selection-background-color: %2;"
        "}")
                      .arg(BlopStyle::paperInk().name(QColor::HexRgb),
                           BlopStyle::paperPrimaryLight().name(QColor::HexRgb)));
    connect(document(), &QTextDocument::contentsChanged, this,
            [this]() { updateHeight(); });
  }

  void updateHeight() {
    const int h = qMax(UiScale::dp(40),
                       int(document()->size().height()) + UiScale::dp(18));
    setFixedHeight(h);
  }
};

class EmbedCropOverlay : public QWidget {
  Q_OBJECT
public:
  explicit EmbedCropOverlay(QWidget *parent = nullptr) : QWidget(parent) {
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("CropBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setStyleSheet(QStringLiteral(
        "QWidget#CropBar { background: rgba(15,23,42,0.92); border-radius: 10px; }"));
    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(UiScale::dp(12), UiScale::dp(8), UiScale::dp(12),
                            UiScale::dp(8));
    auto *hint = new QLabel(QStringLiteral("Ausschnitt ziehen"), bar);
    hint->setStyleSheet(QStringLiteral("color: #E2E8F0; font-size: 12px;"));
    lay->addWidget(hint);
    lay->addStretch(1);
    auto *btnCancel = new QPushButton(QStringLiteral("Abbrechen"), bar);
    auto *btnOk = new QPushButton(QStringLiteral("Übernehmen"), bar);
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnOk->setCursor(Qt::PointingHandCursor);
    btnCancel->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #CBD5E1; border: 1px solid "
        "#475569; border-radius: 8px; padding: 8px 14px; font-weight: 600; }"));
    btnOk->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none;"
        "  border-radius: 8px; padding: 8px 14px; font-weight: 700; }")
                             .arg(BlopTheme::accentPrimary().name(QColor::HexRgb)));
    connect(btnCancel, &QPushButton::clicked, this, &EmbedCropOverlay::cancelled);
    connect(btnOk, &QPushButton::clicked, this, [this]() {
      const QRectF n = normalizedCrop();
      if (n.width() > 0.03 && n.height() > 0.03)
        emit confirmed(n);
      else
        emit cancelled();
    });
    lay->addWidget(btnCancel);
    lay->addWidget(btnOk);
    m_bar = bar;
  }

  void setSourcePixmap(const QPixmap &pm) {
    m_full = pm;
    update();
  }

  QRectF normalizedCrop() const {
    if (m_full.isNull() || width() <= 0 || height() <= 0)
      return QRectF();
    const QRectF img = fittedImageRect();
    if (img.isEmpty())
      return QRectF();
    QRectF sel = m_sel.isNull() ? img : m_sel;
    sel = sel.intersected(img);
    if (sel.width() < 4 || sel.height() < 4)
      return QRectF();
    return QRectF((sel.x() - img.x()) / img.width(),
                  (sel.y() - img.y()) / img.height(),
                  sel.width() / img.width(), sel.height() / img.height());
  }

signals:
  void confirmed(const QRectF &cropNorm);
  void cancelled();

protected:
  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    if (m_bar) {
      const int bw = qMin(width() - UiScale::dp(32), UiScale::dp(420));
      m_bar->setFixedWidth(bw);
      m_bar->adjustSize();
      m_bar->move((width() - m_bar->width()) / 2,
                  height() - m_bar->height() - UiScale::dp(24));
      m_bar->raise();
    }
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.fillRect(rect(), QColor(15, 23, 42, 168));
    const QRectF img = fittedImageRect();
    if (!m_full.isNull())
      p.drawPixmap(img.toRect(), m_full);
    QRectF sel = m_sel.isNull() ? img : m_sel.intersected(img);
    // Dim outside selection.
    if (!sel.isEmpty() && sel != img) {
      QPainterPath outer;
      outer.addRect(img);
      QPainterPath hole;
      hole.addRect(sel);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(15, 23, 42, 120));
      p.drawPath(outer.subtracted(hole));
    }
    p.setPen(QPen(BlopTheme::accentPrimary(), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(sel);
  }

  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() != Qt::LeftButton)
      return;
    if (m_bar && m_bar->geometry().contains(e->pos()))
      return;
    m_drag = true;
    m_origin = e->position();
    m_sel = QRectF(m_origin, m_origin);
    update();
  }

  void mouseMoveEvent(QMouseEvent *e) override {
    if (!m_drag)
      return;
    m_sel = QRectF(m_origin, e->position()).normalized();
    update();
  }

  void mouseReleaseEvent(QMouseEvent *e) override {
    if (!m_drag || e->button() != Qt::LeftButton)
      return;
    m_drag = false;
    update();
  }

  void keyPressEvent(QKeyEvent *e) override {
    if (e->key() == Qt::Key_Escape)
      emit cancelled();
    else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
      const QRectF n = normalizedCrop();
      if (n.width() > 0.03 && n.height() > 0.03)
        emit confirmed(n);
      else
        emit cancelled();
    } else
      QWidget::keyPressEvent(e);
  }

private:
  QRectF fittedImageRect() const {
    if (m_full.isNull())
      return QRectF();
    QSize s = m_full.size();
    const int maxH = height() - UiScale::dp(100);
    s.scale(QSize(width() - UiScale::dp(48), maxH), Qt::KeepAspectRatio);
    const int x = (width() - s.width()) / 2;
    const int y = (maxH - s.height()) / 2 + UiScale::dp(16);
    return QRectF(x, y, s.width(), s.height());
  }

  QPixmap m_full;
  QRectF m_sel;
  QPointF m_origin;
  bool m_drag{false};
  QWidget *m_bar{nullptr};
};

class StrukturEmbedWidget : public QWidget {
  Q_OBJECT
public:
  StrukturEmbedWidget(const QString &strukturPath, StrukturEmbedBlock embed,
                      QWidget *parent = nullptr)
      : QWidget(parent), m_strukturPath(strukturPath), m_embed(embed) {
    setObjectName(QStringLiteral("StrukturEmbed"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("StrukturEmbedCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(QStringLiteral(
        "QFrame#StrukturEmbedCard {"
        "  background: %1; border: 1px solid %2; border-radius: %3px;"
        "}")
                            .arg(QStringLiteral("#FFFFFF"),
                                 BlopStyle::paperBorder().name(QColor::HexRgb),
                                 QString::number(
                                     UiScale::dp(BlopStyle::radiusMdDp()))));
    // No DropShadowEffect — breaks clicks + paints ghost artifacts on Win.

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, UiScale::dp(6), 0, UiScale::dp(6));
    outer->addWidget(card);

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(UiScale::dp(10), UiScale::dp(10), UiScale::dp(10),
                            UiScale::dp(10));
    lay->setSpacing(UiScale::dp(8));

    m_caption = new QLabel(card);
    m_caption->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; font-weight: 600; background: transparent;")
                                 .arg(BlopStyle::paperInkMuted().name(
                                     QColor::HexRgb)));
    lay->addWidget(m_caption);

    m_preview = new QLabel(card);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(UiScale::dp(180));
    m_preview->setCursor(Qt::PointingHandCursor);
    m_preview->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2; border-radius: %3px; }")
                                 .arg(BlopStyle::paperBg().name(QColor::HexRgb),
                                      BlopStyle::paperBorder().name(QColor::HexRgb),
                                      QString::number(
                                          UiScale::dp(BlopStyle::radiusMdDp()))));
    m_preview->installEventFilter(this);
    lay->addWidget(m_preview);

    auto *bar = new QWidget(card);
    auto *barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(0, 0, 0, 0);
    barLay->setSpacing(UiScale::dp(6));

    auto makeQuiet = [bar](const QString &text) {
      auto *b = new QPushButton(text, bar);
      b->setCursor(Qt::PointingHandCursor);
      b->setMinimumHeight(UiScale::dp(32));
      b->setStyleSheet(BlopStyle::paperSecondaryButtonQss());
      return b;
    };

    auto *btnOpen = makeQuiet(QStringLiteral("Öffnen"));
    auto *btnCrop = makeQuiet(QStringLiteral("✂ Zuschneiden"));
    auto *btnFull = makeQuiet(QStringLiteral("Ganzseite"));
    barLay->addWidget(btnOpen);
    barLay->addWidget(btnCrop);
    barLay->addWidget(btnFull);
    barLay->addStretch(1);
    lay->addWidget(bar);

    connect(btnOpen, &QPushButton::clicked, this,
            [this]() { emit openRequested(); });
    connect(btnCrop, &QPushButton::clicked, this, &StrukturEmbedWidget::beginCrop);
    connect(btnFull, &QPushButton::clicked, this, [this]() {
      m_embed.cropRect = std::nullopt;
      refreshPreview();
      emit embedChanged(m_embed);
    });

    refreshPreview();
  }

  StrukturEmbedBlock embed() const { return m_embed; }

  void refreshPreview() {
    Note note;
    const QString abs = StrukturDocument::absoluteNotePath(m_strukturPath,
                                                           m_embed.notePath);
    const QString label = QFileInfo(abs).completeBaseName();
    if (m_caption) {
      m_caption->setText(
          m_embed.cropRect
              ? QStringLiteral("%1 · Seite %2 · Ausschnitt")
                    .arg(label)
                    .arg(m_embed.pageIndex + 1)
              : QStringLiteral("%1 · Seite %2")
                    .arg(label)
                    .arg(m_embed.pageIndex + 1));
    }
    if (!NoteManager::loadNote(abs, note) || note.pages.isEmpty()) {
      m_preview->setText(QStringLiteral("Notiz nicht gefunden"));
      m_preview->setPixmap(QPixmap());
      m_fullPm = QPixmap();
      return;
    }
    const int idx = qBound(0, m_embed.pageIndex, note.pages.size() - 1);
    m_embed.pageIndex = idx;
    const int availW = qMax(240, width() > 40 ? width() - UiScale::dp(40)
                                             : UiScale::dp(520));
    const QSize fit(availW, UiScale::dp(420));
    const QRectF crop = m_embed.cropRect ? *m_embed.cropRect : QRectF();
    const QImage img =
        NotePageRenderer::renderPreview(note.pages[idx], fit, crop);
    m_fullPm = QPixmap::fromImage(
        NotePageRenderer::renderFullPage(note.pages[idx]));
    m_preview->setPixmap(QPixmap::fromImage(img));
    m_preview->setMinimumHeight(img.height() + UiScale::dp(4));
  }

signals:
  void openRequested();
  void embedChanged(const StrukturEmbedBlock &embed);

protected:
  bool eventFilter(QObject *obj, QEvent *ev) override {
    if (obj == m_preview && ev->type() == QEvent::MouseButtonRelease) {
      auto *me = static_cast<QMouseEvent *>(ev);
      if (me->button() == Qt::LeftButton && !m_cropping) {
        emit openRequested();
        return true;
      }
    }
    return QWidget::eventFilter(obj, ev);
  }

  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    QTimer::singleShot(0, this, [this]() { refreshPreview(); });
  }

private:
  void beginCrop() {
    if (m_fullPm.isNull())
      refreshPreview();
    if (m_fullPm.isNull())
      return;
    m_cropping = true;
    QWidget *host = window();
    auto *overlay = new EmbedCropOverlay(host);
    overlay->setGeometry(host->rect());
    overlay->setSourcePixmap(m_fullPm);
    overlay->show();
    overlay->raise();
    overlay->setFocus(Qt::OtherFocusReason);
    connect(overlay, &EmbedCropOverlay::confirmed, this,
            [this, overlay](const QRectF &n) {
              m_embed.cropRect = n;
              m_cropping = false;
              overlay->close();
              refreshPreview();
              emit embedChanged(m_embed);
            });
    connect(overlay, &EmbedCropOverlay::cancelled, this,
            [this, overlay]() {
              m_cropping = false;
              overlay->close();
            });
  }

  QString m_strukturPath;
  StrukturEmbedBlock m_embed;
  QLabel *m_preview{nullptr};
  QLabel *m_caption{nullptr};
  QPixmap m_fullPm;
  bool m_cropping{false};
};

} // namespace struktur_detail

using struktur_detail::NotionTextEdit;
using struktur_detail::StrukturEmbedWidget;
using struktur_detail::notionMenuQss;

StrukturNoteEditor::StrukturNoteEditor(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("StrukturNoteEditor"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  auto *top = new QWidget(this);
  top->setObjectName(QStringLiteral("StrukturTopBar"));
  top->setAttribute(Qt::WA_StyledBackground, true);
  top->setStyleSheet(QStringLiteral(
      "QWidget#StrukturTopBar { background: #FFFFFF; border-bottom: 1px solid %1; }")
                         .arg(BlopStyle::paperBorder().name(QColor::HexRgb)));
  auto *topLay = new QHBoxLayout(top);
  topLay->setContentsMargins(UiScale::dp(20), UiScale::dp(10), UiScale::dp(16),
                             UiScale::dp(10));
  m_titleLabel = new QLabel(QStringLiteral("Struktur"), top);
  m_titleLabel->setObjectName(QStringLiteral("StrukturDocTitle"));
  m_titleLabel->setStyleSheet(QStringLiteral(
      "font-size: 15px; font-weight: 700; color: %1; background: transparent;")
                                  .arg(BlopStyle::paperInk().name(QColor::HexRgb)));
  topLay->addWidget(m_titleLabel, 1);
  auto *btnInsert = new QPushButton(QStringLiteral("+ Notiz einfügen"), top);
  btnInsert->setCursor(Qt::PointingHandCursor);
  btnInsert->setMinimumHeight(UiScale::dp(BlopStyle::touchTargetMinDp() - 8));
  btnInsert->setStyleSheet(BlopStyle::paperPrimaryButtonQss());
  connect(btnInsert, &QPushButton::clicked, this,
          &StrukturNoteEditor::showInsertMenu);
  topLay->addWidget(btnInsert);
  root->addWidget(top);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setStyleSheet(QStringLiteral(
      "QScrollArea { background: #FFFFFF; border: none; }"
      "QScrollArea > QWidget > QWidget { background: #FFFFFF; }"));

  // Seamless Notion column: same pure white as the canvas — no card chrome.
  auto *pageWrap = new QWidget;
  pageWrap->setObjectName(QStringLiteral("StrukturPageWrap"));
  pageWrap->setAttribute(Qt::WA_StyledBackground, true);
  pageWrap->setStyleSheet(QStringLiteral(
      "QWidget#StrukturPageWrap { background: #FFFFFF; }"));
  auto *wrapLay = new QHBoxLayout(pageWrap);
  wrapLay->setContentsMargins(UiScale::dp(16), UiScale::dp(20), UiScale::dp(16),
                              UiScale::dp(64));
  wrapLay->addStretch(1);

  m_host = new QWidget(pageWrap);
  m_host->setObjectName(QStringLiteral("StrukturHost"));
  m_host->setAttribute(Qt::WA_StyledBackground, true);
  m_host->setMaximumWidth(UiScale::dp(720));
  m_host->setMinimumWidth(UiScale::dp(320));
  m_host->setStyleSheet(QStringLiteral(
      "QWidget#StrukturHost {"
      "  background: #FFFFFF; border: none; border-radius: 0;"
      "}"));
  m_blocksLay = new QVBoxLayout(m_host);
  m_blocksLay->setContentsMargins(UiScale::dp(8), UiScale::dp(8),
                                  UiScale::dp(8), UiScale::dp(24));
  m_blocksLay->setSpacing(UiScale::dp(4));
  m_blocksLay->addStretch(1);
  wrapLay->addWidget(m_host, 6);
  wrapLay->addStretch(1);

  m_scroll->setWidget(pageWrap);
  root->addWidget(m_scroll, 1);

  m_saveTimer = new QTimer(this);
  m_saveTimer->setSingleShot(true);
  m_saveTimer->setInterval(500);
  connect(m_saveTimer, &QTimer::timeout, this, &StrukturNoteEditor::saveNow);
}

void StrukturNoteEditor::loadDocument(const QString &path) {
  m_path = path;
  m_loading = true;
  if (!StrukturDocument::load(path, m_doc))
    m_doc = StrukturDocument::createEmpty(QFileInfo(path).completeBaseName());
  if (m_titleLabel)
    m_titleLabel->setText(m_doc.title.isEmpty()
                              ? QFileInfo(path).completeBaseName()
                              : m_doc.title);
  rebuildUiFromDoc();
  m_loading = false;
}

void StrukturNoteEditor::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  refreshAllEmbeds();
}

void StrukturNoteEditor::refreshAllEmbeds() {
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    QLayoutItem *it = m_blocksLay->itemAt(i);
    if (!it || !it->widget())
      continue;
    if (auto *emb = qobject_cast<StrukturEmbedWidget *>(it->widget()))
      emb->refreshPreview();
  }
}

void StrukturNoteEditor::scheduleSave() {
  if (m_loading)
    return;
  emit documentModified();
  m_saveTimer->start();
}

void StrukturNoteEditor::harvestIntoDoc() {
  QVector<StrukturBlock> blocks;
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    QLayoutItem *it = m_blocksLay->itemAt(i);
    if (!it || !it->widget())
      continue;
    QWidget *w = it->widget();
    if (auto *edit = qobject_cast<NotionTextEdit *>(w)) {
      StrukturBlock b;
      b.type = StrukturBlock::Type::Paragraph;
      b.paragraph.text = edit->toPlainText();
      blocks.append(b);
    } else if (auto *emb = qobject_cast<StrukturEmbedWidget *>(w)) {
      StrukturBlock b;
      b.type = StrukturBlock::Type::Embed;
      b.embed = emb->embed();
      blocks.append(b);
    }
  }
  if (blocks.isEmpty()) {
    StrukturBlock b;
    b.type = StrukturBlock::Type::Paragraph;
    blocks.append(b);
  }
  m_doc.blocks = blocks;
}

void StrukturNoteEditor::saveNow() {
  if (m_path.isEmpty())
    return;
  harvestIntoDoc();
  StrukturDocument::save(m_doc, m_path);
}

void StrukturNoteEditor::rebuildUiFromDoc() {
  while (QLayoutItem *it = m_blocksLay->takeAt(0)) {
    if (it->widget())
      it->widget()->deleteLater();
    delete it;
  }

  for (int i = 0; i < m_doc.blocks.size(); ++i) {
    const StrukturBlock &b = m_doc.blocks[i];
    if (b.type == StrukturBlock::Type::Embed) {
      auto *emb = new StrukturEmbedWidget(m_path, b.embed, m_host);
      connect(emb, &StrukturEmbedWidget::openRequested, this, [this, emb]() {
        if (!onOpenEmbed)
          return;
        saveNow();
        const auto e = emb->embed();
        onOpenEmbed(StrukturDocument::absoluteNotePath(m_path, e.notePath),
                    e.pageIndex);
      });
      connect(emb, &StrukturEmbedWidget::embedChanged, this,
              [this](const StrukturEmbedBlock &) { scheduleSave(); });
      m_blocksLay->addWidget(emb);
    } else {
      auto *edit = new NotionTextEdit(m_host);
      edit->setPlainText(b.paragraph.text);
      edit->updateHeight();
      connect(edit, &QPlainTextEdit::textChanged, this, [this, edit]() {
        edit->updateHeight();
        scheduleSave();
      });
      m_blocksLay->addWidget(edit);
    }
  }
  m_blocksLay->addStretch(1);
}

void StrukturNoteEditor::insertEmbed(const StrukturEmbedBlock &embed,
                                     int /*afterIndex*/) {
  harvestIntoDoc();
  StrukturBlock b;
  b.type = StrukturBlock::Type::Embed;
  b.embed = embed;
  m_doc.blocks.append(b);
  StrukturBlock para;
  para.type = StrukturBlock::Type::Paragraph;
  m_doc.blocks.append(para);
  rebuildUiFromDoc();
  scheduleSave();
}

void StrukturNoteEditor::showInsertMenu() {
  QMenu menu(this);
  menu.setStyleSheet(notionMenuQss());
  QAction *aNew = menu.addAction(QStringLiteral("Neue A4-Notiz anlegen"));
  QAction *aExist = menu.addAction(QStringLiteral("Bestehende Notiz wählen…"));
  QAction *aPage =
      menu.addAction(QStringLiteral("Neue Seite in bestehender Notiz…"));
  QAction *chosen = menu.exec(QCursor::pos());
  if (!chosen)
    return;

  if (chosen == aNew) {
    bool ok = false;
    const QString title = QInputDialog::getText(
        this, QStringLiteral("Neue Notiz"), QStringLiteral("Titel:"),
        QLineEdit::Normal, QStringLiteral("Eingebettete Notiz"), &ok);
    if (!ok || !onCreateLinkedNote)
      return;
    const QString path = onCreateLinkedNote(
        title.trimmed().isEmpty() ? QStringLiteral("Eingebettete Notiz")
                                  : title.trimmed());
    if (path.isEmpty())
      return;
    StrukturEmbedBlock emb;
    emb.notePath = StrukturDocument::storeNotePath(m_path, path);
    emb.pageIndex = 0;
    insertEmbed(emb);
    return;
  }

  if (chosen == aExist || chosen == aPage) {
    const QString startDir = QFileInfo(m_path).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Notiz wählen"), startDir,
        QStringLiteral("Blop A4 (*.bnote)"));
    if (path.isEmpty())
      return;
    int pageIndex = 0;
    if (chosen == aPage) {
      if (!onAppendPage)
        return;
      pageIndex = onAppendPage(path);
      if (pageIndex < 0)
        return;
    }
    StrukturEmbedBlock emb;
    emb.notePath = StrukturDocument::storeNotePath(m_path, path);
    emb.pageIndex = pageIndex;
    insertEmbed(emb);
  }
}

#include "strukturnoteeditor.moc"
