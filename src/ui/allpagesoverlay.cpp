#include "allpagesoverlay.h"

#include "blop_dialogs.h"
#include "blop_modal.h"
#include "blop_theme.h"
#include "multipagenoteview.h"
#include "notechrome.h"
#include "uiscale.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

AllPagesOverlay::AllPagesOverlay(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::StrongFocus);
  hide();

  m_card = new QWidget(this);
  m_card->setObjectName(QStringLiteral("AllPagesCard"));
  auto *lay = new QVBoxLayout(m_card);
  lay->setContentsMargins(UiScale::dp(16), UiScale::dp(14), UiScale::dp(16),
                          UiScale::dp(14));
  lay->setSpacing(UiScale::dp(10));

  auto *header = new QHBoxLayout;
  m_title = new QLabel(QStringLiteral("Alle Seiten"), m_card);
  m_title->setStyleSheet(
      QStringLiteral("font-size: 16px; font-weight: 800; background: transparent;"));
  header->addWidget(m_title, 1);

  m_btnDup = new QPushButton(QStringLiteral("Duplizieren"), m_card);
  m_btnDel = new QPushButton(QStringLiteral("Löschen"), m_card);
  m_btnClose = new QPushButton(QStringLiteral("Schließen"), m_card);
  for (auto *b : {m_btnDup, m_btnDel, m_btnClose}) {
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedHeight(UiScale::dp(32));
  }
  header->addWidget(m_btnDup);
  header->addWidget(m_btnDel);
  header->addWidget(m_btnClose);
  lay->addLayout(header);

  m_grid = new QListWidget(m_card);
  m_grid->setViewMode(QListView::IconMode);
  m_grid->setResizeMode(QListView::Adjust);
  m_grid->setMovement(QListView::Snap);
  m_grid->setDragDropMode(QAbstractItemView::InternalMove);
  m_grid->setDefaultDropAction(Qt::MoveAction);
  m_grid->setUniformItemSizes(true);
  m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_grid->setSpacing(UiScale::dp(10));
  m_grid->setIconSize(QSize(UiScale::dp(110), UiScale::dp(148)));
  connect(m_grid, &QListWidget::itemClicked, this,
          &AllPagesOverlay::onItemClicked);
  connect(m_grid, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            emit pageActivated(item->data(Qt::UserRole).toInt());
            dismiss();
          });
  connect(m_grid->model(), &QAbstractItemModel::rowsMoved, this,
          [this](const QModelIndex &, int start, int end, const QModelIndex &,
                 int dest) {
            Q_UNUSED(end);
            if (!m_view || start < 0)
              return;
            int to = dest;
            if (to > start)
              --to;
            if (to == start)
              return;
            m_view->movePage(start, to);
            emit pagesChanged();
            rebuild();
          });
  lay->addWidget(m_grid, 1);

  connect(m_btnClose, &QPushButton::clicked, this, &AllPagesOverlay::dismiss);
  connect(m_btnDup, &QPushButton::clicked, this,
          &AllPagesOverlay::applyBatchDuplicate);
  connect(m_btnDel, &QPushButton::clicked, this,
          &AllPagesOverlay::applyBatchDelete);

  refreshChrome();
}

void AllPagesOverlay::setNoteView(MultiPageNoteView *view) { m_view = view; }

void AllPagesOverlay::setAccentColor(const QColor &c) {
  if (c.isValid())
    m_accent = c;
  refreshChrome();
}

void AllPagesOverlay::present() {
  rebuild();
  refreshChrome();
  QWidget *host = window();
  if (host && m_card) {
    if (m_modal) {
      m_modal->dismiss();
      m_modal.clear();
    }
    if (m_card->parentWidget() != this)
      m_card->setParent(this);
    m_modal = BlopModal::present(host, m_card, BlopModal::Mode::Card,
                                 QStringLiteral("Alle Seiten"));
    if (m_modal) {
      m_modal->setPreferredCardWidth(
          qBound(UiScale::dp(520), host->width() - UiScale::dp(56),
                 UiScale::dp(760)));
      connect(m_modal, &BlopModal::dismissed, this, [this]() {
        if (m_card && m_card->parentWidget() != this)
          m_card->setParent(this);
        m_modal.clear();
        hide();
      });
      hide();
      return;
    }
  }
  show();
  raise();
  setFocus(Qt::OtherFocusReason);
}

