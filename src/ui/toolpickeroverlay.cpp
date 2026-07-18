#include "toolpickeroverlay.h"

#include "moderntoolbar.h"
#include "notechrome.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
struct ToolEntry {
  ToolMode mode;
  const char *name;
  const char *icon;
  int category; // 0 Free-form, 1 Shapes, 2 Review, 3 Messen, 4 Einfügen, 5 Auswählen
};

const ToolEntry kTools[] = {
    {ToolMode::Pen, "Stift", "pen", 0},
    {ToolMode::Pencil, "Bleistift", "pencil", 0},
    {ToolMode::Highlighter, "Textmarker", "highlighter", 0},
    {ToolMode::Eraser, "Radiergummi", "eraser", 0},
    {ToolMode::Shape, "Formen", "shape", 1},
    {ToolMode::Ruler, "Lineal", "ruler", 1},
    {ToolMode::Text, "Text", "text", 2},
    {ToolMode::Image, "Bild", "image", 4},
    {ToolMode::StickyNote, "Notiz", "stickynote", 4},
    {ToolMode::Lasso, "Auswahl", "lasso", 5},
    {ToolMode::Hand, "Hand", "hand", 5},
};

QIcon glyphIcon(const QString &name, const QColor &fg, int px) {
  QPixmap pm(px, px);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const qreal s = px / 64.0;
  p.translate(px / 2.0, px / 2.0);
  p.scale(s, s);
  p.translate(-32, -32);
  blopDrawToolbarGlyph64(&p, name, fg);
  return QIcon(pm);
}
} // namespace

void ToolPickerOverlay::present(QWidget *host, const QColor &accent,
                                SelectFn onSelect) {
  present(host, accent, {}, std::move(onSelect));
}

void ToolPickerOverlay::present(QWidget *host, const QColor &accent,
                                const QSet<ToolMode> &alreadyInToolbar,
                                SelectFn onSelect) {
  if (!host)
    return;
  auto *overlay =
      new ToolPickerOverlay(host, accent, alreadyInToolbar, std::move(onSelect));
  overlay->setGeometry(host->rect());
  overlay->show();
  overlay->raise();
}

