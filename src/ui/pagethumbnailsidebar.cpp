#include "pagethumbnailsidebar.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QIcon>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#include "Note.h"
#include "multipagenoteview.h"
#include "uiscale.h"

namespace {
struct RailMetrics {
  int width;
  int thumbW;
  int thumbH;
  int itemH;
  int pad;
};

RailMetrics railMetrics(QWidget *ref) {
#ifdef Q_OS_ANDROID
  // Phones: slim rail so the canvas keeps breathing room.
  if (!UiScale::isAndroidTablet(ref)) {
    return {UiScale::dp(68), UiScale::dp(52), UiScale::dp(72), UiScale::dp(90),
            UiScale::dp(6)};
  }
#endif
  Q_UNUSED(ref);
  return {UiScale::dp(92), UiScale::dp(72), UiScale::dp(98), UiScale::dp(118),
          UiScale::dp(8)};
}
} // namespace

PageThumbnailSidebar::PageThumbnailSidebar(QWidget *parent)
    : QWidget(parent) {
  setObjectName(QStringLiteral("PageThumbnailSidebar"));
  setAttribute(Qt::WA_StyledBackground, true);
  const RailMetrics m = railMetrics(this);
  m_expandedWidth = m.width;
  setFixedWidth(m_expandedWidth);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  m_btnToggle = new QPushButton(QStringLiteral("›"), this);
  m_btnToggle->setObjectName(QStringLiteral("PageRailToggleBtn"));
  m_btnToggle->setFixedHeight(UiScale::dp(28));
  m_btnToggle->setCursor(Qt::PointingHandCursor);
  m_btnToggle->setToolTip(QStringLiteral("Seitenmanager einklappen"));
  connect(m_btnToggle, &QPushButton::clicked, this,
          &PageThumbnailSidebar::toggleCollapsed);
  root->addWidget(m_btnToggle);

  m_railBody = new QWidget(this);
  m_railBody->setObjectName(QStringLiteral("PageRailBody"));
  auto *lay = new QVBoxLayout(m_railBody);
  lay->setContentsMargins(m.pad, UiScale::dp(6), m.pad, UiScale::dp(10));
  lay->setSpacing(UiScale::dp(6));

  m_list = new QListWidget(m_railBody);
  m_list->setFrameStyle(QFrame::NoFrame);
  m_list->setSpacing(UiScale::dp(6));
  m_list->setIconSize(QSize(m.thumbW, m.thumbH));
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(m_list, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) { onItemClicked(m_list->row(item)); });
  lay->addWidget(m_list, 1);

  m_btnAddPage = new QPushButton(QStringLiteral("+"), m_railBody);
  m_btnAddPage->setObjectName(QStringLiteral("PageRailAddBtn"));
  m_btnAddPage->setFixedHeight(UiScale::dp(30));
  m_btnAddPage->setCursor(Qt::PointingHandCursor);
  m_btnAddPage->setToolTip(QStringLiteral("Seite hinzufügen"));
  connect(m_btnAddPage, &QPushButton::clicked, this,
          &PageThumbnailSidebar::addPageRequested);
  lay->addWidget(m_btnAddPage);

  root->addWidget(m_railBody, 1);
  refreshListStyle();
}

void PageThumbnailSidebar::setCollapsed(bool collapsed) {
  if (m_collapsed == collapsed)
    return;
  m_collapsed = collapsed;
  applyCollapsedState();
  emit collapsedChanged(m_collapsed);
}

void PageThumbnailSidebar::toggleCollapsed() {
  setCollapsed(!m_collapsed);
}

void PageThumbnailSidebar::applyCollapsedState() {
  if (m_railBody)
    m_railBody->setVisible(!m_collapsed);
  if (m_collapsed) {
    setFixedWidth(UiScale::dp(28));
    if (m_btnToggle) {
      m_btnToggle->setText(QStringLiteral("‹"));
      m_btnToggle->setToolTip(QStringLiteral("Seitenmanager aufklappen"));
    }
  } else {
    if (m_expandedWidth <= 0)
      m_expandedWidth = railMetrics(this).width;
    setFixedWidth(m_expandedWidth);
    if (m_btnToggle) {
      m_btnToggle->setText(QStringLiteral("›"));
      m_btnToggle->setToolTip(QStringLiteral("Seitenmanager einklappen"));
    }
  }
  refreshListStyle();
}

void PageThumbnailSidebar::refreshListStyle() {
  const QString accent = m_accentColor.name(QColor::HexRgb);
  setStyleSheet(QStringLiteral(
      "QWidget#PageThumbnailSidebar {"
      "  background-color: rgba(16, 14, 24, 0.72);"
      "  border-left: 1px solid rgba(120, 130, 160, 0.16);"
      "}"
      "QPushButton#PageRailToggleBtn {"
      "  background: transparent;"
      "  color: rgba(232,228,255,0.70);"
      "  border: none;"
      "  border-bottom: 1px solid rgba(120,130,160,0.16);"
      "  font-size: 16px;"
      "  font-weight: 700;"
      "}"
      "QPushButton#PageRailToggleBtn:hover {"
      "  background: rgba(124,92,252,0.14);"
      "  color: #F4F5FB;"
      "}"));

  if (m_list) {
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
  }

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
  if (!m_list)
    return;
  m_list->clear();
  if (!m_view || !m_view->note())
    return;

  const RailMetrics m = railMetrics(this);
  m_expandedWidth = m.width;
  if (!m_collapsed)
    setFixedWidth(m_expandedWidth);
  m_list->setIconSize(QSize(m.thumbW, m.thumbH));
  const Note *note = m_view->note();
  const int count = note->pages.size();
  for (int i = 0; i < count; ++i) {
    QListWidgetItem *item = new QListWidgetItem(m_list);
    item->setTextAlignment(Qt::AlignCenter | Qt::AlignBottom);
    item->setSizeHint(QSize(m.thumbW, m.itemH));
    item->setText(QStringLiteral("%1").arg(i + 1));
    item->setForeground(QColor(232, 228, 255, 200));
    m_list->addItem(item);
    requestThumbnail(i, item, epoch);
  }

  if (m_currentPage >= 0 && m_currentPage < count)
    m_list->setCurrentRow(m_currentPage);
}

void PageThumbnailSidebar::requestThumbnail(int pageIndex, QListWidgetItem *item,
                                            int epoch) {
  if (!m_view || !item)
    return;
  const RailMetrics m = railMetrics(this);
  const QSize thumbSize(m.thumbW, m.thumbH);
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
