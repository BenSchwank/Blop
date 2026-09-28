#include "strukturnoteeditor.h"

#include "blopstyle.h"
#include "blop_inwindow_menu.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "notemanager.h"
#include "notepagerenderer.h"
#include "uiscale.h"

#include <QCursor>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

/// Struktur page palette: light paper by default, follows BlopTheme dark mode.
namespace sp {
static bool dark() { return BlopTheme::instance().isDark(); }
static QString hex(const QColor &c) { return c.name(QColor::HexRgb); }
static QColor page() {
  return dark() ? QColor(0x1E, 0x1F, 0x24) : BlopStyle::paperBg();
}
static QColor ink() {
  return dark() ? QColor(0xE8, 0xEA, 0xEF) : BlopStyle::paperInk();
}
static QColor muted() {
  return dark() ? QColor(0x9A, 0xA1, 0xAE) : BlopStyle::paperInkMuted();
}
static QColor hover() {
  return dark() ? QColor(0x2C, 0x2E, 0x35) : BlopStyle::paperHover();
}
static QColor codeBg() {
  return dark() ? QColor(0x26, 0x28, 0x2E) : QColor(0xF8, 0xFA, 0xFC);
}
static QColor card() {
  return dark() ? QColor(0x25, 0x27, 0x2D) : QColor(0xFF, 0xFF, 0xFF);
}
static QColor previewBg() {
  return dark() ? QColor(0x1A, 0x1B, 0x1F) : QColor(0xF1, 0xF5, 0xF9);
}
static QString cardBorder() {
  return dark() ? QStringLiteral("rgba(255,255,255,0.07)")
                : QStringLiteral("rgba(15,23,42,0.06)");
}
static QString actionsBg() {
  return dark() ? QStringLiteral("rgba(37,39,45,0.96)")
                : QStringLiteral("rgba(255,255,255,0.94)");
}
static QColor checkBorder() {
  return dark() ? QColor(0x5A, 0x5F, 0x6B) : BlopStyle::paperBorder().darker(120);
}
static QColor selection() {
  if (!dark())
    return BlopStyle::paperPrimaryLight();
  QColor c = BlopTheme::accentPrimary();
  c.setAlpha(90);
  return c;
}
} // namespace sp

namespace struktur_detail {

static const QString kBodyHint = QStringLiteral(
    "Schreib einfach los  ·  Enter: neue Zeile  ·  Strg+Enter: neuer Block  ·  "
    "/: Befehle");

/// Notion-like text block: Enter = newline; Ctrl+Enter creates/splits blocks.
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
    setPlaceholderText(kBodyHint);
    document()->setDocumentMargin(UiScale::dp(2));
    QFont f = font();
    f.setPixelSize(UiScale::dp(16));
    setFont(f);
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit#StrukturParagraph {"
        "  background: transparent; color: %1; border: none;"
        "  padding: 6px 2px; selection-background-color: %2;"
        "}")
                      .arg(sp::hex(sp::ink()),
                           sp::selection().name(QColor::HexArgb)));
    connect(document(), &QTextDocument::contentsChanged, this,
            [this]() { updateHeight(); });
  }

  void updateHeight() {
    // Without an explicit text width, document()->size() stays one-line tall
    // and Enter looks like the text vanished.
    const int tw = qMax(40, viewport()->width());
    document()->setTextWidth(tw);
    QFontMetrics fm(font());
    const QString text = toPlainText();
    const int lineCount = qMax(1, text.count(QLatin1Char('\n')) + 1);
    const int byLines = lineCount * fm.lineSpacing() + UiScale::dp(20) +
                        int(document()->documentMargin() * 2);
    const int byDoc = int(document()->size().height()) + UiScale::dp(16);
    setFixedHeight(qMax(UiScale::dp(40), qMax(byLines, byDoc)));
  }

  QString blockKind() const { return m_kind; }

  /// Kinds: "" text, h1, h2, bullet, todo, todo_done, quote, code.
  void setBlockKind(const QString &kind) {
    m_kind = kind;
    QFont f = font();
    f.setFamily(QStringLiteral("Segoe UI"));
    f.setWeight(QFont::Normal);
    const QString ink = sp::hex(sp::ink());
    const QString muted = sp::hex(sp::muted());
    const QString sel = sp::selection().name(QColor::HexArgb);
    QString color = ink;
    QString background = QStringLiteral("transparent");
    QString padding = QStringLiteral("6px 2px");
    QString radius = QStringLiteral("0");
    int px = 16;
    if (kind == QLatin1String("code")) {
      f.setFamily(QStringLiteral("Consolas"));
      px = 14;
      background = sp::hex(sp::codeBg());
      padding = QStringLiteral("8px 10px");
      radius = QStringLiteral("8px");
      setPlaceholderText(
          QStringLiteral("Code  ·  Strg+Enter: Code-Block verlassen"));
    } else if (kind == QLatin1String("h1")) {
      px = 26;
      f.setWeight(QFont::Bold);
      padding = QStringLiteral("10px 2px 4px 2px");
      setPlaceholderText(QStringLiteral("Überschrift 1"));
    } else if (kind == QLatin1String("h2")) {
      px = 20;
      f.setWeight(QFont::DemiBold);
      padding = QStringLiteral("8px 2px 4px 2px");
      setPlaceholderText(QStringLiteral("Überschrift 2"));
    } else if (kind == QLatin1String("quote")) {
      color = muted;
      f.setItalic(true);
      setPlaceholderText(QStringLiteral("Zitat"));
    } else if (kind == QLatin1String("bullet")) {
      setPlaceholderText(
          QStringLiteral("Listenpunkt  ·  leere Zeile + Enter beendet die Liste"));
    } else if (kind == QLatin1String("todo") ||
               kind == QLatin1String("todo_done")) {
      if (kind == QLatin1String("todo_done"))
        color = muted;
      setPlaceholderText(
          QStringLiteral("To-Do  ·  leere Zeile + Enter beendet die Liste"));
    } else {
      setPlaceholderText(kBodyHint);
    }
    if (kind != QLatin1String("quote"))
      f.setItalic(false);
    f.setPixelSize(UiScale::dp(px));
    f.setStrikeOut(kind == QLatin1String("todo_done"));
    QColor hint = sp::muted();
    hint.setAlphaF(0.75);
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit#StrukturParagraph {"
        "  background: %1; color: %2; border: none; border-radius: %3;"
        "  padding: %4; selection-background-color: %5;"
        "  placeholder-text-color: %6;"
        "}")
                      .arg(background, color, radius, padding, sel,
                           hint.name(QColor::HexArgb)));
    QPalette pal = palette();
    pal.setColor(QPalette::PlaceholderText, hint);
    setPalette(pal);
    setFont(f);
    updateHeight();
  }

signals:
  void enterPressed(int cursorPos);
  void slashCommandRequested();
  void focusedChanged(bool on);
  void backspaceOnEmpty();
  /// -1 = previous block, +1 = next block. Emitted at the first/last line.
  void navigateBlock(int direction);
  /// Markdown prefix typed at the start of the block ("# ", "- ", ...).
  void kindShortcut(const QString &kind);

protected:
  void resizeEvent(QResizeEvent *e) override {
    QPlainTextEdit::resizeEvent(e);
    updateHeight();
  }

  void focusInEvent(QFocusEvent *e) override {
    QPlainTextEdit::focusInEvent(e);
    emit focusedChanged(true);
  }

  void focusOutEvent(QFocusEvent *e) override {
    QPlainTextEdit::focusOutEvent(e);
    emit focusedChanged(false);
  }

  void keyPressEvent(QKeyEvent *e) override {
    const bool plainKey = e->modifiers() == Qt::NoModifier;
    if (plainKey && e->key() == Qt::Key_Backspace && toPlainText().isEmpty()) {
      emit backspaceOnEmpty();
      e->accept();
      return;
    }
    if (plainKey && e->key() == Qt::Key_Up && textCursor().blockNumber() == 0) {
      emit navigateBlock(-1);
      e->accept();
      return;
    }
    if (plainKey && e->key() == Qt::Key_Down &&
        textCursor().blockNumber() >= document()->blockCount() - 1) {
      emit navigateBlock(1);
      e->accept();
      return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
      const bool ctrl = e->modifiers() & Qt::ControlModifier;
      const bool listKind = m_kind == QLatin1String("bullet") ||
                            m_kind == QLatin1String("todo") ||
                            m_kind == QLatin1String("todo_done");
      // Ctrl+Enter always starts a new block (also leaves code).
      if (ctrl) {
        emit enterPressed(textCursor().position());
        e->accept();
        return;
      }
      // Lists: Enter with text → next item; empty → end list.
      if (listKind) {
        emit enterPressed(textCursor().position());
        e->accept();
        return;
      }
      // Body / heading / quote / code: Enter = soft newline.
      QPlainTextEdit::keyPressEvent(e);
      return;
    }
    if (e->key() == Qt::Key_Slash && toPlainText().trimmed().isEmpty()) {
      emit slashCommandRequested();
      e->accept();
      return;
    }
    if (e->key() == Qt::Key_Space && m_kind.isEmpty()) {
      const QString kind = shortcutKindFor(toPlainText());
      if (!kind.isEmpty()) {
        clear();
        emit kindShortcut(kind);
        e->accept();
        return;
      }
    }
    QPlainTextEdit::keyPressEvent(e);
    if (m_kind.isEmpty() && toPlainText() == QLatin1String("```")) {
      clear();
      emit kindShortcut(QStringLiteral("code"));
    }
  }