ToolPickerOverlay::ToolPickerOverlay(QWidget *host, const QColor &accent,
                                     const QSet<ToolMode> &alreadyInToolbar,
                                     SelectFn onSelect)
    : QWidget(host),
      m_accent(accent.isValid() ? accent : NoteChrome::accent()),
      m_onSelect(std::move(onSelect)), m_inToolbar(alreadyInToolbar) {
  setAttribute(Qt::WA_DeleteOnClose, true);
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral("background: rgba(0, 0, 0, 0.55);"));

  auto *card = new QFrame(this);
  card->setObjectName(QStringLiteral("ToolPickerCard"));
  card->setStyleSheet(
      QStringLiteral("QFrame#ToolPickerCard {"
                     "  background: %1;"
                     "  border: 1px solid %2;"
                     "  border-radius: 16px;"
                     "}"
                     "QLabel { background: transparent; color: %3; }"
                     "QLineEdit {"
                     "  background: %4; color: %3;"
                     "  border: 1px solid %2; border-radius: 10px;"
                     "  padding: 0 12px; min-height: 34px;"
                     "}"
                     "QPushButton#ToolPickerTab {"
                     "  background: transparent; color: %5;"
                     "  border: none; border-bottom: 2px solid transparent;"
                     "  padding: 8px 12px; font-weight: 600; font-size: 13px;"
                     "}"
                     "QPushButton#ToolPickerTab:checked {"
                     "  color: %3; border-bottom: 2px solid %6;"
                     "}")
          .arg(NoteChrome::panelElevated().name(QColor::HexRgb),
               NoteChrome::border().name(QColor::HexRgb),
               NoteChrome::textPrimary().name(QColor::HexRgb),
               NoteChrome::panelBg().name(QColor::HexRgb),
               NoteChrome::textSecondary().name(QColor::HexRgb),
               m_accent.name(QColor::HexRgb)));

  const int cardW = qMin(720, qMax(520, int(host->width() * 0.58)));
  const int cardH = qMin(500, qMax(380, int(host->height() * 0.62)));
  card->setGeometry((width() - cardW) / 2, (height() - cardH) / 2, cardW, cardH);

  auto *root = new QVBoxLayout(card);
  root->setContentsMargins(UiScale::dp(22), UiScale::dp(18), UiScale::dp(22),
                           UiScale::dp(14));
  root->setSpacing(UiScale::dp(12));

  auto *title = new QLabel(QStringLiteral("Neues Tool wählen"), card);
  title->setStyleSheet(QStringLiteral(
      "font-size: 20px; font-weight: 800; letter-spacing: -0.3px;"));
  root->addWidget(title);

  auto *tabRow = new QHBoxLayout();
  tabRow->setSpacing(UiScale::dp(4));
  m_tabs = new QButtonGroup(card);
  m_tabs->setExclusive(true);
  const char *cats[] = {"Free-form", "Shapes", "Review",
                        "Messen",    "Einfügen", "Auswählen"};
  for (int i = 0; i < 6; ++i) {
    auto *b = new QPushButton(QString::fromUtf8(cats[i]), card);
    b->setObjectName(QStringLiteral("ToolPickerTab"));
    b->setCheckable(true);
    if (i == 0)
      b->setChecked(true);
    m_tabs->addButton(b, i);
    tabRow->addWidget(b);
  }
  tabRow->addStretch(1);
  m_search = new QLineEdit(card);
  m_search->setPlaceholderText(QStringLiteral("Suchtools"));
  m_search->setFixedWidth(UiScale::dp(160));
  tabRow->addWidget(m_search);
  root->addLayout(tabRow);

  auto *scroll = new QScrollArea(card);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
  m_gridHost = new QWidget(scroll);
  m_gridHost->setStyleSheet(QStringLiteral("background: transparent;"));
  scroll->setWidget(m_gridHost);
  root->addWidget(scroll, 1);

  auto *hint = new QLabel(
      QStringLiteral(
          "Ziehen und Ablegen — Tippe +, um ein Tool zur Symbolleiste "
          "hinzuzufügen."),
      card);
  hint->setWordWrap(true);
  hint->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px; font-weight: 500;"
                     " background: %2; border-radius: 10px; padding: 10px 12px;")
          .arg(NoteChrome::textSecondary().name(QColor::HexRgb),
               NoteChrome::panelBg().name(QColor::HexRgb)));
  root->addWidget(hint);

  connect(m_tabs, &QButtonGroup::idClicked, this,
          [this](int id) { rebuildGrid(id); });
  connect(m_search, &QLineEdit::textChanged, this, [this](const QString &) {
    rebuildGrid(m_tabs ? m_tabs->checkedId() : 0);
  });

  rebuildGrid(0);
}

