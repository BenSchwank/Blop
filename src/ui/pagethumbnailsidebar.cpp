#include "pagethumbnailsidebar.h"

#include <QIcon>
#include <QListWidget>
#include <QPixmap>
#include <QSize>
#include <QVBoxLayout>

#include "Note.h"
#include "blopstyle.h"
#include "multipagenoteview.h"
#include "uiscale.h"

PageThumbnailSidebar::PageThumbnailSidebar(QWidget *parent)
    : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFixedWidth(UiScale::dp(96));

  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->setContentsMargins(UiScale::dp(8), UiScale::dp(8), UiScale::dp(8), UiScale::dp(8));
  lay->setSpacing(UiScale::dp(8));

  m_list = new QListWidget(this);
  m_list->setFrameStyle(QFrame::NoFrame);
  m_list->setSpacing(UiScale::dp(8));
  m_list->setIconSize(QSize(UiScale::dp(80), UiScale::dp(110)));
  m_list->setStyleSheet(
      QStringLiteral("QListWidget { background: transparent; border: none; outline: 0; }"
                     "QListWidget::item { border-radius: 10px; color: #E8E4FF; }"
                     "QListWidget::item:selected { background: rgba(124,92,252,0.35); }"
                     "QListWidget::item:hover { background: rgba(255,255,255,0.08); }"));
  connect(m_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) { onItemClicked(m_list->row(item)); });
  lay->addWidget(m_list, 1);
}

void PageThumbnailSidebar::setNoteView(MultiPageNoteView *view) {
  if (m_view == view)
    return;
  m_view = view;
  rebuild();
}

void PageThumbnailSidebar::setAccentColor(const QColor &color) {
  if (m_accentColor == color)
    return;
  m_accentColor = color;
  rebuild();
}

void PageThumbnailSidebar::rebuild() {
  ++m_rebuildEpoch;
  const int epoch = m_rebuildEpoch;
  m_list->clear();
  if (!m_view || !m_view->note())
    return;

  const Note *note = m_view->note();
  const int count = note->pages.size();
  const QSize thumbSize(UiScale::dp(80), UiScale::dp(110));
  for (int i = 0; i < count; ++i) {
    QListWidgetItem *item = new QListWidgetItem(m_list);
    item->setTextAlignment(Qt::AlignCenter | Qt::AlignBottom);
    item->setSizeHint(QSize(UiScale::dp(80), UiScale::dp(130)));
    item->setText(QStringLiteral("%1").arg(i + 1));
    item->setForeground(QColor(232, 228, 255));
    m_list->addItem(item);
    requestThumbnail(i, item, epoch);
  }

  if (m_currentPage >= 0 && m_currentPage < count)
    m_list->setCurrentRow(m_currentPage);
}

void PageThumbnailSidebar::requestThumbnail(int pageIndex, QListWidgetItem *item, int epoch) {
  if (!m_view || !item)
    return;
  const QSize thumbSize(UiScale::dp(80), UiScale::dp(110));
  m_view->generateThumbnailAsync(pageIndex, thumbSize,
      [this, item, epoch](const QPixmap &pm) {
        if (!item || m_rebuildEpoch != epoch)
          return;
        if (!pm.isNull())
          item->setIcon(QIcon(pm));
      });
}

void PageThumbnailSidebar::onCurrentPageChanged(int pageIndex) {
  m_currentPage = pageIndex;
  if (m_list && pageIndex >= 0 && pageIndex < m_list->count())
    m_list->setCurrentRow(pageIndex);
}

void PageThumbnailSidebar::onItemClicked(int row) {
  m_currentPage = row;
  emit pageSelected(row);
}