private:
  static QString shortcutKindFor(const QString &text) {
    if (text == QLatin1String("#"))
      return QStringLiteral("h1");
    if (text == QLatin1String("##"))
      return QStringLiteral("h2");
    if (text == QLatin1String("-") || text == QLatin1String("*"))
      return QStringLiteral("bullet");
    if (text == QLatin1String("[]") || text == QLatin1String("[ ]"))
      return QStringLiteral("todo");
    if (text == QLatin1String(">"))
      return QStringLiteral("quote");
    return QString();
  }

  QString m_kind;
};

void fadeGutterButton(QWidget *w, bool show) {
  if (!w)
    return;
  auto *eff = qobject_cast<QGraphicsOpacityEffect *>(w->graphicsEffect());
  if (!eff) {
    eff = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(eff);
    eff->setOpacity(show ? 1.0 : 0.0);
  }
  const qreal target = show ? 1.0 : 0.0;
  w->setAttribute(Qt::WA_TransparentForMouseEvents, !show);
  const auto olds = w->findChildren<QPropertyAnimation *>(
      QStringLiteral("gutterFade"), Qt::FindDirectChildrenOnly);
  for (QPropertyAnimation *a : olds) {
    a->stop();
    a->deleteLater();
  }
  if (qAbs(eff->opacity() - target) < 0.01)
    return;
  auto *anim = new QPropertyAnimation(eff, "opacity", w);
  anim->setObjectName(QStringLiteral("gutterFade"));
  anim->setDuration(BlopMotion::kFast);
  anim->setEasingCurve(BlopMotion::kEaseStandard);
  anim->setStartValue(eff->opacity());
  anim->setEndValue(target);
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void fadeOverlay(QWidget *w, bool show) {
  if (!w)
    return;
  auto *eff = qobject_cast<QGraphicsOpacityEffect *>(w->graphicsEffect());
  if (!eff) {
    eff = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(eff);
    eff->setOpacity(0.0);
  }
  if (show) {
    w->setVisible(true);
    w->raise();
  }
  const auto olds = w->findChildren<QPropertyAnimation *>(
      QStringLiteral("overlayFade"), Qt::FindDirectChildrenOnly);
  for (QPropertyAnimation *a : olds) {
    a->stop();
    a->deleteLater();
  }
  auto *anim = new QPropertyAnimation(eff, "opacity", w);
  anim->setObjectName(QStringLiteral("overlayFade"));
  anim->setDuration(BlopMotion::kFast);
  anim->setEasingCurve(BlopMotion::kEaseStandard);
  anim->setStartValue(eff->opacity());
  anim->setEndValue(show ? 1.0 : 0.0);
  if (!show) {
    QObject::connect(anim, &QPropertyAnimation::finished, w, [w]() {
      auto *live = qobject_cast<QGraphicsOpacityEffect *>(w->graphicsEffect());
      if (live && live->opacity() < 0.05)
        w->hide();
    });
  }
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

class RowHoverWatch : public QObject {
public:
  RowHoverWatch(QWidget *row, QPushButton *btn, QObject *parent)
      : QObject(parent), m_row(row), m_btn(btn) {}

  void watch(QWidget *w) {
    if (w)
      w->installEventFilter(this);
  }

protected:
  bool eventFilter(QObject *, QEvent *e) override {
    if (e->type() == QEvent::Enter || e->type() == QEvent::Leave)
      QTimer::singleShot(0, this, [this]() { sync(); });
    return false;
  }

private:
  void sync() {
    if (!m_row || !m_btn)
      return;
    const bool inside =
        m_row->rect().contains(m_row->mapFromGlobal(QCursor::pos()));
    if (m_btn->property("rowHover").toBool() == inside)
      return;
    m_btn->setProperty("rowHover", inside);
    const bool on = inside || m_btn->property("editFocus").toBool();
    fadeGutterButton(m_btn, on);
  }

  QPointer<QWidget> m_row;
  QPointer<QPushButton> m_btn;
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
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    auto *card = new QFrame(this);
    m_card = card;
    card->setObjectName(QStringLiteral("StrukturEmbedCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    applyCardStyle(false);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(card);

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(UiScale::dp(12), UiScale::dp(10), UiScale::dp(12),
                            UiScale::dp(12));
    lay->setSpacing(UiScale::dp(8));

    m_caption = new QLabel(card);
    m_caption->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_caption->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px; font-weight: 600; letter-spacing: 0.2px;"
        " background: transparent; border: none; padding-left: %2px;")
                                 .arg(sp::hex(sp::muted()))
                                 .arg(UiScale::dp(22)));
    lay->addWidget(m_caption);

    m_preview = new QLabel(card);
    m_preview->setAlignment(Qt::AlignCenter);
    // Card size is owned by the grid; the pixmap must never push it larger.
    m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_preview->setMinimumSize(1, 1);
    m_preview->setCursor(Qt::PointingHandCursor);
    m_preview->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border: none; border-radius: %2px; }")
                                 .arg(sp::hex(sp::previewBg()),
                                      QString::number(UiScale::dp(10))));
    lay->addWidget(m_preview, 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(140);
    connect(m_refreshTimer, &QTimer::timeout, this,
            [this]() { refreshPreview(); });

    auto *bar = new QWidget(card);
    m_actions = bar;
    bar->setObjectName(QStringLiteral("StrukturEmbedActions"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setStyleSheet(QStringLiteral(
        "QWidget#StrukturEmbedActions {"
        "  background: %1; border: none;"
        "  border-radius: %2px;"
        "}")
                           .arg(sp::actionsBg())
                           .arg(UiScale::dp(10)));
    auto *barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(UiScale::dp(6), UiScale::dp(6), UiScale::dp(6),
                              UiScale::dp(6));
    barLay->setSpacing(UiScale::dp(4));

    auto makeGhost = [bar](const QString &text) {
      auto *b = new QPushButton(text, bar);
      b->setCursor(Qt::PointingHandCursor);
      b->setMinimumHeight(UiScale::dp(30));
      b->setStyleSheet(QStringLiteral(
          "QPushButton {"
          "  background: transparent; border: none; border-radius: 8px;"
          "  color: %1; font-size: 12px; font-weight: 600; padding: 4px 10px;"
          "}"
          "QPushButton:hover { background: %2; color: %3; }")
                           .arg(sp::hex(sp::ink()), sp::hex(sp::hover()),
                                sp::hex(sp::ink())));
      return b;
    };

    auto *btnOpen = makeGhost(QStringLiteral("Öffnen"));
    m_btnCrop = makeGhost(QStringLiteral("Zuschneiden"));
    m_btnPage = makeGhost(QStringLiteral("Ganzseite"));
    barLay->addWidget(btnOpen);
    barLay->addWidget(m_btnCrop);
    barLay->addWidget(m_btnPage);
    bar->hide();
    bar->raise();

    // Quick sizes (top-right): Viertel / Halb / Dreiviertel / Voll.
    auto *sizes = new QWidget(card);
    m_sizes = sizes;
    sizes->setObjectName(QStringLiteral("StrukturEmbedSizes"));
    sizes->setAttribute(Qt::WA_StyledBackground, true);
    sizes->setStyleSheet(QStringLiteral(
        "QWidget#StrukturEmbedSizes { background: %1; border: none;"
        "  border-radius: %2px; }")
                             .arg(sp::actionsBg())
                             .arg(UiScale::dp(9)));
    auto *sizesLay = new QHBoxLayout(sizes);
    sizesLay->setContentsMargins(UiScale::dp(4), UiScale::dp(4), UiScale::dp(4),
                                 UiScale::dp(4));
    sizesLay->setSpacing(UiScale::dp(2));
    const struct {
      const char *glyph;
      const char *tip;
      int span;
    } kSizes[] = {{"\u00BC", "Viertel", 1},
                  {"\u00BD", "Halb", 2},
                  {"\u00BE", "Dreiviertel", 3},
                  {"1", "Voll", 4}};
    for (const auto &s : kSizes) {
      auto *b = new QPushButton(QString::fromUtf8(s.glyph), sizes);
      b->setToolTip(QString::fromUtf8(s.tip));
      b->setCursor(Qt::PointingHandCursor);
      b->setFixedSize(UiScale::dp(28), UiScale::dp(26));
      b->setCheckable(true);
      b->setProperty("span", s.span);
      b->setStyleSheet(QStringLiteral(
          "QPushButton { background: transparent; border: none;"
          "  border-radius: 7px; color: %1; font-size: 13px; font-weight: 700; }"
          "QPushButton:hover { background: %2; }"
          "QPushButton:checked { background: %3; color: #FFFFFF; }")
                           .arg(sp::hex(sp::ink()), sp::hex(sp::hover()),
                                sp::hex(BlopTheme::accentPrimary())));
      connect(b, &QPushButton::clicked, this,
              [this, span = s.span]() { emit spanRequested(span); });
      sizesLay->addWidget(b);
      m_sizeButtons.append(b);
    }
    sizes->hide();

    installEventFilter(this);
    card->installEventFilter(this);
    m_preview->installEventFilter(this);
    m_caption->installEventFilter(this);
    bar->installEventFilter(this);
    sizes->installEventFilter(this);

    connect(btnOpen, &QPushButton::clicked, this,
            [this]() { emit openRequested(); });
    connect(m_btnCrop, &QPushButton::clicked, this,
            &StrukturEmbedWidget::beginCrop);
    connect(m_btnPage, &QPushButton::clicked, this, [this]() {
      m_embed.cropRect = std::nullopt;
      refreshPreview();
      emit embedChanged(m_embed);
    });
  }

  StrukturEmbedBlock embed() const { return m_embed; }

  /// Highlights the quick-size chip matching the card's column span.
  void setActiveSpan(int span) {
    for (QPushButton *b : m_sizeButtons)
      b->setChecked(b->property("span").toInt() == span);
  }

  /// Accent frame for ~1.2s after returning from the opened A4 page.
  void flashHighlight() {
    if (!m_card)
      return;
    applyCardStyle(true);
    QPointer<StrukturEmbedWidget> self(this);
    QTimer::singleShot(1200, this, [self]() {
      if (self)
        self->applyCardStyle(false);
    });
  }

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
    // Render at the card's actual preview size (grid owns the geometry).
    const QSize area = m_preview->size();
    const QSize fit(qMax(UiScale::dp(60), area.width() - UiScale::dp(8)),
                    qMax(UiScale::dp(40), area.height() - UiScale::dp(8)));
    const QRectF crop = m_embed.cropRect ? *m_embed.cropRect : QRectF();
    const QImage img =
        NotePageRenderer::renderPreview(note.pages[idx], fit, crop);
    m_fullPm = QPixmap::fromImage(
        NotePageRenderer::renderFullPage(note.pages[idx]));
    m_preview->setPixmap(QPixmap::fromImage(img));
  }

  void schedulePreviewRefresh() {
    if (m_refreshTimer)
      m_refreshTimer->start();
  }

signals:
  void openRequested();
  void embedChanged(const StrukturEmbedBlock &embed);
  void spanRequested(int colSpan);

protected:
  bool eventFilter(QObject *obj, QEvent *ev) override {
    if (ev->type() == QEvent::Enter || ev->type() == QEvent::Leave)
      QTimer::singleShot(0, this, [this]() { syncActionHover(); });
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
    positionActions();
    schedulePreviewRefresh();
  }

private:
  void applyCardStyle(bool accent) {
    if (!m_card)
      return;
    const int rad = UiScale::dp(14);
    if (accent) {
      m_card->setStyleSheet(QStringLiteral(
          "QFrame#StrukturEmbedCard {"
          "  background: %1; border: 2px solid %2; border-radius: %3px;"
          "}")
                                .arg(sp::hex(sp::card()),
                                     sp::hex(BlopTheme::accentPrimary()),
                                     QString::number(rad)));
    } else {
      m_card->setStyleSheet(QStringLiteral(
          "QFrame#StrukturEmbedCard {"
          "  background: %1; border: 1px solid %2; border-radius: %3px;"
          "}")
                                .arg(sp::hex(sp::card()), sp::cardBorder(),
                                     QString::number(rad)));
    }
  }

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
    connect(overlay, &EmbedCropOverlay::cancelled, this, [this, overlay]() {
      m_cropping = false;
      overlay->close();
    });
  }

  void syncActionHover() {
    if (!m_card || !m_actions)
      return;
    const bool inside =
        m_card->rect().contains(m_card->mapFromGlobal(QCursor::pos()));
    if (inside == m_actionsShown)
      return;
    m_actionsShown = inside;
    if (inside)
      positionActions();
    fadeOverlay(m_actions, inside);
    if (m_sizes)
      fadeOverlay(m_sizes, inside);
  }

  void positionActions() {
    if (!m_actions || !m_card)
      return;
    const bool narrow = m_card->width() < UiScale::dp(300);
    if (m_btnCrop)
      m_btnCrop->setVisible(!narrow);
    if (m_btnPage)
      m_btnPage->setVisible(!narrow);
    m_actions->adjustSize();
    const int margin = UiScale::dp(10);
    const int maxW = qMax(UiScale::dp(40), m_card->width() - 2 * margin);
    const int w = qMin(m_actions->sizeHint().width(), maxW);
    const int h = m_actions->sizeHint().height();
    m_actions->setGeometry(m_card->width() - w - margin,
                           qMax(margin, m_card->height() - h - margin), w, h);
    m_actions->raise();
    if (m_sizes) {
      m_sizes->adjustSize();
      const QSize s = m_sizes->sizeHint();
      m_sizes->setGeometry(m_card->width() - s.width() - UiScale::dp(8),
                           UiScale::dp(6), s.width(), s.height());
      m_sizes->raise();
    }
  }

  QString m_strukturPath;
  StrukturEmbedBlock m_embed;
  QFrame *m_card{nullptr};
  QWidget *m_actions{nullptr};
  QWidget *m_sizes{nullptr};
  QList<QPushButton *> m_sizeButtons;
  QPushButton *m_btnCrop{nullptr};
  QPushButton *m_btnPage{nullptr};
  QTimer *m_refreshTimer{nullptr};
  QLabel *m_preview{nullptr};
  QLabel *m_caption{nullptr};
  QPixmap m_fullPm;
  bool m_cropping{false};
  bool m_actionsShown{false};
};