void ToolPickerOverlay::rebuildGrid(int categoryIndex) {
  if (!m_gridHost)
    return;
  if (auto *old = m_gridHost->layout()) {
    QLayoutItem *it;
    while ((it = old->takeAt(0))) {
      if (it->widget())
        it->widget()->deleteLater();
      delete it;
    }
    delete old;
  }
  auto *grid = new QGridLayout(m_gridHost);
  grid->setContentsMargins(UiScale::dp(8), UiScale::dp(8), UiScale::dp(8),
                           UiScale::dp(8));
  grid->setHorizontalSpacing(UiScale::dp(16));
  grid->setVerticalSpacing(UiScale::dp(16));

  const QString q = m_search ? m_search->text().trimmed() : QString();
  int col = 0;
  int row = 0;
  int shown = 0;
  for (const ToolEntry &e : kTools) {
    if (e.category != categoryIndex)
      continue;
    const QString label = QString::fromUtf8(e.name);
    if (!q.isEmpty() && !label.contains(q, Qt::CaseInsensitive))
      continue;

    auto *cell = new QWidget(m_gridHost);
    auto *cellLay = new QVBoxLayout(cell);
    cellLay->setContentsMargins(0, 0, 0, 0);
    cellLay->setSpacing(UiScale::dp(6));
    cellLay->setAlignment(Qt::AlignHCenter);

    const bool inBar = m_inToolbar.contains(e.mode);
    auto *btn = new QToolButton(cell);
    btn->setFixedSize(UiScale::dp(76), UiScale::dp(76));
    btn->setIcon(glyphIcon(QString::fromUtf8(e.icon), NoteChrome::textPrimary(),
                           UiScale::dp(40)));
    btn->setIconSize(QSize(UiScale::dp(36), UiScale::dp(36)));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QStringLiteral("QToolButton {"
                       "  background: %1;"
                       "  border: 1px solid %2;"
                       "  border-radius: 38px;"
                       "}"
                       "QToolButton:hover {"
                       "  background: rgba(%3,%4,%5,0.22);"
                       "  border-color: %6;"
                       "}")
            .arg(NoteChrome::panelBg().name(QColor::HexRgb),
                 NoteChrome::border().name(QColor::HexRgb))
            .arg(m_accent.red())
            .arg(m_accent.green())
            .arg(m_accent.blue())
            .arg(m_accent.name(QColor::HexRgb)));

    auto *badge = new QLabel(inBar ? QStringLiteral("✓") : QStringLiteral("+"),
                             btn);
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(UiScale::dp(22), UiScale::dp(22));
    badge->move(btn->width() - UiScale::dp(24), UiScale::dp(2));
    badge->setStyleSheet(
        QStringLiteral("background: %1; color: white; border-radius: 11px;"
                       " font-size: 13px; font-weight: 800;")
            .arg(inBar ? QStringLiteral("#3D9B6E")
                       : m_accent.name(QColor::HexRgb)));

    const ToolMode mode = e.mode;
    connect(btn, &QToolButton::clicked, this, [this, mode]() {
      if (m_onSelect)
        m_onSelect(mode);
      emit toolPicked(mode);
      close();
    });

    auto *lbl = new QLabel(label, cell);
    lbl->setAlignment(Qt::AlignHCenter);
    lbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; font-weight: 600;")
            .arg(NoteChrome::textSecondary().name(QColor::HexRgb)));
    cellLay->addWidget(btn, 0, Qt::AlignHCenter);
    cellLay->addWidget(lbl);
    grid->addWidget(cell, row, col);
    ++shown;
    if (++col >= 5) {
      col = 0;
      ++row;
    }
  }

  if (shown == 0) {
    auto *empty = new QLabel(
        QStringLiteral("Keine Tools in dieser Kategorie."), m_gridHost);
    empty->setAlignment(Qt::AlignCenter);
    empty->setStyleSheet(
        QStringLiteral("color: %1; font-size: 13px;")
            .arg(NoteChrome::textSecondary().name(QColor::HexRgb)));
    grid->addWidget(empty, 0, 0, 1, 5);
  }
  grid->setRowStretch(row + 1, 1);
}

bool ToolPickerOverlay::event(QEvent *e) {
  if (e->type() == QEvent::MouseButtonPress) {
    auto *me = static_cast<QMouseEvent *>(e);
    // Close when clicking the dimmed backdrop (not the card).
    for (QObject *c : children()) {
      if (auto *w = qobject_cast<QWidget *>(c)) {
        if (w->objectName() == QStringLiteral("ToolPickerCard") &&
            w->geometry().contains(me->pos()))
          return QWidget::event(e);
      }
    }
    close();
    return true;
  }
  if (e->type() == QEvent::Resize && parentWidget()) {
    setGeometry(parentWidget()->rect());
    for (QObject *c : children()) {
      if (auto *w = qobject_cast<QWidget *>(c)) {
        if (w->objectName() == QStringLiteral("ToolPickerCard")) {
          const int cardW = qMin(720, qMax(520, int(width() * 0.58)));
          const int cardH = qMin(500, qMax(380, int(height() * 0.62)));
          w->setGeometry((width() - cardW) / 2, (height() - cardH) / 2, cardW,
                         cardH);
        }
      }
    }
  }
  return QWidget::event(e);
}
