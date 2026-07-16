#include "pagethumbnailsidebar.h"

#include <QAbstractItemView>
#include <QIcon>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#include "Note.h"
#include "multipagenoteview.h"
#include "uiscale.h"

PageThumbnailSidebar::PageThumbnailSidebar(QWidget *parent)
    : QWidget(parent) {
  setObjectName(QStringLiteral("PageThumbnailSidebar"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedWidth(UiScale::dp(92));
  setStyleSheet(QStringLiteral(
      "QWidget#PageThumbnailSidebar {"
      "  background-color: rgba(16, 14, 24, 0.72);"
      "  border-right: 1px solid rgba(120, 130, 160, 0.16);"
      "}"));

  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->setContentsMargins(UiScale::dp(8), UiScale::dp(10), UiScale::dp(8), UiScale::dp(10));
  lay->setSpacing(UiScale::dp(8));

  m_list = new QListWidget(this);
  m_list->setFrameStyle(QFrame::NoFrame);
  m_list->setSpacing(UiScale::dp(6));
  m_list->setIconSize(QSize(UiScale::dp(72), UiScale::dp(98)));
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(m_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) { onItemClicked(m_list->row(item)); });
  lay->addWidget(m_list, 1);

  m_btnAddPage = new QPushButton(QStringLiteral("+"), this);
  m_btnAddPage->setObjectName(QStringLiteral("PageRailAddBtn"));
  m_btnAddPage->setFixedHeight(UiScale::dp(32));
  m_btnAddPage->setCursor(Qt::PointingHandCursor);
  m_btnAddPage->setToolTip(QStringLiteral("Seite hinzufügen"));
  connect(m_btnAddPage, &QPushButton::clicked, this, &PageThumbnailSidebar::addPageRequested);
  lay->addWidget(m_btnAddPage);

  refreshListStyle();
}

void PageThumbnailSidebar::refreshListStyle() {
  const QString accent = m_accentColor.name(QColor::HexRgb);
  m_list->setStyleSheet(
      QStringLiteral("QListWidget { background: transparent; border: none; outline: 0; }"
                     "QListWidget::item {"
                     "  border: 1px solid rgba(120,130,160,0.18);"
                     "  border-radius: 10px;"
                     "  color: rgba(232,228,255,0.75);"
                     "  padding: 2px;"
                     "  background: rgba(255,255,255,0.03);"
                     "}"
                     "QListWidget::item:selected {"
                     "  background: %1;"
                     "  border: 1.5px solid %2;"
                     "  color: #F4F5FB;"
                     "}"
                     "QListWidget::item:hover {"
                     "  background: rgba(255,255,255,0.07);"
                     "}")
          .arg(QColor(m_accentColor.red(), m_accentColor.green(),
                      m_accentColor.blue(), 48)
                   .name(QColor::HexArgb),
               accent));

  if (m_btnAddPage) {
    m_btnAddPage->setStyleSheet(
        QStringLiteral("QPushButton#PageRailAddBtn {"
                       "  background: transparent;"
                       "  color: rgba(232,228,255,0.70);"
                       "  border: 1px dashed rgba(120,130,160,0.35);"
                       "  border-radius: 10px;"
                       "  font-size: 18px;"
                       "  font-weight: 600;"
                       "}"
                       "QPushButton#PageRailAddBtn:hover {"
                       "  background: %1;"
                       "  border-color: %2;"
                       "  color: #F4F5FB;"
                       "}")
            .arg(QColor(m_accentColor.red(), m_accentColor.green(),
                        m_accentColor.blue(), 40)
                     .name(QColor::HexArgb),
                 accent));
  }
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
  refreshListStyle();
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
  for (int i = 0; i < count; ++i) {
    QListWidgetItem *item = new QListWidgetItem(m_list);
    item->setTextAlignment(Qt::AlignCenter | Qt::AlignBottom);
    item->setSizeHint(QSize(UiScale::dp(72), UiScale::dp(118)));
    item->setText(QStringLiteral("%1").arg(i + 1));
    item->setForeground(QColor(232, 228, 255, 200));
    m_list->addItem(item);
    requestThumbnail(i, item, epoch);
  }

  if (m_currentPage >= 0 && m_currentPage < count)
    m_list->setCurrentRow(m_currentPage);
}

void PageThumbnailSidebar::requestThumbnail(int pageIndex, QListWidgetItem *item, int epoch) {
  if (!m_view || !item)
    return;
  const QSize thumbSize(UiScale::dp(72), UiScale::dp(98));
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