/// Hover handle on a grid card: 6-dot move grip or bottom-right resize corner.
class GridGrip : public QWidget {
  Q_OBJECT
public:
  enum class Kind { Move, Resize };
  GridGrip(Kind kind, QWidget *parent) : QWidget(parent), m_kind(kind) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(kind == Kind::Move ? Qt::OpenHandCursor : Qt::SizeFDiagCursor);
    setFixedSize(kind == Kind::Move ? QSize(UiScale::dp(22), UiScale::dp(26))
                                    : QSize(UiScale::dp(20), UiScale::dp(20)));
    setToolTip(kind == Kind::Move ? QStringLiteral("Ziehen zum Verschieben")
                                  : QStringLiteral("Ziehen für Größe"));
    hide();
  }

signals:
  void dragStarted(const QPoint &globalPos);
  void dragMoved(const QPoint &globalPos);
  void dragFinished(const QPoint &globalPos);

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink = m_hover || m_down ? sp::ink() : sp::muted();
    if (m_kind == Kind::Move) {
      QPainterPath bg;
      bg.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                        UiScale::dp(6), UiScale::dp(6));
      QColor fill = sp::card();
      if (m_hover || m_down)
        fill = sp::hover();
      p.fillPath(bg, fill);
      p.setPen(Qt::NoPen);
      p.setBrush(ink);
      const qreal unit = UiScale::dp(10) / 10.0;
      const qreal r = 1.6 * unit;
      const qreal cx = width() / 2.0;
      const qreal cy = height() / 2.0;
      const qreal dx = 3.5 * unit;
      const qreal dy = 5.0 * unit;
      for (int c = -1; c <= 1; c += 2)
        for (int row = -1; row <= 1; ++row)
          p.drawEllipse(QPointF(cx + c * dx, cy + row * dy), r, r);
    } else {
      QPen pen(ink, UiScale::dp(2), Qt::SolidLine, Qt::RoundCap);
      p.setPen(pen);
      const qreal w = width();
      const qreal h = height();
      const qreal pad = UiScale::dp(5);
      p.drawLine(QPointF(w - pad, h - pad - UiScale::dp(9)),
                 QPointF(w - pad - UiScale::dp(9), h - pad));
      p.drawLine(QPointF(w - pad, h - pad - UiScale::dp(4)),
                 QPointF(w - pad - UiScale::dp(4), h - pad));
    }
  }
  void enterEvent(QEnterEvent *) override {
    m_hover = true;
    update();
  }
  void leaveEvent(QEvent *) override {
    m_hover = false;
    update();
  }
  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() != Qt::LeftButton)
      return;
    m_down = true;
    if (m_kind == Kind::Move)
      setCursor(Qt::ClosedHandCursor);
    emit dragStarted(e->globalPosition().toPoint());
    e->accept();
  }
  void mouseMoveEvent(QMouseEvent *e) override {
    if (m_down)
      emit dragMoved(e->globalPosition().toPoint());
  }
  void mouseReleaseEvent(QMouseEvent *e) override {
    if (!m_down || e->button() != Qt::LeftButton)
      return;
    m_down = false;
    if (m_kind == Kind::Move)
      setCursor(Qt::OpenHandCursor);
    update();
    emit dragFinished(e->globalPosition().toPoint());
  }

private:
  Kind m_kind;
  bool m_hover{false};
  bool m_down{false};
};

