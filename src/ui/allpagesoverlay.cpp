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
#include <QEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

AllPagesOverlay::AllPagesOverlay(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setFocusPolicy(Qt::StrongFocus);
  hide();

  m_card = new QWidget(this);
  m_card->setObjectName(QStringLiteral("AllPagesCard"));
  auto *lay = new QVBoxLayout(m_card);
  lay->setContentsMargins(UiScale::dp(20), UiScale::dp(20), UiScale::dp(20),
                          UiScale::dp(20));
  lay->setSpacing(UiScale::dp(16));

  auto *header = new QHBoxLayout;
  header->setSpacing(UiScale::dp(8));
  m_title = new QLabel(QStringLiteral("Alle Seiten"), m_card);
  m_title->setStyleSheet(
      QStringLiteral("font-size: 16px; font-weight: 600; background: transparent;"));
  header->addWidget(m_title, 1);

  m_btnAdd = new QPushButton(QStringLiteral("+ Seite"), m_card);
  m_btnDup = new QPushButton(QStringLiteral("Duplizieren"), m_card);
  m_btnDel = new QPushButton(QStringLiteral("Löschen"), m_card);
  m_btnClose = new QPushButton(QStringLiteral("✕"), m_card);
  m_btnClose->setFixedSize(UiScale::dp(32), UiScale::dp(32));
  for (auto *b : {m_btnAdd, m_btnDup, m_btnDel}) {
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedHeight(UiScale::dp(32));
    b->setFocusPolicy(Qt::NoFocus);
  }
  m_btnClose->setCursor(Qt::PointingHandCursor);
  m_btnClose->setFocusPolicy(Qt::NoFocus);
  header->addWidget(m_btnAdd);
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
            if (!m_view || start < 0)
              return;
            const int from = start;
            const int last = qMax(start, end);
            int to = dest;
            if (to > from)
              --to;
            QTimer::singleShot(0, this, [this, from, last, to]() {
              if (!m_view)
                return;
              // Move the dragged range as a block (ExtendedSelection).
              const int count = last - from + 1;
              if (count <= 1) {
                if (to != from)
                  m_view->movePage(from, to);
              } else {
                int cursor = to;
                if (to > from)
                  cursor = to - count + 1;
                for (int i = 0; i < count; ++i) {
                  const int src = (to > from) ? from : from + i;
                  m_view->movePage(src, cursor + i);
                }
              }
              emit pagesChanged();
              rebuild();
            });
          });
  m_grid->installEventFilter(this);
  lay->addWidget(m_grid, 1);

  connect(m_btnClose, &QPushButton::clicked, this, &AllPagesOverlay::dismiss);
  connect(m_btnAdd, &QPushButton::clicked, this, [this]() {
    if (!m_view)
      return;
    m_view->addNewPage();
    emit pagesChanged();
    rebuild();
  });
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
  if (m_card) {
    m_card->setProperty("blopOwnsBackground", true);
    m_card->setProperty("blopAllPagesSheet", true);
    m_card->setProperty("blopSoftCardShadow", true);
    m_card->setProperty("blopNoteChrome", false);
  }
  refreshChrome();
  // Prefer the top-level window so the scrim covers title bar + sidebar —
  // not only the editor center (which sits inside the push spacer).
  QWidget *host = window();
  if (host && m_card) {
    if (m_modal) {
      m_modal->disconnect(this);
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
      m_modal->setPreferredCardHeightFrac(0.72);
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
  m_title->setText(QStringLiteral("Alle Seiten (%1)").arg(count));
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
  if (!m_card)
    return;
  m_card->setProperty("blopOwnsBackground", true);
  m_card->setProperty("blopAllPagesSheet", true);
  m_card->setProperty("blopSoftCardShadow", true);
  m_card->setProperty("blopNoteChrome", false);
  const bool dark = BlopTheme::instance().isDark();
  const QString ink = dark ? QStringLiteral("#F8FAFC") : QStringLiteral("#111827");
  const QString quietBg =
      dark ? QStringLiteral("rgba(255,255,255,0.08)") : QStringLiteral("#FFFFFF");
  const QString quietHover =
      dark ? QStringLiteral("rgba(255,255,255,0.14)") : QStringLiteral("#F9FAFB");
  const QString quietBorder =
      dark ? QStringLiteral("rgba(255,255,255,0.10)") : QStringLiteral("#E5E7EB");
  const QString pill = QStringLiteral(
      "QPushButton {"
      "  background: %1; color: %2; border: 1px solid %3;"
      "  border-radius: 6px; padding: 0 12px;"
      "  font-size: 12px; font-weight: 600;"
      "}"
      "QPushButton:hover { background: %4; }");
  if (m_btnAdd) {
    m_btnAdd->setStyleSheet(pill.arg(QStringLiteral("#2563EB"),
                                     QStringLiteral("#FFFFFF"),
                                     QStringLiteral("#2563EB"),
                                     QStringLiteral("#1D4ED8")));
  }
  if (m_btnDup)
    m_btnDup->setStyleSheet(pill.arg(quietBg, ink, quietBorder, quietHover));
  if (m_btnDel) {
    m_btnDel->setStyleSheet(pill.arg(
        quietBg, QStringLiteral("#EF4444"), quietBorder,
        QStringLiteral("rgba(239,68,68,0.15)")));
  }
  if (m_btnClose) {
    m_btnClose->setStyleSheet(pill.arg(QStringLiteral("transparent"), ink,
                                       QStringLiteral("transparent"),
                                       quietHover));
  }
  if (m_title) {
    m_title->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 16px; font-weight: 600; background: transparent;")
                               .arg(ink));
  }
  m_card->setStyleSheet(QStringLiteral(
      "QWidget#AllPagesCard { background: transparent; border: none; }"
      "QListWidget { background: transparent; border: none; outline: 0; color: %1; }"
      "QListWidget::item {"
      "  background: transparent; border: 1px solid transparent;"
      "  border-radius: 8px; padding: 4px;"
      "}"
      "QListWidget::item:selected {"
      "  border: 2px solid #3B82F6; border-radius: 8px;"
      "}")
                             .arg(ink));
}

void AllPagesOverlay::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  // Fallback path only (when BlopModal is unavailable).
  QPainter p(this);
  p.fillRect(rect(), BlopTheme::scrimColor());
}

void AllPagesOverlay::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Back) {
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

bool AllPagesOverlay::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_grid && event->type() == QEvent::KeyPress) {
    auto *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Escape || ke->key() == Qt::Key_Back) {
      dismiss();
      return true;
    }
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
      if (auto *it = m_grid->currentItem()) {
        emit pageActivated(it->data(Qt::UserRole).toInt());
        dismiss();
        return true;
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void AllPagesOverlay::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (!m_card || m_card->parentWidget() != this)
    return;
  // Tighter margins on phone so the page grid has room to breathe.
  const int m = UiScale::isAndroidPhoneUi(this) ? UiScale::dp(12)
                                                 : UiScale::dp(28);
  m_card->setGeometry(m, m, width() - 2 * m, height() - 2 * m);
}