void AllPagesOverlay::dismiss() {
  if (m_modal) {
    m_modal->dismiss();
    return;
  }
  hide();
  if (m_card && m_card->parentWidget() != this)
    m_card->setParent(this);
}

void AllPagesOverlay::rebuild() {
  ++m_epoch;
  const int epoch = m_epoch;
  m_grid->clear();
  if (!m_view || !m_view->note())
    return;

  const int count = m_view->pageCount();
  m_title->setText(QStringLiteral("Alle Seiten  ·  %1").arg(count));
  const QSize thumb(UiScale::dp(110), UiScale::dp(148));
  for (int i = 0; i < count; ++i) {
    auto *item = new QListWidgetItem(m_grid);
    item->setData(Qt::UserRole, i);
    item->setText(QStringLiteral("%1").arg(i + 1));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    item->setSizeHint(QSize(thumb.width() + UiScale::dp(16),
                            thumb.height() + UiScale::dp(28)));
    m_grid->addItem(item);
    m_view->generateThumbnailAsync(i, thumb,
                                   [this, item, epoch](const QPixmap &pm) {
                                     if (m_epoch != epoch || !item)
                                       return;
                                     if (!pm.isNull())
                                       item->setIcon(QIcon(pm));
                                   });
  }
}

void AllPagesOverlay::onItemClicked(QListWidgetItem *item) {
  Q_UNUSED(item);
  // Selection only — activate on double-click / Enter.
}

void AllPagesOverlay::applyBatchDuplicate() {
  if (!m_view)
    return;
  QList<int> idxs;
  for (auto *it : m_grid->selectedItems())
    idxs.append(it->data(Qt::UserRole).toInt());
  if (idxs.isEmpty())
    return;
  m_view->duplicatePages(idxs);
  emit pagesChanged();
  rebuild();
}

void AllPagesOverlay::applyBatchDelete() {
  if (!m_view)
    return;
  QList<int> idxs;
  for (auto *it : m_grid->selectedItems())
    idxs.append(it->data(Qt::UserRole).toInt());
  if (idxs.isEmpty())
    return;
  if (idxs.size() >= m_view->pageCount()) {
    BlopDialogs::notify(this, QStringLiteral("Seiten löschen"),
                        QStringLiteral("Mindestens eine Seite muss bleiben."));
    return;
  }
  if (!BlopDialogs::confirm(
          this, QStringLiteral("Seiten löschen"),
          QStringLiteral("%1 Seite(n) wirklich löschen?").arg(idxs.size()),
          QStringLiteral("Löschen"), QStringLiteral("Abbrechen")))
    return;
  m_view->deletePages(idxs);
  emit pagesChanged();
  rebuild();
}

void AllPagesOverlay::refreshChrome() {
  setStyleSheet(QStringLiteral(
      "QWidget#AllPagesCard {"
      "  background: transparent; border: none; border-radius: 0;"
      "}"
      "QLabel { color: %1; background: transparent; }"
      "QListWidget { background: transparent; border: none; outline: 0; color: %1; }"
      "QListWidget::item {"
      "  background: %2; border: 1px solid %3; border-radius: 10px; padding: 4px;"
      "}"
      "QListWidget::item:selected { border: 2px solid %4; }"
      "QPushButton {"
      "  background: %2; color: %1; border: 1px solid %3; border-radius: 10px;"
      "  padding: 0 12px; font-weight: 600;"
      "}"
      "QPushButton:hover { border-color: %4; }")
                    .arg(NoteChrome::textPrimary().name(),
                         NoteChrome::panelBg().name(),
                         NoteChrome::border().name(), m_accent.name()));
}

void AllPagesOverlay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  // Fallback path only (when BlopModal is unavailable).
  QPainter p(this);
  p.fillRect(rect(), BlopTheme::scrimColor());
}

void AllPagesOverlay::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    dismiss();
    return;
  }
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    if (auto *it = m_grid->currentItem()) {
      emit pageActivated(it->data(Qt::UserRole).toInt());
      dismiss();
      return;
    }
  }
  QWidget::keyPressEvent(event);
}

void AllPagesOverlay::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  const int m = UiScale::dp(28);
  m_card->setGeometry(m, m, width() - 2 * m, height() - 2 * m);
}