/// Notion/Dashboard-style embed board: 4 columns across the page, cards snap
/// to cells, dragging pushes colliding cards down, the grid grows as needed.
class StrukturEmbedGrid : public QWidget {
  Q_OBJECT
public:
  explicit StrukturEmbedGrid(QWidget *parent = nullptr) : QWidget(parent) {
    setObjectName(QStringLiteral("StrukturEmbedGrid"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_ghost = new QWidget(this);
    m_ghost->setObjectName(QStringLiteral("StrukturGridGhost"));
    m_ghost->setAttribute(Qt::WA_StyledBackground, true);
    m_ghost->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QColor acc = BlopTheme::accentPrimary();
    QColor fill = acc;
    fill.setAlpha(46);
    m_ghost->setStyleSheet(
        QStringLiteral("QWidget#StrukturGridGhost { background: %1;"
                       "  border: 2px dashed %2; border-radius: %3px; }")
            .arg(fill.name(QColor::HexArgb), acc.name(QColor::HexRgb))
            .arg(UiScale::dp(14)));
    m_ghost->hide();

    m_moveGrip = new GridGrip(GridGrip::Kind::Move, this);
    m_sizeGrip = new GridGrip(GridGrip::Kind::Resize, this);
    connect(m_moveGrip, &GridGrip::dragStarted, this,
            [this](const QPoint &g) { beginInteraction(false, g); });
    connect(m_moveGrip, &GridGrip::dragMoved, this,
            [this](const QPoint &g) { updateInteraction(g); });
    connect(m_moveGrip, &GridGrip::dragFinished, this,
            [this](const QPoint &) { endInteraction(); });
    connect(m_sizeGrip, &GridGrip::dragStarted, this,
            [this](const QPoint &g) { beginInteraction(true, g); });
    connect(m_sizeGrip, &GridGrip::dragMoved, this,
            [this](const QPoint &g) { updateInteraction(g); });
    connect(m_sizeGrip, &GridGrip::dragFinished, this,
            [this](const QPoint &) { endInteraction(); });
    setMouseTracking(true);
  }

  static int gap() { return UiScale::dp(12); }
  static int rowHeight() { return UiScale::dp(150); }

  QList<StrukturEmbedWidget *> embeds() const {
    QList<StrukturEmbedWidget *> out;
    for (const Card &c : m_cards)
      out.append(c.widget);
    return out;
  }

  int count() const { return m_cards.size(); }

  QVector<StrukturGridItem> items() const {
    QVector<StrukturGridItem> out;
    for (const Card &c : m_cards) {
      StrukturGridItem it = c.item;
      if (c.widget)
        it.embed = c.widget->embed();
      out.append(it);
    }
    return out;
  }

  int indexOf(const StrukturEmbedWidget *w) const {
    for (int i = 0; i < m_cards.size(); ++i)
      if (m_cards[i].widget == w)
        return i;
    return -1;
  }

  StrukturEmbedWidget *embedAt(int index) const {
    return (index >= 0 && index < m_cards.size()) ? m_cards[index].widget
                                                  : nullptr;
  }

  /// Adds a card; the grid takes ownership of the widget.
  void addCard(StrukturEmbedWidget *w, const StrukturGridItem &item) {
    if (!w)
      return;
    w->setParent(this);
    Card c;
    c.widget = w;
    c.item = item;
    StrukturGrid::clampItem(c.item);
    m_cards.append(c);
    w->installEventFilter(this);
    connect(w, &StrukturEmbedWidget::spanRequested, this,
            [this, w](int span) { setSpan(indexOf(w), span); });
    w->setActiveSpan(c.item.colSpan);
    w->show();
    QVector<StrukturGridItem> its = currentItems();
    StrukturGrid::resolve(its, m_cards.size() - 1);
    applyItems(its, false);
    m_moveGrip->raise();
    m_sizeGrip->raise();
  }

  /// First free cell for a new card of `colSpan` (fills holes before growing).
  StrukturGridItem suggestSlot(int colSpan, int rowSpan) const {
    StrukturGridItem probe;
    probe.colSpan = colSpan;
    probe.rowSpan = rowSpan;
    const QVector<StrukturGridItem> its = currentItems();
    const int rows = StrukturGrid::rowCount(its);
    for (int r = 0; r <= rows; ++r) {
      for (int c = 0; c + colSpan <= StrukturGrid::kColumns; ++c) {
        probe.row = r;
        probe.col = c;
        bool free = true;
        for (const StrukturGridItem &o : its)
          if (StrukturGrid::overlaps(probe, o)) {
            free = false;
            break;
          }
        if (free)
          return probe;
      }
    }
    probe.row = rows;
    probe.col = 0;
    return probe;
  }

  void flash(int index) {
    if (auto *w = embedAt(index))
      w->flashHighlight();
  }

  void refreshPreviews() {
    for (const Card &c : m_cards)
      if (c.widget)
        c.widget->refreshPreview();
  }

signals:
  void layoutChanged();

protected:
  void resizeEvent(QResizeEvent *e) override {
    QWidget::resizeEvent(e);
    if (!m_active)
      applyItems(currentItems(), false);
  }

  void paintEvent(QPaintEvent *e) override {
    QWidget::paintEvent(e);
    if (!m_active)
      return;
    // Dotted cell raster while moving/resizing.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor dot = sp::muted();
    dot.setAlpha(110);
    QPen pen(dot, 1, Qt::DotLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const int rows = qMax(1, (height() + gap()) / (rowHeight() + gap()));
    for (int r = 0; r < rows; ++r)
      for (int c = 0; c < StrukturGrid::kColumns; ++c) {
        StrukturGridItem cell;
        cell.col = c;
        cell.row = r;
        cell.colSpan = 1;
        cell.rowSpan = 1;
        p.drawRoundedRect(QRectF(cellRect(cell)).adjusted(0.5, 0.5, -0.5, -0.5),
                          UiScale::dp(10), UiScale::dp(10));
      }
  }

  void leaveEvent(QEvent *e) override {
    QWidget::leaveEvent(e);
    if (!m_active)
      QTimer::singleShot(0, this, [this]() { syncHover(); });
  }

  bool eventFilter(QObject *obj, QEvent *ev) override {
    if (ev->type() == QEvent::Enter || ev->type() == QEvent::Leave) {
      if (qobject_cast<StrukturEmbedWidget *>(obj) && !m_active)
        QTimer::singleShot(0, this, [this]() { syncHover(); });
    }
    return QWidget::eventFilter(obj, ev);
  }

private:
  struct Card {
    StrukturEmbedWidget *widget{nullptr};
    StrukturGridItem item;
  };

  int columnWidth() const {
    return qMax(UiScale::dp(40),
                (width() - (StrukturGrid::kColumns - 1) * gap()) /
                    StrukturGrid::kColumns);
  }

  QRect cellRect(const StrukturGridItem &it) const {
    const int cw = columnWidth();
    const int x = it.col * (cw + gap());
    const int y = it.row * (rowHeight() + gap());
    int w = it.colSpan * cw + (it.colSpan - 1) * gap();
    if (it.col + it.colSpan >= StrukturGrid::kColumns)
      w = width() - x; // absorb rounding at the right edge
    const int h = it.rowSpan * rowHeight() + (it.rowSpan - 1) * gap();
    return QRect(x, y, qMax(1, w), h);
  }

  QVector<StrukturGridItem> currentItems() const {
    QVector<StrukturGridItem> out;
    for (const Card &c : m_cards)
      out.append(c.item);
    return out;
  }

  int heightForRows(int rows) const {
    return rows <= 0 ? 0 : rows * rowHeight() + (rows - 1) * gap();
  }

  /// Moves every card (except the one under the pointer) to `its`.
  void applyItems(const QVector<StrukturGridItem> &its, bool animate,
                  int skip = -1) {
    for (int i = 0; i < m_cards.size() && i < its.size(); ++i) {
      m_cards[i].item = its[i];
      StrukturEmbedWidget *w = m_cards[i].widget;
      if (!w || i == skip)
        continue;
      const QRect target = cellRect(its[i]);
      QPropertyAnimation *running = nullptr;
      const auto anims = w->findChildren<QPropertyAnimation *>(
          QStringLiteral("gridMove"), Qt::FindDirectChildrenOnly);
      for (QPropertyAnimation *a : anims)
        if (a->state() == QAbstractAnimation::Running)
          running = a;
      if (running && running->endValue().toRect() == target && animate)
        continue;
      if (running) {
        running->setObjectName(QString());
        running->stop();
      }
      if (!animate || !isVisible()) {
        w->setGeometry(target);
        continue;
      }
      if (w->geometry() == target)
        continue;
      auto *anim = new QPropertyAnimation(w, "geometry", w);
      anim->setObjectName(QStringLiteral("gridMove"));
      anim->setDuration(BlopMotion::kFast);
      anim->setEasingCurve(BlopMotion::kEaseStandard);
      anim->setStartValue(w->geometry());
      anim->setEndValue(target);
      anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    int rows = StrukturGrid::rowCount(its);
    if (m_active)
      rows += 1; // room to drop below the last row
    setFixedHeight(qMax(heightForRows(1), heightForRows(rows)));
  }

  void syncHover() {
    if (m_active)
      return;
    const QPoint local = mapFromGlobal(QCursor::pos());
    int hit = -1;
    if (rect().contains(local)) {
      for (int i = 0; i < m_cards.size(); ++i)
        if (m_cards[i].widget &&
            m_cards[i].widget->geometry().adjusted(-4, -4, 4, 4).contains(local)) {
          hit = i;
          break;
        }
    }
    m_hover = hit;
    if (hit < 0) {
      m_moveGrip->hide();
      m_sizeGrip->hide();
      return;
    }
    const QRect r = m_cards[hit].widget->geometry();
    m_moveGrip->move(r.left() + UiScale::dp(8), r.top() + UiScale::dp(7));
    m_sizeGrip->move(r.right() - m_sizeGrip->width() - UiScale::dp(2),
                     r.bottom() - m_sizeGrip->height() - UiScale::dp(2));
    m_moveGrip->show();
    m_sizeGrip->show();
    m_moveGrip->raise();
    m_sizeGrip->raise();
  }

  QScrollArea *scrollArea() const {
    for (QWidget *p = parentWidget(); p; p = p->parentWidget())
      if (auto *sa = qobject_cast<QScrollArea *>(p))
        return sa;
    return nullptr;
  }

  void autoScroll(const QPoint &globalPos) {
    QScrollArea *sa = scrollArea();
    if (!sa)
      return;
    const QPoint vp = sa->viewport()->mapFromGlobal(globalPos);
    const int edge = UiScale::dp(48);
    QScrollBar *bar = sa->verticalScrollBar();
    if (vp.y() < edge)
      bar->setValue(bar->value() - UiScale::dp(14));
    else if (vp.y() > sa->viewport()->height() - edge)
      bar->setValue(bar->value() + UiScale::dp(14));
  }

  void beginInteraction(bool resize, const QPoint &globalPos) {
    if (m_hover < 0 || m_hover >= m_cards.size())
      return;
    m_active = true;
    m_resizing = resize;
    m_dragIndex = m_hover;
    StrukturEmbedWidget *w = m_cards[m_dragIndex].widget;
    m_pressOffset = w->mapFromGlobal(globalPos);
    m_preview = currentItems();
    w->raise();
    m_ghost->setGeometry(cellRect(m_cards[m_dragIndex].item));
    m_ghost->show();
    m_ghost->stackUnder(w);
    (m_resizing ? m_moveGrip : m_sizeGrip)->hide();
    applyItems(m_preview, false, m_dragIndex);
    update();
  }

  void updateInteraction(const QPoint &globalPos) {
    if (!m_active || m_dragIndex < 0)
      return;
    autoScroll(globalPos);
    StrukturEmbedWidget *w = m_cards[m_dragIndex].widget;
    const QPoint local = mapFromGlobal(globalPos);
    const int cw = columnWidth();
    const int stepX = cw + gap();
    const int stepY = rowHeight() + gap();
    QVector<StrukturGridItem> its = currentItemsFrom(m_preview);
    StrukturGridItem &it = its[m_dragIndex];

    if (!m_resizing) {
      QPoint topLeft = local - m_pressOffset;
      topLeft.setX(qBound(0, topLeft.x(), qMax(0, width() - w->width())));
      topLeft.setY(qMax(0, topLeft.y()));
      w->move(topLeft);
      it.col = qBound(0, qRound(double(topLeft.x()) / stepX),
                      StrukturGrid::kColumns - it.colSpan);
      it.row = qMax(0, qRound(double(topLeft.y()) / stepY));
      m_moveGrip->move(topLeft + QPoint(UiScale::dp(8), UiScale::dp(7)));
    } else {
      const QRect base = cellRect(it);
      const int freeW = qBound(cw / 2, local.x() - base.left(),
                               width() - base.left());
      const int freeH = qBound(rowHeight() / 2, local.y() - base.top(),
                               heightForRows(StrukturGrid::kMaxRowSpan));
      w->resize(freeW, freeH);
      it.colSpan = qBound(1, qRound(double(freeW + gap()) / stepX),
                          StrukturGrid::kColumns - it.col);
      it.rowSpan = qBound(1, qRound(double(freeH + gap()) / stepY),
                          StrukturGrid::kMaxRowSpan);
      m_sizeGrip->move(base.left() + freeW - m_sizeGrip->width() - UiScale::dp(2),
                       base.top() + freeH - m_sizeGrip->height() - UiScale::dp(2));
    }
    StrukturGrid::resolve(its, m_dragIndex);
    m_pending = its;
    const QRect ghost = cellRect(its[m_dragIndex]);
    if (m_ghost->geometry() != ghost)
      m_ghost->setGeometry(ghost);
    applyItems(its, true, m_dragIndex);
    // Keep the grid tall enough for the card following the pointer.
    const int needed = w->geometry().bottom() + gap();
    if (needed > height())
      setFixedHeight(needed);
    update();
  }

  QVector<StrukturGridItem> currentItemsFrom(
      const QVector<StrukturGridItem> &base) const {
    QVector<StrukturGridItem> its = base;
    if (its.size() != m_cards.size())
      its = currentItems();
    return its;
  }

  void endInteraction() {
    if (!m_active)
      return;
    const int idx = m_dragIndex;
    const QVector<StrukturGridItem> finalItems =
        m_pending.size() == m_cards.size() ? m_pending : currentItems();
    m_active = false;
    m_resizing = false;
    m_dragIndex = -1;
    m_pending.clear();
    m_preview.clear();
    m_ghost->hide();
    applyItems(finalItems, true);
    if (StrukturEmbedWidget *w = embedAt(idx))
      w->setActiveSpan(m_cards[idx].item.colSpan);
    update();
    QTimer::singleShot(BlopMotion::kFast + 20, this, [this]() { syncHover(); });
    emit layoutChanged();
  }

  void setSpan(int index, int span) {
    if (index < 0 || index >= m_cards.size())
      return;
    QVector<StrukturGridItem> its = currentItems();
    StrukturGridItem &it = its[index];
    it.colSpan = qBound(1, span, StrukturGrid::kColumns);
    if (it.col + it.colSpan > StrukturGrid::kColumns)
      it.col = StrukturGrid::kColumns - it.colSpan;
    StrukturGrid::resolve(its, index);
    applyItems(its, true);
    if (StrukturEmbedWidget *w = embedAt(index))
      w->setActiveSpan(its[index].colSpan);
    QTimer::singleShot(BlopMotion::kFast + 20, this, [this]() { syncHover(); });
    emit layoutChanged();
  }

  QVector<Card> m_cards;
  QWidget *m_ghost{nullptr};
  GridGrip *m_moveGrip{nullptr};
  GridGrip *m_sizeGrip{nullptr};
  int m_hover{-1};
  bool m_active{false};
  bool m_resizing{false};
  int m_dragIndex{-1};
  QPoint m_pressOffset;
  QVector<StrukturGridItem> m_preview;
  QVector<StrukturGridItem> m_pending;
};

} // namespace struktur_detail

using struktur_detail::NotionTextEdit;
using struktur_detail::RowHoverWatch;
using struktur_detail::StrukturEmbedGrid;
using struktur_detail::StrukturEmbedWidget;
using struktur_detail::fadeGutterButton;

static NotionTextEdit *paragraphEdit(QWidget *row) {
  if (!row)
    return nullptr;
  if (auto *edit = qobject_cast<NotionTextEdit *>(row))
    return edit;
  return row->findChild<NotionTextEdit *>(QStringLiteral("StrukturParagraph"),
                                          Qt::FindDirectChildrenOnly);
}

static void focusEdit(NotionTextEdit *edit, bool cursorAtEnd) {
  if (!edit)
    return;
  edit->setFocus(Qt::OtherFocusReason);
  QTextCursor c = edit->textCursor();
  c.movePosition(cursorAtEnd ? QTextCursor::End : QTextCursor::Start);
  edit->setTextCursor(c);
}

StrukturNoteEditor::StrukturNoteEditor(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("StrukturNoteEditor"));
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);

  auto *pageWrap = new QWidget;
  pageWrap->setObjectName(QStringLiteral("StrukturPageWrap"));
  pageWrap->setAttribute(Qt::WA_StyledBackground, true);
  auto *wrapLay = new QHBoxLayout(pageWrap);
  // Full-width page: generous side margins instead of a narrow center column.
  wrapLay->setContentsMargins(UiScale::dp(64), UiScale::dp(40), UiScale::dp(64),
                              UiScale::dp(120));

  m_host = new QWidget(pageWrap);
  m_host->setObjectName(QStringLiteral("StrukturHost"));
  m_host->setAttribute(Qt::WA_StyledBackground, true);
  auto *column = new QVBoxLayout(m_host);
  column->setContentsMargins(0, 0, 0, UiScale::dp(24));
  column->setSpacing(UiScale::dp(2));

  const int titlePadLeft = UiScale::dp(28) + UiScale::dp(4) + UiScale::dp(2);
  m_titleEdit = new QLineEdit(m_host);
  m_titleEdit->setObjectName(QStringLiteral("StrukturDocTitle"));
  m_titleEdit->setFrame(false);
  m_titleEdit->setPlaceholderText(QStringLiteral("Ohne Titel"));
  QFont titleFont = m_titleEdit->font();
  titleFont.setPixelSize(UiScale::dp(30));
  titleFont.setWeight(QFont::Bold);
  m_titleEdit->setFont(titleFont);
  m_titleEdit->setMinimumHeight(UiScale::dp(48));
  m_titleEdit->setProperty("titlePadLeft", titlePadLeft);
  column->addWidget(m_titleEdit);
  connect(m_titleEdit, &QLineEdit::textChanged, this,
          [this](const QString &) { scheduleSave(); });
  connect(m_titleEdit, &QLineEdit::returnPressed, this,
          [this]() { focusFirstParagraph(); });

  auto *blocksHost = new QWidget(m_host);
  blocksHost->setObjectName(QStringLiteral("StrukturBlocks"));
  blocksHost->setAttribute(Qt::WA_StyledBackground, true);
  m_blocksLay = new QVBoxLayout(blocksHost);
  m_blocksLay->setContentsMargins(0, UiScale::dp(8), 0, 0);
  m_blocksLay->setSpacing(UiScale::dp(2));
  m_blocksLay->addStretch(1);
  column->addWidget(blocksHost);
  wrapLay->addWidget(m_host, 1);

  m_scroll->setWidget(pageWrap);
  root->addWidget(m_scroll, 1);

  m_saveTimer = new QTimer(this);
  m_saveTimer->setSingleShot(true);
  m_saveTimer->setInterval(500);
  connect(m_saveTimer, &QTimer::timeout, this, &StrukturNoteEditor::saveNow);

  applyThemeStyles();
  connect(&BlopTheme::instance(), &BlopTheme::themeChanged, this, [this]() {
    applyThemeStyles();
    if (m_path.isEmpty())
      return;
    // Block widgets bake colors into their QSS — rebuild from the saved doc.
    saveNow();
    const bool wasLoading = m_loading;
    m_loading = true;
    rebuildUiFromDoc();
    m_loading = wasLoading;
  });
}

void StrukturNoteEditor::applyThemeStyles() {
  const QString page = sp::hex(sp::page());
  setStyleSheet(QStringLiteral("QWidget#StrukturNoteEditor { background: %1; }")
                    .arg(page));
  if (m_scroll) {
    m_scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
        "QScrollArea > QWidget > QWidget { background: %1; }")
                                .arg(page));
  }
  if (auto *wrap = findChild<QWidget *>(QStringLiteral("StrukturPageWrap")))
    wrap->setStyleSheet(
        QStringLiteral("QWidget#StrukturPageWrap { background: %1; }").arg(page));
  if (m_host)
    m_host->setStyleSheet(QStringLiteral(
        "QWidget#StrukturHost { background: %1; border: none; }").arg(page));
  if (auto *blocks = findChild<QWidget *>(QStringLiteral("StrukturBlocks")))
    blocks->setStyleSheet(QStringLiteral(
        "QWidget#StrukturBlocks, QWidget#StrukturParagraphRow,"
        "QWidget#StrukturKindGutter, QWidget#StrukturEmbed,"
        "QWidget#StrukturEmbedGrid { background: %1; }")
                              .arg(page));
  if (m_titleEdit) {
    m_titleEdit->setStyleSheet(
        QStringLiteral("QLineEdit#StrukturDocTitle {"
                       "  background: transparent; border: none; color: %1;"
                       "  padding: 2px 2px 10px %2px;"
                       "  selection-background-color: %3;"
                       "}"
                       "QLineEdit#StrukturDocTitle:focus { border: none; }")
            .arg(sp::hex(sp::ink()),
                 QString::number(m_titleEdit->property("titlePadLeft").toInt()),
                 sp::selection().name(QColor::HexArgb)));
  }
}

QString StrukturNoteEditor::displayTitle() const {
  const QString t = m_titleEdit ? m_titleEdit->text().trimmed() : QString();
  if (!t.isEmpty())
    return t;
  return QFileInfo(m_path).completeBaseName();
}

void StrukturNoteEditor::loadDocument(const QString &path) {
  m_path = path;
  m_loading = true;
  if (!StrukturDocument::load(path, m_doc))
    m_doc = StrukturDocument::createEmpty(QFileInfo(path).completeBaseName());
  if (m_titleEdit) {
    m_titleEdit->blockSignals(true);
    m_titleEdit->setText(m_doc.title.isEmpty()
                             ? QFileInfo(path).completeBaseName()
                             : m_doc.title);
    m_titleEdit->blockSignals(false);
  }
  rebuildUiFromDoc();
  m_loading = false;
}

void StrukturNoteEditor::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  refreshAllEmbeds();
}

void StrukturNoteEditor::revealBlock(int blockIndex) {
  QWidget *w = blockWidgetAt(blockIndex);
  if (!w || !m_scroll)
    return;
  QPointer<QWidget> target = w;
  const int item = (blockIndex == m_lastOpenedEmbed) ? m_lastOpenedGridItem : -1;
  QTimer::singleShot(0, this, [this, target, item]() {
    if (!target || !m_scroll)
      return;
    auto *grid = qobject_cast<StrukturEmbedGrid *>(target.data());
    if (!grid) {
      m_scroll->ensureWidgetVisible(target, 0, UiScale::dp(48));
      return;
    }
    if (StrukturEmbedWidget *card = grid->embedAt(item)) {
      m_scroll->ensureWidgetVisible(card, 0, UiScale::dp(48));
      grid->flash(item);
      return;
    }
    m_scroll->ensureWidgetVisible(grid, 0, UiScale::dp(48));
    for (int i = 0; i < grid->count(); ++i)
      grid->flash(i);
  });
}

void StrukturNoteEditor::revealLastOpenedEmbed() {
  if (m_lastOpenedEmbed >= 0)
    revealBlock(m_lastOpenedEmbed);
}

void StrukturNoteEditor::refreshAllEmbeds() {
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    QLayoutItem *it = m_blocksLay->itemAt(i);
    if (!it || !it->widget())
      continue;
    if (auto *grid = qobject_cast<StrukturEmbedGrid *>(it->widget()))
      grid->refreshPreviews();
  }
}

void StrukturNoteEditor::scheduleSave() {
  if (m_loading)
    return;
  emit documentModified();
  m_saveTimer->start();
}

void StrukturNoteEditor::harvestIntoDoc() {
  if (m_titleEdit)
    m_doc.title = m_titleEdit->text();
  QVector<StrukturBlock> blocks;
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    QLayoutItem *it = m_blocksLay->itemAt(i);
    if (!it || !it->widget())
      continue;
    QWidget *w = it->widget();
    if (auto *grid = qobject_cast<StrukturEmbedGrid *>(w)) {
      StrukturBlock b;
      b.type = StrukturBlock::Type::EmbedGrid;
      b.items = grid->items();
      if (!b.items.isEmpty())
        blocks.append(b);
      continue;
    }
    if (NotionTextEdit *edit = paragraphEdit(w)) {
      StrukturBlock b;
      b.type = StrukturBlock::Type::Paragraph;
      b.paragraph.text = edit->toPlainText();
      b.paragraph.kind = w->property("blockKind").toString();
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

int StrukturNoteEditor::blockIndexOfWidget(QWidget *w) const {
  if (!w || !m_blocksLay)
    return -1;
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    QLayoutItem *it = m_blocksLay->itemAt(i);
    if (it && it->widget() == w)
      return i;
  }
  return -1;
}

QWidget *StrukturNoteEditor::blockWidgetAt(int index) const {
  if (!m_blocksLay || index < 0 || index >= m_blocksLay->count())
    return nullptr;
  QLayoutItem *it = m_blocksLay->itemAt(index);
  return it ? it->widget() : nullptr;
}

int StrukturNoteEditor::insertIndex(int desired) const {
  if (!m_blocksLay)
    return 0;
  int stretchAt = m_blocksLay->count();
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    QLayoutItem *it = m_blocksLay->itemAt(i);
    if (it && it->spacerItem() && !it->widget()) {
      stretchAt = i;
      break;
    }
  }
  if (desired < 0)
    desired = stretchAt;
  return qBound(0, desired, stretchAt);
}

void StrukturNoteEditor::wireParagraphRow(QWidget *row, QPlainTextEdit *plain) {
  auto *edit = static_cast<NotionTextEdit *>(plain);
  connect(edit, &QPlainTextEdit::textChanged, this, [this, edit]() {
    edit->updateHeight();
    scheduleSave();
  });
  connect(edit, &NotionTextEdit::enterPressed, this,
          [this, row, edit](int cursorPos) {
            const QString full = edit->toPlainText();
            const QString kind = edit->blockKind();
            const bool listKind = kind == QLatin1String("bullet") ||
                                  kind == QLatin1String("todo") ||
                                  kind == QLatin1String("todo_done");
            // Enter on an empty list item ends the list (Notion).
            if (listKind && full.trimmed().isEmpty()) {
              applyRowKind(row, QString());
              return;
            }
            const int pos = qBound(0, cursorPos, full.size());
            const QString before = full.left(pos);
            const QString after = full.mid(pos);
            edit->setPlainText(before);
            edit->updateHeight();
            const int idx = blockIndexOfWidget(row);
            QString nextKind;
            if (kind == QLatin1String("bullet"))
              nextKind = kind;
            else if (listKind)
              nextKind = QStringLiteral("todo");
            insertParagraphAfter(idx, after, nextKind);
          });
  connect(edit, &NotionTextEdit::backspaceOnEmpty, this, [this, row, edit]() {
    // First Backspace strips the block style, the second removes the row.
    if (!edit->blockKind().isEmpty()) {
      applyRowKind(row, QString());
      return;
    }
    removeEmptyParagraph(row);
  });
  connect(edit, &NotionTextEdit::kindShortcut, this,
          [this, row](const QString &kind) { applyRowKind(row, kind); });
  connect(edit, &NotionTextEdit::navigateBlock, this,
          [this, row](int direction) { focusSiblingParagraph(row, direction); });
  connect(edit, &NotionTextEdit::slashCommandRequested, this, [this, row]() {
    showInsertMenuAt(blockIndexOfWidget(row), QCursor::pos());
  });
  if (auto *btn =
          row->findChild<QPushButton *>(QStringLiteral("StrukturLinePlus"))) {
    connect(btn, &QPushButton::clicked, this, [this, row, btn]() {
      showInsertMenuAt(blockIndexOfWidget(row),
                       btn->mapToGlobal(QPoint(0, btn->height())));
    });
    connect(edit, &NotionTextEdit::focusedChanged, btn, [btn](bool on) {
      btn->setProperty("editFocus", on);
      fadeGutterButton(btn, on || btn->property("rowHover").toBool());
    });
  }
}

QWidget *StrukturNoteEditor::makeParagraphRow(const QString &text,
                                             const QString &kind) {
  QWidget *parent =
      m_blocksLay && m_blocksLay->parentWidget() ? m_blocksLay->parentWidget()
                                                 : m_host;
  auto *row = new QWidget(parent);
  row->setObjectName(QStringLiteral("StrukturParagraphRow"));
  row->setProperty("blockKind", kind);
  auto *lay = new QHBoxLayout(row);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(UiScale::dp(4));

  auto *btnPlus = new QPushButton(QStringLiteral("+"), row);
  btnPlus->setObjectName(QStringLiteral("StrukturLinePlus"));
  btnPlus->setFixedSize(UiScale::dp(28), UiScale::dp(28));
  btnPlus->setCursor(Qt::PointingHandCursor);
  btnPlus->setFocusPolicy(Qt::NoFocus);
  btnPlus->setToolTip(QStringLiteral("Einfügen"));
  btnPlus->setProperty("editFocus", false);
  btnPlus->setProperty("rowHover", false);
  btnPlus->setStyleSheet(QStringLiteral(
      "QPushButton#StrukturLinePlus {"
      "  background: transparent; border: none; border-radius: 6px;"
      "  color: %1; font-size: 18px; font-weight: 600;"
      "}"
      "QPushButton#StrukturLinePlus:hover { background: %2; color: %3; }")
                             .arg(sp::hex(sp::muted()), sp::hex(sp::hover()),
                                  sp::hex(sp::ink())));
  auto *eff = new QGraphicsOpacityEffect(btnPlus);
  eff->setOpacity(0);
  btnPlus->setGraphicsEffect(eff);
  btnPlus->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  lay->addWidget(btnPlus, 0, Qt::AlignTop);

  // Kind gutter (bullet, checkbox, quote bar); rebuilt by applyRowKind.
  auto *gutter = new QWidget(row);
  gutter->setObjectName(QStringLiteral("StrukturKindGutter"));
  gutter->setFixedWidth(0);
  gutter->hide();
  lay->addWidget(gutter, 0);

  auto *edit = new NotionTextEdit(row);
  QString initial = text;
  // Older documents stored the checkbox glyph inside the text.
  if ((kind == QLatin1String("todo") || kind == QLatin1String("todo_done")) &&
      initial.startsWith(QStringLiteral("☐ ")))
    initial.remove(0, 2);
  edit->setPlainText(initial);
  QTimer::singleShot(0, edit, [edit]() { edit->updateHeight(); });
  lay->addWidget(edit, 1);

  auto *hover = new RowHoverWatch(row, btnPlus, row);
  hover->watch(row);
  hover->watch(btnPlus);
  hover->watch(edit);

  wireParagraphRow(row, edit);
  applyRowKind(row, kind);
  return row;
}

void StrukturNoteEditor::applyRowKind(QWidget *row, const QString &kind) {
  if (!row)
    return;
  NotionTextEdit *edit = paragraphEdit(row);
  auto *gutter =
      row->findChild<QWidget *>(QStringLiteral("StrukturKindGutter"),
                                Qt::FindDirectChildrenOnly);
  if (!edit || !gutter)
    return;
  row->setProperty("blockKind", kind);
  edit->setBlockKind(kind);

  const auto old = gutter->findChildren<QWidget *>(QString(),
                                                   Qt::FindDirectChildrenOnly);
  for (QWidget *w : old)
    w->deleteLater();
  delete gutter->layout();

  auto *glay = new QHBoxLayout(gutter);
  glay->setContentsMargins(0, 0, 0, 0);
  glay->setSpacing(0);
  const QString ink = sp::hex(sp::ink());

  if (kind == QLatin1String("bullet")) {
    auto *dot = new QLabel(QStringLiteral("•"), gutter);
    dot->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    dot->setFixedWidth(UiScale::dp(22));
    dot->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 20px; background: transparent; padding-top: 4px;")
                           .arg(ink));
    glay->addWidget(dot, 0, Qt::AlignTop);
    gutter->setFixedWidth(UiScale::dp(22));
    gutter->show();
  } else if (kind == QLatin1String("todo") ||
             kind == QLatin1String("todo_done")) {
    const bool done = kind == QLatin1String("todo_done");
    auto *box = new QPushButton(gutter);
    box->setCursor(Qt::PointingHandCursor);
    box->setFocusPolicy(Qt::NoFocus);
    box->setFixedSize(UiScale::dp(18), UiScale::dp(18));
    box->setText(done ? QStringLiteral("✓") : QString());
    box->setStyleSheet(
        QStringLiteral(
            "QPushButton { background: %1; color: white; border: 1.5px solid %2;"
            "  border-radius: 4px; font-size: 12px; font-weight: 700; padding: 0; }"
            "QPushButton:hover { border-color: %3; }")
            .arg(done ? BlopTheme::accentPrimary().name(QColor::HexRgb)
                      : QStringLiteral("transparent"),
                 done ? BlopTheme::accentPrimary().name(QColor::HexRgb)
                      : sp::hex(sp::checkBorder()),
                 BlopTheme::accentPrimary().name(QColor::HexRgb)));
    connect(box, &QPushButton::clicked, this, [this, row, done]() {
      applyRowKind(row, done ? QStringLiteral("todo")
                             : QStringLiteral("todo_done"));
      scheduleSave();
    });
    auto *wrap = new QVBoxLayout;
    wrap->setContentsMargins(0, UiScale::dp(9), UiScale::dp(6), 0);
    wrap->addWidget(box, 0, Qt::AlignTop);
    wrap->addStretch(1);
    glay->addLayout(wrap);
    gutter->setFixedWidth(UiScale::dp(26));
    gutter->show();
  } else if (kind == QLatin1String("quote")) {
    auto *bar = new QFrame(gutter);
    bar->setFixedWidth(UiScale::dp(3));
    bar->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 1px;")
                           .arg(sp::hex(sp::muted())));
    glay->addWidget(bar);
    glay->addSpacing(UiScale::dp(12));
    gutter->setFixedWidth(UiScale::dp(15));
    gutter->show();
  } else {
    gutter->setFixedWidth(0);
    gutter->hide();
  }
}

StrukturEmbedWidget *
StrukturNoteEditor::makeEmbedWidget(const StrukturEmbedBlock &embed,
                                    QWidget *parent) {
  if (!parent) {
    parent = m_blocksLay && m_blocksLay->parentWidget()
                 ? m_blocksLay->parentWidget()
                 : m_host;
  }
  auto *emb = new StrukturEmbedWidget(m_path, embed, parent);
  wireEmbed(emb);
  return emb;
}

void StrukturNoteEditor::wireEmbed(StrukturEmbedWidget *emb) {
  if (!emb)
    return;
  connect(emb, &StrukturEmbedWidget::openRequested, this, [this, emb]() {
    if (!onOpenEmbed)
      return;
    saveNow();
    auto *grid = qobject_cast<StrukturEmbedGrid *>(emb->parentWidget());
    m_lastOpenedEmbed = blockIndexOfWidget(grid ? static_cast<QWidget *>(grid)
                                                : emb);
    m_lastOpenedGridItem = grid ? grid->indexOf(emb) : -1;
    const auto e = emb->embed();
    onOpenEmbed(StrukturDocument::absoluteNotePath(m_path, e.notePath),
                e.pageIndex);
  });
  connect(emb, &StrukturEmbedWidget::embedChanged, this,
          [this](const StrukturEmbedBlock &) { scheduleSave(); });
}

QWidget *
StrukturNoteEditor::makeEmbedGrid(const QVector<StrukturGridItem> &items) {
  QWidget *parent =
      m_blocksLay && m_blocksLay->parentWidget() ? m_blocksLay->parentWidget()
                                                 : m_host;
  auto *grid = new StrukturEmbedGrid(parent);
  for (const StrukturGridItem &it : items)
    grid->addCard(makeEmbedWidget(it.embed, grid), it);
  connect(grid, &StrukturEmbedGrid::layoutChanged, this,
          [this]() { scheduleSave(); });
  return grid;
}

void StrukturNoteEditor::rebuildUiFromDoc() {
  while (QLayoutItem *it = m_blocksLay->takeAt(0)) {
    if (it->widget())
      it->widget()->deleteLater();
    delete it;
  }

  for (int i = 0; i < m_doc.blocks.size(); ++i) {
    const StrukturBlock &b = m_doc.blocks[i];
    if (b.type == StrukturBlock::Type::EmbedGrid)
      m_blocksLay->addWidget(makeEmbedGrid(b.items));
    else if (b.type == StrukturBlock::Type::Embed) {
      StrukturGridItem it;
      it.embed = b.embed;
      m_blocksLay->addWidget(makeEmbedGrid({it}));
    } else
      m_blocksLay->addWidget(makeParagraphRow(b.paragraph.text, b.paragraph.kind));
  }
  m_blocksLay->addStretch(1);
}

void StrukturNoteEditor::insertParagraphAfter(int blockIndex,
                                              const QString &initialText,
                                              const QString &kind) {
  const int insertAt =
      blockIndex < 0 ? insertIndex(-1) : insertIndex(blockIndex + 1);
  auto *row = makeParagraphRow(initialText, kind);
  m_blocksLay->insertWidget(insertAt, row);
  scheduleSave();
  focusEdit(paragraphEdit(row), false);
}

void StrukturNoteEditor::insertEmbed(const StrukturEmbedBlock &embed,
                                     int afterIndex) {
  StrukturEmbedBlock e = embed;
  e.halfWidth = false;

  // Land in the grid right before the slash line (or the grid itself) so
  // several embeds build one board instead of stacking separate blocks.
  int anchor = afterIndex < 0 ? insertIndex(-1) - 1 : afterIndex;
  QWidget *anchorW = blockWidgetAt(anchor);
  NotionTextEdit *anchorEdit = paragraphEdit(anchorW);
  const bool anchorEmptyPara =
      anchorEdit && anchorEdit->toPlainText().trimmed().isEmpty();
  StrukturEmbedGrid *grid = qobject_cast<StrukturEmbedGrid *>(anchorW);
  if (!grid && anchorEmptyPara)
    grid = qobject_cast<StrukturEmbedGrid *>(blockWidgetAt(anchor - 1));
  if (grid) {
    StrukturGridItem it = grid->suggestSlot(2, 2);
    if (it.row >= StrukturGrid::rowCount(grid->items()))
      it.colSpan = StrukturGrid::kColumns; // new row: start full width
    it.embed = e;
    grid->addCard(makeEmbedWidget(e, grid), it);
    m_lastOpenedEmbed = blockIndexOfWidget(grid);
    m_lastOpenedGridItem = grid->count() - 1;
    scheduleSave();
    revealBlock(m_lastOpenedEmbed);
    return;
  }

  const int insertAt =
      afterIndex < 0 ? insertIndex(-1) : insertIndex(afterIndex + 1);
  StrukturGridItem it;
  it.embed = e;
  m_blocksLay->insertWidget(insertAt, makeEmbedGrid({it}));
  auto *row = makeParagraphRow(QString());
  m_blocksLay->insertWidget(insertAt + 1, row);
  scheduleSave();
  focusEdit(paragraphEdit(row), false);
}

void StrukturNoteEditor::removeEmptyParagraph(QWidget *row) {
  if (!row || !paragraphEdit(row) || !m_blocksLay)
    return;
  int paras = 0;
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    if (paragraphEdit(blockWidgetAt(i)))
      ++paras;
  }
  if (paras <= 1)
    return;

  const int idx = blockIndexOfWidget(row);
  QWidget *prev = nullptr;
  QWidget *next = nullptr;
  for (int i = idx - 1; i >= 0; --i) {
    if (paragraphEdit(blockWidgetAt(i))) {
      prev = blockWidgetAt(i);
      break;
    }
  }
  for (int i = idx + 1; i < m_blocksLay->count(); ++i) {
    if (paragraphEdit(blockWidgetAt(i))) {
      next = blockWidgetAt(i);
      break;
    }
  }
  m_blocksLay->removeWidget(row);
  if (prev)
    focusEdit(paragraphEdit(prev), true);
  else if (next)
    focusEdit(paragraphEdit(next), false);
  row->deleteLater();
  scheduleSave();
}

void StrukturNoteEditor::focusSiblingParagraph(QWidget *row, int direction) {
  if (!m_blocksLay)
    return;
  const int idx = blockIndexOfWidget(row);
  if (idx < 0)
    return;
  if (direction < 0) {
    for (int i = idx - 1; i >= 0; --i) {
      if (auto *edit = paragraphEdit(blockWidgetAt(i))) {
        focusEdit(edit, true);
        return;
      }
    }
  } else {
    for (int i = idx + 1; i < m_blocksLay->count(); ++i) {
      if (auto *edit = paragraphEdit(blockWidgetAt(i))) {
        focusEdit(edit, false);
        return;
      }
    }
  }
}

void StrukturNoteEditor::focusFirstParagraph() {
  if (!m_blocksLay)
    return;
  for (int i = 0; i < m_blocksLay->count(); ++i) {
    if (auto *edit = paragraphEdit(blockWidgetAt(i))) {
      focusEdit(edit, false);
      return;
    }
  }
}

void StrukturNoteEditor::showInsertMenu() {
  showInsertMenuAt(-1, QCursor::pos());
}

void StrukturNoteEditor::showInsertMenuAt(int afterBlockIndex,
                                          const QPoint &globalPos) {
  const int after = afterBlockIndex;
  BlopInWindowMenu::Item sep;
  sep.separator = true;
  // If the current block is still empty, the command restyles it in place
  // instead of leaving an empty line behind (Notion behaviour).
  auto place = [this, after](const QString &kind) {
    QWidget *row = blockWidgetAt(after);
    NotionTextEdit *edit = paragraphEdit(row);
    if (edit && edit->toPlainText().trimmed().isEmpty()) {
      applyRowKind(row, kind);
      focusEdit(edit, true);
      scheduleSave();
      return;
    }
    insertParagraphAfter(after, QString(), kind);
  };
  BlopInWindowMenu::show(
      this, globalPos,
      {{QStringLiteral("Text"), QIcon(), [place]() { place(QString()); }},
       {QStringLiteral("Überschrift 1"), QIcon(),
        [place]() { place(QStringLiteral("h1")); }},
       {QStringLiteral("Überschrift 2"), QIcon(),
        [place]() { place(QStringLiteral("h2")); }},
       {QStringLiteral("Aufzählung"), QIcon(),
        [place]() { place(QStringLiteral("bullet")); }},
       {QStringLiteral("To-Do"), QIcon(),
        [place]() { place(QStringLiteral("todo")); }},
       {QStringLiteral("Zitat"), QIcon(),
        [place]() { place(QStringLiteral("quote")); }},
       {QStringLiteral("Code"), QIcon(),
        [place]() { place(QStringLiteral("code")); }},
       sep,
       {QStringLiteral("Neue A4-Notiz anlegen"), QIcon(),
        [this, after]() { promptLinkedNoteTitle(after); }},
       {QStringLiteral("Bestehende Notiz wählen…"), QIcon(),
        [this, after]() { pickExistingNote(after, false); }},
       {QStringLiteral("Neue Seite in bestehender Notiz…"), QIcon(),
        [this, after]() { pickExistingNote(after, true); }}});
}

void StrukturNoteEditor::promptLinkedNoteTitle(int afterBlockIndex) {
  auto *form = new QWidget;
  auto *lay = new QVBoxLayout(form);
  lay->setContentsMargins(UiScale::dp(18), UiScale::dp(16), UiScale::dp(18),
                          UiScale::dp(16));
  lay->setSpacing(UiScale::dp(12));

  auto *hint =
      new QLabel(QStringLiteral("Titel der eingebetteten A4-Notiz"), form);
  // The modal card follows BlopTheme surfaces (dark in dark mode), not the
  // paper palette of the Struktur sheet.
  hint->setStyleSheet(QStringLiteral(
      "color: %1; font-size: 14px; font-weight: 600; background: transparent;")
                          .arg(BlopTheme::textPrimary().name(QColor::HexRgb)));
  auto *edit = new QLineEdit(form);
  edit->setText(QStringLiteral("Eingebettete Notiz"));
  edit->setMinimumHeight(UiScale::dp(40));
  edit->setStyleSheet(QStringLiteral(
      "QLineEdit {"
      "  background: %1; color: %2; border: 1px solid %3;"
      "  border-radius: %4px; padding: 6px 10px;"
      "  selection-background-color: %5;"
      "}")
                          .arg(BlopTheme::surfaceMuted().name(QColor::HexRgb),
                               BlopTheme::textPrimary().name(QColor::HexRgb),
                               BlopTheme::borderDefault().name(QColor::HexRgb),
                               QString::number(
                                   UiScale::dp(BlopStyle::radiusMdDp())),
                               BlopTheme::accentSubtle().name(QColor::HexRgb)));
  auto *cancel = new QPushButton(QStringLiteral("Abbrechen"), form);
  auto *ok = new QPushButton(QStringLiteral("Anlegen"), form);
  cancel->setCursor(Qt::PointingHandCursor);
  ok->setCursor(Qt::PointingHandCursor);
  cancel->setStyleSheet(BlopTheme::secondaryButtonQss());
  ok->setStyleSheet(BlopTheme::primaryButtonQss());
  cancel->setMinimumHeight(UiScale::dp(36));
  ok->setMinimumHeight(UiScale::dp(36));
  auto *row = new QHBoxLayout;
  row->setSpacing(UiScale::dp(8));
  row->addStretch(1);
  row->addWidget(cancel);
  row->addWidget(ok);

  lay->addWidget(hint);
  lay->addWidget(edit);
  lay->addLayout(row);

  auto *modal = BlopModal::present(this, form, BlopModal::Mode::Card,
                                   QStringLiteral("Neue Notiz"),
                                   UiScale::dp(380));
  if (!modal) {
    form->deleteLater();
    return;
  }
  connect(modal, &BlopModal::dismissed, form, &QObject::deleteLater);
  connect(cancel, &QPushButton::clicked, modal, &BlopModal::dismiss);

  auto submit = [this, form, edit, modal, afterBlockIndex]() {
    if (form->property("submitted").toBool())
      return;
    form->setProperty("submitted", true);
    QString title = edit->text().trimmed();
    if (title.isEmpty())
      title = QStringLiteral("Eingebettete Notiz");
    modal->dismiss();
    if (!onCreateLinkedNote)
      return;
    const QString path = onCreateLinkedNote(title);
    if (path.isEmpty())
      return;
    StrukturEmbedBlock emb;
    emb.notePath = StrukturDocument::storeNotePath(m_path, path);
    emb.pageIndex = 0;
    insertEmbed(emb, afterBlockIndex);
  };
  connect(ok, &QPushButton::clicked, form, submit);
  connect(edit, &QLineEdit::returnPressed, form, submit);
  QTimer::singleShot(0, edit, [edit]() {
    edit->setFocus();
    edit->selectAll();
  });
}

void StrukturNoteEditor::pickExistingNote(int afterBlockIndex, bool appendPage) {
  const QString startDir = QFileInfo(m_path).absolutePath();
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Notiz wählen"), startDir,
      QStringLiteral("Blop A4 (*.bnote)"));
  if (path.isEmpty())
    return;
  int pageIndex = 0;
  if (appendPage) {
    if (!onAppendPage)
      return;
    pageIndex = onAppendPage(path);
    if (pageIndex < 0)
      return;
  }
  StrukturEmbedBlock emb;
  emb.notePath = StrukturDocument::storeNotePath(m_path, path);
  emb.pageIndex = pageIndex;
  insertEmbed(emb, afterBlockIndex);
}

#include "strukturnoteeditor.moc"
